/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "toolbox.hpp"
#include "rise_set.hpp"
#include "planet_phenomena.hpp"

namespace astro::toolbox::test {

TEST(GoldenSection, IndependentlyRoundedCoefficients) {
  // Preserve the two source literals' bit patterns, not a recomputed reciprocal or complement (#372).
  ASSERT_EQ(std::bit_cast<uint64_t>(INV_PHI), 0x3fe3c6ef372fe950ULL);
  ASSERT_EQ(std::bit_cast<uint64_t>(INV_PHI_SQ), 0x3fd8722191a02d61ULL);
  ASSERT_NE(INV_PHI_SQ, 1.0 - INV_PHI);
}


TEST(GoldenSection, BudgetExhaustionDoesNotRecheckTheFinalBracket) {
  std::vector<double> calls;
  const auto f = [&calls](const double x) {
    calls.push_back(x);
    return x;
  };
  const GoldenSectionRules rules {
    .lower_probe = [](const double lo, const double hi) { return lo + (INV_PHI_SQ * (hi - lo)); },
    .move_lower = [](const double lhs, const double rhs) { return lhs > rhs; },
    .finished = [](const double width, const double tolerance) { return width <= tolerance; },
  };

  const auto exhausted = golden_section_bracket(f, 0.0, 1.0, 0.7, 1, rules);
  ASSERT_FALSE(exhausted.converged);
  ASSERT_EQ(exhausted.lower, 0.0);
  ASSERT_EQ(exhausted.upper, INV_PHI);
  ASSERT_LE(exhausted.upper - exhausted.lower, 0.7);
  const std::vector<double> expected { INV_PHI_SQ, INV_PHI, INV_PHI_SQ * INV_PHI };
  ASSERT_EQ(calls, expected);

  calls.clear();
  const auto converged = golden_section_bracket(f, 0.0, 1.0, 0.7, 2, rules);
  ASSERT_TRUE(converged.converged);
  ASSERT_EQ(converged.lower, exhausted.lower);
  ASSERT_EQ(converged.upper, exhausted.upper);
  ASSERT_EQ(calls, expected);

  calls.clear();
  const auto zero_budget = golden_section_bracket(f, 0.0, 1.0, 2.0, 0, rules);
  ASSERT_FALSE(zero_budget.converged);
  ASSERT_EQ(calls.size(), 2U);
}


TEST(GoldenSection, RiseToleranceAndNanComparisonsKeepTheirOriginalPolarity) {
  std::vector<double> calls;
  const auto f = [&calls](const double x) {
    calls.push_back(x);
    return std::numeric_limits<double>::quiet_NaN();
  };

  const double stopped = astro::rise_set::detail::golden_section_argmin(
    f, 0.0, 1.0, std::numeric_limits<double>::quiet_NaN()
  );
  ASSERT_EQ(stopped, 0.5);
  ASSERT_EQ(calls.size(), 2U);

  calls.clear();
  const double updated = astro::rise_set::detail::golden_section_argmin(f, 0.0, 1.0, 0.7);
  ASSERT_EQ(updated, (INV_PHI_SQ + 1.0) / 2.0);
  ASSERT_EQ(calls.size(), 3U);
  ASSERT_EQ(calls.back(), INV_PHI_SQ + (INV_PHI * (1.0 - INV_PHI_SQ)));
}


TEST(GoldenSection, RiseHonorsTheEdgeProbeTolerance) {
  std::vector<double> calls;
  const auto f = [&calls](const double x) {
    calls.push_back(x);
    return (x - 0.3) * (x - 0.3);
  };

  const double ordinary = astro::rise_set::detail::golden_section_argmin(f, 0.0, 1.0);
  const auto ordinary_calls = calls;
  calls.clear();
  const double edge = astro::rise_set::detail::golden_section_argmin(
    f, 0.0, 1.0, astro::rise_set::EDGE_PROBE_TOLERANCE_DAYS
  );

  ASSERT_NEAR(ordinary, 0.3, astro::rise_set::EXTREMUM_SEARCH_TOLERANCE_DAYS);
  ASSERT_NEAR(edge, 0.3, astro::rise_set::EDGE_PROBE_TOLERANCE_DAYS);
  ASSERT_LT(calls.size(), ordinary_calls.size());
  ASSERT_NE(edge, ordinary);
}


TEST(GoldenSection, PhenomenaWitnessKeepsOverflowSafeMidpoint) {
  const double bound = std::numeric_limits<double>::max();
  const std::array<std::pair<double, double>, 4> brackets {{
    {bound, bound}, {-bound, -bound},
    {bound, std::nextafter(bound, 0.0)}, {std::nextafter(-bound, 0.0), -bound},
  }};
  std::vector<double> calls;
  const auto plateau = [&calls](const double x) {
    calls.push_back(x);
    return 0.0;
  };

  for (const auto& [lo, hi] : brackets) {
    calls.clear();
    ASSERT_THROW(
      static_cast<void>(astro::planet::phenomena::detail::refine_extremum(
        astro::planet::Planet::MERCURY, lo, hi, false, plateau
      )),
      std::runtime_error
    );
    ASSERT_EQ(calls.size(), 5U);
    for (const double x : calls) {
      ASSERT_TRUE(std::isfinite(x));
    }
    ASSERT_EQ(calls.at(2), std::midpoint(lo, hi));
    ASSERT_EQ(calls.at(3), std::midpoint(lo, hi));
    ASSERT_EQ(calls.at(4), std::midpoint(lo, hi));
  }

  calls.clear();
  ASSERT_TRUE(std::isinf(astro::rise_set::detail::golden_section_argmin(plateau, bound, bound)));
  ASSERT_EQ(calls.size(), 2U);
}

} // namespace astro::toolbox::test
