using System;
using System.IO;
using System.Text.Json;

namespace LeasingRiskEngine.Data;

public enum DatabaseProvider
{
    SqlServer,
    Sqlite,
}

/// <summary>
/// Which database the app stores its data in. Read from the "Database"
/// section of appsettings.json next to the executable.
/// </summary>
public sealed class DatabaseOptions
{
    /// <summary>SQL Server Express LocalDB, installed with Visual Studio.</summary>
    public const string DefaultSqlServerConnectionString =
        @"Server=(localdb)\MSSQLLocalDB;Database=LeasingRiskEngine;" +
        "Trusted_Connection=True;TrustServerCertificate=True";

    public DatabaseProvider Provider { get; init; } = DatabaseProvider.SqlServer;

    /// <summary>Empty means "use the default for the provider".</summary>
    public string ConnectionString { get; init; } = "";

    /// <summary>%LOCALAPPDATA%\LeasingRiskEngine\leasing.db</summary>
    public static string DefaultSqlitePath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "LeasingRiskEngine", "leasing.db");

    public string ResolveConnectionString()
    {
        if (!string.IsNullOrWhiteSpace(ConnectionString)) return ConnectionString;
        return Provider == DatabaseProvider.Sqlite
            ? $"Data Source={DefaultSqlitePath}"
            : DefaultSqlServerConnectionString;
    }

    /// <summary>
    /// Loads the options from a JSON file. A missing file or a missing
    /// "Database" section gives the defaults; a malformed one throws.
    /// </summary>
    public static DatabaseOptions Load(string jsonPath)
    {
        if (!File.Exists(jsonPath)) return new DatabaseOptions();

        using var doc = JsonDocument.Parse(
            File.ReadAllText(jsonPath),
            new JsonDocumentOptions
            {
                CommentHandling = JsonCommentHandling.Skip,
                AllowTrailingCommas = true,
            });

        if (!doc.RootElement.TryGetProperty("Database", out var section))
            return new DatabaseOptions();

        var provider = DatabaseProvider.SqlServer;
        if (section.TryGetProperty("Provider", out var p) &&
            p.GetString() is { Length: > 0 } providerName &&
            !Enum.TryParse(providerName, ignoreCase: true, out provider))
        {
            throw new InvalidDataException(
                $"Unknown database provider '{providerName}' in {jsonPath}. " +
                "Use SqlServer or Sqlite.");
        }

        var connectionString =
            section.TryGetProperty("ConnectionString", out var c)
                ? c.GetString() ?? ""
                : "";

        return new DatabaseOptions
        {
            Provider = provider,
            ConnectionString = connectionString,
        };
    }
}
