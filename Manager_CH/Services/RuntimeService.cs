// Services/RuntimeService.cs — runtime/runtime_status.json polling + apply
// ACK.  UI polls via Tick; ApplyAsync writes preset + config (revision+1)
// and waits for applied_revision == revision.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SecondaryMotion.Manager.Services;

public class RuntimeStatus {
    public string State = "";
    public int AppliedRevision = -1;
    public string Character = "";
    public bool Profile = false;
    public bool Bones = false;
    public string Mode = "";
    public string Gait = "";
    public bool Fresh = false;      // file rewritten recently (game running)
    public bool FileExists = false;
}

public class RuntimeService {
    readonly string _statusPath;
    readonly string _baseDir;

    public RuntimeService(string baseDir) {
        _baseDir = baseDir;
        _statusPath = Path.Combine(baseDir, "runtime", "runtime_status.json");
    }

    public RuntimeStatus ReadStatus() {
        var s = new RuntimeStatus();
        if (!File.Exists(_statusPath)) return s;
        s.FileExists = true;
        try {
            s.Fresh = (DateTime.Now - File.GetLastWriteTime(_statusPath)).TotalSeconds < 5;
            Dictionary<string, object> cfg;
            if (!JsonMini.ParseTopLevel(File.ReadAllText(_statusPath, Encoding.UTF8), out cfg)) return s;
            s.State = JsonMini.GetStr(cfg, "state", "");
            s.AppliedRevision = (int)JsonMini.GetNum(cfg, "applied_revision", -1);
            s.Character = JsonMini.GetStr(cfg, "character", "");
            s.Profile = JsonMini.GetBool(cfg, "profile", false);
            s.Bones = JsonMini.GetBool(cfg, "bones", false);
            s.Mode = JsonMini.GetStr(cfg, "mode", "");
            s.Gait = JsonMini.GetStr(cfg, "gait", "");
        } catch { }
        return s;
    }

    // True once runtime_status.applied_revision == revision.
    public bool IsApplied(int revision) {
        var s = ReadStatus();
        return s.AppliedRevision == revision;
    }
}
