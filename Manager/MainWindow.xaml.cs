using System.Windows;
using SecondaryMotion.Manager.Pages;

namespace SecondaryMotion.Manager;

public partial class MainWindow : Window {
    public MainWindow() {
        InitializeComponent();
        NavList.SelectionChanged += (s, e) => {
            switch (NavList.SelectedIndex) {
                case 1: ContentFrame.Navigate(new CharactersPage()); break;
                case 2: ContentFrame.Navigate(new PresetsPage()); break;
                case 3: ContentFrame.Navigate(new DeveloperPage()); break;
                default: ContentFrame.Navigate(new MainPage()); break;
            }
        };
        ContentFrame.Navigate(new MainPage());
        // scratch gait-cadence log: remove the txt when the window closes
        Closed += (s, e) => ViewModels.MainViewModel.Instance?.Shutdown();
    }
}
