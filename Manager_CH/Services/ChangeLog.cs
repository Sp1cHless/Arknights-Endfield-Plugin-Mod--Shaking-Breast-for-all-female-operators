// Services/ChangeLog.cs - human-readable operation log for the Manager.
// Every user-visible change (apply, character/preset ops, install) is
// appended to logs\manager_changes.log next to the exe.  This is the
// first file to look at when a user reports "my change had no effect".
using System;
using System.IO;

namespace SecondaryMotion.Manager.Services;

public static class ChangeLog {
    static string _path = "";

    public static void Init(string managerDir) {
        try {
            _path = Path.Combine(managerDir, "logs", "manager_changes.log");
            Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
        } catch {
            _path = "";
        }
    }

    public static void Append(string line) {
        if (_path.Length == 0) return;
        try {
            File.AppendAllText(_path,
                DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + " " + line + "\r\n");
        } catch { }
    }
}
