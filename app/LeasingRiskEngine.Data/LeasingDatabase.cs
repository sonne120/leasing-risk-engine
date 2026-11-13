using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Data.Sqlite;
using Microsoft.EntityFrameworkCore;

namespace LeasingRiskEngine.Data;

/// <summary>
/// Owns the connection settings and creates the schema on first use.
/// Every operation gets its own short-lived DbContext, so nothing is tracked
/// (and kept alive) between operations.
/// </summary>
public sealed class LeasingDatabase
{
    private readonly DbContextOptions<LeasingDbContext> _contextOptions;
    private readonly object _gate = new();
    private Task? _ready;

    public DatabaseOptions Options { get; }

    public LeasingDatabase(DatabaseOptions options)
    {
        Options = options;

        var connectionString = options.ResolveConnectionString();
        var builder = new DbContextOptionsBuilder<LeasingDbContext>();
        if (options.Provider == DatabaseProvider.Sqlite)
            builder.UseSqlite(connectionString);
        else
            builder.UseSqlServer(connectionString);
        _contextOptions = builder.Options;
    }

    public LeasingDbContext CreateContext() => new(_contextOptions);

    /// <summary>
    /// Runs <paramref name="work"/> on a thread-pool thread with a fresh
    /// context, after making sure the database exists. Callers on the UI
    /// thread never block on a connection, even with a provider whose async
    /// calls are synchronous underneath (SQLite).
    /// </summary>
    public Task<T> RunAsync<T>(
        Func<LeasingDbContext, CancellationToken, Task<T>> work,
        CancellationToken ct = default) =>
        Task.Run(async () =>
        {
            await EnsureCreatedAsync().WaitAsync(ct).ConfigureAwait(false);
            await using var db = CreateContext();
            return await work(db, ct).ConfigureAwait(false);
        }, ct);

    /// <summary>
    /// Creates the database and its tables if they do not exist yet. A failed
    /// attempt is not cached, so the next operation tries again.
    /// </summary>
    public Task EnsureCreatedAsync()
    {
        lock (_gate)
        {
            if (_ready is null || _ready.IsFaulted || _ready.IsCanceled)
                _ready = Task.Run(CreateAsync);
            return _ready;
        }
    }

    private async Task CreateAsync()
    {
        if (Options.Provider == DatabaseProvider.Sqlite)
        {
            var file = new SqliteConnectionStringBuilder(
                Options.ResolveConnectionString()).DataSource;
            var dir = Path.GetDirectoryName(Path.GetFullPath(file));
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        }

        await using var db = CreateContext();
        await db.Database.EnsureCreatedAsync().ConfigureAwait(false);
    }
}
