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
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "ephemeris_events.hpp"

namespace astro::ephemeris::test {

namespace {

namespace search = detail::event_search;

constexpr search::Bounds LINEAR_BOUNDS { .first_derivative = 1, .second_derivative = 0 };
constexpr search::Bounds QUADRATIC_BOUNDS { .first_derivative = 1, .second_derivative = 2 };

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

template <typename Provider>
auto partitioned_roots(const Provider& provider, const double query_start, const double query_end)
  -> std::vector<double> {
  std::vector<double> result;
  for (const int cell : std::array { 0, 1 }) {
    const auto found = roots(
      provider,
      static_cast<double>(cell),
      static_cast<double>(cell + 1),
      LINEAR_BOUNDS
    );
    for (const auto root : found) {
      if (query_start <= root.jde_tt and root.jde_tt < query_end) {
        result.push_back(root.jde_tt);
      }
    }
  }
  return result;
}

} // namespace

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
  const auto full = partitioned_roots(linear, 0, 2);
  ASSERT_EQ(full, (std::vector<double> { 1 }));
  const auto negative = [](const double time) -> search::Value { return sum_value(time, 0.5); };
  const auto before_zero = roots(negative, -1, 0, LINEAR_BOUNDS);
  ASSERT_EQ(before_zero.size(), 1);
  ASSERT_EQ(before_zero.at(0).jde_tt, -0.5);
}

TEST(EphemerisEventsKernel, RoundedInteriorRootsKeepTheirReportedTimeOwnership) {
  for (const double offset : std::array { 0x1p-55, -0x1p-55 }) {
    const auto linear = [offset](const double time) -> search::Value { return sum_value(time - 1, offset); };
    const auto full = partitioned_roots(linear, 0, 2);
    ASSERT_EQ(full, (std::vector<double> { 1 }));
    ASSERT_TRUE(partitioned_roots(linear, 0, 1).empty());
    ASSERT_EQ(partitioned_roots(linear, 1, 2), full);
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
  std::vector<double> narrow;
  for (const auto root : found) {
    if (0 <= root.jde_tt and root.jde_tt < 0.01) {
      narrow.push_back(root.jde_tt);
    }
  }
  ASSERT_EQ(narrow.size(), 2);
  ASSERT_NEAR(narrow.at(0), 0.001, 1e-8);
  ASSERT_NEAR(narrow.at(1), 0.002, 1e-8);
}

TEST(EphemerisEventsKernel, DoubleAndTripleRootsRemainExplicitlyUnresolved) {
  for (const int order : std::array { 2, 3 }) {
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

TEST(EphemerisEventsKernel, EvaluationCandidateAndRefinementBudgetsAreReal) {
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

} // namespace astro::ephemeris::test
