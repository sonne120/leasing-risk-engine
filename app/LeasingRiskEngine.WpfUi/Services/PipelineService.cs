using System;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;
using LeasingBridge;

namespace LeasingRiskEngine.WpfUi.Services;

public sealed class PipelineService : IPipelineService
{
    private readonly MonteCarloPipelineManaged _pipeline;
    private readonly Dispatcher _dispatcher;
    private int _busyFlag;

    public event Action<SnapshotManaged>? Progress;
    public event Action<bool>?            BusyChanged;

    public bool IsBusy => Volatile.Read(ref _busyFlag) == 1;

    public PipelineService()
    {
        _pipeline = new MonteCarloPipelineManaged();
        _dispatcher = Application.Current.Dispatcher;

        _pipeline.Progress += OnNativeProgress;
    }

    private void OnNativeProgress(SnapshotManaged s)
    {
        if (_dispatcher.CheckAccess())
        {
            Progress?.Invoke(s);
        }
        else
        {
            _dispatcher.BeginInvoke(new Action(() => Progress?.Invoke(s)));
        }
    }

    public Task<SimResultManaged> RunDiscountBondAsync(
        HullWhiteParams p, int steps, int paths, SchemeKindManaged scheme,
        int workerThreads, int progressEveryMs, ulong seed) =>
        RunGuarded(() => _pipeline.RunDiscountBond(
            p, steps, paths, scheme, workerThreads, progressEveryMs, seed));

    public Task<SimResultManaged> RunGbmTerminalAsync(
        GbmParams p, int steps, int paths, SchemeKindManaged scheme,
        int workerThreads, int progressEveryMs, ulong seed) =>
        RunGuarded(() => _pipeline.RunGbmTerminal(
            p, steps, paths, scheme, workerThreads, progressEveryMs, seed));

    public void Cancel() => _pipeline.Cancel();

    public void Dispose()
    {
        _pipeline.Progress -= OnNativeProgress;
        _pipeline.Dispose();
    }

    private Task<SimResultManaged> RunGuarded(Func<SimResultManaged> work) =>
        Task.Run(() =>
        {
            if (Interlocked.CompareExchange(ref _busyFlag, 1, 0) != 0)
                throw new InvalidOperationException(
                    "Pipeline is already running a simulation.");
            try
            {
                RaiseBusy(true);
                return work();
            }
            finally
            {
                Volatile.Write(ref _busyFlag, 0);
                RaiseBusy(false);
            }
        });

    private void RaiseBusy(bool busy)
    {
        if (_dispatcher.CheckAccess())
            BusyChanged?.Invoke(busy);
        else
            _dispatcher.BeginInvoke(new Action(() => BusyChanged?.Invoke(busy)));
    }
}
