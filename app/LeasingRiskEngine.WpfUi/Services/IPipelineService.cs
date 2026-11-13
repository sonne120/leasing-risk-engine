using System;
using System.Threading.Tasks;
using LeasingBridge;

namespace LeasingRiskEngine.WpfUi.Services;

public interface IPipelineService : IDisposable
{
    event Action<SnapshotManaged>? Progress;
    event Action<bool>?            BusyChanged;

    bool IsBusy { get; }

    Task<SimResultManaged> RunDiscountBondAsync(
        HullWhiteParams p, int steps, int paths, SchemeKindManaged scheme,
        int workerThreads, int progressEveryMs, ulong seed);

    Task<SimResultManaged> RunGbmTerminalAsync(
        GbmParams p, int steps, int paths, SchemeKindManaged scheme,
        int workerThreads, int progressEveryMs, ulong seed);

    void Cancel();
}
