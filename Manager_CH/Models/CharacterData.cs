// Models/CharacterData.cs — per-character merged view (DB defaults + preset overrides)
using System.Globalization;

namespace SecondaryMotion.Manager.Models;

public class CharacterData {
    public string Id = "";
    public string DisplayName = "";
    public bool Enabled = true;
    public string Mode = "synthetic";           // off|synthetic|amplify_native
    public double AmpScale = 1.0;
    public double[] Amp = { 0, 3.6, 8.5, 12, 8.5 };       // up amplitude (idx4=zipline)
    public double[] AmpDown = { 0, 0, 0, 0, 0 };          // 0 = symmetric (=up)
    public double[] Freq = { 1.2, 1.5, 1.7, 2.0, 1.7 };
    public double EnvAttack = 0.15, EnvFreq = 0.20, EnvIdle = 0.015;
    public double NativeFactor = 2.0;
    public bool JumpEnabled = false;
    public string Axis = "";                    // ""=auto | X|Y|Z
    public double AxisSign = 1.0;
    public string BoneRight = "";               // DB bone names (wizard)
    public string BoneLeft = "";
    public string OriginalDisplayName = "";     // DB name at load time

    public CharacterData Clone() => (CharacterData)MemberwiseClone();

    public string ModeDisplay => Mode == "off" ? "Original" : Mode == "amplify_native" ? "Amplify Native" : "Synthetic";

    static string F(double v) => v.ToString("0.##", CultureInfo.InvariantCulture);
}
