// Portfolio risk metrics on an empirical sample distribution.

#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace leasing::risk {
struct VaRResult {
    double var95        = 0.0;
    double var99        = 0.0;
    double cvar95       = 0.0;    // Expected Shortfall at 95%
    double cvar99       = 0.0;
    double mean         = 0.0;
    double stddev       = 0.0;
    double worst_case   = 0.0;    // = max(losses)  = sample[n-1]
    double best_case    = 0.0;    // = min(losses)  = sample[0]
    int    sample_size  = 0;
};

class RiskMetrics {
public:
    // Computes all metrics in one pass.
    static VaRResult calculate_in_place(std::vector<double>& losses)
    {
        VaRResult r;
        r.sample_size = static_cast<int>(losses.size());
        if (losses.empty()) return r;

        std::sort(losses.begin(), losses.end());

        r.best_case  = losses.front();
        r.worst_case = losses.back();
        r.mean       = std::accumulate(losses.begin(), losses.end(), 0.0)
                       / losses.size();

        double sumsq = 0.0;
        for (double v : losses) sumsq += (v - r.mean) * (v - r.mean);
        r.stddev = (losses.size() > 1)
            ? std::sqrt(sumsq / (losses.size() - 1))
            : 0.0;

        r.var95  = quantile_sorted(losses, 0.95);
        r.var99  = quantile_sorted(losses, 0.99);
        r.cvar95 = tail_mean_sorted(losses, 0.95);
        r.cvar99 = tail_mean_sorted(losses, 0.99);
        return r;
    }

    // Non-mutating overload — takes a copy.
    static VaRResult calculate(std::vector<double> losses) {
        return calculate_in_place(losses);
    }

    static double quantile_sorted(const std::vector<double>& sorted, double q)
    {
        if (sorted.empty())      return 0.0;
        if (sorted.size() == 1)  return sorted.front();
        if (q <= 0.0)            return sorted.front();
        if (q >= 1.0)            return sorted.back();

        const double h = q * (sorted.size() - 1);       // fractional rank
        const std::size_t i = static_cast<std::size_t>(std::floor(h));
        const double frac = h - static_cast<double>(i);
        return sorted[i] + frac * (sorted[i + 1] - sorted[i]);
    }

    // Mean of the tail above the α-quantile of a sorted vector.
    static double tail_mean_sorted(const std::vector<double>& sorted, double alpha)
    {
        if (sorted.empty()) return 0.0;
        if (sorted.size() == 1 || alpha <= 0.0) {
            // Mean of the whole sample.
            return std::accumulate(sorted.begin(), sorted.end(), 0.0)
                   / sorted.size();
        }
        if (alpha >= 1.0) return sorted.back();

        const double n     = static_cast<double>(sorted.size());
        const double h     = alpha * n;                // tail starts here
        const std::size_t i0 = static_cast<std::size_t>(std::floor(h));
        const double frac = 1.0 - (h - i0);            // partial first slice
        const double tail_mass = n - h;                // (1 - α) * n

        double sum = frac * sorted[i0];
        for (std::size_t i = i0 + 1; i < sorted.size(); ++i) {
            sum += sorted[i];
        }
        return sum / tail_mass;
    }
};

} // namespace leasing::risk
