// ViewModels/Converters.cs — small value converters.
using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace SecondaryMotion.Manager.ViewModels;

public class InverseBoolToVisibilityConverter : IValueConverter {
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        => value is bool b && b ? Visibility.Collapsed : Visibility.Visible;

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        => throw new NotSupportedException();
}

public class NotNullConverter : IValueConverter {
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        => value != null;

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        => throw new NotSupportedException();
}

// double <-> TextBox text (invariant culture; commit on focus loss)
public class DoubleToTextConverter : IValueConverter {
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        => value is double d ? d.ToString("0.##", CultureInfo.InvariantCulture) : "0";

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) {
        if (value is string s && double.TryParse(s, System.Globalization.NumberStyles.Float,
                                                 CultureInfo.InvariantCulture, out double d))
            return d;
        Services.ChangeLog.Append("[UI] rejected input: \"" + value + "\"");
        return Binding.DoNothing;
    }
}
