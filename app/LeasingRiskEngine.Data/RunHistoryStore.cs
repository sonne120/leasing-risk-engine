using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.EntityFrameworkCore;

namespace LeasingRiskEngine.Data;

public interface IRunHistoryStore
{
    /// <summary>Stores a finished run. CreatedUtc is set here.</summary>
    Task AddAsync(SimulationRunRecord run, CancellationToken ct = default);

    /// <summary>The newest runs first, at most <paramref name="limit"/>.</summary>
    Task<IReadOnlyList<SimulationRunRecord>> ListRecentAsync(
        int limit, CancellationToken ct = default);

    /// <summary>Deletes the whole history and returns how many runs it held.</summary>
    Task<int> ClearAsync(CancellationToken ct = default);
}

public sealed class RunHistoryStore : IRunHistoryStore
{
    private readonly LeasingDatabase _database;

    public RunHistoryStore(LeasingDatabase database) => _database = database;

    public Task AddAsync(SimulationRunRecord run, CancellationToken ct = default) =>
        _database.RunAsync(async (db, token) =>
        {
            run.Id = 0;
            run.CreatedUtc = DateTime.UtcNow;
            db.Runs.Add(run);
            await db.SaveChangesAsync(token).ConfigureAwait(false);
            return true;
        }, ct);

    public Task<IReadOnlyList<SimulationRunRecord>> ListRecentAsync(
        int limit, CancellationToken ct = default) =>
        _database.RunAsync<IReadOnlyList<SimulationRunRecord>>(async (db, token) =>
            await db.Runs
                .AsNoTracking()
                .OrderByDescending(r => r.CreatedUtc)
                .ThenByDescending(r => r.Id)
                .Take(limit)
                .ToListAsync(token)
                .ConfigureAwait(false), ct);

    public Task<int> ClearAsync(CancellationToken ct = default) =>
        _database.RunAsync(async (db, token) =>
            await db.Runs.ExecuteDeleteAsync(token).ConfigureAwait(false), ct);
}
