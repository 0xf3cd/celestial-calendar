/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <span>
#include <cmath>
#include <chrono>
#include <format>
#include <string>
#include <vector>
#include <cstddef>
#include <ostream>
#include <algorithm>
#include <functional>
#include <string_view>

/*
 * A benchmark is only as good as its ability to be re-run and land on the same number. Two ways of
 * losing that are designed out here rather than left to whoever writes the next benchmark:
 *
 *   - Measuring case A's rounds to completion and only then starting case B. On a machine ramping
 *     its clocks, A pays for the ramp and B does not, and the ratio between them is off by tens of
 *     percent. Taking the minimum over many rounds does not help: the bias is systematic, so every
 *     one of A's rounds carries it. `run` interleaves instead -- every case is measured once per
 *     round, adjacent in time -- and reports the median of the per-round ratios, where the drift
 *     divides out.
 *   - Measuring in a fixed order within the round. That leaves whatever drifts inside a round
 *     landing on the same case every time. The starting case rotates per round.
 *
 * Timing still begins only after a warm-up long enough for clocks and caches to settle.
 */

namespace bench {

/** @brief What a case's timings looked like across the rounds, in nanoseconds per iteration. */
struct Stats {
  double median = 0.0;
  double min    = 0.0;
  double p10    = 0.0;
  double p90    = 0.0;
};


/** @brief One thing to measure. `body(iterations)` must run exactly that many iterations. */
struct Case {
  std::string_view name;

  // Taking the iteration count means the callable is invoked once per round rather than once per
  // iteration, so one indirect call is amortized over `Plan::iterations`. That is what makes it
  // invisible -- not the indirection being free.
  std::function<void(std::size_t)> body;
};


/** @brief How to run a benchmark: one struct rather than six parameters, so every knob is named
 *         at the call site. */
struct Plan {
  std::string_view title;
  std::size_t iterations = 1;    // Iterations per round, per case.
  std::size_t rounds     = 31;   // Enough for the quantiles to settle without a slow benchmark.
  std::chrono::milliseconds warm_up { 2000 };
};


namespace detail {

/** @brief The `q`-quantile of already-sorted `samples`: nearest index, never interpolated, so
 *         every figure in the report is a sample that was actually observed. */
[[nodiscard]] inline auto quantile(const std::span<const double> samples, const double q) -> double {
  if (samples.empty()) {
    return 0.0;
  }
  const auto last = static_cast<double>(samples.size() - 1);
  const auto index = static_cast<std::size_t>(std::lround(q * last));
  return samples[index]; // `std::span::at` is C++26; `index <= size() - 1` by construction above.
}


[[nodiscard]] inline auto summarize(std::vector<double> samples) -> Stats {
  std::ranges::sort(samples);
  return {
    .median = quantile(samples, 0.5),
    .min    = quantile(samples, 0.0),
    .p10    = quantile(samples, 0.1),
    .p90    = quantile(samples, 0.9),
  };
}


/** @brief One paired ratio: a percentage within a factor of two, a factor past that -- `-99.994%`
 *         rounds to `-100.0%`, which reads as free. */
[[nodiscard]] inline auto format_ratio(const double ratio) -> std::string {
  if (ratio >= 2.0) {
    return std::format("{:.1f}x slower", ratio);
  }
  if (ratio > 0.0 and ratio <= 0.5) { // `> 0` keeps a degenerate zero out of the reciprocal.
    return std::format("{:.0f}x faster", 1.0 / ratio);
  }
  return std::format("{:+.1f}%", 100.0 * (ratio - 1.0));
}


/** @brief Run `body` once for `iterations` and return the nanoseconds each iteration took. */
[[nodiscard]] inline auto time_once(const Case& bench_case, const std::size_t iterations) -> double {
  const auto started = std::chrono::steady_clock::now();
  bench_case.body(iterations);
  const auto elapsed = std::chrono::steady_clock::now() - started;
  return std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(iterations);
}

} // namespace detail


/**
 * @brief Run every case for `plan.rounds` rounds and write the report to `out`.
 * @param plan How long to warm up and how much to measure.
 * @param cases What to measure. The first case is the baseline the ratios are taken against.
 * @param out Where the report goes.
 * @details See the note at the top of this header for what the rounds do about measurement bias.
 */
inline void run(const Plan& plan, const std::span<const Case> cases, std::ostream& out) {
  if (cases.empty()) {
    return;
  }

  for (const auto& bench_case : cases) {
    const auto until = std::chrono::steady_clock::now() + plan.warm_up / cases.size();
    while (std::chrono::steady_clock::now() < until) {
      bench_case.body(plan.iterations);
    }
  }

  std::vector<std::vector<double>> timings { cases.size() };
  std::vector<std::vector<double>> ratios { cases.size() };

  for (std::size_t round = 0; round < plan.rounds; ++round) {
    std::vector<double> this_round(cases.size(), 0.0);
    for (std::size_t step = 0; step < cases.size(); ++step) {
      const auto index = (step + round) % cases.size(); // Rotate which case goes first.
      this_round.at(index) = detail::time_once(cases[index], plan.iterations);
    }
    for (std::size_t index = 0; index < cases.size(); ++index) {
      timings.at(index).push_back(this_round.at(index));
      ratios.at(index).push_back(this_round.at(index) / this_round.at(0));
    }
  }

  out << std::format("{} -- {} rounds of {} iterations, {} ms warm-up\n",
                     plan.title, plan.rounds, plan.iterations, plan.warm_up.count());

  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto stats = detail::summarize(timings.at(index));
    out << std::format("  {:<34} median {:9.1f}  min {:9.1f}  p10..p90 {:9.1f}..{:9.1f} ns/iter\n",
                       cases[index].name, stats.median, stats.min, stats.p10, stats.p90);
  }

  // Ratios are paired inside a round, so machine drift cancels; the absolute figures above do not
  // have that property and should not be compared across runs, let alone across machines.
  if (cases.size() > 1) {
    out << std::format("\n  vs {} (per-round paired ratio):\n", cases[0].name);
    for (std::size_t index = 1; index < cases.size(); ++index) {
      const auto stats = detail::summarize(ratios.at(index));
      out << std::format("  {:<34} median {:>13}   p10..p90 {}..{}\n",
                         cases[index].name,
                         detail::format_ratio(stats.median),
                         detail::format_ratio(stats.p10),
                         detail::format_ratio(stats.p90));
    }
  }

  out << '\n';
}

} // namespace bench
