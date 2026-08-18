// ViewModels/DeveloperViewModel.cs — Developer Mode (V2 §16-28):
// Detect -> Scan -> Bones -> Axis Test -> Tune -> Validate -> Save.
// All commands go through the runtime developer_command.json channel.
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Text;
using System.Windows.Threading;
using SecondaryMotion.Manager.Models;
using SecondaryMotion.Manager.Services;

namespace SecondaryMotion.Manager.ViewModels;

public class DeveloperViewModel : ViewModelBase, IDisposable {
    readonly AppCtx _ctx;
    readonly DevCommandService _dev;
    readonly DispatcherTimer _poll;
    string _lastScanChar = "";

    public DeveloperViewModel(AppCtx ctx) {
        _ctx = ctx;
        _dev = new DevCommandService(ctx.BaseDir);
        ScanCommand = new RelayCommand(_ => Scan());
        StartTestCommand = new RelayCommand(_ => StartAxisTest());
        StopTestCommand = new RelayCommand(_ => StopAxisTest());
        ValidateCommand = new RelayCommand(_ => Validate());
        SaveCommand = new RelayCommand(_ => Save());
        SetRightCommand = new RelayCommand(_ => {
            if (SelectedBone.Length > 0) RightBone = SelectedBone;
        });
        SetLeftCommand = new RelayCommand(_ => {
            if (SelectedBone.Length > 0) LeftBone = SelectedBone;
        });
        _poll = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        _poll.Tick += (s, e) => Poll();
        _poll.Start();
        Poll();
    }

    public void Dispose() => _poll.Stop();

    // ---- Detect ----
    string _character = "-";
    public string Character { get => _character; set => Set(ref _character, value); }

    string _dbStatus = "-";
    public string DbStatus { get => _dbStatus; set => Set(ref _dbStatus, value); }

    bool _supported;
    public bool IsSupported { get => _supported; set => Set(ref _supported, value); }

    // ---- Scan ----
    public ObservableCollection<string> Bones { get; } = new();
    List<string> _allBones = new();
    string _scanStatus = "Not scanned";
    public string ScanStatus { get => _scanStatus; set => Set(ref _scanStatus, value); }

    string _boneFilter = "breast,xiong";
    public string BoneFilter {
        get => _boneFilter;
        set { if (Set(ref _boneFilter, value)) ApplyBoneFilter(); }
    }
    void ApplyBoneFilter() {
        Bones.Clear();
        var kws = _boneFilter.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        foreach (var b in _allBones) {
            if (kws.Length == 0) { Bones.Add(b); continue; }
            foreach (var k in kws)
                if (b.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0) { Bones.Add(b); break; }
        }
    }
    public RelayCommand ScanCommand { get; }

    // ---- Axis Test ----
    public int AxisIndex { get; set; } = 2;    // X=0 Y=1 Z=2
    public int SignIndex { get; set; } = 0;    // +=0 -=1
    public double TestAngle { get; set; } = 5.0;
    bool _testActive;
    public bool TestActive { get => _testActive; set => Set(ref _testActive, value); }
    public RelayCommand StartTestCommand { get; }
    public RelayCommand StopTestCommand { get; }

    // ---- Validate / Save ----
    public ObservableCollection<string> Validation { get; } = new();
    string _saveStatus = "";
    public string SaveStatus { get => _saveStatus; set => Set(ref _saveStatus, value); }
    public RelayCommand ValidateCommand { get; }
    public RelayCommand SaveCommand { get; }

    // ---- unknown-character wizard (bone picker) ----
    string _selectedBone = "";
    public string SelectedBone { get => _selectedBone; set => Set(ref _selectedBone, value); }
    string _rightBone = "";
    public string RightBone { get => _rightBone; set => Set(ref _rightBone, value); }
    string _leftBone = "";
    public string LeftBone { get => _leftBone; set => Set(ref _leftBone, value); }
    string _newDisplayName = "";
    public string NewDisplayName { get => _newDisplayName; set => Set(ref _newDisplayName, value); }
    public RelayCommand SetRightCommand { get; }
    public RelayCommand SetLeftCommand { get; }

    void Poll() {
        var st = _ctx.Runtime.ReadStatus();
        if (st.Fresh && st.Character.Length > 0) {
            Character = st.Character;
            var c = _ctx.Find(st.Character);
            IsSupported = c != null;
            DbStatus = c != null ? "Supported" : "Unknown";
            if (c != null && c.DisplayName.Length > 0) Character += " (" + c.DisplayName + ")";
        } else {
            Character = "-";
            DbStatus = "-";
            IsSupported = false;
        }
        OnPropertyChanged(nameof(IsSupported));

        // scan completion: bone_dumps/<char>.json appeared
        if (_lastScanChar.Length > 0 && st.Character.Length > 0) {
            var dump = BoneDumpPath(_lastScanChar);
            if (File.Exists(dump)) {
                try {
                    _allBones.Clear();
                    foreach (var line in File.ReadAllLines(dump, Encoding.UTF8)) {
                        var t = line.Trim();
                        if (t.StartsWith("{\"name\"")) {
                            int i = t.IndexOf("\"name\": \"") + 9;
                            int j = t.IndexOf("\", \"depth\"", i);
                            if (i > 8 && j > i) _allBones.Add(t.Substring(i, j - i));
                        }
                    }
                    ApplyBoneFilter();
                    ScanStatus = "Done — " + _allBones.Count + " bones dumped (" + Bones.Count + " shown)";
                    _lastScanChar = "";
                } catch { }
            }
        }
    }

    string BoneDumpPath(string charId) {
        return Path.Combine(_ctx.BaseDir, "developer", "bone_dumps", charId + ".json");
    }

    void Scan() {
        if (Character.StartsWith("-")) { ScanStatus = "No character detected — enter the game first"; return; }
        string id = _ctx.Runtime.ReadStatus().Character;
        if (id.Length == 0) { ScanStatus = "No character id"; return; }
        _lastScanChar = id;
        Bones.Clear();
        ScanStatus = "Scanning... (check the game)";
        _dev.OneShot("bone_scan");
    }

    void StartAxisTest() {
        string axis = AxisIndex == 0 ? "X" : AxisIndex == 1 ? "Y" : "Z";
        double sign = SignIndex == 0 ? 1 : -1;
        _dev.AxisTest(axis, sign, TestAngle, 5000);
        TestActive = true;
    }

    void StopAxisTest() {
        _dev.Clear();
        TestActive = false;
    }

    void Validate() {
        Validation.Clear();
        var st = _ctx.Runtime.ReadStatus();
        string id = st.Character;
        Validation.Add("Character ID: " + (id.Length > 0 ? "PASS (" + id + ")" : "FAIL"));
        bool supported = _ctx.Find(id) != null;
        Validation.Add("Database support: " + (supported
            ? "PASS"
            : "FAIL — unknown character (Scan -> pick Right/Left bones -> Save first)"));
        Validation.Add("Bones found (runtime): " + (st.Bones ? "PASS" : (supported ? "FAIL" : "n/a until saved")));
        Validation.Add("Runtime write active: " + (st.Fresh ? "PASS" : "FAIL — game not running"));
        Validation.Add("Profile enabled: " + (st.Profile ? "PASS" : (supported ? "FAIL" : "n/a until saved")));
        if (supported) {
            var c = _ctx.Find(id);
            Validation.Add("Axis: " + (c != null && c.Axis.Length > 0 ? "PASS (" + c.Axis + ")" : "PASS (auto)"));
            Validation.Add("Scale finite: " + (c != null && double.IsFinite(c.AmpScale) ? "PASS" : "FAIL"));
            bool gaitOk = c != null;
            if (c != null)
                foreach (var f in c.Freq) if (f <= 0 || double.IsNaN(f)) gaitOk = false;
            Validation.Add("Gait parameters: " + (gaitOk ? "PASS" : "FAIL"));
        }
        bool allPass = true;
        foreach (var v in Validation) if (v.Contains("FAIL")) allPass = false;
        SaveStatus = allPass ? "READY TO SAVE" : "Fix the FAIL items above";
        OnPropertyChanged(nameof(SaveStatus));
    }

    void Save() {
        var st = _ctx.Runtime.ReadStatus();
        string id = st.Character;
        if (id.Length == 0) {
            SaveStatus = "Save failed: no character detected in game";
            OnPropertyChanged(nameof(SaveStatus));
            return;
        }
        var existing = _ctx.Find(id);
        string right = RightBone.Length > 0 ? RightBone : existing?.BoneRight ?? "";
        string left = LeftBone.Length > 0 ? LeftBone : existing?.BoneLeft ?? "";
        if (right.Length == 0 || left.Length == 0) {
            SaveStatus = "Save failed: pick Right and Left bones from the scan list first";
            OnPropertyChanged(nameof(SaveStatus));
            return;
        }
        string display = NewDisplayName.Trim().Length > 0
            ? NewDisplayName.Trim()
            : existing != null && existing.DisplayName.Length > 0 ? existing.DisplayName : id;
        try {
            var data = existing ?? new CharacterData { Id = id };
            data.DisplayName = display;
            // NEW characters inherit Aurora's CURRENT params (user-tuned)
            // as their default template: gait/envelope/scale/mode/jump.
            // Existing saves keep their own params (only name/bones update).
            if (existing == null) {
                var aurora = _ctx.Find("chr_0014_aurora");
                if (aurora != null) {
                    data.Amp = (double[])aurora.Amp.Clone();
                    data.AmpDown = (double[])aurora.AmpDown.Clone();
                    data.Freq = (double[])aurora.Freq.Clone();
                    data.AmpScale = aurora.AmpScale;
                    data.EnvAttack = aurora.EnvAttack;
                    data.EnvFreq = aurora.EnvFreq;
                    data.EnvIdle = aurora.EnvIdle;
                    data.Mode = aurora.Mode;
                    data.NativeFactor = aurora.NativeFactor;
                    data.JumpEnabled = aurora.JumpEnabled;
                }
            }
            _ctx.Db.SaveCharacter(data, display, right, left);
            // reload the merged model so the character becomes supported
            _ctx.Reload();
            // bump revision so the runtime reloads the database
            _ctx.Config.Write(_ctx.Config.NextRevision(), _ctx.ActivePreset, true);
            SaveStatus = "Saved ✓ — " + id + " (" + display + ") in characters.default.json";
        } catch (Exception ex) {
            SaveStatus = "Save failed: " + ex.Message;
        }
        OnPropertyChanged(nameof(SaveStatus));
    }
}
