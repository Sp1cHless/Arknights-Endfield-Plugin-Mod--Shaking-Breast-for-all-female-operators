// Services/AppCtx.cs — shared service container + apply coordinator.
// One instance for the whole app; ViewModels bind to it.  Apply flow:
//   PresetService.Write -> ConfigService.Write(revision+1) -> poll
//   runtime_status.applied_revision until it matches (or timeout).
using System;
using System.Collections.Generic;
using System.IO;
using System.Windows.Threading;
using SecondaryMotion.Manager.Models;

namespace SecondaryMotion.Manager.Services;

public enum ApplyState { Idle, Applying, Applied, SavedForLater, Error }

public class AppCtx {
    public readonly string BaseDir;
    public readonly CharacterDatabaseService Db;
    public readonly PresetService Presets;
    public readonly ConfigService Config;
    public readonly RuntimeService Runtime;

    public List<CharacterData> Characters = new();
    public Dictionary<string, CharacterData> ById = new();

    public ApplyState ApplyStatus { get; private set; } = ApplyState.Idle;
    public string ApplyMessage { get; private set; } = "";
    public event Action? ApplyChanged;

    int _pendingRevision = -1;
    long _applyStartMs = 0;
    readonly DispatcherTimer _ackTimer;

    public AppCtx(string baseDir, string managerDir) {
        BaseDir = baseDir;
        Db = new CharacterDatabaseService(baseDir, managerDir);
        Presets = new PresetService(baseDir, managerDir);
        Config = new ConfigService(baseDir);
        Runtime = new RuntimeService(baseDir);
        _ackTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
        _ackTimer.Tick += (s, e) => PollAck();
    }

    public bool Load() {
        Characters = Db.Load();
        ById = new Dictionary<string, CharacterData>();
        foreach (var c in Characters) ById[c.Id] = c;
        Config.Load();
        Presets.LoadInto(Config.ActivePreset, Characters, ById, false);
        return Db.Loaded || Characters.Count > 0;
    }

    public CharacterData? Find(string id) => ById.TryGetValue(id, out var c) ? c : null;

    // Save edited display names back to the DB (names live in the DB, not
    // presets).  Called from CharactersViewModel before the preset apply.
    public bool SaveDisplayNames() {
        bool changed = false;
        foreach (var c in Characters) {
            if (c.OriginalDisplayName != c.DisplayName) {
                Db.SaveDisplayName(c.Id, c.DisplayName);
                c.OriginalDisplayName = c.DisplayName;
                changed = true;
            }
        }
        return changed;
    }

    // Remove a character's override entry from the current preset.
    public void RemoveFromPreset(string id) {
        Presets.RemoveOverride(Config.ActivePreset, id);
        int rev = Config.NextRevision();
        Config.Write(rev, Config.ActivePreset, true);
    }

    // Full delete: remove the DB entry (character becomes "unknown" again,
    // Developer wizard can re-scan) + drop its preset override + reload.
    public void DeleteCharacter(string id) {
        Db.DeleteCharacter(id);
        Presets.RemoveOverride(Config.ActivePreset, id);
        Reload();
        int rev = Config.NextRevision();
        Config.Write(rev, Config.ActivePreset, true);
    }

    public string ActivePreset => Config.ActivePreset;

    // Reload the database + merged model (after a wizard Save).
    public void Reload() {
        Load();
        SetApply(ApplyState.Idle, L10n.Get("Msg_DbReloaded", Characters.Count));
    }

    // ---- apply ----
    public void Apply() {
        try {
            int rev = Config.NextRevision();
            // diff against the LAST Apply, not the instant before/after this
            // one — an Apply is triggered by the user exactly when the model
            // already holds their edits, so an instant compare is always
            // "(no change)".
            var before = _lastApplied;
            Presets.Write(Config.ActivePreset, Characters);
            // mirror the preset into the game data dir - the runtime reads
            // presets ONLY from the game dir, so an Apply without this mirror
            // updates the tool folder but the game keeps the old values.
            Presets.SyncMirror(Config.ActivePreset);
            Config.Write(rev, Config.ActivePreset, true);
            var after = Snapshot();
            _lastApplied = after;
            if (before != null)
                ChangeLog.Append("[Apply] rev=" + rev + " preset=" + Config.ActivePreset +
                                 ": " + DiffText(before, after));
            else
                ChangeLog.Append("[Apply] rev=" + rev + " preset=" + Config.ActivePreset +
                                 " (first apply)");
            StartAck(rev);
        } catch (Exception ex) {
            ChangeLog.Append("[Apply] ERROR: " + ex.Message);
            SetApply(ApplyState.Error, L10n.Get("Msg_ApplyFailed", ex.Message));
        }
    }

    Dictionary<string, string>? _lastApplied;

    // Compact per-character state for change detection.
    Dictionary<string, string> Snapshot() {
        var d = new Dictionary<string, string>();
        foreach (var c in Characters)
            d[c.Id] = Compact(c);
        return d;
    }

    static string Compact(CharacterData c) {
        string g = "";
        for (int i = 0; i < 5; i++)
            g += (i == 0 ? "" : " ") + c.Amp[i] + "/" + c.AmpDown[i] + "/" + c.Freq[i];
        return "en=" + (c.Enabled ? 1 : 0) + " mo=" + c.Mode + " ax=" + c.Axis +
               (c.AxisSign < 0 ? "-" : "+") + " sc=" + c.AmpScale + " ga=[" + g + "]" +
               " env=" + c.EnvAttack + "/" + c.EnvFreq + "/" + c.EnvIdle +
               " nat=" + c.NativeFactor + " jmp=" + (c.JumpEnabled ? 1 : 0);
    }

    static string DiffText(Dictionary<string, string> before,
                           Dictionary<string, string> after) {
        var parts = new List<string>();
        foreach (var kv in after) {
            if (!before.TryGetValue(kv.Key, out var old))
                parts.Add("+" + kv.Key + " " + kv.Value);
            else if (old != kv.Value)
                parts.Add(kv.Key + " {" + old + "} -> {" + kv.Value + "}");
        }
        foreach (var kv in before)
            if (!after.ContainsKey(kv.Key))
                parts.Add("-" + kv.Key);
        return parts.Count == 0 ? "(no change)" : string.Join(" | ", parts);
    }

    // Global plugin switch: write config.json (enabled + revision+1) only.
    // enabled=false -> runtime stops ALL writes (clean native game).
    public void SetPluginActive(bool active) {
        try {
            int rev = Config.NextRevision();
            Config.Write(rev, Config.ActivePreset, active);
            StartAck(rev);
        } catch (Exception ex) {
            SetApply(ApplyState.Error, L10n.Get("Msg_SwitchFailed", ex.Message));
        }
    }

    void StartAck(int rev) {
        _pendingRevision = rev;
        _applyStartMs = Environment.TickCount;
        SetApply(ApplyState.Applying, L10n.Get("Msg_Applying"));
        _ackTimer.Start();
    }

    void PollAck() {
        if (_pendingRevision < 0) return;
        if (Runtime.IsApplied(_pendingRevision)) {
            _ackTimer.Stop();
            int rev = _pendingRevision;
            _pendingRevision = -1;
            SetApply(ApplyState.Applied, L10n.Get("Msg_Applied", rev));
            return;
        }
        if (Environment.TickCount - _applyStartMs > 15000) {
            _ackTimer.Stop();
            _pendingRevision = -1;
            SetApply(ApplyState.SavedForLater, L10n.Get("Msg_SavedForLater"));
        }
    }

    void SetApply(ApplyState st, string msg) {
        ApplyStatus = st;
        ApplyMessage = msg;
        ApplyChanged?.Invoke();
    }

    // ---- preset CRUD helpers used by PresetsViewModel ----
    public void SwitchPreset(string name) {
        // load the target preset over the DB, then apply it
        Characters = Db.Load();
        ById = new Dictionary<string, CharacterData>();
        foreach (var c in Characters) ById[c.Id] = c;
        Presets.LoadInto(name, Characters, ById, true);
        int rev = Config.NextRevision();
        Config.Write(rev, name, true);
        Apply();  // writes the preset file too (full snapshot)
    }

    public void ReloadPresetIntoModel(string name) {
        Characters = Db.Load();
        ById = new Dictionary<string, CharacterData>();
        foreach (var c in Characters) ById[c.Id] = c;
        Presets.LoadInto(name, Characters, ById, true);
    }

    public List<string> ListPresets() => Presets.ListPresets();

    public void ResetFromDb() {
        Characters = Db.Load();
        ById = new Dictionary<string, CharacterData>();
        foreach (var c in Characters) ById[c.Id] = c;
    }
}
