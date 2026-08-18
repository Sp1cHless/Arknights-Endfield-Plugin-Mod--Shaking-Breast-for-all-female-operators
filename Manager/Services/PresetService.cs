// Services/PresetService.cs — presets/<name>.json read/write + CRUD.
// A preset stores the FULL current state of every character (enabled, mode,
// params); bones/axis come from the character DB at runtime.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using SecondaryMotion.Manager.Models;

namespace SecondaryMotion.Manager.Services;

public class PresetService {
    readonly string _presetsDir;

    public PresetService(string baseDir) {
        _presetsDir = Path.Combine(baseDir, "presets");
    }

    public string PathOf(string name) => Path.Combine(_presetsDir, name + ".json");

    public List<string> ListPresets() {
        var names = new List<string>();
        if (!Directory.Exists(_presetsDir)) return names;
        foreach (var f in Directory.GetFiles(_presetsDir, "*.json"))
            names.Add(Path.GetFileNameWithoutExtension(f));
        names.Sort(StringComparer.OrdinalIgnoreCase);
        return names;
    }

    // Merge preset overrides into `chars` (in place).  keepPresetOnly=true
    // keeps characters that exist in the preset but not in the DB.
    public void LoadInto(string name, List<CharacterData> chars,
                         Dictionary<string, CharacterData> byId, bool keepPresetOnly) {
        string p = PathOf(name);
        if (!File.Exists(p)) return;
        var map = new Dictionary<string, Dictionary<string, object>>();
        if (!JsonMini.ParseObjectMap(File.ReadAllText(p, Encoding.UTF8), "characters", map)) return;
        foreach (var kv in map) {
            CharacterData c;
            if (!byId.TryGetValue(kv.Key, out c)) {
                if (!keepPresetOnly) continue;
                c = new CharacterData { Id = kv.Key, DisplayName = kv.Key };
                chars.Add(c);
                byId[c.Id] = c;
            }
            var node = kv.Value;
            c.Enabled = JsonMini.GetBool(node, "enabled", c.Enabled);
            string mode = JsonMini.GetStr(node, "motion_mode", "");
            if (mode.Length > 0) c.Mode = mode;
            c.AmpScale = JsonMini.GetNum(node, "amplitude_scale", c.AmpScale);
            object o;
            if (node.TryGetValue("gait", out o) && o is Dictionary<string, object> gait)
                CharacterDatabaseService.ApplyGait(gait, c);
            if (node.TryGetValue("envelope", out o) && o is Dictionary<string, object> env) {
                c.EnvAttack = JsonMini.GetNum(env, "amplitude_attack_tau_sec", c.EnvAttack);
                c.EnvFreq = JsonMini.GetNum(env, "frequency_tau_sec", c.EnvFreq);
                c.EnvIdle = JsonMini.GetNum(env, "to_idle_release_tau_sec", c.EnvIdle);
            }
            if (node.TryGetValue("jump", out o) && o is Dictionary<string, object> jump)
                c.JumpEnabled = JsonMini.GetBool(jump, "enabled", c.JumpEnabled);
            if (node.TryGetValue("native_amplify", out o) && o is Dictionary<string, object> na)
                c.NativeFactor = JsonMini.GetNum(na, "factor", c.NativeFactor);
            if (node.TryGetValue("axis", out o) && o is Dictionary<string, object> axis) {
                c.Axis = JsonMini.GetStr(axis, "name", c.Axis);
                c.AxisSign = JsonMini.GetNum(axis, "sign", 1.0) < 0 ? -1 : 1;
            }
        }
    }

    // Full preset write (all characters, current state) — atomic.
    public void Write(string name, List<CharacterData> chars) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"schema_version\": 1,\r\n  \"name\": ").Append(JsonMini.Str(name))
          .Append(",\r\n  \"characters\": {\r\n");
        for (int i = 0; i < chars.Count; i++) {
            var c = chars[i];
            sb.Append("    ").Append(JsonMini.Str(c.Id)).Append(": {\r\n");
            sb.Append("      \"enabled\": ").Append(c.Enabled ? "true" : "false").Append(",\r\n");
            sb.Append("      \"motion_mode\": ").Append(JsonMini.Str(c.Mode)).Append(",\r\n");
            sb.Append("      \"amplitude_scale\": ").Append(JsonMini.Num(c.AmpScale)).Append(",\r\n");
            if (c.Axis.Length > 0)
                sb.Append("      \"axis\": { \"name\": ").Append(JsonMini.Str(c.Axis))
                  .Append(", \"sign\": ").Append(c.AxisSign < 0 ? "-1" : "1").Append(" },\r\n");
            sb.Append("      \"gait\": {\r\n");
            string[] gnames = { "idle", "walk", "run", "sprint", "zipline" };
            for (int g = 0; g < 5; g++)
                sb.Append("        ")
                  .Append(CharacterDatabaseService.GaitEntryJson(gnames[g], c.Amp[g], c.AmpDown[g], c.Freq[g]))
                  .Append(g < 4 ? "," : "").Append("\r\n");
            sb.Append("      },\r\n");
            sb.Append("      \"envelope\": {\r\n")
              .Append("        \"amplitude_attack_tau_sec\": ").Append(JsonMini.Num(c.EnvAttack)).Append(",\r\n")
              .Append("        \"frequency_tau_sec\": ").Append(JsonMini.Num(c.EnvFreq)).Append(",\r\n")
              .Append("        \"to_idle_release_tau_sec\": ").Append(JsonMini.Num(c.EnvIdle)).Append("\r\n")
              .Append("      },\r\n");
            sb.Append("      \"jump\": { \"enabled\": ").Append(c.JumpEnabled ? "true" : "false")
              .Append(", \"mode\": \"").Append(c.JumpEnabled ? "landing_damped" : "off").Append("\" },\r\n");
            sb.Append("      \"native_amplify\": { \"factor\": ").Append(JsonMini.Num(c.NativeFactor)).Append(" }\r\n");
            sb.Append("    }").Append(i < chars.Count - 1 ? "," : "").Append("\r\n");
        }
        sb.Append("  }\r\n}\r\n");
        AtomicWrite(PathOf(name), sb.ToString());
    }

    public void Delete(string name) {
        string p = PathOf(name);
        if (File.Exists(p)) File.Delete(p);
    }

    // Remove one character's override entry from a preset file.  The
    // character then falls back to its DB defaults on next load.
    public void RemoveOverride(string name, string characterId) {
        string p = PathOf(name);
        if (!File.Exists(p)) return;
        var chars = new List<CharacterData>();
        var byId = new Dictionary<string, CharacterData>();
        LoadInto(name, chars, byId, true);
        if (!byId.ContainsKey(characterId)) return;
        byId.Remove(characterId);
        chars.RemoveAll(c => c.Id == characterId);
        Write(name, chars);
    }

    public static void AtomicWrite(string path, string content) {
        string tmp = path + ".tmp";
        File.WriteAllText(tmp, content, new UTF8Encoding(false));
        if (File.Exists(path)) File.Delete(path);
        File.Move(tmp, path);
    }
}
