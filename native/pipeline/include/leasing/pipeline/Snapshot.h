#pragma once

namespace leasing::pipeline {
struct Snapshot {
    int    paths_done    = 0;     // payoffs aggregated so far
    int    paths_total   = 0;     // target total (0 if cancelled)
    double mean          = 0.0;   // running mean of the payoffs
    double stddev        = 0.0;   // running sample stddev
    double stderr_mean   = 0.0;   // SE of the mean
    double min_payoff    = 0.0;   // extremes of the empirical distribution
    double max_payoff    = 0.0;
    double elapsed_ms    = 0.0;   // wall time since run() started
    bool   cancelled     = false; // true if cancel() was called mid-run
    bool   final_        = false; // true for the last snapshot of a run
};

} 