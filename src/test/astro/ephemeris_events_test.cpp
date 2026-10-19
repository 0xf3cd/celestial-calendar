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
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "ephemeris_events.hpp"

namespace astro::ephemeris::test {

namespace {

namespace search = detail::event_search;

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
  return search::roots_in_interval(provider, TWO_CELL_GRID, query, bounds, budget);
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
  return search::longitude_roots_in_interval(provider, target, TWO_CELL_GRID, query, bounds, budget);
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
  const auto found = search::roots_in_interval(linear, shifted, query, LINEAR_BOUNDS, budget);
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
    static_cast<void>(search::roots_in_interval(linear, TWO_CELL_GRID, TWO_CELL_GRID.domain, LINEAR_BOUNDS, budget)),
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
      static_cast<void>(search::roots_in_interval(counted, invalid, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget)),
      std::invalid_argument
    );
  }
  const search::Interval outside_domain { .start_jde_tt = -1.0, .end_jde_tt = 1.0 };
  ASSERT_THROW(
    static_cast<void>(search::roots_in_interval(counted, TWO_CELL_GRID, outside_domain, LINEAR_BOUNDS, budget)),
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
  const auto found = search::roots_in_interval(endpoint, grid, grid.domain, LINEAR_BOUNDS, budget);
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
  ASSERT_EQ(search::canonical_longitude_difference(toolbox::AngleDeg { -180.0 }).deg(), 180.0);
  ASSERT_EQ(search::canonical_longitude_difference(toolbox::AngleDeg { 540.0 }).deg(), 180.0);
  ASSERT_FALSE(std::signbit(search::canonical_longitude_difference(toolbox::AngleDeg { -0.0 }).deg()));

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
  const auto found = search::longitude_roots_in_interval(
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
    static_cast<void>(search::longitude_roots_in_interval(
      counted, toolbox::AngleDeg { 0.0 }, TWO_CELL_GRID, outside, LINEAR_BOUNDS, budget
    )),
    std::invalid_argument
  );
  ASSERT_THROW(
    static_cast<void>(search::longitude_roots_in_interval(
      counted, toolbox::AngleDeg { 0.0 }, invalid, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget
    )),
    std::invalid_argument
  );
  ASSERT_THROW(
    static_cast<void>(search::longitude_roots_in_interval(
      counted, toolbox::AngleDeg { std::numeric_limits<double>::infinity() },
      TWO_CELL_GRID, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget
    )),
    std::invalid_argument
  );
  ASSERT_THROW(
    static_cast<void>(search::longitude_roots_in_interval(
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
      const auto scalar_found = search::roots_in_interval(
        scalar, TWO_CELL_GRID, query, LINEAR_BOUNDS, scalar_budget, 0.0, QUANTUM_TT_DAYS
      );
      const auto longitude_found = search::longitude_roots_in_interval(
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
  ASSERT_TRUE(search::roots_in_interval(
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
    static_cast<void>(search::roots_in_interval(
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
  const auto distinct = search::roots_in_interval(
    close_pair, TWO_CELL_GRID, TWO_CELL_GRID.domain, pair_bounds, distinct_budget, 1e-8
  );
  ASSERT_EQ(distinct.size(), 2);
  ASSERT_EQ(distinct.at(0).jde_tt, 1.0 - 0.015625);
  ASSERT_EQ(distinct.at(1).jde_tt, 1.0 + 0.015625);
  search::Budget pair_budget { .evaluations_remaining = 20000, .candidates_remaining = 1000 };
  ASSERT_THROW(
    static_cast<void>(search::roots_in_interval(
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
      static_cast<void>(search::roots_in_interval(
        counted, TWO_CELL_GRID, FIRST_CELL_INTERVAL, LINEAR_BOUNDS, budget, 0.0, quantum
      )),
      std::invalid_argument
    );
  }
  ASSERT_EQ(calls, 0);
  ASSERT_EQ(budget.evaluations_remaining, 100);
}

} // namespace astro::ephemeris::test
