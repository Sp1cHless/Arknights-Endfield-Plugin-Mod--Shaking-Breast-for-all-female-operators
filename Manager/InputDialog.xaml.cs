using System.Windows;
using System.Windows.Controls;

namespace SecondaryMotion.Manager;

public partial class InputDialog : Window {
    public string Answer { get; set; } = "";
    public string TitleText { get; }
    public string PromptText { get; }

    public InputDialog(string title, string prompt, string def) {
        TitleText = title;
        PromptText = prompt;
        Answer = def;
        InitializeComponent();
        DataContext = this;
        Input.Focus();
        Input.SelectAll();
    }

    void Ok_Click(object sender, RoutedEventArgs e) {
        Answer = Input.Text.Trim();
        DialogResult = true;
    }

    void Cancel_Click(object sender, RoutedEventArgs e) {
        DialogResult = false;
    }
}
