# leasing-risk-engine

## Overview

leasing-risk-engine is a desktop quantitative workbench for leasing contracts. A native C++20 core computes payment schedules, runs Monte Carlo simulations of interest-rate and asset models, and derives portfolio risk metrics. A WPF frontend drives the core through a C++/CLI bridge and shows simulation progress in real time.

## Architecture

A native computational core and a desktop frontend, connected by a mixed-mode bridge assembly:

- **native/** — header-mostly C++20 libraries. `core` (schedules, NPV, IRR), `montecarlo` (SDE models, Euler and Milstein schemes, path generator), `risk` (VaR, CVaR, stress scenarios), `pipeline` (streaming simulation) and `common` (lock-free ring primitives).
- **bridge/** — C++/CLI assembly `LeasingBridge.dll`. A pure-native facade hides the templates; managed wrappers expose contracts, calculators, simulations and the streaming pipeline to .NET.
- **app/** — C#/.NET 8 WPF app (Prism + MVVM, WPF-UI, LiveCharts2). Four views: Contracts, Monte Carlo, Risk Dashboard and Stress Tests.

### Monte Carlo pipeline

```
        ┌─────────────────────────────────────────────────────────────┐
        │  WPF UI  (C# / .NET 8, Prism MVVM)                          │
        │  model parameters, paths, steps, threads  •  Run / Cancel   │
        └─────────────────────────────────────────────────────────────┘
                                    │ C++/CLI bridge (LeasingBridge.dll)
                                    ▼
        ┌─────────────────────────────────────────────────────────────┐
        │  Producer  (1 thread)                                       │
        │  PathGenerator<Scheme>: Euler or Milstein                   │
        │  • acquire a path buffer from the MemPool                   │
        │  • simulate one SDE path (Hull-White / CIR / GBM)           │
        │  • enqueue the buffer pointer                               │
        └─────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
        ┌─────────────────────────────────────────────────────────────┐
        │  MpmcQueue<path*>  (bounded, lock-free, Vyukov)             │
        └─────────────────────────────────────────────────────────────┘
               │               │                │               │
               ▼               ▼                ▼               ▼
        ┌──────────┐   ┌──────────┐    ┌──────────┐    ┌──────────┐
        │ Worker 0 │   │ Worker 1 │    │ Worker 2 │    │ Worker N │
        │ (thread) │   │ (thread) │    │ (thread) │    │ (thread) │
        │  payoff  │   │  payoff  │    │  payoff  │    │  payoff  │
        │  (path)  │   │  (path)  │    │  (path)  │    │  (path)  │
        └────┬─────┘   └────┬─────┘    └────┬─────┘    └────┬─────┘
             │              │               │               │
             └──────────────┴───────┬───────┴───────────────┘
                                    │  buffer goes back to the MemPool
                                    ▼
        ┌─────────────────────────────────────────────────────────────┐
        │  MpmcQueue<double>  (payoffs)                               │
        └─────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
        ┌─────────────────────────────────────────────────────────────┐
        │  Aggregator  (calling thread)                               │
        │  • Welford running mean / stddev, min, max                  │
        │  • publishes a Snapshot every N ms via ProgressBus          │
        │  • exactly one final Snapshot per run                       │
        └─────────────────────────────────────────────────────────────┘
                                    │ .NET event
                                    ▼
        ┌─────────────────────────────────────────────────────────────┐
        │  WPF UI: live mean + 95% CI chart, progress tiles           │
        └─────────────────────────────────────────────────────────────┘
```

Key properties:
- One producer generates paths; N workers price them; one aggregator folds the results. Stages are connected only by bounded lock-free MPMC queues.
- Path buffers come from a fixed-size lock-free pool, so the hot loop does not allocate.
- Statistics are computed online (Welford), so the UI sees a converging mean and confidence interval while the run is still in progress.
- Cancellation is cooperative: every stage checks one atomic flag, and a cancelled run still returns the statistics gathered so far.

## Features

- Annuity and differential payment schedules with totals
- NPV and IRR of a cashflow stream
- Monte Carlo pricing under Hull-White, CIR and GBM with Euler or Milstein schemes
- Antithetic variance reduction
- Streaming simulation with live progress and cancel
- VaR 95/99, CVaR, worst and best case on the simulated loss distribution
- Stress tests (rate shift, volatility shock) using common random numbers
- Optional C++26 `std::execution` (stdexec) version of the simulation driver

## Building

Native core and tests (CMake, any platform):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```
Add `-DLEASING_WITH_STDEXEC=ON` to also build the stdexec demo.

Bridge and desktop UI (Windows, Visual Studio 2022 with C++/CLI support):
```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
cmake --build build-win --config Release
dotnet build app/leasing-risk-engine.sln -c Release -p:Platform=x64
```
The bridge needs MSVC, so the WPF app builds and runs on Windows only. See [BUILD_WINDOWS.md](BUILD_WINDOWS.md) for prerequisites and troubleshooting.

## Tech stack

C++20, CMake, lock-free SPSC/MPMC queues + memory pool, Monte Carlo SDE schemes (Euler, Milstein), stdexec (P2300), C++/CLI, C#/.NET 8, WPF, Prism (MVVM + DI), WPF-UI, LiveCharts2.
