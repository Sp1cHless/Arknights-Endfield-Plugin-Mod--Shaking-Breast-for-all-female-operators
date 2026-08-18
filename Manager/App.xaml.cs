using System;
using System.IO;
using System.Text;
using System.Windows;
using Microsoft.Win32;
using SecondaryMotion.Manager.Services;

namespace SecondaryMotion.Manager;

public partial class App : Application {
    public static AppCtx Ctx = null!;
    public static string ManagerDir = "";

    protected override void OnStartup(StartupEventArgs e) {
        base.OnStartup(e);
        try {
            ManagerDir = AppDomain.CurrentDomain.BaseDirectory;
            var settings = new SettingsService(ManagerDir);
            var diag = new StringBuilder();
            diag.AppendLine("manager_dir: " + ManagerDir);
            diag.AppendLine("settings.game_data_dir: " + (settings.GameDataDir ?? "(none)"));

            // Data root: settings.game_data_dir (recommended — Manager lives
            // in its own folder); fallback: this exe's own folder (when the
            // whole tool folder was copied into the game).
            string dataRoot;
            if (settings.GameDataDir != null &&
                Directory.Exists(Path.Combine(settings.GameDataDir, "data"))) {
                dataRoot = settings.GameDataDir;
            } else if (Directory.Exists(Path.Combine(ManagerDir, "data"))) {
                dataRoot = ManagerDir;
            } else {
                // first run: ask for the game's SecondaryMotion folder
                var dlg = new OpenFolderDialog {
                    Title = "Select the game's SecondaryMotion folder (contains data/, presets/, runtime/)"
                };
                if (dlg.ShowDialog() != true) {
                    MessageBox.Show("A game SecondaryMotion folder is required.",
                                    "Secondary Motion", MessageBoxButton.OK,
                                    MessageBoxImage.Warning);
                    Shutdown(1);
                    return;
                }
                dataRoot = dlg.FolderName;
                settings.Save(dataRoot);
            }
            diag.AppendLine("data_root: " + dataRoot);
            var stPath = Path.Combine(dataRoot, "runtime", "runtime_status.json");
            diag.AppendLine("status_path: " + stPath);
            diag.AppendLine("status_exists: " + File.Exists(stPath));
            if (File.Exists(stPath))
                diag.AppendLine("status_age_s: " +
                    (DateTime.Now - File.GetLastWriteTime(stPath)).TotalSeconds.ToString("0.0"));

            Ctx = new AppCtx(dataRoot);
            Ctx.Load();
            diag.AppendLine("chars_loaded: " + Ctx.Characters.Count);
            diag.AppendLine("preset: " + Ctx.ActivePreset + " rev=" + Ctx.Config.Revision);
            File.WriteAllText(Path.Combine(ManagerDir, "manager_startup.log"), diag.ToString());

            var win = new MainWindow();
            MainWindow = win;
            win.Show();
        } catch (Exception ex) {
            try {
                File.WriteAllText(Path.Combine(ManagerDir, "manager_crash.log"), ex.ToString());
            } catch { }
            throw;
        }
    }
}
