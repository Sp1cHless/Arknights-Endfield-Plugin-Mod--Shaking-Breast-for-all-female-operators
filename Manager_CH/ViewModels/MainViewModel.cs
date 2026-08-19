// ViewModels/MainViewModel.cs — status line + FULL parameter editor for the
// current main character (same fields as the Characters page) + Apply.
using System;
using System.IO;
using System.Windows.Threading;
using SecondaryMotion.Manager.Services;

namespace SecondaryMotion.Manager.ViewModels;

public class MainViewModel : ViewModelBase, IDisposable {
    public static MainViewModel? Instance;
    readonly AppCtx _ctx;
    readonly DispatcherTimer _poll;

    public MainViewModel(AppCtx ctx) {
        Instance = this;
        _ctx = ctx;
        _poll = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
        _poll.Tick += (s, e) => Poll();
        _poll.Start();
        // start tailing the plugin log from its current end (skip history)
        try {
            var lp = Path.Combine(ctx.BaseDir, "..", "..", "plugin", "breast_probe_log.txt");
            if (File.Exists(lp)) _gaitLogPos = new FileInfo(lp).Length;
        } catch { }
        _ctx.ApplyChanged += OnApplyChanged;
        ApplyCommand = new RelayCommand(_ => Apply());
        Poll();
    }

    public void Dispose() {
        _poll.Stop();
        _ctx.ApplyChanged -= OnApplyChanged;
    }

    void OnApplyChanged() {
        OnPropertyChanged(nameof(ApplyMessage));
        OnPropertyChanged(nameof(IsApplying));
    }

    // ---- status ----
    string _gameState = "Not detected";
    public string GameState { get => _gameState; set => Set(ref _gameState, value); }

    string _pluginState = "-";
    public string PluginState { get => _pluginState; set => Set(ref _pluginState, value); }

    string _character = "-";
    public string Character { get => _character; set => Set(ref _character, value); }

    public string Preset => _ctx.ActivePreset + " (rev " + _ctx.Config.Revision + ")";

    public string ApplyMessage => _ctx.ApplyMessage;
    public bool IsApplying => _ctx.ApplyStatus == ApplyState.Applying;

    // Global plugin switch (clean native when off).  Write-through on toggle.
    public bool PluginActive {
        get => _ctx.Config.Enabled;
        set {
            if (_ctx.Config.Enabled == value) return;
            _ctx.SetPluginActive(value);
            OnPropertyChanged();
        }
    }

    public string DataDir => _ctx.BaseDir;

    // ---- current character FULL editor ----
    CharacterItem? _current;
    public CharacterItem? Current {
        get => _current;
        private set {
            if (Set(ref _current, value))
                OnPropertyChanged(nameof(HasCurrent));
        }
    }
    public bool HasCurrent => _current != null;

    public RelayCommand ApplyCommand { get; }

    void Apply() => _ctx.Apply();

    void Poll() {
        var st = _ctx.Runtime.ReadStatus();
        if (st.Fresh && st.FileExists) {
            GameState = "Running";
            PluginState = st.State + (st.Profile && st.Bones ? " | character ready" : " | no character");
            var c = _ctx.Find(st.Character);
            // show the friendly name; the raw id is a developer detail
            Character = c != null && c.DisplayName.Length > 0 ? c.DisplayName
                       : st.Character.Length > 0 ? st.Character : "-";
            // Reuse the current CharacterItem when the character did not
            // change — recreating it every poll resets any in-progress edit
            // (NumberBox text) back to the model value.
            if (c != null) {
                if (_current == null || _current.Data.Id != c.Id)
                    Current = new CharacterItem(c);
            } else {
                Current = null;
            }
        } else if (st.FileExists) {
            GameState = "Game closed (status stale)";
            PluginState = "-";
            Character = "-";
            Current = null;
        } else {
            GameState = "Not detected (no runtime_status.json)";
            PluginState = "-";
            Character = "-";
            Current = null;
        }
        OnPropertyChanged(nameof(Preset));
        OnPropertyChanged(nameof(PluginActive));

        UpdateGaitLog(st);
    }

    // ---- gait-cadence scratch log (dumb but robust) ----
    // Watches the plugin log for [CLIP] *_loop dur=... lines of walk/run/
    // sprint, and appends one line per (character, clip, duration) to
    // <manager dir>\data\gait_log.txt.  Half-loop time (= dur/2, the
    // per-step period for a 2-step cycle) is included for cadence tuning.
    // Character switches add a separator.  The file is deleted when the
    // window closes.
    string _gaitLogPath => Path.Combine(AppContext.BaseDirectory, "data", "gait_log.txt");
    long _gaitLogPos = 0;
    string _gaitLastChar = "";
    string _gaitLastEntry = "";
    readonly System.Text.RegularExpressions.Regex _gaitClipRx =
        new System.Text.RegularExpressions.Regex(
            @"\[CLIP\] (\S+?_(walk|run|sprint)_loop) dur=([0-9.]+)s",
            System.Text.RegularExpressions.RegexOptions.Compiled);

    void UpdateGaitLog(RuntimeStatus st) {
        try {
            var logPath = Path.Combine(
                _ctx.BaseDir, "..", "..", "plugin", "breast_probe_log.txt");
            if (!File.Exists(logPath)) return;
            var fi = new FileInfo(logPath);
            if (fi.Length < _gaitLogPos) _gaitLogPos = 0;  // log rotated
            if (fi.Length == _gaitLogPos) return;

            string name = "?";
            var c = _ctx.Find(st.Character);
            if (c != null && c.DisplayName.Length > 0) name = c.DisplayName;
            else if (st.Character.Length > 0) name = st.Character;

            // character switch -> separator line
            if (_gaitLastChar.Length > 0 && st.Character.Length > 0 &&
                st.Character != _gaitLastChar) {
                File.AppendAllText(_gaitLogPath,
                    Environment.NewLine + "--- switch to " + name + " ---" + Environment.NewLine);
            }
            if (st.Character.Length > 0) _gaitLastChar = st.Character;

            using (var fs = new FileStream(logPath, FileMode.Open,
                       FileAccess.Read, FileShare.ReadWrite | FileShare.Delete)) {
                fs.Seek(_gaitLogPos, SeekOrigin.Begin);
                var buf = new byte[fi.Length - _gaitLogPos];
                int rd = fs.Read(buf, 0, buf.Length);
                _gaitLogPos = fi.Length;
                if (rd <= 0) return;
                var txt = System.Text.Encoding.UTF8.GetString(buf, 0, rd);
                foreach (System.Text.RegularExpressions.Match m in _gaitClipRx.Matches(txt)) {
                    string gait = m.Groups[2].Value;
                    double dur = double.Parse(m.Groups[3].Value,
                                              System.Globalization.CultureInfo.InvariantCulture);
                    string entry = name + " | " + gait + " | loop=" +
                        dur.ToString("0.000", System.Globalization.CultureInfo.InvariantCulture) +
                        "s | half=" + (dur / 2).ToString("0.000",
                            System.Globalization.CultureInfo.InvariantCulture) + "s";
                    if (entry == _gaitLastEntry) continue;  // dedup
                    _gaitLastEntry = entry;
                    File.AppendAllText(_gaitLogPath, entry + Environment.NewLine);
                }
            }
        } catch {
            // scratch feature: never crash the UI for it
        }
    }

    public void Shutdown() {
        try { if (File.Exists(_gaitLogPath)) File.Delete(_gaitLogPath); } catch { }
    }
}
