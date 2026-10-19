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

#include <array>
#include <cfenv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ephemeris_events.hpp"
#include "event_search_test_support.hpp"

namespace astro::ephemeris::test {

namespace {

namespace search = detail::event_search;
namespace range_test = astro::detail::event_search::test;

constexpr std::array EVENT_TARGETS {
  Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS,
  Target::JUPITER, Target::SATURN, Target::URANUS, Target::NEPTUNE, Target::PLUTO,
  Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING,
};

static_assert(std::same_as<decltype(search::sign(search::Value {})), int32_t>);

constexpr search::Bounds LINEAR_BOUNDS { .first_derivative = 1, .second_derivative = 0 };
constexpr search::Bounds QUADRATIC_BOUNDS { .first_derivative = 1, .second_derivative = 2 };
constexpr search::Grid TWO_CELL_GRID {
  .domain = { .start_jde_tt = 0.0, .end_jde_tt = 2.0 },
  .cell_width_tt_days = 1.0,
};
constexpr search::Interval FIRST_CELL_INTERVAL { .start_jde_tt = 0.0, .end_jde_tt = 1.0 };
constexpr search::Interval SECOND_CELL_INTERVAL { .start_jde_tt = 1.0, .end_jde_tt = 2.0 };

// Knuth, TAOCP Vol. 2, Section 4.2.2: TwoSum gives the exact addition residual.
auto sum_value(const double left, const double right) -> search::Value {
  const double value = left + right;
  const double recovered_right = value - left;
  const double error = (left - (value - recovered_right)) + (right - recovered_right);
  return { .value = value, .error = std::fabs(error) };
}

template <typename Provider>
auto roots(
  const Provider& provider,
  const double start_jde_tt,
  const double end_jde_tt,
  const search::Bounds bounds,
  const double resolution = 0.0,
  const search::EndpointOwnership ownership = search::EndpointOwnership::LEFT
) -> std::vector<search::Root> {
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  const auto candidates = search::isolate_cell(provider, start_jde_tt, end_jde_tt, bounds, budget, ownership);
  std::vector<search::Root> result;
  for (const auto& candidate : candidates) {
    if (candidate.isolation == search::Isolation::UNRESOLVED) {
      throw std::runtime_error { "Unresolved synthetic event region" };
    }
    result.push_back(search::refine(provider, candidate, bounds, budget, resolution));
  }
  return result;
}

auto reported_times(
  const std::vector<search::Root>& found,
  const double query_start,
  const double query_end
) -> std::vector<double> {
  std::vector<double> result;
  for (const auto root : found) {
    if (query_start <= root.jde_tt and root.jde_tt < query_end) {
      result.push_back(root.jde_tt);
    }
  }
  return result;
}

template <typename Provider>
auto two_cell_times(
  const Provider& provider,
  const double query_start,
  const double query_end
) -> std::vector<double> {
  std::vector<search::Root> found;
  for (const int32_t cell : std::array<int32_t, 2> { 0, 1 }) {
    const auto current = roots(
      provider,
      static_cast<double>(cell),
      static_cast<double>(cell + 1),
      LINEAR_BOUNDS
    );
    found.insert(found.end(), current.begin(), current.end());
  }
  return reported_times(found, query_start, query_end);
}

/** @brief Exercise whole-grid traversal with a fresh shared work budget. */
template <typename Provider>
auto range_roots(
  const Provider& provider,
  const search::Interval query,
  const search::Bounds bounds = LINEAR_BOUNDS
) -> std::vector<search::Root> {
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  return range_test::roots_in_interval(provider, TWO_CELL_GRID, query, bounds, budget);
}

/** @brief Carry the exact addition residual as a circular longitude allowance. */
auto longitude_sum(const double first, const double second) -> search::LongitudeValue {
  const auto evaluated = sum_value(first, second);
  return { .longitude = toolbox::AngleDeg { evaluated.value }, .error_deg = evaluated.error };
}

/** @brief Refine synthetic longitude crossings, rejecting unresolved contacts rather than classifying them. */
template <typename Provider>
auto longitude_roots(
  const Provider& provider,
  const toolbox::AngleDeg target,
  const search::Interval cell,
  const search::Bounds bounds
) -> std::vector<search::Root> {
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  const auto candidates = search::isolate_longitude_cell(provider, target, cell, bounds, budget);
  const auto scalar = [&](const double jde_tt) { return search::longitude_residual(provider(jde_tt), target); };
  std::vector<search::Root> result;
  for (const auto& candidate : candidates) {
    if (candidate.isolation != search::Isolation::UNIQUE) {
      throw std::runtime_error { "Unresolved synthetic longitude contact" };
    }
    result.push_back(search::refine(scalar, candidate, bounds, budget));
  }
  return result;
}

/** @brief Search a synthetic longitude on the shared two-cell canonical grid. */
template <typename Provider>
auto longitude_range_roots(
  const Provider& provider,
  const toolbox::AngleDeg target,
  const search::Interval query,
  const search::Bounds bounds = LINEAR_BOUNDS
) -> std::vector<search::Root> {
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  return range_test::longitude_roots_in_interval(provider, target, TWO_CELL_GRID, query, bounds, budget);
}

} // namespace

TEST(EphemerisEventsKernel, FiniteNonnegativeKeepsBoundaryAdmission) {
  for (const double value : std::array { 0.0, -0.0, std::numeric_limits<double>::denorm_min(), 1.0 }) {
    ASSERT_TRUE(search::finite_nonnegative(value));
  }
  for (const double value : std::array {
         -std::numeric_limits<double>::denorm_min(),
         -1.0,
         std::numeric_limits<double>::infinity(),
         -std::numeric_limits<double>::infinity(),
         std::numeric_limits<double>::quiet_NaN(),
       }) {
    ASSERT_FALSE(search::finite_nonnegative(value));
  }
}

TEST(EphemerisEventsKernel, MagnitudeFloorPreservesOutwardSubtraction) {
  for (const search::Value observed : std::array {
         search::Value { .value = 1.0, .error = 0.25 },
         search::Value { .value = -1.0, .error = 0.25 },
         search::Value { .value = 0.0, .error = 0.25 },
         search::Value { .value = 1.0, .error = 1.0 },
         search::Value { .value = std::numeric_limits<double>::denorm_min(), .error = 0.0 },
       }) {
    ASSERT_EQ(search::magnitude_floor(observed), search::lower(std::fabs(observed.value) - observed.error));
  }
}

TEST(EphemerisEventsKernel, AnalyticPolynomialsHaveCompleteCounts) {
  // Constructed polynomials, not ephemeris goldens. Product factors give the
  // exact roots; the allowance covers the four factors' binary64 arithmetic on [0,1].
  const auto four = [](const double time) -> search::Value {
    constexpr std::array zeros { 0.2, 0.4, 0.6, 0.8 };
    double value = 1.0;
    bool exact = false;
    for (const double zero : zeros) {
      value *= time - zero;
      exact = exact or time == zero;
    }
    return { .value = value, .error = exact ? 0.0 : 4e-15 };
  };
  const search::Bounds bounds { .first_derivative = 13.2, .second_derivative = 26.8 };
  const auto found = roots(four, 0, 1, bounds, 1e-9);
  constexpr std::array expected { 0.2, 0.4, 0.6, 0.8 };
  ASSERT_EQ(found.size(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    ASSERT_NEAR(found.at(index).jde_tt, expected.at(index), 1e-9);
    ASSERT_EQ(found.at(index).increasing, index % 2 != 0);
  }
}

TEST(EphemerisEventsKernel, ClosePairIsNotHiddenBySameSignEndpoints) {
  const auto pair = [](const double time) -> search::Value {
    return {
      .value = (time - 0.4999) * (time - 0.5001),
      .error = (time == 0.4999 or time == 0.5001) ? 0.0 : 1e-15,
    };
  };
  const auto found = roots(pair, 0, 1, QUADRATIC_BOUNDS, 1e-9);
  ASSERT_EQ(found.size(), 2);
  ASSERT_NEAR(found.at(0).jde_tt, 0.4999, 1e-9);
  ASSERT_NEAR(found.at(1).jde_tt, 0.5001, 1e-9);
  ASSERT_FALSE(found.at(0).increasing);
  ASSERT_TRUE(found.at(1).increasing);
}

TEST(EphemerisEventsKernel, ExactSharedNodesBelongToTheRightCell) {
  const auto linear = [](const double time) -> search::Value { return sum_value(time, -1); };
  ASSERT_TRUE(roots(linear, 0, 1, LINEAR_BOUNDS).empty());
  const auto right = roots(linear, 1, 2, LINEAR_BOUNDS);
  ASSERT_EQ(right.size(), 1);
  ASSERT_EQ(right.at(0).jde_tt, 1);
  const auto full = two_cell_times(linear, 0, 2);
  ASSERT_EQ(full, (std::vector<double> { 1 }));
  const auto negative = [](const double time) -> search::Value { return sum_value(time, 0.5); };
  const auto before_zero = roots(negative, -1, 0, LINEAR_BOUNDS);
  ASSERT_EQ(before_zero.size(), 1);
  ASSERT_EQ(before_zero.at(0).jde_tt, -0.5);
}

TEST(EphemerisEventsKernel, RoundedInteriorRootsKeepTheirReportedTimeOwnership) {
  for (const double offset : std::array { 0x1p-55, -0x1p-55 }) {
    const auto linear = [offset](const double time) -> search::Value { return sum_value(time - 1, offset); };
    const auto full = two_cell_times(linear, 0, 2);
    ASSERT_EQ(full, (std::vector<double> { 1 }));
    ASSERT_TRUE(two_cell_times(linear, 0, 1).empty());
    ASSERT_EQ(two_cell_times(linear, 1, 2), full);
  }
}

TEST(EphemerisEventsKernel, FinalLegalSampleCanOwnTheRightEndpoint) {
  const double end = 2488069.5;
  const double last = std::nextafter(end, -std::numeric_limits<double>::infinity());
  const auto linear = [last](const double time) -> search::Value { return sum_value(time, -last); };
  const auto found = roots(linear, 2488068, last, LINEAR_BOUNDS, 0, search::EndpointOwnership::BOTH);
  ASSERT_EQ(found.size(), 1);
  ASSERT_EQ(found.at(0).jde_tt, last);
  ASSERT_LT(found.at(0).jde_tt, end);
}

TEST(EphemerisEventsKernel, UncertifiedDomainTailIsNotSilentlyDiscarded) {
  const double last = std::nextafter(1.0, 0.0);
  std::size_t forbidden_calls = 0;
  const auto actual = [&](const double time) -> search::Value {
    if (time > last) {
      ++forbidden_calls;
      throw std::runtime_error { "Outside synthetic position domain" };
    }
    return sum_value(time - 1, 0x1p-54);
  };
  const auto endpoint = [&](const double time) -> search::Value {
    if (time > last) {
      return search::shifted(actual(last), time - last, 1);
    }
    return actual(time);
  };
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  const auto candidates = search::isolate_cell(endpoint, 0, 1, LINEAR_BOUNDS, budget);
  ASSERT_FALSE(candidates.empty());
  ASSERT_EQ(candidates.back().isolation, search::Isolation::UNRESOLVED);
  ASSERT_EQ(candidates.back().right.jde_tt, 1);
  ASSERT_EQ(forbidden_calls, 0);
  ASSERT_THROW(static_cast<void>(roots(endpoint, 0, 1, LINEAR_BOUNDS)), std::runtime_error);
}

TEST(EphemerisEventsKernel, FiniteErrorPreventsFalseExclusionAndUniqueness) {
  const auto shifted_chord = [](const double time) -> search::Value {
    return { .value = (time * (time - 1)) + 0.3, .error = 0.1 };
  };
  search::Budget chord_budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const auto chord = search::isolate_cell(
    shifted_chord,
    0,
    1,
    QUADRATIC_BOUNDS,
    chord_budget,
    search::EndpointOwnership::LEFT,
    0
  );
  ASSERT_EQ(chord.size(), 1);
  ASSERT_EQ(chord.at(0).isolation, search::Isolation::UNRESOLVED);

  const auto noisy_square = [](const double time) -> search::Value {
    return { .value = ((time - 0.5) * (time - 0.5)) + (2 * time) - 1, .error = 1 };
  };
  search::Budget square_budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const auto square = search::isolate_cell(
    noisy_square,
    0,
    1,
    QUADRATIC_BOUNDS,
    square_budget,
    search::EndpointOwnership::LEFT,
    0
  );
  ASSERT_EQ(square.size(), 1);
  ASSERT_EQ(square.at(0).isolation, search::Isolation::UNRESOLVED);
}

TEST(EphemerisEventsKernel, AnObservedZeroWithErrorIsNotAnExactRoot) {
  const auto uncertain = [](const double time) -> search::Value {
    return { .value = 0.5 * time, .error = 0.025 };
  };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const search::Bounds bounds { .first_derivative = 0.6, .second_derivative = 0.1 };
  const auto candidates = search::isolate_cell(
    uncertain,
    0,
    1,
    bounds,
    budget,
    search::EndpointOwnership::LEFT,
    0
  );
  ASSERT_EQ(candidates.size(), 1);
  ASSERT_EQ(candidates.at(0).isolation, search::Isolation::UNRESOLVED);
  ASSERT_FALSE(search::exact_zero({ .value = 0, .error = 0.025 }));
  ASSERT_TRUE(search::exact_zero({ .value = 0, .error = 0 }));
}

TEST(EphemerisEventsKernel, HeterogeneousErrorsDoNotBorrowTheSmallerAllowance) {
  // These observations also enclose the constant f=-1/2, which has no zero.
  const auto asymmetric = [](const double time) -> search::Value {
    return { .value = time - 0.5, .error = time };
  };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const auto candidates = search::isolate_cell(
    asymmetric,
    0,
    1,
    { .first_derivative = 0, .second_derivative = 0 },
    budget,
    search::EndpointOwnership::LEFT,
    0
  );
  ASSERT_EQ(candidates.size(), 1);
  ASSERT_EQ(candidates.at(0).isolation, search::Isolation::UNRESOLVED);
}

TEST(EphemerisEventsKernel, NarrowQueriesRetainWholeCellBounds) {
  const auto pair = [](const double time) -> search::Value {
    return {
      .value = (time - 0.001) * (time - 0.002),
      .error = (time == 0.001 or time == 0.002) ? 0.0 : 1e-15,
    };
  };
  const search::Bounds bounds { .first_derivative = 2, .second_derivative = 2 };
  const auto found = roots(pair, 0, 1, bounds, 1e-8);
  const auto narrow = reported_times(found, 0, 0.01);
  ASSERT_EQ(narrow.size(), 2);
  ASSERT_NEAR(narrow.at(0), 0.001, 1e-8);
  ASSERT_NEAR(narrow.at(1), 0.002, 1e-8);
}

TEST(EphemerisEventsKernel, DoubleAndTripleRootsRemainExplicitlyUnresolved) {
  for (const int32_t order : std::array<int32_t, 2> { 2, 3 }) {
    const auto stationary = [order](const double time) -> search::Value {
      const double offset = time - 0.5;
      const double value = order == 2 ? offset * offset : offset * offset * offset;
      return { .value = value, .error = offset == 0 ? 0.0 : 1e-15 };
    };
    search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
    const auto candidates = search::isolate_cell(
      stationary,
      0,
      1,
      { .first_derivative = 1, .second_derivative = 3 },
      budget,
      search::EndpointOwnership::LEFT,
      8
    );
    ASSERT_FALSE(candidates.empty());
    for (const auto& candidate : candidates) {
      ASSERT_EQ(candidate.isolation, search::Isolation::UNRESOLVED);
    }
  }
}

TEST(EphemerisEventsKernel, RefinementHonorsItsErrorAndResolution) {
  const auto uncertain = [](const double time) -> search::Value {
    return { .value = time - 0.5, .error = 1e-6 };
  };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const auto candidates = search::isolate_cell(uncertain, 0, 1, LINEAR_BOUNDS, budget);
  ASSERT_EQ(candidates.size(), 1);
  const auto resolved = search::refine(uncertain, candidates.at(0), LINEAR_BOUNDS, budget, 2e-6);
  ASSERT_EQ(resolved.jde_tt, 0.5);
  ASSERT_THROW(static_cast<void>(search::refine(uncertain, candidates.at(0), LINEAR_BOUNDS, budget, 1e-8)),
               std::runtime_error);
}

TEST(EphemerisEventsKernel, RefinementAccountsForRoundedMidpoint) {
  for (const double start_jde_tt : std::array { 1.0, 2451545.0 }) {
    const double unit = std::nextafter(start_jde_tt, std::numeric_limits<double>::infinity()) - start_jde_tt;
    for (const double offset : std::array { 0.25, 2.75 }) {
      for (const double direction : std::array { -1.0, 1.0 }) {
        const auto linear = [start_jde_tt, unit, offset, direction](const double jde_tt) -> search::Value {
          return { .value = direction * (((jde_tt - start_jde_tt) / unit) - offset), .error = 0.0 };
        };
        const search::Bounds bounds { .first_derivative = 1.0 / unit, .second_derivative = 0.0 };
        const double resolution = search::upper(1.5 * unit);
        const auto found = roots(linear, start_jde_tt, start_jde_tt + (3.0 * unit), bounds, resolution);
        ASSERT_EQ(found.size(), 1);
        // Quarter-ULP roots are exact in these scaled coordinates, not in JDE(TT).
        const double error_units = std::fabs(((found.at(0).jde_tt - start_jde_tt) / unit) - offset);
        ASSERT_LE(error_units, resolution / unit);
        ASSERT_EQ(found.at(0).increasing, direction > 0.0);
      }
    }
  }
}

TEST(EphemerisEventsKernel, EachBudgetStopsWorkIndependently) {
  const auto linear = [](const double time) -> search::Value { return sum_value(time, -0.4); };
  search::Budget no_evaluations { .evaluations_remaining = 1, .candidates_remaining = 100 };
  ASSERT_THROW(static_cast<void>(search::isolate_cell(linear, 0, 1, LINEAR_BOUNDS, no_evaluations)),
               std::runtime_error);
  search::Budget no_candidates { .evaluations_remaining = 100, .candidates_remaining = 0 };
  ASSERT_THROW(static_cast<void>(search::isolate_cell(linear, 0, 1, LINEAR_BOUNDS, no_candidates)),
               std::runtime_error);
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const auto candidates = search::isolate_cell(linear, 0, 1, LINEAR_BOUNDS, budget);
  ASSERT_EQ(candidates.size(), 1);
  ASSERT_THROW(static_cast<void>(search::refine(linear, candidates.at(0), LINEAR_BOUNDS, budget, 0, 0)),
               std::runtime_error);
  search::Budget empty { .evaluations_remaining = 0, .candidates_remaining = 100 };
  ASSERT_THROW(static_cast<void>(search::refine(linear, candidates.at(0), LINEAR_BOUNDS, empty)),
               std::runtime_error);
}

TEST(EphemerisEventsKernel, InvalidInputsAndProviderFailuresCannotReturnAPrefix) {
  const auto linear = [](const double time) -> search::Value { return { .value = time, .error = 0 }; };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const search::Bounds invalid_bounds { .first_derivative = -1, .second_derivative = 0 };
  ASSERT_THROW(static_cast<void>(search::isolate_cell(linear, 1, 0, LINEAR_BOUNDS, budget)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(search::isolate_cell(linear, 0, 1, invalid_bounds, budget)), std::invalid_argument);
  for (const search::Value invalid : std::array {
         search::Value { .value = std::numeric_limits<double>::quiet_NaN(), .error = 0 },
         search::Value { .value = 0, .error = std::numeric_limits<double>::infinity() },
         search::Value { .value = 0, .error = -1 },
       }) {
    const auto provider = [invalid](const double) -> search::Value { return invalid; };
    ASSERT_THROW(static_cast<void>(search::isolate_cell(provider, 0, 1, LINEAR_BOUNDS, budget)), std::runtime_error);
  }
  const auto failure = [](const double) -> search::Value { throw std::runtime_error { "Provider failed" }; };
  ASSERT_THROW(static_cast<void>(search::isolate_cell(failure, 0, 1, LINEAR_BOUNDS, budget)), std::runtime_error);
}

TEST(EphemerisEventsKernel, CanonicalRangesKeepRoundedBoundaryOwnership) {
  for (const double offset : std::array { 0x1p-55, -0x1p-55 }) {
    const auto linear = [offset](const double time) -> search::Value { return sum_value(time - 1, offset); };
    const auto full = range_roots(linear, TWO_CELL_GRID.domain);
    ASSERT_EQ(full.size(), 1);
    ASSERT_EQ(full.at(0).jde_tt, 1.0);
    ASSERT_TRUE(range_roots(linear, FIRST_CELL_INTERVAL).empty());
    const auto right = range_roots(linear, SECOND_CELL_INTERVAL);
    ASSERT_EQ(right.size(), 1);
    ASSERT_EQ(right.at(0).jde_tt, full.at(0).jde_tt);
    ASSERT_EQ(reported_times(full, 1.0, 2.0), two_cell_times(linear, 1.0, 2.0));
  }
}

TEST(EphemerisEventsKernel, CanonicalRangesOwnExactStartsButNotFinalEnds) {
  for (const double root : std::array { 0.0, 1.0 }) {
    const auto linear = [root](const double time) -> search::Value { return sum_value(time, -root); };
    const auto full = range_roots(linear, TWO_CELL_GRID.domain);
    ASSERT_EQ(full.size(), 1);
    ASSERT_EQ(full.at(0).jde_tt, root);
    const search::Interval query { .start_jde_tt = root, .end_jde_tt = 2.0 };
    const auto clipped = range_roots(linear, query);
    ASSERT_EQ(clipped.size(), 1);
    ASSERT_EQ(clipped.at(0).jde_tt, root);
  }
  const auto final = [](const double time) -> search::Value { return sum_value(time, -2.0); };
  ASSERT_TRUE(range_roots(final, TWO_CELL_GRID.domain).empty());
}

TEST(EphemerisEventsKernel, CanonicalGridHandlesShiftedAndPartialCells) {
  const search::Grid shifted {
    .domain = { .start_jde_tt = -1.25, .end_jde_tt = 1.25 },
    .cell_width_tt_days = 0.75,
  };
  ASSERT_EQ(search::grid_cell_count(shifted), 4);
  const auto linear = [](const double time) -> search::Value { return sum_value(time, -1.125); };
  search::Budget budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
  const search::Interval query { .start_jde_tt = 1.0, .end_jde_tt = 1.25 };
  const auto found = range_test::roots_in_interval(linear, shifted, query, LINEAR_BOUNDS, budget);
  ASSERT_EQ(found.size(), 1);
  ASSERT_EQ(found.at(0).jde_tt, 1.125);

  const search::Grid rounded_tail {
    .domain = { .start_jde_tt = 0.0, .end_jde_tt = std::nextafter(0.3, 1.0) },
    .cell_width_tt_days = 0.1,
  };
  ASSERT_EQ(search::grid_cell_count(rounded_tail), 3);

  const search::Grid rounded_quotient { .domain = SECOND_CELL_INTERVAL, .cell_width_tt_days = 0.1 };
  const uint32_t count = search::grid_cell_count(rounded_quotient);
  ASSERT_EQ(search::containing_cell(rounded_quotient, count, 1.2), 2);
  ASSERT_EQ(search::containing_cell(rounded_quotient, count, std::nextafter(1.2, 1.0)), 1);
}

TEST(EphemerisEventsKernel, UnresolvedRangeRegionsAreRelevantOnlyToTheirQuery) {
  const auto tangent = [](const double time) -> search::Value {
    const double offset = time - 0.25;
    const double value = offset * offset;
    return { .value = value, .error = 2 * std::numeric_limits<double>::epsilon() * std::fabs(value) };
  };
  const search::Bounds bounds { .first_derivative = 3.5, .second_derivative = 2.0 };
  ASSERT_TRUE(range_roots(tangent, SECOND_CELL_INTERVAL, bounds).empty());
  ASSERT_THROW(static_cast<void>(range_roots(tangent, FIRST_CELL_INTERVAL, bounds)), std::runtime_error);
}

TEST(EphemerisEventsKernel, ARangeFailureCannotReturnCompletedEarlierCells) {
  const auto linear = [](const double time) -> search::Value { return sum_value(time, -0.25); };
  search::Budget budget { .evaluations_remaining = 4, .candidates_remaining = 100 };
  ASSERT_THROW(
    static_cast<void>(range_test::roots_in_interval(linear, TWO_CELL_GRID, TWO_CELL_GRID.domain, LINEAR_BOUNDS, budget)),
    std::runtime_error
  );
  ASSERT_EQ(budget.evaluations_remaining, 0);
}

TEST(EphemerisEventsKernel, RangeAdmissionPrecedesProviderWork) {
  std::size_t calls = 0;
  const auto counted = [&calls](const double time) -> search::Value {
    ++calls;
    return { .value = time, .error = 0.0 };
  };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  for (const double width : std::array {
         0.0, -1.0, std::numeric_limits<double>::denorm_min(),
         std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
       }) {
    const search::Grid invalid { .domain = TWO_CELL_GRID.domain, .cell_width_tt_days = width };
    ASSERT_THROW(
      static_cast<void>(range_test::roots_in_interval(counted, invalid, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget)),
      std::invalid_argument
    );
  }
  const search::Interval outside_domain { .start_jde_tt = -1.0, .end_jde_tt = 1.0 };
  ASSERT_THROW(
    static_cast<void>(range_test::roots_in_interval(counted, TWO_CELL_GRID, outside_domain, LINEAR_BOUNDS, budget)),
    std::invalid_argument
  );
  ASSERT_EQ(calls, 0);
  ASSERT_EQ(budget.evaluations_remaining, 100);
}

TEST(EphemerisEventsKernel, RangeProviderOwnsItsVirtualDomainEndpoint) {
  const double last = std::nextafter(1.0, 0.0);
  std::size_t forbidden_calls = 0;
  const auto physical = [last, &forbidden_calls](const double time) -> search::Value {
    if (time > last) {
      ++forbidden_calls;
      throw std::runtime_error { "Outside synthetic position domain" };
    }
    return sum_value(time, -0.75);
  };
  const auto endpoint = [&physical, last](const double time) -> search::Value {
    if (time > last) {
      return search::shifted(physical(last), time - last, 1.0);
    }
    return physical(time);
  };
  const search::Grid grid { .domain = FIRST_CELL_INTERVAL, .cell_width_tt_days = 1.0 };
  search::Budget budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
  const auto found = range_test::roots_in_interval(endpoint, grid, grid.domain, LINEAR_BOUNDS, budget);
  ASSERT_EQ(found.size(), 1);
  ASSERT_EQ(found.at(0).jde_tt, 0.75);
  ASSERT_EQ(forbidden_calls, 0);
}

TEST(EphemerisEventsKernel, EventRateCentersKeepEveryExistingTargetDomain) {
  for (const auto target : EVENT_TARGETS) {
    const auto [first, end] = longitude_rate_domain(target);
    for (const double requested : std::array { first, std::nextafter(first, end), std::nextafter(end, first), end }) {
      const double center = detail::event_rate_center(target, requested);
      ASSERT_GE(center, first);
      ASSERT_LT(center, end);
      ASSERT_LE(std::fabs(center - requested), detail::EVENT_RATE_CENTER_QUANTUM_TT_DAYS);
      ASSERT_NO_THROW(detail::validate_rate_input(target, center));
    }
    ASSERT_EQ(detail::event_rate_center(target, first), first);
    ASSERT_EQ(detail::event_rate_center(target, end), end - detail::EVENT_RATE_CENTER_QUANTUM_TT_DAYS);
  }
}

TEST(EphemerisEventsKernel, EventRateCentersRepairInexactStencilsWithoutChangingTheRateApi) {
  const double requested = std::nextafter(2097152.0, 0.0);
  ASSERT_THROW(detail::validate_rate_input(Target::SUN, requested), std::invalid_argument);
  ASSERT_EQ(detail::event_rate_center(Target::SUN, requested), 2097152.0);
  const double half_quantum = detail::EVENT_RATE_CENTER_QUANTUM_TT_DAYS / 2.0;
  ASSERT_EQ(detail::event_rate_center(Target::SUN, half_quantum), 0.0);
  ASSERT_EQ(detail::event_rate_center(Target::SUN, -half_quantum), 0.0);
  ASSERT_EQ(detail::event_rate_center(Target::SUN, 3.0 * half_quantum), 4.0 * half_quantum);
}

TEST(EphemerisEventsKernel, EventRateValuesChargeTheActualCenterDisplacement) {
  std::size_t calls = 0;
  const auto linear = [&calls](const double center) -> search::Value {
    ++calls;
    detail::validate_rate_input(Target::SUN, center);
    return { .value = center, .error = 0.0 };
  };
  const double requested = std::nextafter(2097152.0, 0.0);
  const auto shifted = detail::event_rate_value(linear, Target::SUN, requested, 1.0);
  ASSERT_EQ(shifted.value, 2097152.0);
  ASSERT_GE(shifted.error, std::fabs(shifted.value - requested));
  const auto unchanged = detail::event_rate_value(linear, Target::SUN, 2097152.0, 1.0);
  ASSERT_EQ(unchanged.value, 2097152.0);
  ASSERT_EQ(unchanged.error, 0.0);
  const auto [first, end] = longitude_rate_domain(Target::SUN);
  const auto virtual_endpoint = detail::event_rate_value(linear, Target::SUN, end, 1.0);
  ASSERT_GE(virtual_endpoint.error, end - virtual_endpoint.value);
  ASSERT_EQ(calls, 3);
  ASSERT_GE(virtual_endpoint.value, first);
  ASSERT_LT(virtual_endpoint.value, end);
}

TEST(EphemerisEventsKernel, EventRateAdmissionAndBadObservationsCannotBeHiddenByDisplacement) {
  std::size_t calls = 0;
  const auto counted = [&calls](const double center) -> search::Value {
    ++calls;
    return { .value = center, .error = 0.0 };
  };
  const auto [first, end] = longitude_rate_domain(Target::SUN);
  for (const double time : std::array {
         std::nextafter(first, -std::numeric_limits<double>::infinity()),
         std::nextafter(end, std::numeric_limits<double>::infinity()),
         std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
       }) {
    ASSERT_THROW(static_cast<void>(detail::event_rate_value(counted, Target::SUN, time, 1.0)), std::invalid_argument);
  }
  ASSERT_THROW(static_cast<void>(detail::event_rate_value(counted, Target::SUN, 2451545.0, -1.0)), std::invalid_argument);
  ASSERT_THROW(
    static_cast<void>(detail::event_rate_value(counted, static_cast<Target>(uint32_t { 100 }), 2451545.0, 1.0)),
    std::invalid_argument
  );
  ASSERT_EQ(calls, 0);
  const auto invalid = [](const double) -> search::Value { return { .value = 0.0, .error = -1.0 }; };
  ASSERT_THROW(
    static_cast<void>(detail::event_rate_value(invalid, Target::SUN, std::nextafter(2097152.0, 0.0), 1e12)),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, LongitudeDifferencesPreserveOrientationAndCanonicalOpposition) {
  const search::LongitudeValue longitude_100 { .longitude = toolbox::AngleDeg { 100.0 }, .error_deg = 0.0 };
  const search::LongitudeValue longitude_40 { .longitude = toolbox::AngleDeg { 40.0 }, .error_deg = 0.0 };
  ASSERT_EQ(search::project_longitude_difference(longitude_100, longitude_40).value, 60.0);
  ASSERT_EQ(search::project_longitude_difference(longitude_40, longitude_100).value, -60.0);
  ASSERT_TRUE(search::exact_zero(search::project_longitude_difference(longitude_100, longitude_100)));
  ASSERT_EQ(toolbox::canonical_longitude_difference(toolbox::AngleDeg { -180.0 }).deg(), 180.0);
  ASSERT_EQ(toolbox::canonical_longitude_difference(toolbox::AngleDeg { 540.0 }).deg(), 180.0);
  ASSERT_FALSE(std::signbit(toolbox::canonical_longitude_difference(toolbox::AngleDeg { -0.0 }).deg()));

  // Exact degree identities modulo 360, independent of any ephemeris.
  const search::LongitudeValue before { .longitude = toolbox::AngleDeg { 359.0 }, .error_deg = 0.0 };
  const search::LongitudeValue after { .longitude = toolbox::AngleDeg { 1.0 }, .error_deg = 0.0 };
  ASSERT_EQ(search::project_longitude_difference(after, before).value, 2.0);
  ASSERT_EQ(search::project_longitude_difference(before, after).value, -2.0);
}

TEST(EphemerisEventsKernel, LongitudeDifferenceAllowancesIncludeBothInputsAndSubtractionRoundoff) {
  const search::LongitudeValue first { .longitude = toolbox::AngleDeg { 1.0 }, .error_deg = 0.0 };
  const search::LongitudeValue second { .longitude = toolbox::AngleDeg { 0x1p-54 }, .error_deg = 0.0 };
  const auto rounded = search::project_longitude_difference(first, second);
  ASSERT_EQ(rounded.value, 1.0);
  ASSERT_GE(rounded.error, 0x1p-54);
  const auto noisy = search::project_longitude_difference(
    { .longitude = first.longitude, .error_deg = 0.125 },
    { .longitude = second.longitude, .error_deg = 0.25 }
  );
  ASSERT_GE(noisy.error, 0.375);
  ASSERT_THROW(
    static_cast<void>(search::longitude_residual(
      { .longitude = toolbox::AngleDeg { 180.0 }, .error_deg = 0.0 },
      toolbox::AngleDeg { 0.0 }
    )),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, LongitudeCellsCrossZeroInBothDirectionsWithoutAntipodalRoots) {
  const search::Bounds bounds { .first_derivative = 16.0, .second_derivative = 0.0 };
  const auto increasing = [](const double time) { return longitude_sum(344.0, 16.0 * time); };
  const auto decreasing = [](const double time) { return longitude_sum(16.0, -16.0 * time); };
  const auto forward = longitude_roots(increasing, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain, bounds);
  const auto backward = longitude_roots(decreasing, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain, bounds);
  ASSERT_EQ(forward.size(), 1);
  ASSERT_EQ(backward.size(), 1);
  ASSERT_EQ(forward.at(0).jde_tt, 1.0);
  ASSERT_EQ(backward.at(0).jde_tt, 1.0);
  ASSERT_TRUE(forward.at(0).increasing);
  ASSERT_FALSE(backward.at(0).increasing);
  const auto antipodal = [](const double time) { return longitude_sum(164.0, 16.0 * time); };
  ASSERT_TRUE(longitude_roots(antipodal, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain, bounds).empty());
  ASSERT_TRUE(longitude_roots(increasing, toolbox::AngleDeg { 180.0 }, TWO_CELL_GRID.domain, bounds).empty());
}

TEST(EphemerisEventsKernel, LongitudeCellsKeepHalfOpenOwnershipAndTargetBranchInvariance) {
  const search::Bounds bounds { .first_derivative = 16.0, .second_derivative = 0.0 };
  const auto longitude = [](const double time) { return longitude_sum(344.0, 16.0 * time); };
  for (const int32_t branch : std::array<int32_t, 5> { -2, -1, 0, 1, 2 }) {
    const toolbox::AngleDeg target { 360.0 * static_cast<double>(branch) };
    const auto whole = longitude_roots(longitude, target, TWO_CELL_GRID.domain, bounds);
    ASSERT_TRUE(longitude_roots(longitude, target, FIRST_CELL_INTERVAL, bounds).empty());
    const auto right = longitude_roots(longitude, target, SECOND_CELL_INTERVAL, bounds);
    ASSERT_EQ(whole.size(), 1);
    ASSERT_EQ(right.size(), 1);
    ASSERT_EQ(whole.at(0).jde_tt, 1.0);
    ASSERT_EQ(right.at(0).jde_tt, whole.at(0).jde_tt);
  }
}

TEST(EphemerisEventsKernel, RelativeLongitudeCellsPreserveTheSignOfTheRequestedDifference) {
  const search::Bounds bounds { .first_derivative = 16.0, .second_derivative = 0.0 };
  const auto relative = [](const double time) -> search::LongitudeValue {
    const auto difference = search::project_longitude_difference(
      longitude_sum(344.0, 16.0 * time),
      longitude_sum(300.0, 0.0)
    );
    return { .longitude = toolbox::AngleDeg { difference.value }, .error_deg = difference.error };
  };
  const auto reversed = [](const double time) -> search::LongitudeValue {
    const auto difference = search::project_longitude_difference(
      longitude_sum(300.0, 0.0),
      longitude_sum(344.0, 16.0 * time)
    );
    return { .longitude = toolbox::AngleDeg { difference.value }, .error_deg = difference.error };
  };
  const auto positive = longitude_roots(relative, toolbox::AngleDeg { 60.0 }, TWO_CELL_GRID.domain, bounds);
  const auto negative = longitude_roots(reversed, toolbox::AngleDeg { -60.0 }, TWO_CELL_GRID.domain, bounds);
  ASSERT_EQ(positive.size(), 1);
  ASSERT_EQ(negative.size(), 1);
  ASSERT_EQ(positive.at(0).jde_tt, 1.0);
  ASSERT_EQ(negative.at(0).jde_tt, 1.0);
  ASSERT_TRUE(positive.at(0).increasing);
  ASSERT_FALSE(negative.at(0).increasing);
  ASSERT_TRUE(longitude_roots(relative, toolbox::AngleDeg { -60.0 }, TWO_CELL_GRID.domain, bounds).empty());
}

TEST(EphemerisEventsKernel, LongitudeCellsLeaveContactsUnresolvedAndRejectUncertifiedLifts) {
  const auto tangent = [](const double time) -> search::LongitudeValue {
    const double offset = time - 1.0;
    const double value = offset * offset;
    return {
      .longitude = toolbox::AngleDeg { value },
      .error_deg = 2.0 * std::numeric_limits<double>::epsilon() * value,
    };
  };
  const search::Bounds tangent_bounds { .first_derivative = 2.0, .second_derivative = 2.0 };
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  const auto candidates = search::isolate_longitude_cell(
    tangent, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain, tangent_bounds, budget
  );
  ASSERT_FALSE(candidates.empty());
  for (const auto& candidate : candidates) {
    ASSERT_EQ(candidate.isolation, search::Isolation::UNRESOLVED);
  }
  const auto fast = [](const double time) { return longitude_sum(0.0, 360.0 * time); };
  const search::Bounds fast_bounds { .first_derivative = 360.0, .second_derivative = 0.0 };
  search::Budget limited { .evaluations_remaining = 10, .candidates_remaining = 10 };
  ASSERT_THROW(
    static_cast<void>(search::isolate_longitude_cell(
      fast, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain, fast_bounds, limited
    )),
    std::runtime_error
  );
  ASSERT_EQ(limited.evaluations_remaining, 9);
}

TEST(EphemerisEventsKernel, LongitudeCellAdmissionAndObservationFailuresAreExplicit) {
  std::size_t calls = 0;
  const auto counted = [&calls](const double time) {
    ++calls;
    return longitude_sum(344.0, 16.0 * time);
  };
  const search::Bounds bounds { .first_derivative = 16.0, .second_derivative = 0.0 };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  ASSERT_THROW(
    static_cast<void>(search::isolate_longitude_cell(
      counted, toolbox::AngleDeg { std::numeric_limits<double>::quiet_NaN() }, TWO_CELL_GRID.domain, bounds, budget
    )),
    std::invalid_argument
  );
  ASSERT_EQ(calls, 0);
  ASSERT_EQ(budget.evaluations_remaining, 100);
  for (const search::LongitudeValue invalid : std::array {
         search::LongitudeValue { .longitude = toolbox::AngleDeg { 0.0 }, .error_deg = -1.0 },
         search::LongitudeValue { .longitude = toolbox::AngleDeg { 0.0 }, .error_deg = std::numeric_limits<double>::infinity() },
         search::LongitudeValue { .longitude = toolbox::AngleDeg { std::numeric_limits<double>::infinity() }, .error_deg = 0.0 },
       }) {
    const auto bad = [invalid](const double) { return invalid; };
    ASSERT_THROW(
      static_cast<void>(search::isolate_longitude_cell(bad, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain, bounds, budget)),
      std::runtime_error
    );
  }
  ASSERT_EQ(budget.evaluations_remaining, 97);
}

TEST(EphemerisEventsKernel, LongitudeRangesKeepRoundedBoundaryOwnershipAcrossTargetBranches) {
  // The roots are 1 +/- 2^-55; both report the binary64 time 1.
  for (const double offset : std::array { 0x1p-55, -0x1p-55 }) {
    const auto linear = [offset](const double time) { return longitude_sum(time - 1.0, offset); };
    for (const double degrees : std::array { -360.0, 0.0, 360.0 }) {
      const toolbox::AngleDeg target { degrees };
      const auto full = longitude_range_roots(linear, target, TWO_CELL_GRID.domain);
      const auto right = longitude_range_roots(linear, target, SECOND_CELL_INTERVAL);
      ASSERT_EQ(full.size(), 1);
      ASSERT_EQ(right.size(), 1);
      ASSERT_EQ(full.at(0).jde_tt, 1.0);
      ASSERT_EQ(right.at(0).jde_tt, full.at(0).jde_tt);
      ASSERT_TRUE(longitude_range_roots(linear, target, FIRST_CELL_INTERVAL).empty());
    }
  }
}

TEST(EphemerisEventsKernel, LongitudeRangesOwnExactStartsAndExcludeTheFinalEnd) {
  for (const double root : std::array { 0.0, 1.0, 2.0 }) {
    const auto linear = [root](const double time) { return longitude_sum(time, -root); };
    const auto found = longitude_range_roots(linear, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain);
    if (root == TWO_CELL_GRID.domain.end_jde_tt) {
      ASSERT_TRUE(found.empty());
    } else {
      ASSERT_EQ(found.size(), 1);
      ASSERT_EQ(found.at(0).jde_tt, root);
    }
  }
}

TEST(EphemerisEventsKernel, LongitudeRangesRejectOnlyQueryRelevantUnresolvedContacts) {
  const auto tangent = [](const double time) -> search::LongitudeValue {
    const double offset = time - 0.25;
    const double value = offset * offset;
    return {
      .longitude = toolbox::AngleDeg { value },
      .error_deg = 2.0 * std::numeric_limits<double>::epsilon() * value,
    };
  };
  const search::Bounds bounds { .first_derivative = 3.5, .second_derivative = 2.0 };
  ASSERT_TRUE(longitude_range_roots(tangent, toolbox::AngleDeg { 0.0 }, SECOND_CELL_INTERVAL, bounds).empty());
  ASSERT_THROW(
    static_cast<void>(longitude_range_roots(tangent, toolbox::AngleDeg { 0.0 }, FIRST_CELL_INTERVAL, bounds)),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, LongitudeRangeFailureCannotReturnCompletedEarlierCells) {
  bool visited_root = false;
  const auto fails_later = [&visited_root](const double time) -> search::LongitudeValue {
    if (time > 1.0) {
      throw std::runtime_error { "Synthetic longitude provider failed in the second cell" };
    }
    visited_root = visited_root or time == 0.25;
    return longitude_sum(time, -0.25);
  };
  ASSERT_THROW(
    static_cast<void>(longitude_range_roots(fails_later, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID.domain)),
    std::runtime_error
  );
  ASSERT_TRUE(visited_root);
}

TEST(EphemerisEventsKernel, LongitudeRangesUseTheProvidersVirtualFinalEndpoint) {
  const double last = std::nextafter(1.0, 0.0);
  std::size_t forbidden_calls = 0;
  const auto physical = [last, &forbidden_calls](const double time) -> search::LongitudeValue {
    if (time > last) {
      ++forbidden_calls;
      throw std::runtime_error { "Outside synthetic longitude domain" };
    }
    return longitude_sum(time, -0.75);
  };
  const auto extended = [&physical, last](const double time) -> search::LongitudeValue {
    if (time > last) {
      const auto evaluated = physical(last);
      return {
        .longitude = evaluated.longitude,
        .error_deg = search::upper(evaluated.error_deg + search::upper(time - last)),
      };
    }
    return physical(time);
  };
  const search::Grid grid { .domain = FIRST_CELL_INTERVAL, .cell_width_tt_days = 1.0 };
  search::Budget budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
  const auto found = range_test::longitude_roots_in_interval(
    extended, toolbox::AngleDeg { 0.0 }, grid, grid.domain, LINEAR_BOUNDS, budget
  );
  ASSERT_EQ(found.size(), 1);
  ASSERT_EQ(found.at(0).jde_tt, 0.75);
  ASSERT_EQ(forbidden_calls, 0);
}

TEST(EphemerisEventsKernel, LongitudeRangeAdmissionPrecedesProviderWork) {
  std::size_t calls = 0;
  const auto counted = [&calls](const double time) {
    ++calls;
    return longitude_sum(time, 0.0);
  };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  const search::Interval outside { .start_jde_tt = -1.0, .end_jde_tt = 1.0 };
  const search::Grid invalid { .domain = TWO_CELL_GRID.domain, .cell_width_tt_days = 0.0 };
  ASSERT_THROW(
    static_cast<void>(range_test::longitude_roots_in_interval(
      counted, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID, outside, LINEAR_BOUNDS, budget
    )),
    std::invalid_argument
  );
  ASSERT_THROW(
    static_cast<void>(range_test::longitude_roots_in_interval(
      counted, toolbox::AngleDeg { 0.0 }, invalid, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget
    )),
    std::invalid_argument
  );
  ASSERT_THROW(
    static_cast<void>(range_test::longitude_roots_in_interval(
      counted, toolbox::AngleDeg { std::numeric_limits<double>::infinity() },
      TWO_CELL_GRID, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget
    )),
    std::invalid_argument
  );
  ASSERT_THROW(
    static_cast<void>(range_test::longitude_roots_in_interval(
      counted, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget, -1.0
    )),
    std::invalid_argument
  );
  ASSERT_EQ(calls, 0);
  ASSERT_EQ(budget.evaluations_remaining, 100);
}

TEST(EphemerisEventsKernel, EventLongitudesUseActualDescendingPositionsWithoutChangingPairedRates) {
  constexpr double JDE_TT = 2451545.0;
  for (const auto target : EVENT_TARGETS) {
    const auto expected = [target]() -> toolbox::AngleDeg {
      using astro::lunar_node::Node;
      if (target == Target::MEAN_DESCENDING) {
        return astro::lunar_node::position(Node::MEAN_DESCENDING, JDE_TT);
      }
      if (target == Target::TRUE_DESCENDING) {
        return astro::lunar_node::position(Node::TRUE_DESCENDING, JDE_TT);
      }
      return detail::longitude_for_rate(target, JDE_TT);
    };
    ASSERT_EQ(detail::event_longitude(target, JDE_TT).deg(), expected().deg());
  }
  for (const auto node : std::array { Target::MEAN_DESCENDING, Target::TRUE_DESCENDING }) {
    ASSERT_NE(detail::event_longitude(node, JDE_TT).deg(), detail::longitude_for_rate(node, JDE_TT).deg());
  }
  ASSERT_THROW(
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Deliberately exercise unknown-target rejection.
    static_cast<void>(detail::event_longitude(static_cast<Target>(uint32_t { 100 }), JDE_TT)),
    std::invalid_argument
  );
}

TEST(EphemerisEventsKernel, ConstantLongitudeRelationsUseAllTargetIdentitiesAndRejectUnknownValues) {
  uint32_t constant_relations = 0;
  uint32_t variable_relations = 0;
  for (const auto target_a : EVENT_TARGETS) {
    for (const auto target_b : EVENT_TARGETS) {
      const auto relation = detail::constant_longitude_difference(target_a, target_b);
      using enum Target;
      const bool mean_pair = (target_a == MEAN_ASCENDING and target_b == MEAN_DESCENDING)
                             or (target_a == MEAN_DESCENDING and target_b == MEAN_ASCENDING);
      const bool true_pair = (target_a == TRUE_ASCENDING and target_b == TRUE_DESCENDING)
                             or (target_a == TRUE_DESCENDING and target_b == TRUE_ASCENDING);
      const bool constant = target_a == target_b or mean_pair or true_pair;
      ASSERT_EQ(relation.has_value(), constant);
      if (relation.has_value()) {
        ++constant_relations;
        ASSERT_EQ(relation->deg(), target_a == target_b ? 0.0 : 180.0);
      } else {
        ++variable_relations;
      }
    }
  }
  ASSERT_EQ(constant_relations, 18);
  ASSERT_EQ(variable_relations, 178);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Deliberately test validation before identity shortcuts.
  const auto unknown = static_cast<Target>(uint32_t { 100 });
  ASSERT_THROW(static_cast<void>(detail::constant_longitude_difference(unknown, unknown)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(detail::constant_longitude_difference(Target::SUN, unknown)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(detail::constant_longitude_difference(unknown, Target::SUN)), std::invalid_argument);
}

TEST(EphemerisEventsKernel, ReportingLatticesDetermineScalarAndLongitudeQueryOwnership) {
  constexpr double QUANTUM_TT_DAYS = 0.125;
  for (const double root : std::array { 1.0 - 0.015625, 1.0 + 0.015625 }) {
    const auto scalar = [root](const double time) { return sum_value(time, -root); };
    const auto longitude = [root](const double time) { return longitude_sum(time, -root); };
    for (const auto query : std::array { TWO_CELL_GRID.domain, FIRST_CELL_INTERVAL, SECOND_CELL_INTERVAL }) {
      search::Budget scalar_budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
      search::Budget longitude_budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
      const auto scalar_found = range_test::roots_in_interval(
        scalar, TWO_CELL_GRID, query, LINEAR_BOUNDS, scalar_budget, 0.0, QUANTUM_TT_DAYS
      );
      const auto longitude_found = range_test::longitude_roots_in_interval(
        longitude, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID, query, LINEAR_BOUNDS,
        longitude_budget, 0.0, QUANTUM_TT_DAYS
      );
      ASSERT_EQ(scalar_found.size(), query.end_jde_tt == 1.0 ? 0 : 1);
      ASSERT_EQ(longitude_found.size(), scalar_found.size());
      if (not scalar_found.empty()) {
        ASSERT_EQ(scalar_found.at(0).jde_tt, 1.0);
        ASSERT_EQ(longitude_found.at(0).jde_tt, scalar_found.at(0).jde_tt);
      }
    }
  }
}

TEST(EphemerisEventsKernel, ReportingLatticesDoNotClampExcludedEndsToLegalRateCenters) {
  const auto near_end = [](const double time) { return sum_value(time, -(2.0 - 0.015625)); };
  search::Budget budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
  ASSERT_TRUE(range_test::roots_in_interval(
    near_end, TWO_CELL_GRID, TWO_CELL_GRID.domain, LINEAR_BOUNDS, budget, 0.0, 0.125
  ).empty());
  ASSERT_EQ(search::reported_time(0.0625, 0.125), 0.0);
  ASSERT_EQ(search::reported_time(-0.0625, 0.125), 0.0);
  ASSERT_EQ(search::reported_time(0.1875, 0.125), 0.25);
}

TEST(EphemerisEventsKernel, ReportingLatticesKeepMappedUnresolvedRegionsAndDistinctRootsExplicit) {
  const auto tangent = [](const double time) -> search::Value {
    const double offset = time - (1.0 - 0.015625);
    const double value = offset * offset;
    return { .value = value, .error = 2.0 * std::numeric_limits<double>::epsilon() * value };
  };
  const search::Bounds tangent_bounds { .first_derivative = 3.0, .second_derivative = 2.0 };
  search::Budget tangent_budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  ASSERT_THROW(
    static_cast<void>(range_test::roots_in_interval(
      tangent, TWO_CELL_GRID, SECOND_CELL_INTERVAL, tangent_bounds, tangent_budget, 0.0, 0.125
    )),
    std::runtime_error
  );

  // Exact dyadic roots 1 +/- 1/64 collide on the 1/8-day reporting lattice.
  const auto close_pair = [](const double time) -> search::Value {
    const double offset = time - 1.0;
    const double value = (offset * offset) - 0x1p-12;
    const bool exact_root = offset == 0.015625 or offset == -0.015625;
    return { .value = value, .error = exact_root ? 0.0 : 1e-15 };
  };
  const search::Bounds pair_bounds { .first_derivative = 2.0, .second_derivative = 2.0 };
  search::Budget distinct_budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  const auto distinct = range_test::roots_in_interval(
    close_pair, TWO_CELL_GRID, TWO_CELL_GRID.domain, pair_bounds, distinct_budget, 1e-8
  );
  ASSERT_EQ(distinct.size(), 2);
  ASSERT_EQ(distinct.at(0).jde_tt, 1.0 - 0.015625);
  ASSERT_EQ(distinct.at(1).jde_tt, 1.0 + 0.015625);
  search::Budget pair_budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  ASSERT_THROW(
    static_cast<void>(range_test::roots_in_interval(
      close_pair, TWO_CELL_GRID, TWO_CELL_GRID.domain, pair_bounds, pair_budget, 1e-8, 0.125
    )),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, ReportingLatticeAdmissionPrecedesProviderWork) {
  std::size_t calls = 0;
  const auto counted = [&calls](const double time) {
    ++calls;
    return sum_value(time, -1.0);
  };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 100 };
  for (const double quantum : std::array {
         -0.125, 0.1, 0.75, std::numeric_limits<double>::denorm_min(),
         std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
       }) {
    ASSERT_THROW(
      static_cast<void>(range_test::roots_in_interval(
        counted, TWO_CELL_GRID, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget, 0.0, quantum
      )),
      std::invalid_argument
    );
  }
  ASSERT_EQ(calls, 0);
  ASSERT_EQ(budget.evaluations_remaining, 100);
}

TEST(EphemerisEventsKernel, CurvatureCertificatesCoverUnequalSpansAndEitherSecantSign) {
  for (const double orientation : std::array { -1.0, 1.0 }) {
    for (const double slope : std::array { -100.0, 0.0, 100.0 }) {
      const auto observed = [orientation, slope](const double time) -> search::Sample {
        return {
          .jde_tt = time,
          .evaluated = { .value = (orientation * time * time) + (slope * time), .error = 1e-10 },
        };
      };
      const auto curvature = search::curvature_sign(observed(-0.75), observed(-0.5), observed(1.0), 0.0);
      ASSERT_EQ(curvature, static_cast<int32_t>(orientation));
    }
  }
  const search::Sample left { .jde_tt = 0.0, .evaluated = { .value = 1.0, .error = 2.0 } };
  const search::Sample middle { .jde_tt = 0.5, .evaluated = { .value = 0.0, .error = 2.0 } };
  const search::Sample right { .jde_tt = 1.0, .evaluated = { .value = 1.0, .error = 2.0 } };
  ASSERT_EQ(search::curvature_sign(left, middle, right, 0.0), 0);
}

TEST(EphemerisEventsKernel, PositiveDividedDifferenceDoesNotProveTheWholeSpanIsConvex) {
  // f(t)=t^3 has a positive divided difference here, but f'' changes sign inside the span.
  const auto cubic = [](const double time) -> search::Sample {
    return { .jde_tt = time, .evaluated = { .value = time * time * time, .error = 1e-15 } };
  };
  ASSERT_EQ(search::curvature_sign(cubic(-0.25), cubic(0.5), cubic(1.0), 6.0), 0);
  ASSERT_EQ(search::curvature_sign(cubic(1.0), cubic(1.0625), cubic(1.125), 6.0), 1);
}

TEST(EphemerisEventsKernel, NumericalContactsUseExactCanonicalLevelsAndCertifiedSides) {
  const search::LongitudeValue at_station { .longitude = toolbox::AngleDeg { 359.0 }, .error_deg = 1e-9 };
  const search::Value positive { .value = 0.01, .error = 1e-9 };
  const search::Value negative { .value = -0.01, .error = 1e-9 };
  for (const double target : std::array { -1.0, 359.0, 719.0 }) {
    ASSERT_EQ(
      search::contact_direction(at_station, toolbox::AngleDeg { target }, positive, positive, 1),
      search::ContactDirection::TANGENT
    );
    ASSERT_EQ(
      search::contact_direction(at_station, toolbox::AngleDeg { target }, negative, negative, -1),
      search::ContactDirection::TANGENT
    );
    ASSERT_EQ(
      search::contact_direction(at_station, toolbox::AngleDeg { target }, negative, positive, 0),
      search::ContactDirection::INCREASING
    );
    ASSERT_EQ(
      search::contact_direction(at_station, toolbox::AngleDeg { target }, positive, negative, 0),
      search::ContactDirection::DECREASING
    );
  }
  for (const double nearby : std::array { std::nextafter(359.0, 0.0), std::nextafter(359.0, 360.0) }) {
    ASSERT_THROW(
      static_cast<void>(search::contact_direction(at_station, toolbox::AngleDeg { nearby }, positive, positive, 1)),
      std::runtime_error
    );
  }
  ASSERT_THROW(
    static_cast<void>(search::contact_direction(at_station, toolbox::AngleDeg { 359.0 }, positive, positive, 0)),
    std::runtime_error
  );
  ASSERT_THROW(
    static_cast<void>(search::contact_direction(at_station, toolbox::AngleDeg { 359.0 }, positive, positive, -1)),
    std::runtime_error
  );
  ASSERT_THROW(
    static_cast<void>(search::contact_direction(at_station, toolbox::AngleDeg { 359.0 }, positive, { 0.0, 1e-9 }, 1)),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, CurvatureAndContactRejectInvalidObservationsAndBounds) {
  const search::Sample left { .jde_tt = 0.0, .evaluated = { .value = 1.0, .error = 0.0 } };
  const search::Sample middle { .jde_tt = 0.5, .evaluated = { .value = 0.0, .error = 0.0 } };
  const search::Sample right { .jde_tt = 1.0, .evaluated = { .value = 1.0, .error = 0.0 } };
  ASSERT_THROW(static_cast<void>(search::curvature_sign(left, left, right, 0.0)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(search::curvature_sign(left, middle, right, -1.0)), std::invalid_argument);
  const search::Sample invalid { .jde_tt = 0.5, .evaluated = { .value = 0.0, .error = -1.0 } };
  ASSERT_THROW(static_cast<void>(search::curvature_sign(left, invalid, right, 0.0)), std::runtime_error);
  const search::LongitudeValue at_station { .longitude = toolbox::AngleDeg { 0.0 }, .error_deg = 1e-9 };
  ASSERT_THROW(
    static_cast<void>(search::contact_direction(at_station, toolbox::AngleDeg { 0.0 }, { 1.0, -1.0 }, { 1.0, 0.0 }, 1)),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, CompleteContactSearchUsesTheSameStationForFullAndSplitQueries) {
  constexpr double STATION_JDE_TT = 0.5;
  const auto longitude = [](const double time) -> search::LongitudeValue {
    const double offset = time - STATION_JDE_TT;
    return { .longitude = toolbox::AngleDeg { 359.0 + (offset * offset) }, .error_deg = 1e-12 };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = 2.0 * (time - STATION_JDE_TT), .error = 1e-12 };
  };
  const search::TrajectoryBounds bounds {
    .longitude = { .first_derivative = 3.0, .second_derivative = 2.0 },
    .rate = { .first_derivative = 2.0, .second_derivative = 0.0 },
    .third_derivative = 0.0,
    .rate_remainder = 0.0,
  };
  const search::Interval left { .start_jde_tt = 0.0, .end_jde_tt = STATION_JDE_TT };
  const search::Interval right { .start_jde_tt = STATION_JDE_TT, .end_jde_tt = 2.0 };
  for (const auto query : std::array { TWO_CELL_GRID.domain, left, right }) {
    search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
    const auto found = range_test::longitude_contacts_in_interval(
      longitude, rate, toolbox::AngleDeg { -1.0 }, TWO_CELL_GRID, query, bounds, budget, 1e-8, 0.125
    );
    ASSERT_EQ(found.size(), query.end_jde_tt == STATION_JDE_TT ? 0 : 1);
    if (not found.empty()) {
      ASSERT_EQ(found.at(0).jde_tt, STATION_JDE_TT);
      ASSERT_EQ(found.at(0).direction, search::ContactDirection::TANGENT);
    }
  }
}

TEST(EphemerisEventsKernel, ContactSearchKeepsResolvablePairsAndThrowsForBothUnresolvedNeighborLevels) {
  const auto longitude = [](const double time) -> search::LongitudeValue {
    const double offset = time - 0.5;
    return { .longitude = toolbox::AngleDeg { 359.0 + (offset * offset) }, .error_deg = 1e-12 };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = 2.0 * (time - 0.5), .error = 1e-12 };
  };
  const search::TrajectoryBounds bounds {
    .longitude = { .first_derivative = 3.0, .second_derivative = 2.0 },
    .rate = { .first_derivative = 2.0, .second_derivative = 0.0 },
    .third_derivative = 0.0,
    .rate_remainder = 0.0,
  };
  search::Budget pair_budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  const auto pair = range_test::longitude_contacts_in_interval(
    longitude, rate, toolbox::AngleDeg { 359.0625 }, TWO_CELL_GRID, TWO_CELL_GRID.domain,
    bounds, pair_budget, 1e-8
  );
  ASSERT_EQ(pair.size(), 2);
  ASSERT_NEAR(pair.at(0).jde_tt, 0.25, 1e-8);
  ASSERT_NEAR(pair.at(1).jde_tt, 0.75, 1e-8);
  ASSERT_EQ(pair.at(0).direction, search::ContactDirection::DECREASING);
  ASSERT_EQ(pair.at(1).direction, search::ContactDirection::INCREASING);
  for (const double target : std::array { std::nextafter(359.0, 0.0), std::nextafter(359.0, 360.0) }) {
    search::Budget budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
    ASSERT_THROW(
      static_cast<void>(range_test::longitude_contacts_in_interval(
        longitude, rate, toolbox::AngleDeg { target }, TWO_CELL_GRID, TWO_CELL_GRID.domain, bounds, budget, 1e-8
      )),
      std::runtime_error
    );
  }
}

TEST(EphemerisEventsKernel, CanonicalRateZerosAloneCannotManufactureLongitudeContacts) {
  const auto longitude = [](const double time) -> search::LongitudeValue {
    return { .longitude = toolbox::AngleDeg { time }, .error_deg = 1e-12 };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = time - 0.5, .error = 1e-12 };
  };
  const search::TrajectoryBounds bounds {
    .longitude = LINEAR_BOUNDS,
    .rate = LINEAR_BOUNDS,
    .third_derivative = 0.0,
    .rate_remainder = 2.0,
  };
  search::Budget budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
  ASSERT_THROW(
    static_cast<void>(range_test::longitude_contacts_in_interval(
      longitude, rate, toolbox::AngleDeg { 0.5 }, TWO_CELL_GRID, TWO_CELL_GRID.domain, bounds, budget, 1e-8
    )),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, ContactSearchCannotReturnAnEarlierTangentWhenALaterCellFails) {
  bool visited_contact = false;
  const auto longitude = [&visited_contact](const double time) -> search::LongitudeValue {
    if (time > 1.0) {
      throw std::runtime_error { "Synthetic longitude failed after the contact cell" };
    }
    visited_contact = visited_contact or time == 0.5;
    const double offset = time - 0.5;
    return { .longitude = toolbox::AngleDeg { offset * offset }, .error_deg = 1e-12 };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = 2.0 * (time - 0.5), .error = 1e-12 };
  };
  const search::TrajectoryBounds bounds {
    .longitude = { .first_derivative = 3.0, .second_derivative = 2.0 },
    .rate = { .first_derivative = 2.0, .second_derivative = 0.0 },
    .third_derivative = 0.0,
    .rate_remainder = 0.0,
  };
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  ASSERT_THROW(
    static_cast<void>(range_test::longitude_contacts_in_interval(
      longitude, rate, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID, TWO_CELL_GRID.domain, bounds, budget, 1e-8
    )),
    std::runtime_error
  );
  ASSERT_TRUE(visited_contact);
}

TEST(EphemerisEventsKernel, NoisySubdivisionMidpointsStayInsideCertifiedBrackets) {
  // Two exact dyadic roots would each be shared, uncertain endpoints without the inner bridge.
  const auto pair = [](const double time) -> search::Value {
    const double offset = time - 0.5;
    return { .value = (offset * offset) - 0.0625, .error = 1e-12 };
  };
  search::Budget budget { .evaluations_remaining = 1000, .candidates_remaining = 100 };
  const search::Bounds bounds { .first_derivative = 1.0, .second_derivative = 2.0 };
  const auto candidates = search::isolate_cell(pair, 0.0, 1.0, bounds, budget);
  ASSERT_EQ(candidates.size(), 2);
  ASSERT_EQ(candidates.at(0).isolation, search::Isolation::UNIQUE);
  ASSERT_EQ(candidates.at(1).isolation, search::Isolation::UNIQUE);
  const auto first = search::refine(pair, candidates.at(0), bounds, budget, 1e-8);
  const auto second = search::refine(pair, candidates.at(1), bounds, budget, 1e-8);
  ASSERT_EQ(first.jde_tt, 0.25);
  ASSERT_EQ(second.jde_tt, 0.75);
  ASSERT_FALSE(first.increasing);
  ASSERT_TRUE(second.increasing);
}

TEST(EphemerisEventsKernel, AResolvedBracketCanImproveItsRepresentativeWithoutRelaxingTheTimeCertificate) {
  const auto quadratic = [](const double time) -> search::Value {
    return { .value = (time * time) - 2.0, .error = 0.0 };
  };
  const search::Bounds bounds { .first_derivative = 4.0, .second_derivative = 2.0 };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 10 };
  const auto candidates = search::isolate_cell(quadratic, 1.0, 2.0, bounds, budget);
  ASSERT_EQ(candidates.size(), 1);
  const auto root = search::refine(quadratic, candidates.at(0), bounds, budget, 0.5);
  ASSERT_LT(std::fabs(root.jde_tt - std::sqrt(2.0)), std::fabs(1.25 - std::sqrt(2.0)));
  ASSERT_LE(std::fabs(root.jde_tt - std::sqrt(2.0)), 0.5);
  ASSERT_TRUE(root.increasing);
}

TEST(EphemerisEventsKernel, SecantPolishingCannotTrustAnUncertainZeroOrChangeIdentityWithTheBudget) {
  const search::Sample left { .jde_tt = 1.0, .evaluated = { .value = -1.0, .error = 0.0 } };
  const search::Sample right { .jde_tt = 2.0, .evaluated = { .value = 2.0, .error = 0.0 } };
  const search::Candidate candidate { .left = left, .right = right, .isolation = search::Isolation::UNIQUE };
  const search::Bounds bounds { .first_derivative = 4.0, .second_derivative = 2.0 };
  std::size_t calls = 0;
  const auto uncertain = [&calls](const double time) -> search::Value {
    ++calls;
    return { .value = 0.0, .error = 100.0 + (time * time) };
  };
  search::Budget budget { .evaluations_remaining = 10, .candidates_remaining = 10 };
  ASSERT_EQ(search::refine(uncertain, candidate, bounds, budget, 0.6).jde_tt, 1.5);
  ASSERT_EQ(calls, 1);
  search::Budget empty { .evaluations_remaining = 0, .candidates_remaining = 0 };
  ASSERT_THROW(static_cast<void>(search::refine(uncertain, candidate, bounds, empty, 0.6)), std::runtime_error);
  ASSERT_EQ(calls, 1);
}

TEST(EphemerisEventsKernel, ContactRegionsMayExceedRateBracketsWithoutReducingTheDerivativeRemainder) {
  const auto longitude = [](const double time) -> search::LongitudeValue {
    const double offset = time - 0.5;
    return { .longitude = toolbox::AngleDeg { offset * offset }, .error_deg = 1e-12 };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = 2.0 * (time - 0.5), .error = 0.0 };
  };
  const search::Candidate candidate {
    .left = { .jde_tt = 0.49, .evaluated = rate(0.49) },
    .right = { .jde_tt = 0.51, .evaluated = rate(0.51) },
    .isolation = search::Isolation::UNIQUE,
  };
  const search::TrajectoryBounds bounds {
    .longitude = { .first_derivative = 1.0, .second_derivative = 2.0 },
    .rate = { .first_derivative = 2.0, .second_derivative = 0.0 },
    .third_derivative = 0.0,
    .rate_remainder = 0.1,
  };
  const search::Interval cell { .start_jde_tt = 0.0, .end_jde_tt = 1.0 };
  search::Budget budget { .evaluations_remaining = 100, .candidates_remaining = 10 };
  const auto contact = range_test::numerical_contact(
    longitude, rate, candidate, cell, toolbox::AngleDeg { 0.0 }, bounds, budget, 1e-8, 0.0
  );
  if (not contact.has_value()) {
    FAIL() << "Exact numerical level lost its certified expanded contact region";
    return;
  }
  ASSERT_EQ(contact->root.jde_tt, 0.5);
  ASSERT_EQ(contact->root.direction, search::ContactDirection::TANGENT);
  ASSERT_EQ(contact->region.start_jde_tt, 0.0);
  ASSERT_EQ(contact->region.end_jde_tt, 1.0);
  const search::TrajectoryBounds unresolved {
    .longitude = bounds.longitude,
    .rate = bounds.rate,
    .third_derivative = 1e6,
    .rate_remainder = bounds.rate_remainder,
  };
  ASSERT_THROW(
    static_cast<void>(range_test::numerical_contact(
      longitude, rate, candidate, cell, toolbox::AngleDeg { 0.0 }, unresolved, budget, 1e-8, 0.0
    )),
    std::runtime_error
  );
}

TEST(EphemerisEventsKernel, LooseThirdDerivativeBoundsCanRequireANarrowerContactRegion) {
  const auto longitude = [](const double time) -> search::LongitudeValue {
    const double offset = time - 0.5;
    return { .longitude = toolbox::AngleDeg { offset * offset }, .error_deg = 1e-12 };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = 2.0 * (time - 0.5), .error = 0.0 };
  };
  const search::Candidate candidate {
    .left = { .jde_tt = 0.25, .evaluated = rate(0.25) },
    .right = { .jde_tt = 0.75, .evaluated = rate(0.75) },
    .isolation = search::Isolation::UNIQUE,
  };
  const search::TrajectoryBounds bounds {
    .longitude = { .first_derivative = 1.0, .second_derivative = 2.0 },
    .rate = { .first_derivative = 2.0, .second_derivative = 0.0 },
    .third_derivative = 100.0,
    .rate_remainder = 0.0,
  };
  search::Budget budget { .evaluations_remaining = 200, .candidates_remaining = 10 };
  const auto contact = range_test::numerical_contact(
    longitude, rate, candidate, { .start_jde_tt = 0.0, .end_jde_tt = 1.0 }, toolbox::AngleDeg { 0.0 }, bounds, budget, 1e-8, 0.0
  );
  if (not contact.has_value()) {
    FAIL() << "Exact numerical level lost its certified narrow contact region";
    return;
  }
  ASSERT_EQ(contact->root.jde_tt, 0.5);
  ASSERT_GT(contact->region.start_jde_tt, candidate.left.jde_tt);
  ASSERT_LT(contact->region.end_jde_tt, candidate.right.jde_tt);
}

} // namespace astro::ephemeris::test

namespace astro::ephemeris::test {

// PyMeeus c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92, collected 2026-10-19;
// source-only apparent longitudes and an independent 11-point h=1/16-day station rate.
// Reference epochs retain their source terms; see provenance/ephemeris-events/ATTRIBUTION.md.
// The measured model gaps are below 9e-7 day here; 5e-6 day retains separation from incorrect branches.
inline constexpr double EVENT_REFERENCE_TOLERANCE_TT_DAYS = 5e-6;

TEST(EphemerisEvents, IndependentCrossingsIncludeAllThreeMercuryPasses) {
  const auto solar = longitude_events(Target::SUN, toolbox::AngleDeg { 0.0 }, 2460745.5, 2460755.5);
  ASSERT_EQ(solar.size(), 1);
  ASSERT_NEAR(solar.at(0).jde_tt, 2460754.876817532, EVENT_REFERENCE_TOLERANCE_TT_DAYS);
  ASSERT_EQ(solar.at(0).direction, EventDirection::INCREASING);
  const auto mercury = longitude_events(Target::MERCURY, toolbox::AngleDeg { 0.0 }, 2460735.5, 2460785.5);
  const std::array expected {
    std::pair { 2460737.8783325925, EventDirection::INCREASING },
    std::pair { 2460764.5966444390, EventDirection::DECREASING },
    std::pair { 2460781.7682453170, EventDirection::INCREASING },
  };
  ASSERT_EQ(mercury.size(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    ASSERT_NEAR(mercury.at(index).jde_tt, expected.at(index).first, EVENT_REFERENCE_TOLERANCE_TT_DAYS);
    ASSERT_EQ(mercury.at(index).direction, expected.at(index).second);
    ASSERT_EQ(mercury.at(index).longitude_rate.deg_per_tt_day,
              longitude_rate(Target::MERCURY, mercury.at(index).jde_tt).deg_per_tt_day);
    if (index > 0) {
      ASSERT_LT(mercury.at(index - 1).jde_tt, mercury.at(index).jde_tt);
    }
  }
}

TEST(EphemerisEvents, DirectedPairReferencesAndWholeTurnBranchesStayDistinct) {
  for (const auto& [target, expected] : std::array {
         std::pair { 60.0, 2460680.1893874330 }, std::pair { -60.0, 2460700.0811263844 } }) {
    const auto pair = longitude_difference_events(
      Target::MOON, Target::SUN, toolbox::AngleDeg { target }, 2460676.5, 2460707.5
    );
    const auto shifted = longitude_difference_events(
      Target::MOON, Target::SUN, toolbox::AngleDeg { target + 360.0 }, 2460676.5, 2460707.5
    );
    ASSERT_EQ(pair.size(), 1);
    ASSERT_EQ(shifted.size(), 1);
    ASSERT_NEAR(pair.at(0).jde_tt, expected, EVENT_REFERENCE_TOLERANCE_TT_DAYS);
    ASSERT_EQ(pair.at(0).jde_tt, shifted.at(0).jde_tt);
    ASSERT_EQ(pair.at(0).direction, EventDirection::INCREASING);
    ASSERT_NEAR(pair.at(0).signed_difference.deg(), target, 1e-3);
    const auto reversed = longitude_difference_events(
      Target::SUN, Target::MOON, toolbox::AngleDeg { -target }, 2460676.5, 2460707.5
    );
    ASSERT_EQ(reversed.size(), 1);
    ASSERT_EQ(pair.at(0).jde_tt, reversed.at(0).jde_tt);
    ASSERT_EQ(reversed.at(0).direction, EventDirection::DECREASING);
  }
  const auto mercury_sun = longitude_difference_events(
    Target::MERCURY, Target::SUN, toolbox::AngleDeg { 0.0 }, 2460755.5, 2460763.5
  );
  ASSERT_EQ(mercury_sun.size(), 1);
  ASSERT_NEAR(mercury_sun.at(0).jde_tt, 2460759.3260307834, EVENT_REFERENCE_TOLERANCE_TT_DAYS);
  ASSERT_EQ(mercury_sun.at(0).direction, EventDirection::DECREASING);
}

TEST(EphemerisEvents, StationReferencesAndConstructedContactsUseOneCanonicalTime) {
  const auto found = stations(Target::MERCURY, 2460735.5, 2460785.5);
  const std::array expected { 2460749.7828102987, 2460772.9644271974 };
  ASSERT_EQ(found.size(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const auto& station = found.at(index);
    SCOPED_TRACE(index);
    ASSERT_NEAR(station.jde_tt, expected.at(index), EVENT_REFERENCE_TOLERANCE_TT_DAYS);
    const auto contact = longitude_events(Target::MERCURY, station.longitude, station.jde_tt - 0.5, station.jde_tt + 0.5);
    ASSERT_EQ(contact.size(), 1);
    ASSERT_EQ(contact.at(0).jde_tt, station.jde_tt);
    ASSERT_EQ(contact.at(0).direction, EventDirection::TANGENT);
  }
  ASSERT_EQ(found.at(0).motion_before, Motion::DIRECT);
  ASSERT_EQ(found.at(0).motion_after, Motion::RETROGRADE);
  ASSERT_EQ(found.at(1).motion_before, Motion::RETROGRADE);
  ASSERT_EQ(found.at(1).motion_after, Motion::DIRECT);
  ASSERT_TRUE(stations(Target::SUN, 2460735.5, 2460785.5).empty());
  ASSERT_TRUE(stations(Target::MOON, 2460735.5, 2460785.5).empty());
  ASSERT_TRUE(stations(Target::MEAN_DESCENDING, 2460735.5, 2460785.5).empty());
}

TEST(EphemerisEvents, PublicCrossingsAndStationsHaveExactHalfOpenReportedTimeOwnership) {
  const auto crossing = longitude_events(Target::SUN, toolbox::AngleDeg { 0.0 }, 2460745.5, 2460755.5);
  ASSERT_EQ(crossing.size(), 1);
  const double split = crossing.at(0).jde_tt;
  ASSERT_TRUE(longitude_events(Target::SUN, toolbox::AngleDeg { 0.0 }, 2460745.5, split).empty());
  const auto right = longitude_events(Target::SUN, toolbox::AngleDeg { 360.0 }, split, 2460755.5);
  ASSERT_EQ(right.size(), 1);
  ASSERT_EQ(right.at(0).jde_tt, split);
  const auto stationary = stations(Target::MERCURY, 2460745.5, 2460755.5);
  ASSERT_EQ(stationary.size(), 1);
  const double station_split = stationary.at(0).jde_tt;
  ASSERT_TRUE(stations(Target::MERCURY, 2460745.5, station_split).empty());
  const auto station_right = stations(Target::MERCURY, station_split, 2460755.5);
  ASSERT_EQ(station_right.size(), 1);
  ASSERT_EQ(station_right.at(0).jde_tt, station_split);
}

TEST(EphemerisEvents, ConstantNodeRelationsAndMalformedQueriesFailBeforeSearching) {
  ASSERT_THROW(
    static_cast<void>(longitude_difference_events(
      Target::TRUE_ASCENDING, Target::TRUE_DESCENDING, toolbox::AngleDeg { -180.0 }, 2460676.5, 2460707.5
    )),
    std::invalid_argument
  );
  ASSERT_TRUE(longitude_difference_events(
    Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, toolbox::AngleDeg { 0.0 }, 2460676.5, 2460707.5
  ).empty());
  ASSERT_THROW(
    static_cast<void>(longitude_difference_events(
      Target::SUN, Target::SUN, toolbox::AngleDeg { 1.0 }, 2460676.5, 2460707.5
    )),
    std::invalid_argument
  );
  ASSERT_THROW(static_cast<void>(stations(Target::SUN, 2460676.5, 2460676.5)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(stations(Target::SUN, 2460676.5, 2464676.5)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(stations(static_cast<Target>(255), 2460676.5, 2460707.5)), std::invalid_argument);
  ASSERT_THROW(
    static_cast<void>(longitude_events(Target::SUN, toolbox::AngleDeg { std::numeric_limits<double>::infinity() }, 2460676.5, 2460707.5)),
    std::invalid_argument
  );
  const auto [pluto_start, pluto_end] = longitude_rate_domain(Target::PLUTO);
  ASSERT_THROW(static_cast<void>(stations(Target::PLUTO, pluto_start - 1.0, pluto_start + 1.0)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(stations(Target::PLUTO, pluto_end - 1.0, pluto_end + 1.0)), std::invalid_argument);
}

TEST(EphemerisEvents, EveryTargetDispatchesThroughThePublicLongitudeQuery) {
  constexpr double KNOWN_JDE_TT = 2460700.12345678;
  for (const auto target : EVENT_TARGETS) {
    const auto level = detail::event_longitude(target, KNOWN_JDE_TT);
    const auto found = longitude_events(target, level, KNOWN_JDE_TT - 0.2, KNOWN_JDE_TT + 0.2);
    ASSERT_EQ(found.size(), 1) << static_cast<uint32_t>(target);
    ASSERT_NEAR(found.at(0).jde_tt, KNOWN_JDE_TT, 0.001) << static_cast<uint32_t>(target);
    ASSERT_EQ(found.at(0).longitude.deg(), detail::event_longitude(target, found.at(0).jde_tt).deg());
  }
}

TEST(EphemerisEvents, TrueNodeStationsHiddenInsideSixHoursAreBothReturned) {
  // Model-self-consistency regression: both endpoint rates are negative, with two interior reversals.
  const auto found = stations(Target::TRUE_ASCENDING, 2456695.0, 2456695.25);
  ASSERT_EQ(found.size(), 2);
  ASSERT_NEAR(found.at(0).jde_tt, 2456695.1997015439, 0.001);
  ASSERT_NEAR(found.at(1).jde_tt, 2456695.2487048060, 0.001);
  ASSERT_LT(found.at(0).jde_tt, found.at(1).jde_tt);
  ASSERT_EQ(found.at(0).motion_before, Motion::RETROGRADE);
  ASSERT_EQ(found.at(0).motion_after, Motion::DIRECT);
  ASSERT_EQ(found.at(1).motion_before, Motion::DIRECT);
  ASSERT_EQ(found.at(1).motion_after, Motion::RETROGRADE);
}

TEST(EphemerisEvents, OppositionRepresentationsAndPairHalfOpenOwnershipAgree) {
  const auto positive = longitude_difference_events(
    Target::MOON, Target::SUN, toolbox::AngleDeg { 180.0 }, 2460676.5, 2460707.5
  );
  const auto negative = longitude_difference_events(
    Target::MOON, Target::SUN, toolbox::AngleDeg { -180.0 }, 2460676.5, 2460707.5
  );
  ASSERT_EQ(positive.size(), 1);
  ASSERT_EQ(negative.size(), 1);
  const double split = positive.at(0).jde_tt;
  ASSERT_EQ(split, negative.at(0).jde_tt);
  ASSERT_TRUE(longitude_difference_events(
    Target::MOON, Target::SUN, toolbox::AngleDeg { 180.0 }, 2460676.5, split
  ).empty());
  const auto right = longitude_difference_events(Target::MOON, Target::SUN, toolbox::AngleDeg { -180.0 }, split, 2460707.5);
  ASSERT_EQ(right.size(), 1);
  ASSERT_EQ(right.at(0).jde_tt, split);
}

TEST(EphemerisEvents, ACanonicalRelativeStationConstructsARepeatablePairTangent) {
  const search::Interval query { .start_jde_tt = 2460735.5, .end_jde_tt = 2460755.5 };
  const auto grid = detail::event_grid(Target::MERCURY, Target::SUN, query);
  search::Budget budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  std::vector<search::Root> relative_stations;
  search::visit_query_cells(grid, query, [&](const search::Interval cell) {
    const auto first = detail::event_bounds(Target::MERCURY, cell);
    const auto second = detail::event_bounds(Target::SUN, cell);
    const auto rate = [&](const double jde_tt) {
      return detail::subtract_event_rates(detail::event_rate_observation(Target::MERCURY, jde_tt, first),
                                          detail::event_rate_observation(Target::SUN, jde_tt, second));
    };
    const auto bounds = detail::difference_bounds(first, second);
    const auto candidates = search::isolate_cell(rate, cell.start_jde_tt, cell.end_jde_tt, bounds.rate, budget);
    search::append_interval_roots(
      rate, candidates, query, bounds.rate, budget, detail::EVENT_TIME_RESOLUTION_TT_DAYS,
      relative_stations, detail::EVENT_RATE_CENTER_QUANTUM_TT_DAYS
    );
  });
  ASSERT_EQ(relative_stations.size(), 1);
  const double stationary_jde_tt = relative_stations.at(0).jde_tt;
  const auto first = detail::event_longitude(Target::MERCURY, stationary_jde_tt);
  const auto second = detail::event_longitude(Target::SUN, stationary_jde_tt);
  const auto level = search::project_longitude_difference(
    { .longitude = first, .error_deg = 0.0 }, { .longitude = second, .error_deg = 0.0 }
  );
  const auto found = longitude_difference_events(
    Target::MERCURY, Target::SUN, toolbox::AngleDeg { level.value }, stationary_jde_tt - 0.5, stationary_jde_tt + 0.5
  );
  ASSERT_EQ(found.size(), 1);
  ASSERT_EQ(found.at(0).jde_tt, stationary_jde_tt);
  ASSERT_EQ(found.at(0).direction, EventDirection::TANGENT);
  const double nearby_level = level.value + (relative_stations.at(0).increasing ? 1e-5 : -1e-5);
  const auto two_crossings = longitude_difference_events(
    Target::MERCURY, Target::SUN, toolbox::AngleDeg { nearby_level }, stationary_jde_tt - 0.1, stationary_jde_tt + 0.1
  );
  ASSERT_EQ(two_crossings.size(), 2);
  ASSERT_LT(two_crossings.at(0).jde_tt, stationary_jde_tt);
  ASSERT_GT(two_crossings.at(1).jde_tt, stationary_jde_tt);
  ASSERT_LT(two_crossings.at(1).jde_tt - two_crossings.at(0).jde_tt, detail::EVENT_CELL_WIDTH_TT_DAYS);
  ASSERT_NE(two_crossings.at(0).direction, two_crossings.at(1).direction);
  for (const double nearby : std::array { std::nextafter(level.value, 0.0), std::nextafter(level.value, 180.0) }) {
    ASSERT_THROW(
      static_cast<void>(longitude_difference_events(
        Target::MERCURY, Target::SUN, toolbox::AngleDeg { nearby }, stationary_jde_tt - 0.5, stationary_jde_tt + 0.5
      )),
      std::runtime_error
    );
  }
}

TEST(EphemerisEvents, DirectedRoundingIsRejectedBeforeProviderWork) {
  const int32_t original = std::fegetround();
  if (std::fesetround(FE_DOWNWARD) != 0) {
    GTEST_SKIP() << "The platform cannot select downward rounding";
  }
  EXPECT_THROW(static_cast<void>(stations(Target::SUN, 2460676.5, 2460707.5)), std::runtime_error);
  EXPECT_THROW(
    static_cast<void>(longitude_events(Target::SUN, toolbox::AngleDeg { 0.0 }, 2460676.5, 2460707.5)),
    std::runtime_error
  );
  EXPECT_THROW(
    static_cast<void>(longitude_difference_events(
      Target::SUN, Target::SUN, toolbox::AngleDeg { 0.0 }, 2460676.5, 2460707.5
    )),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(longitude_difference_events(
      Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, toolbox::AngleDeg { 180.0 }, 2460676.5, 2460707.5
    )),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(longitude_difference_events(
      Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, toolbox::AngleDeg { 0.0 }, 2460676.5, 2460707.5
    )),
    std::runtime_error
  );
  ASSERT_EQ(std::fesetround(original), 0);
}

TEST(EphemerisEvents, FixedAnalysisBandsCoverWholeCellsAndTheirSamplingHalo) {
  const double first = detail::EVENT_CENTURY_START_JDE_TT;
  const double last = detail::EVENT_CENTURY_END_JDE_TT;
  for (const auto target : EVENT_TARGETS) {
    const auto index = static_cast<std::size_t>(target);
    const auto tight = detail::EVENT_BOUNDS_CENTURY.at(index);
    const auto wide = detail::EVENT_BOUNDS_WIDE.at(index);
    for (const auto cell : std::array {
           search::Interval { .start_jde_tt = first + 0.625, .end_jde_tt = first + 0.875 },
           search::Interval { .start_jde_tt = last - 0.875, .end_jde_tt = last - 0.625 } }) {
      ASSERT_EQ(detail::event_bounds(target, cell).position_error_deg, tight.position_error_deg);
    }
    for (const auto cell : std::array {
           search::Interval { .start_jde_tt = first + 0.375, .end_jde_tt = first + 0.625 },
           search::Interval { .start_jde_tt = last - 0.625, .end_jde_tt = last - 0.375 } }) {
      ASSERT_EQ(detail::event_bounds(target, cell).position_error_deg, wide.position_error_deg);
    }
    const bool expected_station_free = target == Target::SUN or target == Target::MOON
                                      or target == Target::MEAN_ASCENDING or target == Target::MEAN_DESCENDING;
    ASSERT_EQ(detail::has_no_station(tight), expected_station_free);
    ASSERT_EQ(detail::has_no_station(wide), expected_station_free);
  }
}

TEST(EphemerisEvents, WideBandPhysicalQueriesAndAStraddlingQueryKeepTheirCanonicalRoots) {
  // Model-self-consistency checks of wide-band composition, not independent physical reference epochs.
  for (const double time : std::array { 2415000.12345678, 2488090.12345678 }) {
    for (const auto target : std::array { Target::SUN, Target::MOON, Target::MARS, Target::JUPITER }) {
      const auto level = detail::event_longitude(target, time);
      const auto found = longitude_events(target, level, time - 0.2, time + 0.2);
      ASSERT_EQ(found.size(), 1);
      ASSERT_NEAR(found.at(0).jde_tt, time, detail::EVENT_TIME_RESOLUTION_TT_DAYS);
    }
    const auto level = search::project_longitude_difference(
      { .longitude = detail::event_longitude(Target::MOON, time), .error_deg = 0.0 },
      { .longitude = detail::event_longitude(Target::SUN, time), .error_deg = 0.0 }
    );
    const auto found = longitude_difference_events(
      Target::MOON, Target::SUN, toolbox::AngleDeg { level.value }, time - 0.2, time + 0.2
    );
    ASSERT_EQ(found.size(), 1);
    ASSERT_NEAR(found.at(0).jde_tt, time, detail::EVENT_TIME_RESOLUTION_TT_DAYS);
  }
  const double time = detail::EVENT_CENTURY_START_JDE_TT + 0.7;
  const auto level = detail::event_longitude(Target::SUN, time);
  const auto short_query = longitude_events(Target::SUN, level, time - 0.1, time + 0.1);
  const auto straddling = longitude_events(Target::SUN, level, time - 2.0, time + 2.0);
  ASSERT_EQ(short_query.size(), 1);
  ASSERT_EQ(straddling.size(), 1);
  ASSERT_EQ(short_query.at(0).jde_tt, straddling.at(0).jde_tt);
}

TEST(EphemerisEvents, DirectedPairsRetainEveryPassAndSignedAngle) {
  // PyMeeus source/date/generation and the separate 3e-5-day tolerance: provenance/ephemeris-events/ATTRIBUTION.md.
  const auto retrograde = longitude_difference_events(
    Target::MERCURY, Target::URANUS, toolbox::AngleDeg { -55.0 }, 2460735.5, 2460785.5
  );
  const std::array expected { 2460737.000839077, 2460765.032595448, 2460782.5520821437 };
  ASSERT_EQ(retrograde.size(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    ASSERT_NEAR(retrograde.at(index).jde_tt, expected.at(index), 3e-5);
    ASSERT_EQ(retrograde.at(index).direction, index == 1 ? EventDirection::DECREASING : EventDirection::INCREASING);
    if (index > 0) {
      ASSERT_LT(retrograde.at(index - 1).jde_tt, retrograde.at(index).jde_tt);
    }
  }
  for (const double angle : std::array { 0.0, 90.0, 120.0 }) {
    const auto passes = longitude_difference_events(
      Target::MOON, Target::SUN, toolbox::AngleDeg { angle }, 2460676.5, 2460796.5
    );
    ASSERT_EQ(passes.size(), 4);
    for (std::size_t index = 1; index < passes.size(); ++index) {
      ASSERT_LT(passes.at(index - 1).jde_tt, passes.at(index).jde_tt);
      ASSERT_EQ(passes.at(index).direction, EventDirection::INCREASING);
    }
  }
}

TEST(EphemerisEvents, PublicWorkLimitsAndNonfiniteTimesRemainExplicit) {
  ASSERT_EQ(detail::EVENT_MAXIMUM_EVALUATIONS, std::size_t { 65536 });
  ASSERT_EQ(detail::EVENT_MAXIMUM_CANDIDATES, std::size_t { 4096 });
  ASSERT_EQ(detail::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS, 3660.0);
  constexpr double START = 2460676.5;
  ASSERT_TRUE(stations(Target::SUN, START, START + detail::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS).empty());
  search::Budget budget { .evaluations_remaining = detail::EVENT_MAXIMUM_EVALUATIONS,
                          .candidates_remaining = detail::EVENT_MAXIMUM_CANDIDATES };
  std::size_t calls = 0;
  const auto provider = [&](const double) -> search::Value { ++calls; return { .value = 1.0, .error = 0.0 }; };
  for (std::size_t index = 0; index < detail::EVENT_MAXIMUM_EVALUATIONS; ++index) {
    static_cast<void>(search::sample(provider, START, budget));
  }
  ASSERT_THROW(static_cast<void>(search::sample(provider, START, budget)), std::runtime_error);
  ASSERT_EQ(calls, detail::EVENT_MAXIMUM_EVALUATIONS);
  ASSERT_EQ(budget.evaluations_remaining, 0);
  for (const double invalid : std::array {
         std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
         -std::numeric_limits<double>::infinity() }) {
    for (const auto interval : std::array {
           search::Interval { .start_jde_tt = invalid, .end_jde_tt = START + 1.0 },
           search::Interval { .start_jde_tt = START, .end_jde_tt = invalid } }) {
      ASSERT_THROW(static_cast<void>(stations(Target::SUN, interval.start_jde_tt, interval.end_jde_tt)), std::invalid_argument);
      ASSERT_THROW(
        static_cast<void>(longitude_events(Target::SUN, toolbox::AngleDeg { 0.0 }, interval.start_jde_tt, interval.end_jde_tt)),
        std::invalid_argument
      );
      ASSERT_THROW(
        static_cast<void>(longitude_difference_events(
          Target::MOON, Target::SUN, toolbox::AngleDeg { 0.0 }, interval.start_jde_tt, interval.end_jde_tt
        )),
        std::invalid_argument
      );
    }
  }
}

TEST(EphemerisEventsKernel, ACellBoundaryContactUsesItsFixedTwoSidedWindow) {
  const auto longitude = [](const double time) -> search::LongitudeValue {
    const double x = time - 0.25;
    return { .longitude = toolbox::AngleDeg { x * x }, .error_deg = 1e-15 };
  };
  const auto rate = [](const double time) -> search::Value {
    return { .value = 2.0 * (time - 0.25), .error = 0.0 };
  };
  const search::TrajectoryBounds bounds {
    .longitude = { .first_derivative = 1.5, .second_derivative = 2.0 },
    .rate = { .first_derivative = 2.0, .second_derivative = 0.0 },
    .third_derivative = 0.0,
    .rate_remainder = 0.0,
  };
  search::Budget budget { .evaluations_remaining = 10000, .candidates_remaining = 1000 };
  const search::Grid grid {
    .domain = { .start_jde_tt = 0.0, .end_jde_tt = 1.0 }, .cell_width_tt_days = 0.25,
  };
  const auto found = range_test::longitude_contacts_in_interval(
    longitude, rate, toolbox::AngleDeg { 0.0 }, grid, grid.domain, bounds, budget, 0x1p-10, 0x1p-30
  );
  ASSERT_EQ(found.size(), 1U);
  ASSERT_EQ(found.front().jde_tt, 0.25);
  ASSERT_EQ(found.front().direction, search::ContactDirection::TANGENT);
}

} // namespace astro::ephemeris::test
