// Services/JsonMini.cs — minimal JSON parser + writer for the shapes used
// by this tool (objects of numbers/bools/strings/nested objects).  No
// external dependency.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace SecondaryMotion.Manager.Services;

public static class JsonMini {
    // ---- parsing ----
    public static bool ParseTopLevel(string s, out Dictionary<string, object> obj) {
        obj = null;
        int i = 0;
        if (!SkipWs(s, ref i)) return false;
        return ParseObj(s, ref i, out obj);
    }

    public static bool ParseObjectMap(string s, string rootKey,
                                      Dictionary<string, Dictionary<string, object>> outMap) {
        int i = 0;
        if (!SkipWs(s, ref i)) return false;
        Dictionary<string, object> root;
        if (!ParseObj(s, ref i, out root)) return false;
        Dictionary<string, object> target = root;
        if (rootKey != null) {
            object o;
            if (!TryGet(root, rootKey, out o)) return false;
            target = (Dictionary<string, object>)o;
        }
        foreach (var kv in target)
            outMap[kv.Key] = (Dictionary<string, object>)kv.Value;
        return true;
    }

    public static bool TryGet(Dictionary<string, object> node, string key, out object val)
        => node.TryGetValue(key, out val);

    public static double GetNum(Dictionary<string, object> node, string key, double def = 0) {
        object o;
        if (node.TryGetValue(key, out o)) {
            try { return Convert.ToDouble(o, CultureInfo.InvariantCulture); } catch { }
        }
        return def;
    }

    public static bool GetBool(Dictionary<string, object> node, string key, bool def = false) {
        object o;
        if (node.TryGetValue(key, out o)) {
            try { return Convert.ToBoolean(o); } catch { }
        }
        return def;
    }

    public static string GetStr(Dictionary<string, object> node, string key, string def = "") {
        object o;
        if (node.TryGetValue(key, out o) && o is string s) return s;
        return def;
    }

    static bool SkipWs(string s, ref int i) {
        while (i < s.Length && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n' || s[i] == '\uFEFF')) i++;
        return i < s.Length;
    }

    static bool ParseObj(string s, ref int i, out Dictionary<string, object> obj) {
        obj = new Dictionary<string, object>();
        if (!SkipWs(s, ref i) || s[i] != '{') return false;
        i++;
        while (true) {
            if (!SkipWs(s, ref i)) return false;
            if (s[i] == '}') { i++; return true; }
            string key;
            if (!ParseStr(s, ref i, out key)) return false;
            if (!SkipWs(s, ref i) || s[i] != ':') return false;
            i++;
            object val;
            if (!ParseVal(s, ref i, out val)) return false;
            obj[key] = val;
            if (!SkipWs(s, ref i)) return false;
            if (s[i] == ',') { i++; continue; }
            if (s[i] == '}') { i++; return true; }
            return false;
        }
    }

    static bool ParseVal(string s, ref int i, out object val) {
        val = null;
        if (!SkipWs(s, ref i)) return false;
        char c = s[i];
        if (c == '"') { string str; if (!ParseStr(s, ref i, out str)) return false; val = str; return true; }
        if (c == '{') { Dictionary<string, object> o; if (!ParseObj(s, ref i, out o)) return false; val = o; return true; }
        if (c == 't' && i + 4 <= s.Length && s.Substring(i, 4) == "true") { i += 4; val = true; return true; }
        if (c == 'f' && i + 5 <= s.Length && s.Substring(i, 5) == "false") { i += 5; val = false; return true; }
        int j = i;
        while (j < s.Length && (char.IsDigit(s[j]) || s[j] == '-' || s[j] == '+' || s[j] == '.' || s[j] == 'e' || s[j] == 'E')) j++;
        if (j == i) return false;
        string num = s.Substring(i, j - i);
        i = j;
        double d;
        if (!double.TryParse(num, NumberStyles.Float, CultureInfo.InvariantCulture, out d)) return false;
        val = d;
        return true;
    }

    static bool ParseStr(string s, ref int i, out string str) {
        str = "";
        if (s[i] != '"') return false;
        i++;
        var sb = new StringBuilder();
        while (i < s.Length) {
            char c = s[i];
            if (c == '"') { i++; str = sb.ToString(); return true; }
            if (c == '\\' && i + 1 < s.Length) {
                char e = s[i + 1];
                if (e == '"' || e == '\\' || e == '/') { sb.Append(e); i += 2; continue; }
                if (e == 'n') { sb.Append('\n'); i += 2; continue; }
                if (e == 'r') { sb.Append('\r'); i += 2; continue; }
                if (e == 't') { sb.Append('\t'); i += 2; continue; }
                if (e == 'u' && i + 5 < s.Length) {
                    sb.Append((char)Convert.ToInt32(s.Substring(i + 2, 4), 16));
                    i += 6;
                    continue;
                }
            }
            sb.Append(c);
            i++;
        }
        return false;
    }

    // ---- writing ----
    public static string Str(string s) {
        var sb = new StringBuilder("\"");
        foreach (char c in s) {
            if (c == '"') sb.Append("\\\"");
            else if (c == '\\') sb.Append("\\\\");
            else if (c < 0x20) sb.AppendFormat("\\u{0:x4}", (int)c);
            else sb.Append(c);
        }
        return sb.Append('"').ToString();
    }

    public static string Num(double v) => v.ToString("0.##", CultureInfo.InvariantCulture);
}
