// Services/DevCommandService.cs — writes runtime/developer_command.json
// (atomic, revision+1 each write).  The runtime worker parses it and only
// consumes plain command state; no direct IL2CPP interaction.
using System;
using System.Text;

namespace SecondaryMotion.Manager.Services;

public class DevCommandService {
    readonly string _path;
    int _revision = 0;

    public DevCommandService(string baseDir) {
        _path = System.IO.Path.Combine(baseDir, "runtime", "developer_command.json");
    }

    public void Clear() => Write("none");

    public void AxisTest(string axis, double sign, double angleDeg, int expiresMs = 5000) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"revision\": ").Append(++_revision).Append(",\r\n")
          .Append("  \"command\": \"axis_test\",\r\n")
          .Append("  \"axis\": ").Append(JsonMini.Str(axis)).Append(",\r\n")
          .Append("  \"sign\": ").Append(sign < 0 ? "-1" : "1").Append(",\r\n")
          .Append("  \"angle_deg\": ").Append(JsonMini.Num(angleDeg)).Append(",\r\n")
          .Append("  \"expires_ms\": ").Append(expiresMs).Append("\r\n}\r\n");
        PresetService.AtomicWrite(_path, sb.ToString());
    }

    public void OneShot(string command) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"revision\": ").Append(++_revision).Append(",\r\n")
          .Append("  \"command\": ").Append(JsonMini.Str(command)).Append("\r\n}\r\n");
        PresetService.AtomicWrite(_path, sb.ToString());
    }

    public void Record(bool start) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"revision\": ").Append(++_revision).Append(",\r\n")
          .Append("  \"command\": \"record\",\r\n")
          .Append("  \"start\": ").Append(start ? "true" : "false").Append("\r\n}\r\n");
        PresetService.AtomicWrite(_path, sb.ToString());
    }

    public void Write(string command) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"revision\": ").Append(++_revision).Append(",\r\n")
          .Append("  \"command\": ").Append(JsonMini.Str(command)).Append("\r\n}\r\n");
        PresetService.AtomicWrite(_path, sb.ToString());
    }
}
