using System;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;
using LeasingBridge;

namespace LeasingRiskEngine.WpfUi.Services;

public sealed class PipelineService : IPipelineService
{
    private static readonly long MinProgressIntervalTicks = Stopwatch.Frequency / 30;

    private static readonly TimeSpan ShutdownTimeout = TimeSpan.FromSeconds(5);

    private readonly MonteCarloPipelineManaged _pipeline;
    private readonly Dispatcher _dispatcher;
    private int _busyFlag;
    private int _disposed;

    private Task<SimResultManaged>? _current;

    private SnapshotManaged? _pendingSnapshot;
    private int  _deliveryQueued;
    private long _lastProgressTicks;

    public event Action<SnapshotManaged>? Progress;
    public event Action<bool>?            BusyChanged;

    public bool IsBusy => Volatile.Read(ref _busyFlag) == 1;

    public PipelineService()
    {
        _pipeline = new MonteCarloPipelineManaged();
        _dispatcher = Application.Current.Dispatcher;

        _pipeline.Progress += OnNativeProgress;
    }

    // Runs on the thread that drives the simulation, never on the UI thread.
    private void OnNativeProgress(SnapshotManaged s)
    {
        if (!s.Final)
        {
            long now = Stopwatch.GetTimestamp();
            if (now - _lastProgressTicks < MinProgressIntervalTicks) return;
            _lastProgressTicks = now;
        }

        Interlocked.Exchange(ref _pendingSnapshot, s);
        if (Interlocked.Exchange(ref _deliveryQueued, 1) == 0)
            _dispatcher.BeginInvoke(new Action(DeliverPendingSnapshot));
    }

    private void DeliverPendingSnapshot()
    {
        Volatile.Write(ref _deliveryQueued, 0);
        var s = Interlocked.Exchange(ref _pendingSnapshot, null);
        if (s != null) Progress?.Invoke(s);
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
        if (Interlocked.Exchange(ref _disposed, 1) != 0) return;

        _pipeline.Progress -= OnNativeProgress;
        _pipeline.Cancel();


        bool finished = true;
        try
        {
            finished = Volatile.Read(ref _current)?.Wait(ShutdownTimeout) ?? true;
        }
        catch (AggregateException)
        {
            // A faulted run has still finished.
        }

        if (finished) _pipeline.Dispose();
    }

    private Task<SimResultManaged> RunGuarded(Func<SimResultManaged> work)
    {
        ObjectDisposedException.ThrowIf(Volatile.Read(ref _disposed) != 0, this);

        var task = Task.Run(() =>
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

        Volatile.Write(ref _current, task);
        return task;
    }

    private void RaiseBusy(bool busy)
    {
        if (_dispatcher.CheckAccess())
            BusyChanged?.Invoke(busy);
        else
            _dispatcher.BeginInvoke(new Action(() => BusyChanged?.Invoke(busy)));
    }
}
