using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.Data.Sqlite;
using Microsoft.EntityFrameworkCore;
using Xunit;

namespace LeasingRiskEngine.Data.Tests;

/// <summary>Each test gets its own SQLite file in the temp folder.</summary>
public sealed class StoreTests : IDisposable
{
    private readonly string _dir =
        Path.Combine(Path.GetTempPath(), "leasing-tests-" + Guid.NewGuid().ToString("N"));
    private readonly LeasingDatabase _database;

    public StoreTests()
    {
        // The folder is deliberately not created here: the database must do it.
        _database = new LeasingDatabase(new DatabaseOptions
        {
            Provider = DatabaseProvider.Sqlite,
            ConnectionString = $"Data Source={Path.Combine(_dir, "nested", "test.db")}",
        });
    }

    public void Dispose()
    {
        SqliteConnection.ClearAllPools();
        if (Directory.Exists(_dir)) Directory.Delete(_dir, recursive: true);
    }

    private static SavedContract Sample(string number, string customer) => new()
    {
        CustomerName = customer,
        Number = number,
        Principal = 850_000,
        DownPayment = 170_000,
        AnnualRate = 0.185,
        TermMonths = 36,
        PaymentType = 0,
        StartDate = new DateTime(2026, 11, 13),
        MonthlyPayment = 24_750.12,
        TotalPayments = 891_004.32,
        TotalInterest = 211_004.32,
    };

    [Fact]
    public async Task Contract_round_trips_with_its_customer()
    {
        var store = new ContractStore(_database);
        await store.SaveAsync(Sample("L-1", "  ТОВ Альфа  "));

        var saved = Assert.Single(await store.ListAsync());
        Assert.Equal("L-1", saved.Number);
        Assert.Equal("ТОВ Альфа", saved.CustomerName);
        Assert.Equal(850_000, saved.Principal);
        Assert.Equal(0.185, saved.AnnualRate);
        Assert.Equal(36, saved.TermMonths);
        Assert.Equal(new DateTime(2026, 11, 13), saved.StartDate);
        Assert.Equal(24_750.12, saved.MonthlyPayment);
        Assert.Equal("L-1 — ТОВ Альфа", saved.Title);
    }

    [Fact]
    public async Task Saving_the_same_number_updates_instead_of_duplicating()
    {
        var store = new ContractStore(_database);
        await store.SaveAsync(Sample("L-1", "Alpha"));
        await store.SaveAsync(Sample("L-1", "Beta") with { TermMonths = 48 });

        var saved = Assert.Single(await store.ListAsync());
        Assert.Equal("Beta", saved.CustomerName);
        Assert.Equal(48, saved.TermMonths);
    }

    [Fact]
    public async Task Contracts_of_one_customer_share_a_customer_row()
    {
        var store = new ContractStore(_database);
        await store.SaveAsync(Sample("L-1", "Alpha"));
        await store.SaveAsync(Sample("L-2", "Alpha"));
        await store.SaveAsync(Sample("L-3", "Beta"));

        Assert.Equal(3, (await store.ListAsync()).Count);
        await using var db = _database.CreateContext();
        Assert.Equal(2, await db.Customers.CountAsync());
    }

    [Fact]
    public async Task Delete_removes_only_the_named_contract()
    {
        var store = new ContractStore(_database);
        await store.SaveAsync(Sample("L-1", "Alpha"));
        await store.SaveAsync(Sample("L-2", "Alpha"));

        Assert.True(await store.DeleteAsync("L-1"));
        Assert.False(await store.DeleteAsync("L-1"));
        Assert.Equal("L-2", Assert.Single(await store.ListAsync()).Number);
    }

    [Theory]
    [InlineData("", "L-1")]
    [InlineData("   ", "L-1")]
    [InlineData("Alpha", "")]
    public async Task Save_rejects_missing_customer_or_number(string customer, string number)
    {
        var store = new ContractStore(_database);
        await Assert.ThrowsAsync<ArgumentException>(
            () => store.SaveAsync(Sample(number, customer)));
    }

    [Fact]
    public async Task Run_history_lists_newest_first_and_respects_the_limit()
    {
        var history = new RunHistoryStore(_database);
        for (int i = 1; i <= 5; ++i)
        {
            await history.AddAsync(new SimulationRunRecord
            {
                Kind = "MonteCarlo", Scenario = $"run {i}", Scheme = "Euler",
                Steps = 252, Paths = 1000 * i, Mean = i, StdError = 0.01,
                ParametersJson = "{\"Seed\":12648430}",
            });
        }

        var recent = await history.ListRecentAsync(3);
        Assert.Equal(new[] { "run 5", "run 4", "run 3" },
                     recent.Select(r => r.Scenario).ToArray());
        Assert.Null(recent[0].Var95);
        Assert.Equal(0.01, recent[0].StdError);
        Assert.Equal("{\"Seed\":12648430}", recent[0].ParametersJson);
        Assert.True(recent[0].CreatedUtc > DateTime.UtcNow.AddMinutes(-1));

        Assert.Equal(5, await history.ClearAsync());
        Assert.Empty(await history.ListRecentAsync(10));
    }

    [Fact]
    public async Task Data_survives_reopening_the_database()
    {
        await new ContractStore(_database).SaveAsync(Sample("L-1", "Alpha"));

        var reopened = new LeasingDatabase(_database.Options);
        Assert.Single(await new ContractStore(reopened).ListAsync());
    }

    [Fact]
    public async Task A_failed_first_connection_is_retried()
    {
        // A file where the database folder should be makes creation fail.
        Directory.CreateDirectory(_dir);
        var blocker = Path.Combine(_dir, "nested");
        File.WriteAllText(blocker, "not a folder");

        var store = new ContractStore(_database);
        await Assert.ThrowsAnyAsync<Exception>(() => store.ListAsync());

        File.Delete(blocker);
        Assert.Empty(await store.ListAsync());
    }
}

public sealed class OptionsTests
{
    private static string WriteTemp(string json)
    {
        var path = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N") + ".json");
        File.WriteAllText(path, json);
        return path;
    }

    [Fact]
    public void Missing_file_means_sql_server_localdb()
    {
        var options = DatabaseOptions.Load(Path.Combine(Path.GetTempPath(), "no-such-file.json"));
        Assert.Equal(DatabaseProvider.SqlServer, options.Provider);
        Assert.Equal(DatabaseOptions.DefaultSqlServerConnectionString,
                     options.ResolveConnectionString());
    }

    [Fact]
    public void Sqlite_without_a_connection_string_uses_local_app_data()
    {
        var path = WriteTemp("{ \"Database\": { \"Provider\": \"sqlite\" } } // comment");
        try
        {
            var options = DatabaseOptions.Load(path);
            Assert.Equal(DatabaseProvider.Sqlite, options.Provider);
            Assert.Equal($"Data Source={DatabaseOptions.DefaultSqlitePath}",
                         options.ResolveConnectionString());
            Assert.EndsWith(Path.Combine("LeasingRiskEngine", "leasing.db"),
                            DatabaseOptions.DefaultSqlitePath);
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void Explicit_connection_string_wins()
    {
        var path = WriteTemp(
            "{ \"Database\": { \"Provider\": \"SqlServer\", \"ConnectionString\": \"Server=db01;Database=X\" } }");
        try
        {
            Assert.Equal("Server=db01;Database=X",
                         DatabaseOptions.Load(path).ResolveConnectionString());
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void Unknown_provider_is_an_error()
    {
        var path = WriteTemp("{ \"Database\": { \"Provider\": \"Oracle\" } }");
        try { Assert.Throws<InvalidDataException>(() => DatabaseOptions.Load(path)); }
        finally { File.Delete(path); }
    }

    [Fact]
    public void Sql_server_schema_can_be_generated_for_the_model()
    {
        // No server needed: this only checks that the model maps onto SQL Server.
        var database = new LeasingDatabase(new DatabaseOptions());
        using var db = database.CreateContext();
        var script = db.Database.GenerateCreateScript();

        Assert.Contains("CREATE TABLE [Customers]", script);
        Assert.Contains("CREATE TABLE [Contracts]", script);
        Assert.Contains("CREATE TABLE [SimulationRuns]", script);
        Assert.Contains("CREATE UNIQUE INDEX [IX_Contracts_Number]", script);
    }
}
