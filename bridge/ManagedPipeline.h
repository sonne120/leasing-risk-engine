
#pragma once

#include "ManagedErrors.h"
#include "ManagedSde.h"
#include "ManagedTypes.h"
#include "ManagedMonteCarlo.h"
#include "NativeFacade.h"

#include <leasing/pipeline/Snapshot.h>

#include <vcclr.h>                        // msclr::gcroot
#include <msclr/gcroot.h>

using namespace System;
using namespace System::Runtime::InteropServices;

namespace LeasingBridge {
// Snapshot projected into .NET.
public ref class SnapshotManaged {
public:
    property int    PathsDone;
    property int    PathsTotal;
    property double Mean;
    property double StdDev;
    property double StdError;
    property double MinPayoff;
    property double MaxPayoff;
    property double ElapsedMs;
    property bool   Cancelled;
    property bool   Final;
};

public delegate void ProgressHandler(SnapshotManaged^ snapshot);

// Native-callable entry point handed to ProgressBus. It has to be a free
// function: the address of a ref-class member cannot be taken as a native
// function pointer (C3374). user_data is a gcroot<MonteCarloPipelineManaged^>*.
inline void __cdecl PipelineProgressTrampoline(
    const leasing::pipeline::Snapshot* s, void* user_data);

public ref class MonteCarloPipelineManaged {
public:
    MonteCarloPipelineManaged()
    {
        m_euler    = leasing::bridge::create_euler_pipeline();
        m_milstein = leasing::bridge::create_milstein_pipeline();
    }

    ~MonteCarloPipelineManaged() { this->!MonteCarloPipelineManaged(); }

    !MonteCarloPipelineManaged()
    {
        if (m_euler) {
            leasing::bridge::destroy_euler_pipeline(m_euler);
            m_euler = nullptr;
        }
        if (m_milstein) {
            leasing::bridge::destroy_milstein_pipeline(m_milstein);
            m_milstein = nullptr;
        }
    }

    event ProgressHandler^ Progress;

    // Thread-safe, non-blocking.
    void Cancel()
    {
        leasing::bridge::cancel_euler_pipeline(m_euler);
        leasing::bridge::cancel_milstein_pipeline(m_milstein);
    }

    // Hull-White zero-coupon bond price (streaming version).
    SimResultManaged^ RunDiscountBond(HullWhiteParams params,
                                       int steps, int paths,
                                       SchemeKindManaged scheme,
                                       int workerThreads,
                                       int progressEveryMs,
                                       unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, false };
            auto payoff = leasing::bridge::make_zero_bond_payoff(
                params.Expiration, steps);
            return Dispatch(sde, cfg, paths, payoff, scheme,
                            workerThreads, progressEveryMs, seed);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

    // GBM terminal distribution (streaming version).
    SimResultManaged^ RunGbmTerminal(GbmParams params,
                                      int steps, int paths,
                                      SchemeKindManaged scheme,
                                      int workerThreads,
                                      int progressEveryMs,
                                      unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, false };
            auto payoff = leasing::bridge::make_terminal_value_payoff();
            return Dispatch(sde, cfg, paths, payoff, scheme,
                            workerThreads, progressEveryMs, seed);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

    SimResultManaged^ RunEuropeanCall(GbmParams params,
                                       double strike,
                                       double discountRate,
                                       int steps, int paths,
                                       SchemeKindManaged scheme,
                                       int workerThreads,
                                       int progressEveryMs,
                                       unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, false };
            auto payoff = leasing::bridge::make_european_call_payoff(
                strike, discountRate, params.Expiration);
            return Dispatch(sde, cfg, paths, payoff, scheme,
                            workerThreads, progressEveryMs, seed);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

internal:
    // Called by PipelineProgressTrampoline on the thread running the simulation.
    void OnNativeSnapshot(const leasing::pipeline::Snapshot* s)
    {
        auto ms = gcnew SnapshotManaged();
        ms->PathsDone  = s->paths_done;
        ms->PathsTotal = s->paths_total;
        ms->Mean       = s->mean;
        ms->StdDev     = s->stddev;
        ms->StdError   = s->stderr_mean;
        ms->MinPayoff  = s->min_payoff;
        ms->MaxPayoff  = s->max_payoff;
        ms->ElapsedMs  = s->elapsed_ms;
        ms->Cancelled  = s->cancelled;
        ms->Final      = s->final_;

        try {
            Progress(ms);
        }
        catch (Exception^ ex) {
            if (m_callback_error == nullptr)
                m_callback_error = ex;
            Cancel();
        }
    }

private:
    void ThrowIfDisposed()
    {
        if (m_euler == nullptr || m_milstein == nullptr)
            throw gcnew ObjectDisposedException("MonteCarloPipelineManaged");
    }

    SimResultManaged^ Dispatch(const leasing::mc::Sde&    sde,
                                leasing::mc::PathConfig    cfg,
                                int                        paths,
                                const leasing::mc::Payoff& payoff,
                                SchemeKindManaged          scheme,
                                int                        worker_threads,
                                int                        progress_every_ms,
                                unsigned long long         seed)
    {
        ThrowIfDisposed();
        m_callback_error = nullptr;

        msclr::gcroot<MonteCarloPipelineManaged^> self_root(this);

        leasing::pipeline::ProgressBus bus;
        bus.set_raw_callback(&PipelineProgressTrampoline,
                             static_cast<void*>(&self_root));

        leasing::mc::SimResult result;
        if (scheme == SchemeKindManaged::Milstein) {
            result = leasing::bridge::run_milstein_pipeline(
                m_milstein, sde, cfg, paths,
                worker_threads, progress_every_ms, seed, payoff, bus);
        } else {
            result = leasing::bridge::run_euler_pipeline(
                m_euler, sde, cfg, paths,
                worker_threads, progress_every_ms, seed, payoff, bus);
        }

        if (m_callback_error != nullptr) {
            Exception^ inner = m_callback_error;
            m_callback_error = nullptr;
            throw gcnew InvalidOperationException(
                "A Progress handler threw; the simulation was cancelled.",
                inner);
        }

        auto r = gcnew SimResultManaged();
        r->Mean     = result.mean;
        r->StdDev   = result.stddev;
        r->StdError = result.stderr_mean;
        r->Ci95Low  = result.ci95_low;
        r->Ci95High = result.ci95_high;
        r->Paths    = result.paths;
        return r;
    }

    leasing::bridge::EulerPipelineHandle    m_euler;
    leasing::bridge::MilsteinPipelineHandle m_milstein;

    Exception^ m_callback_error;
};

inline void __cdecl PipelineProgressTrampoline(
    const leasing::pipeline::Snapshot* s, void* user_data)
{
    if (!s || !user_data) return;
    auto* root =
        static_cast<msclr::gcroot<MonteCarloPipelineManaged^>*>(user_data);
    MonteCarloPipelineManaged^ self = *root;
    self->OnNativeSnapshot(s);
}

} 
