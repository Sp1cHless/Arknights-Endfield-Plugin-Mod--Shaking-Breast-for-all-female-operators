// Services/ConfigService.cs — runtime/config.json (revision, active preset,
// enabled).  The revision bump is the hot-apply trigger for the runtime.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SecondaryMotion.Manager.Services;

public class ConfigService {
    readonly string _path;

    public ConfigService(string baseDir) {
        _path = Path.Combine(baseDir, "runtime", "config.json");
    }

    public int Revision { get; private set; }
    public string ActivePreset { get; private set; } = "Default";
    public bool Enabled { get; private set; } = true;
    public bool Loaded { get; private set; }

    public void Load() {
        Loaded = false;
        if (!File.Exists(_path)) return;
        try {
            Dictionary<string, object> cfg;
            if (!JsonMini.ParseTopLevel(File.ReadAllText(_path, Encoding.UTF8), out cfg)) return;
            Revision = (int)JsonMini.GetNum(cfg, "revision", 0);
            ActivePreset = JsonMini.GetStr(cfg, "active_preset", "Default");
            Enabled = JsonMini.GetBool(cfg, "enabled", true);
            Loaded = true;
        } catch { }
    }

    public int NextRevision() => Revision + 1;

    // Atomic write with the NEW revision (apply trigger).
    public void Write(int newRevision, string presetName, bool enabled) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"revision\": ").Append(newRevision).Append(",\r\n")
          .Append("  \"enabled\": ").Append(enabled ? "true" : "false").Append(",\r\n")
          .Append("  \"active_preset\": ").Append(JsonMini.Str(presetName)).Append("\r\n}\r\n");
        PresetService.AtomicWrite(_path, sb.ToString());
        Revision = newRevision;
        ActivePreset = presetName;
        Enabled = enabled;
    }
}
