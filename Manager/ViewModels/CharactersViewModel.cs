// ViewModels/CharactersViewModel.cs — full per-character editor.
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows.Threading;
using SecondaryMotion.Manager.Models;
using SecondaryMotion.Manager.Services;

namespace SecondaryMotion.Manager.ViewModels;

public class CharacterItem : ViewModelBase {
    public CharacterData Data { get; }
    public CharacterItem(CharacterData d) { Data = d; }

    // shown name only — the raw id (chr_xxx) is a developer detail
    public string Display => Data.DisplayName.Length > 0 ? Data.DisplayName : Data.Id;

    public string DisplayName {
        get => Data.DisplayName;
        set { Data.DisplayName = value; OnPropertyChanged(); OnPropertyChanged(nameof(Display)); }
    }

    public bool Enabled {
        get => Data.Enabled;
        set { Data.Enabled = value; OnPropertyChanged(); }
    }

    public int ModeIndex {
        get => Data.Mode == "off" ? 0 : Data.Mode == "amplify_native" ? 2 : 1;
        set {
            Data.Mode = value == 0 ? "off" : value == 2 ? "amplify_native" : "synthetic";
            OnPropertyChanged();
        }
    }

    public double AmpScale { get => Data.AmpScale; set { Data.AmpScale = value; OnPropertyChanged(); } }

    public double IdleAmp { get => Data.Amp[0]; set { Data.Amp[0] = value; OnPropertyChanged(); } }
    public double WalkAmp { get => Data.Amp[1]; set { Data.Amp[1] = value; OnPropertyChanged(); } }
    public double RunAmp { get => Data.Amp[2]; set { Data.Amp[2] = value; OnPropertyChanged(); } }
    public double SprintAmp { get => Data.Amp[3]; set { Data.Amp[3] = value; OnPropertyChanged(); } }
    public double ZiplineAmp { get => Data.Amp[4]; set { Data.Amp[4] = value; OnPropertyChanged(); } }

    // down amplitudes.  RUNTIME NAMING NOTE: the amplitude_deg channel
    // swings the FIRST half-cycle which in-game is the DOWNWARD swing;
    // amplitude_down_deg is the UPWARD swing.  The UI names are swapped
    // against the field names (Up column binds AmpDown etc.).  Display
    // defaults to the symmetric value (= up) when no explicit down is set.
    public double IdleDown { get => Data.AmpDown[0] > 0 ? Data.AmpDown[0] : Data.Amp[0]; set { Data.AmpDown[0] = value; OnPropertyChanged(); } }
    public double WalkDown { get => Data.AmpDown[1] > 0 ? Data.AmpDown[1] : Data.Amp[1]; set { Data.AmpDown[1] = value; OnPropertyChanged(); } }
    public double RunDown { get => Data.AmpDown[2] > 0 ? Data.AmpDown[2] : Data.Amp[2]; set { Data.AmpDown[2] = value; OnPropertyChanged(); } }
    public double SprintDown { get => Data.AmpDown[3] > 0 ? Data.AmpDown[3] : Data.Amp[3]; set { Data.AmpDown[3] = value; OnPropertyChanged(); } }
    public double ZiplineDown { get => Data.AmpDown[4] > 0 ? Data.AmpDown[4] : Data.Amp[4]; set { Data.AmpDown[4] = value; OnPropertyChanged(); } }

    public double IdleFreq { get => Data.Freq[0]; set { Data.Freq[0] = value; OnPropertyChanged(); } }
    public double WalkFreq { get => Data.Freq[1]; set { Data.Freq[1] = value; OnPropertyChanged(); } }
    public double RunFreq { get => Data.Freq[2]; set { Data.Freq[2] = value; OnPropertyChanged(); } }
    public double SprintFreq { get => Data.Freq[3]; set { Data.Freq[3] = value; OnPropertyChanged(); } }
    public double ZiplineFreq { get => Data.Freq[4]; set { Data.Freq[4] = value; OnPropertyChanged(); } }

    public int AxisIndex {
        get => Data.Axis.Length == 0 ? 0 : Array.IndexOf(new[] { "X", "Y", "Z" }, Data.Axis) + 1;
        set {
            Data.Axis = value == 0 ? "" : new[] { "X", "Y", "Z" }[value - 1];
            OnPropertyChanged();
        }
    }

    public int SignIndex { get => Data.AxisSign < 0 ? 1 : 0; set { Data.AxisSign = value == 0 ? 1 : -1; OnPropertyChanged(); } }

    public double EnvAttack { get => Data.EnvAttack; set { Data.EnvAttack = value; OnPropertyChanged(); } }
    public double EnvFreq { get => Data.EnvFreq; set { Data.EnvFreq = value; OnPropertyChanged(); } }
    public double EnvIdle { get => Data.EnvIdle; set { Data.EnvIdle = value; OnPropertyChanged(); } }
    public double NativeFactor { get => Data.NativeFactor; set { Data.NativeFactor = value; OnPropertyChanged(); } }
    public bool JumpEnabled { get => Data.JumpEnabled; set { Data.JumpEnabled = value; OnPropertyChanged(); } }
}

public class CharactersViewModel : ViewModelBase {
    readonly AppCtx _ctx;

    public ObservableCollection<CharacterItem> Items { get; } = new();

    CharacterItem? _selected;
    public CharacterItem? Selected {
        get => _selected;
        set { if (Set(ref _selected, value)) OnPropertyChanged(nameof(HasSelection)); }
    }
    public bool HasSelection => _selected != null;

    public string ApplyMessage => _ctx.ApplyMessage;
    public bool IsApplying => _ctx.ApplyStatus == ApplyState.Applying;

    public RelayCommand ApplyCommand { get; }
    public RelayCommand RefreshCommand { get; }
    public RelayCommand DeleteCommand { get; }

    public CharactersViewModel(AppCtx ctx) {
        _ctx = ctx;
        ApplyCommand = new RelayCommand(_ => {
            _ctx.SaveDisplayNames();
            _ctx.Apply();
        });
        RefreshCommand = new RelayCommand(_ => Refresh());
        DeleteCommand = new RelayCommand(_ => DeleteSelected());
        _ctx.ApplyChanged += () => {
            OnPropertyChanged(nameof(ApplyMessage));
            OnPropertyChanged(nameof(IsApplying));
        };
        Refresh();
    }

    void DeleteSelected() {
        if (Selected == null) return;
        string id = Selected.Data.Id;
        _ctx.RemoveFromPreset(id);
        Refresh();
    }

    public void Refresh() {
        var sel = Selected?.Data.Id;
        Items.Clear();
        foreach (var c in _ctx.Characters) Items.Add(new CharacterItem(c));
        if (sel != null)
            foreach (var it in Items)
                if (it.Data.Id == sel) { Selected = it; break; }
    }
}
