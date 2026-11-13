using System;
using System.Globalization;
using System.Windows.Data;

namespace LeasingRiskEngine.WpfUi.Converters;

public sealed class CurrencyConverter : IValueConverter
{
    private static readonly CultureInfo UaCulture = new("uk-UA");

    public object? Convert(object? value, Type targetType,
                           object? parameter, CultureInfo culture)
    {
        if (value is double d)
            return d.ToString("N2", UaCulture) + " ₴";
        return value;
    }

    public object? ConvertBack(object? value, Type targetType,
                               object? parameter, CultureInfo culture)
        => Binding.DoNothing;
}
