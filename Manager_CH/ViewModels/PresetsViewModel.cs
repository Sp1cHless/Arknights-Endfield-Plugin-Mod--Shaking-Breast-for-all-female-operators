// ViewModels/PresetsViewModel.cs — preset select / new / duplicate /
// rename / delete / import / export / reset.  Every command has a real
// backend action (PresetService + ConfigService + apply ACK).
using System;
using System.Collections.ObjectModel;
using System.IO;
using Microsoft.Win32;
using SecondaryMotion.Manager.Services;

namespace SecondaryMotion.Manager.ViewModels;

public class PresetsViewModel : ViewModelBase {
    readonly AppCtx _ctx;

    public ObservableCollection<string> Presets { get; } = new();

    string _selected = "Default";
    public string Selected {
        get => _selected;
        set {
            if (value != null && Set(ref _selected, value)) {
                // select = switch preset and hot-apply
                _ctx.SwitchPreset(_selected);
                RefreshModelNotified();
            }
        }
    }

    public string ApplyMessage => _ctx.ApplyMessage;
    public bool IsApplying => _ctx.ApplyStatus == ApplyState.Applying;

    public RelayCommand NewCommand { get; }
    public RelayCommand DuplicateCommand { get; }
    public RelayCommand RenameCommand { get; }
    public RelayCommand DeleteCommand { get; }
    public RelayCommand ImportCommand { get; }
    public RelayCommand ExportCommand { get; }
    public RelayCommand ResetCommand { get; }

    public Func<string, string, string, string?>? AskText;   // injected by UI
    public Func<string, string, bool>? Confirm;

    public event Action? ModelChanged;   // UI re-binds character list

    public PresetsViewModel(AppCtx ctx) {
        _ctx = ctx;
        NewCommand = new RelayCommand(_ => New());
        DuplicateCommand = new RelayCommand(_ => Duplicate());
        RenameCommand = new RelayCommand(_ => Rename());
        DeleteCommand = new RelayCommand(_ => Delete());
        ImportCommand = new RelayCommand(_ => Import());
        ExportCommand = new RelayCommand(_ => Export());
        ResetCommand = new RelayCommand(_ => ResetPreset());
        _ctx.ApplyChanged += () => {
            OnPropertyChanged(nameof(ApplyMessage));
            OnPropertyChanged(nameof(IsApplying));
        };
        RefreshList();
    }

    void RefreshList() {
        Presets.Clear();
        foreach (var n in _ctx.ListPresets()) Presets.Add(n);
        _selected = _ctx.ActivePreset;
        OnPropertyChanged(nameof(Selected));
    }

    void RefreshModelNotified() => ModelChanged?.Invoke();

    string? Ask(string title, string prompt, string def)
        => AskText?.Invoke(title, prompt, def);

    bool Confirm_(string title, string msg)
        => Confirm?.Invoke(title, msg) ?? false;

    void New() {
        string? name = Ask("New Preset", "Preset name:", "");
        if (string.IsNullOrEmpty(name)) return;
        var presets = new PresetService(_ctx.BaseDir, App.ManagerDir);
        _ctx.ReloadPresetIntoModel(name);          // build model (db + empty preset)
        _ctx.Apply();                               // writes preset + config revision+1
        ChangeLog.Append("[Preset] new: " + name);
        RefreshList();
        _selected = name; OnPropertyChanged(nameof(Selected));
        RefreshModelNotified();
    }

    void Duplicate() {
        string? name = Ask("Duplicate Preset", "New preset name:", _ctx.ActivePreset + "_copy");
        if (string.IsNullOrEmpty(name)) return;
        var presets = new PresetService(_ctx.BaseDir, App.ManagerDir);
        string src = presets.PathOf(_ctx.ActivePreset);
        if (File.Exists(src)) File.Copy(src, presets.PathOf(name), false);
        presets.SyncMirror(name);
        ChangeLog.Append("[Preset] duplicate: " + _ctx.ActivePreset + " -> " + name);
        RefreshList();
    }

    void Rename() {
        string? name = Ask("Rename Preset", "New name:", _ctx.ActivePreset);
        if (string.IsNullOrEmpty(name) || name == _ctx.ActivePreset) return;
        var presets = new PresetService(_ctx.BaseDir, App.ManagerDir);
        string src = presets.PathOf(_ctx.ActivePreset);
        if (!File.Exists(src)) return;
        if (File.Exists(presets.PathOf(name))) return;
        File.Move(src, presets.PathOf(name));
        presets.SyncMirror(name);
        presets.SyncMirror("Default");
        ChangeLog.Append("[Preset] rename: " + _ctx.ActivePreset + " -> " + name);
        if (_ctx.ActivePreset == "Default")
            File.Copy(presets.PathOf(name), presets.PathOf("Default"));  // Default is fallback
        _ctx.Config.Write(_ctx.Config.NextRevision(), name, true);
        RefreshList();
        RefreshModelNotified();
    }

    void Delete() {
        if (_ctx.ActivePreset == "Default") return;
        if (!Confirm_("Delete", "Delete preset '" + _ctx.ActivePreset + "'?")) return;
        var presets = new PresetService(_ctx.BaseDir, App.ManagerDir);
        ChangeLog.Append("[Preset] delete: " + _ctx.ActivePreset);
        presets.Delete(_ctx.ActivePreset);
        _ctx.ReloadPresetIntoModel("Default");
        _ctx.Apply();
        RefreshList();
        RefreshModelNotified();
    }

    void Import() {
        var dlg = new OpenFileDialog { Filter = "JSON|*.json", Title = "Import preset" };
        if (dlg.ShowDialog() != true) return;
        string? name = Ask("Import Preset", "Preset name:", Path.GetFileNameWithoutExtension(dlg.FileName));
        if (string.IsNullOrEmpty(name)) return;
        var presets = new PresetService(_ctx.BaseDir, App.ManagerDir);
        File.Copy(dlg.FileName, presets.PathOf(name), true);
        presets.SyncMirror(name);
        ChangeLog.Append("[Preset] import: " + dlg.FileName + " as " + name);
        _ctx.ReloadPresetIntoModel(name);
        _ctx.Apply();
        RefreshList();
        RefreshModelNotified();
    }

    void Export() {
        var dlg = new SaveFileDialog { Filter = "JSON|*.json", FileName = _ctx.ActivePreset + ".json", Title = "Export preset" };
        if (dlg.ShowDialog() != true) return;
        var presets = new PresetService(_ctx.BaseDir, App.ManagerDir);
        File.Copy(presets.PathOf(_ctx.ActivePreset), dlg.FileName, true);
    }

    void ResetPreset() {
        if (!Confirm_("Reset", "Reset preset '" + _ctx.ActivePreset + "' to database defaults?")) return;
        _ctx.ResetFromDb();
        _ctx.Apply();
        RefreshModelNotified();
    }
}
