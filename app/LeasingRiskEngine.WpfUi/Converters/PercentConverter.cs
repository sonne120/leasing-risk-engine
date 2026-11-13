using System;
using System.Globalization;
using System.Windows.Data;

namespace LeasingRiskEngine.WpfUi.Converters;

public sealed class PercentConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType,
                           object? parameter, CultureInfo culture)
    {
        if (value is double d) return d * 100.0;
        return 0.0;
    }

    public object? ConvertBack(object? value, Type targetType,
                               object? parameter, CultureInfo culture)
    {
        if (value is double d) return d / 100.0;
        return 0.0;
    }
}
