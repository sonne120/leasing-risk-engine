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

## Building on Windows (short guide)

**Prerequisites**
- Visual Studio 2022 (17.8 or newer) with the *Desktop development with C++* workload, including **C++/CLI support for v143 build tools** (not installed by default), and the *.NET desktop development* workload (.NET 8 SDK)
- CMake 3.20 or newer (ships with VS 2022)
- Git

**Steps** (run from the *x64 Native Tools Command Prompt for VS 2022*)

```cmd
:: 1. Configure - should print "LeasingBridge: configured for C++/CLI (Windows + MSVC)"
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64

:: 2. Build the native modules and the bridge
cmake --build build-win --config Release -j

:: 3. Run the native tests (ring_tests, core_tests, mc_tests)
ctest --test-dir build-win -C Release --output-on-failure

:: 4. Build the WPF front-end
cd app
dotnet restore leasing-risk-engine.sln
dotnet build leasing-risk-engine.sln -c Release -p:Platform=x64
```

The bridge is written to `build-win\bridge\Release\LeasingBridge.dll`, and a post-build step copies it next to the WPF exe.

**Common problems**
- *"skipping - C++/CLI needs MSVC on Windows"* during configure: the command was run from a non-MSVC shell (Git Bash, MSYS) or with a non-MSVC generator.
- *`BadImageFormatException`*: platform mismatch. The bridge is x64 only, so the WPF project must be x64 too, not AnyCPU.
- *`Could not load file or assembly 'LeasingBridge.dll'`*: the DLL is not next to the exe. Build the bridge first, then rebuild the app.
