// Services/L10n.cs - tiny localization helper for ViewModel/Service strings.
// Reads from the active ResourceDictionary (same source as DynamicResource),
// so a language switch takes effect on the next UI update.
using System;
using System.Windows;

namespace SecondaryMotion.Manager.Services;

public static class L10n {
    public static string Get(string key, params object[] args) {
        var s = Application.Current?.TryFindResource(key) as string;
        if (s == null) s = key;
        return args.Length > 0 ? string.Format(s, args) : s;
    }
}
