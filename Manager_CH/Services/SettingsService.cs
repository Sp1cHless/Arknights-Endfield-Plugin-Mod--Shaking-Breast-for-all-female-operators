// Services/SettingsService.cs — Manager's own settings file, stored NEXT to
// the Manager exe (so the whole Manager folder is self-contained and
// movable).  Holds the game's SecondaryMotion data directory.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SecondaryMotion.Manager.Services;

public class SettingsService {
    readonly string _path;
    public string? GameDataDir { get; private set; }

    public SettingsService(string managerDir) {
        _path = Path.Combine(managerDir, "settings.json");
        Load();
    }

    public void Load() {
        GameDataDir = null;
        if (!File.Exists(_path)) return;
        try {
            Dictionary<string, object> cfg;
            if (JsonMini.ParseTopLevel(File.ReadAllText(_path, Encoding.UTF8), out cfg))
                GameDataDir = JsonMini.GetStr(cfg, "game_data_dir", "");
            if (GameDataDir != null && GameDataDir.Length == 0) GameDataDir = null;
        } catch { }
    }

    public void Save(string gameDataDir) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"game_data_dir\": ").Append(JsonMini.Str(gameDataDir)).Append("\r\n}\r\n");
        PresetService.AtomicWrite(_path, sb.ToString());
        GameDataDir = gameDataDir;
    }
}
