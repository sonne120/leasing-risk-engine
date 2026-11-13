#pragma once

#include "sde/Sde.h"
#include "scheme/Scheme.h"
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace leasing::mc {
struct PathConfig {
    double T          = 1.0;   // horizon in years
    int    steps      = 252;   // number of Δt intervals
    bool   antithetic = false;
};

template <Scheme S>
class PathGenerator {
public:
    PathGenerator(Sde sde, PathConfig cfg, std::uint64_t seed)
        : sde_(std::move(sde)),
          cfg_(cfg),
          rng_(seed),
          normal_(0.0, 1.0),
          dt_(cfg.T / cfg.steps),
          sqrt_dt_(std::sqrt(dt_))
    {}

    // Fills `out` with (steps + 1) values.
    void next_path(std::vector<double>& out) {
        out.resize(static_cast<std::size_t>(cfg_.steps + 1));
        out[0] = sde_.initial_value;
        for (int i = 0; i < cfg_.steps; ++i) {
            const double dW = normal_(rng_) * sqrt_dt_;
            out[i + 1] = S::step(sde_, i * dt_, out[i], dt_, dW);
        }
    }

    void next_antithetic_pair(std::vector<double>& p1,
                              std::vector<double>& p2)
    {
        p1.resize(static_cast<std::size_t>(cfg_.steps + 1));
        p2.resize(static_cast<std::size_t>(cfg_.steps + 1));
        p1[0] = p2[0] = sde_.initial_value;
        for (int i = 0; i < cfg_.steps; ++i) {
            const double z  = normal_(rng_);
            const double dW = z * sqrt_dt_;
            const double t  = i * dt_;
            p1[i + 1] = S::step(sde_, t, p1[i], dt_,  dW);
            p2[i + 1] = S::step(sde_, t, p2[i], dt_, -dW);
        }
    }

    const PathConfig& config() const noexcept { return cfg_; }
    double            dt()    const noexcept { return dt_; }

private:
    Sde          sde_;
    PathConfig   cfg_;
    std::mt19937_64 rng_;
    std::normal_distribution<double> normal_;
    double       dt_;
    double       sqrt_dt_;
};

} 
