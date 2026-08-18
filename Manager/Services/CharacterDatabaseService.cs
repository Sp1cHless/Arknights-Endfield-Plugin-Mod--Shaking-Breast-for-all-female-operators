// Services/CharacterDatabaseService.cs — data/characters.default.json
// (technical facts: display name, bones/axis are shown; defaults feed the
// merged model when the preset has no override).
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using SecondaryMotion.Manager.Models;

namespace SecondaryMotion.Manager.Services;

public class CharacterDatabaseService {
    readonly string _path;
    public bool Loaded { get; private set; }

    public CharacterDatabaseService(string baseDir) {
        _path = Path.Combine(baseDir, "data", "characters.default.json");
    }

    public List<CharacterData> Load() {
        var list = new List<CharacterData>();
        Loaded = false;
        if (!File.Exists(_path)) return list;
        try {
            var map = new Dictionary<string, Dictionary<string, object>>();
            if (!JsonMini.ParseObjectMap(File.ReadAllText(_path, Encoding.UTF8), "characters", map))
                return list;
            foreach (var kv in map) {
                var c = new CharacterData { Id = kv.Key };
                var node = kv.Value;
                c.DisplayName = JsonMini.GetStr(node, "display_name", "");
                c.OriginalDisplayName = c.DisplayName;
                object o;
                if (node.TryGetValue("bones", out o) && o is Dictionary<string, object> bones) {
                    c.BoneRight = JsonMini.GetStr(bones, "right", "");
                    c.BoneLeft = JsonMini.GetStr(bones, "left", "");
                }
                if (node.TryGetValue("defaults", out o) && o is Dictionary<string, object> defs) {
                    if (defs.TryGetValue("gait", out o) && o is Dictionary<string, object> gait)
                        ApplyGait(gait, c);
                    if (defs.TryGetValue("envelope", out o) && o is Dictionary<string, object> env) {
                        c.EnvAttack = JsonMini.GetNum(env, "amplitude_attack_tau_sec", c.EnvAttack);
                        c.EnvFreq = JsonMini.GetNum(env, "frequency_tau_sec", c.EnvFreq);
                        c.EnvIdle = JsonMini.GetNum(env, "to_idle_release_tau_sec", c.EnvIdle);
                    }
                    if (defs.TryGetValue("native_amplify", out o) && o is Dictionary<string, object> na)
                        c.NativeFactor = JsonMini.GetNum(na, "factor", c.NativeFactor);
                }
                if (node.TryGetValue("axis", out o) && o is Dictionary<string, object> axis) {
                    c.Axis = JsonMini.GetStr(axis, "name", "");
                    c.AxisSign = JsonMini.GetNum(axis, "sign", 1.0) < 0 ? -1 : 1;
                }
                list.Add(c);
            }
            Loaded = true;
        } catch { }
        return list;
    }

    public static void ApplyGait(Dictionary<string, object> gait, CharacterData c) {
        string[] names = { "idle", "walk", "run", "sprint", "zipline" };
        for (int i = 0; i < 5; i++) {
            object o;
            if (!gait.TryGetValue(names[i], out o) || !(o is Dictionary<string, object> g)) continue;
            c.Amp[i] = JsonMini.GetNum(g, "amplitude_deg", c.Amp[i]);
            c.AmpDown[i] = JsonMini.GetNum(g, "amplitude_down_deg", 0.0);
            c.Freq[i] = JsonMini.GetNum(g, "frequency_hz", c.Freq[i]);
        }
    }

    // Serialize one gait entry (used by preset + DB writers).
    public static string GaitEntryJson(string name, double amp, double down, double freq) {
        var sb = new StringBuilder();
        sb.Append(JsonMini.Str(name))
          .Append(": { \"amplitude_deg\": ").Append(JsonMini.Num(amp));
        if (down > 0)
            sb.Append(", \"amplitude_down_deg\": ").Append(JsonMini.Num(down));
        sb.Append(", \"frequency_hz\": ").Append(JsonMini.Num(freq)).Append(" }");
        return sb.ToString();
    }

    // Update only the display_name of an existing entry (keeps bones/axis/
    // defaults untouched).  Backup + atomic write.
    public void SaveDisplayName(string id, string newName) {
        Dictionary<string, object> root;
        if (!File.Exists(_path)) return;
        JsonMini.ParseTopLevel(File.ReadAllText(_path, Encoding.UTF8), out root);
        if (!root.ContainsKey("characters")) return;
        var chars = (Dictionary<string, object>)root["characters"];
        if (!chars.ContainsKey(id)) return;
        var entry = (Dictionary<string, object>)chars[id];
        entry["display_name"] = newName;
        File.Copy(_path, _path + ".bak", true);
        PresetService.AtomicWrite(_path, SerializeCharacters(chars));
    }

    static string SerializeCharacters(Dictionary<string, object> chars) {
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"schema_version\": 1,\r\n  \"characters\": {\r\n");
        int i = 0;
        foreach (var kv in chars) {
            sb.Append("    ").Append(JsonMini.Str(kv.Key)).Append(": ")
              .Append(WriteEntry((Dictionary<string, object>)kv.Value))
              .Append(i < chars.Count - 1 ? "," : "").Append("\r\n");
            i++;
        }
        sb.Append("  }\r\n}\r\n");
        return sb.ToString();
    }

    // Write/update one character entry in characters.default.json
    // (backup .bak -> atomic tmp+rename).  Used by the Developer Wizard.
    public void SaveCharacter(CharacterData c, string displayName,
                              string rightBone, string leftBone) {
        Dictionary<string, object> root;
        if (File.Exists(_path))
            JsonMini.ParseTopLevel(File.ReadAllText(_path, Encoding.UTF8), out root);
        else
            root = new Dictionary<string, object>();
        if (!root.ContainsKey("characters"))
            root["characters"] = new Dictionary<string, object>();
        var chars = (Dictionary<string, object>)root["characters"];

        var entry = new Dictionary<string, object>();
        entry["display_name"] = displayName;
        var bones = new Dictionary<string, object>();
        bones["right"] = rightBone;
        bones["left"] = leftBone;
        bones["allow_fallback_candidates"] = true;
        entry["bones"] = bones;
        if (c.Axis.Length > 0) {
            var axis = new Dictionary<string, object>();
            axis["name"] = c.Axis;
            axis["sign"] = c.AxisSign < 0 ? -1.0 : 1.0;
            entry["axis"] = axis;
        }
        entry["bone_scale"] = c.AmpScale;
        var defaults = new Dictionary<string, object>();
        var gait = new Dictionary<string, object>();
        string[] gnames = { "idle", "walk", "run", "sprint", "zipline" };
        for (int i = 0; i < 5; i++) {
            var g = new Dictionary<string, object>();
            g["amplitude_deg"] = c.Amp[i];
            if (c.AmpDown[i] > 0) g["amplitude_down_deg"] = c.AmpDown[i];
            g["frequency_hz"] = c.Freq[i];
            gait[gnames[i]] = g;
        }
        defaults["gait"] = gait;
        var env = new Dictionary<string, object>();
        env["amplitude_attack_tau_sec"] = c.EnvAttack;
        env["frequency_tau_sec"] = c.EnvFreq;
        env["to_idle_release_tau_sec"] = c.EnvIdle;
        defaults["envelope"] = env;
        var jump = new Dictionary<string, object>();
        jump["enabled"] = false;
        jump["mode"] = "off";
        defaults["jump"] = jump;
        var na = new Dictionary<string, object>();
        na["factor"] = c.NativeFactor;
        defaults["native_amplify"] = na;
        entry["defaults"] = defaults;
        chars[c.Id] = entry;
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"schema_version\": 1,\r\n  \"characters\": {\r\n");
        int i2 = 0;
        foreach (var kv in chars) {
            sb.Append("    ").Append(JsonMini.Str(kv.Key)).Append(": ")
              .Append(WriteEntry((Dictionary<string, object>)kv.Value))
              .Append(i2 < chars.Count - 1 ? "," : "").Append("\r\n");
            i2++;
        }
        sb.Append("  }\r\n}\r\n");

        if (File.Exists(_path))
            File.Copy(_path, _path + ".bak", true);
        PresetService.AtomicWrite(_path, sb.ToString());
    }

    // Remove one character entirely from the DB (back to "unknown" so the
    // Developer wizard can re-scan and re-record it).  Backup + atomic write.
    public void DeleteCharacter(string id) {
        Dictionary<string, object> root;
        if (!File.Exists(_path)) return;
        if (!JsonMini.ParseTopLevel(File.ReadAllText(_path, Encoding.UTF8), out root))
            return;
        if (!root.ContainsKey("characters")) return;
        var chars = (Dictionary<string, object>)root["characters"];
        if (!chars.ContainsKey(id)) return;
        chars.Remove(id);
        var sb = new StringBuilder();
        sb.Append("{\r\n  \"schema_version\": 1,\r\n  \"characters\": {\r\n");
        int i2 = 0;
        foreach (var kv in chars) {
            sb.Append("    ").Append(JsonMini.Str(kv.Key)).Append(": ")
              .Append(WriteEntry((Dictionary<string, object>)kv.Value))
              .Append(i2 < chars.Count - 1 ? "," : "").Append("\r\n");
            i2++;
        }
        sb.Append("  }\r\n}\r\n");
        File.Copy(_path, _path + ".bak", true);
        PresetService.AtomicWrite(_path, sb.ToString());
    }

    static string WriteEntry(Dictionary<string, object> e) {
        var sb = new StringBuilder();
        sb.Append("{\r\n      \"display_name\": ").Append(JsonMini.Str((string)e["display_name"])).Append(",\r\n");
        var bones = (Dictionary<string, object>)e["bones"];
        sb.Append("      \"bones\": { \"right\": ").Append(JsonMini.Str((string)bones["right"]))
          .Append(", \"left\": ").Append(JsonMini.Str((string)bones["left"]))
          .Append(", \"allow_fallback_candidates\": true }");
        if (e.ContainsKey("axis")) {
            var axis = (Dictionary<string, object>)e["axis"];
            sb.Append(",\r\n      \"axis\": { \"name\": ").Append(JsonMini.Str((string)axis["name"]))
              .Append(", \"sign\": ").Append(JsonMini.Num((double)axis["sign"])).Append(" }");
        }
        sb.Append(",\r\n      \"bone_scale\": ").Append(JsonMini.Num((double)e["bone_scale"]));
        var d = (Dictionary<string, object>)e["defaults"];
        var gait = (Dictionary<string, object>)d["gait"];
        sb.Append(",\r\n      \"defaults\": {\r\n        \"gait\": {\r\n");
        string[] gnames2 = { "idle", "walk", "run", "sprint" };
        for (int i = 0; i < 4; i++) {
            var g = (Dictionary<string, object>)gait[gnames2[i]];
            sb.Append("          ").Append(CharacterDatabaseService.GaitEntryJson(
                gnames2[i],
                JsonMini.GetNum(g, "amplitude_deg", 0),
                JsonMini.GetNum(g, "amplitude_down_deg", 0),
                JsonMini.GetNum(g, "frequency_hz", 1.5)))
              .Append(i < 3 ? "," : "").Append("\r\n");
        }
        var env = (Dictionary<string, object>)d["envelope"];
        sb.Append("        },\r\n        \"envelope\": {\r\n")
          .Append("          \"amplitude_attack_tau_sec\": ").Append(JsonMini.Num((double)env["amplitude_attack_tau_sec"])).Append(",\r\n")
          .Append("          \"frequency_tau_sec\": ").Append(JsonMini.Num((double)env["frequency_tau_sec"])).Append(",\r\n")
          .Append("          \"to_idle_release_tau_sec\": ").Append(JsonMini.Num((double)env["to_idle_release_tau_sec"])).Append("\r\n")
          .Append("        },\r\n");
        var jump = (Dictionary<string, object>)d["jump"];
        sb.Append("        \"jump\": { \"enabled\": false, \"mode\": \"off\" },\r\n");
        var na = (Dictionary<string, object>)d["native_amplify"];
        sb.Append("        \"native_amplify\": { \"factor\": ").Append(JsonMini.Num((double)na["factor"])).Append(" }\r\n")
          .Append("      }\r\n    }");
        return sb.ToString();
    }
}
