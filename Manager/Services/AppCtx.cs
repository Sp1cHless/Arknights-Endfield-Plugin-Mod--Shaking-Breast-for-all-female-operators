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

    public AppCtx(string baseDir) {
        BaseDir = baseDir;
        Db = new CharacterDatabaseService(baseDir);
        Presets = new PresetService(baseDir);
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
        SetApply(ApplyState.Idle, "Database reloaded — " + Characters.Count + " characters");
    }

    // ---- apply ----
    public void Apply() {
        try {
            int rev = Config.NextRevision();
            Presets.Write(Config.ActivePreset, Characters);
            Config.Write(rev, Config.ActivePreset, true);
            StartAck(rev);
        } catch (Exception ex) {
            SetApply(ApplyState.Error, "Apply failed: " + ex.Message);
        }
    }

    // Global plugin switch: write config.json (enabled + revision+1) only.
    // enabled=false -> runtime stops ALL writes (clean native game).
    public void SetPluginActive(bool active) {
        try {
            int rev = Config.NextRevision();
            Config.Write(rev, Config.ActivePreset, active);
            StartAck(rev);
        } catch (Exception ex) {
            SetApply(ApplyState.Error, "Switch failed: " + ex.Message);
        }
    }

    void StartAck(int rev) {
        _pendingRevision = rev;
        _applyStartMs = Environment.TickCount;
        SetApply(ApplyState.Applying, "Applying...");
        _ackTimer.Start();
    }

    void PollAck() {
        if (_pendingRevision < 0) return;
        if (Runtime.IsApplied(_pendingRevision)) {
            _ackTimer.Stop();
            int rev = _pendingRevision;
            _pendingRevision = -1;
            SetApply(ApplyState.Applied, "Applied ✓ (revision " + rev + ")");
            return;
        }
        if (Environment.TickCount - _applyStartMs > 15000) {
            _ackTimer.Stop();
            _pendingRevision = -1;
            SetApply(ApplyState.SavedForLater, "Saved — will apply when game starts");
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
