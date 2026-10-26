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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "moon_phase.hpp"

namespace astro::moon_phase::test {

using detail::NumericPolicy;
using phase_moments::PhaseKind;

static_assert(std::is_constructible_v<new_moon::RootGenerator, double>);
static_assert(not std::is_constructible_v<new_moon::RootGenerator, double, double>);
static_assert(not std::is_constructible_v<new_moon::RootGenerator, double, PhaseKind>);
static_assert(not std::is_constructible_v<phase_moments::RootGenerator, double>);
static_assert(not std::is_constructible_v<phase_moments::RootGenerator, double, double>);
static_assert(std::is_constructible_v<phase_moments::RootGenerator, double, PhaseKind>);
static_assert(std::is_copy_assignable_v<new_moon::RootGenerator>);
static_assert(std::is_copy_assignable_v<phase_moments::RootGenerator>);
static_assert(std::is_move_assignable_v<new_moon::RootGenerator>);
static_assert(std::is_move_assignable_v<phase_moments::RootGenerator>);


TEST(MoonPhaseSolver, NumericalPolicyBoundaries) {
  // Existing conjunction and phase conventions (#372), independent of ephemeris/libm rounding.
  ASSERT_FALSE(detail::bracket_rejected<NumericPolicy::CONJUNCTION>(359.0, +0.0, 0.0));
  ASSERT_FALSE(detail::bracket_rejected<NumericPolicy::CONJUNCTION>(359.0, -0.0, 0.0));
  ASSERT_TRUE(detail::bracket_rejected<NumericPolicy::PHASE>(359.0, +0.0, 0.0));
  ASSERT_TRUE(detail::bracket_rejected<NumericPolicy::PHASE>(359.0, -0.0, 0.0));
  ASSERT_TRUE(detail::bracket_rejected<NumericPolicy::CONJUNCTION>(345.0, 1.0, 0.0));
  ASSERT_TRUE(detail::bracket_rejected<NumericPolicy::PHASE>(345.0, 1.0, 0.0));
  ASSERT_TRUE(detail::bracket_rejected<NumericPolicy::CONJUNCTION>(359.0, 15.0, 0.0));
  ASSERT_TRUE(detail::bracket_rejected<NumericPolicy::PHASE>(359.0, 15.0, 0.0));
  ASSERT_FALSE(detail::bracket_rejected<NumericPolicy::CONJUNCTION>(359.0, 1.0, 0.0));
  ASSERT_FALSE(detail::bracket_rejected<NumericPolicy::PHASE>(359.0, 1.0, 0.0));
  ASSERT_FALSE(detail::bracket_rejected<NumericPolicy::PHASE>(89.0, 91.0, 90.0));

  ASSERT_EQ(detail::miss_at<NumericPolicy::CONJUNCTION>(180.0, 0.0), 180.0);
  ASSERT_EQ(detail::miss_at<NumericPolicy::PHASE>(180.0, 0.0), -180.0);
  ASSERT_EQ(detail::objective_at<NumericPolicy::CONJUNCTION>(200.0, 0.0), 200.0);
  ASSERT_EQ(detail::objective_at<NumericPolicy::PHASE>(200.0, 0.0), -160.0);
  ASSERT_EQ(detail::objective_at<NumericPolicy::CONJUNCTION>(345.0, 0.0), 345.0);

  const double after_345 = std::nextafter(345.0, 360.0);
  ASSERT_EQ(detail::objective_at<NumericPolicy::CONJUNCTION>(after_345, 0.0), after_345 - 360.0);
  const double after_180 = std::nextafter(180.0, 360.0);
  ASSERT_EQ(detail::miss_at<NumericPolicy::CONJUNCTION>(after_180, 0.0), after_180 - 360.0);

  ASSERT_TRUE(std::signbit(detail::objective_at<NumericPolicy::CONJUNCTION>(-0.0, 0.0)));
  ASSERT_FALSE(std::signbit(detail::objective_at<NumericPolicy::PHASE>(-0.0, 0.0)));
}


TEST(MoonPhaseSolver, NonFiniteElongationIsRejectedBeforePolicyMaps) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  // NaN is a domain-exterior helper probe; longitude_diff never returns it.
  ASSERT_TRUE(std::isnan(detail::objective_at<NumericPolicy::CONJUNCTION>(nan, 0.0)));
  ASSERT_TRUE(std::isnan(detail::miss_at<NumericPolicy::CONJUNCTION>(nan, 0.0)));
  ASSERT_THROW(static_cast<void>(detail::objective_at<NumericPolicy::PHASE>(nan, 0.0)), std::invalid_argument);
  ASSERT_THROW(static_cast<void>(detail::miss_at<NumericPolicy::PHASE>(nan, 0.0)), std::invalid_argument);

  for (const double jde : { nan, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() }) {
    ASSERT_THROW(static_cast<void>(longitude_diff(jde)), std::invalid_argument);
  }
}


TEST(MoonPhaseSolver, ExplicitIterationCapsReachNewton) {
  const double root = new_moon::moments(2024).front();
  const double left = root - 0.4;
  const double right = root + 0.6;
  const auto conjunction_objective = [](const double jde) -> double {
    const double diff = longitude_diff(jde);
    return diff > 345.0 ? diff - 360.0 : diff;
  };
  const auto phase_objective = [](const double jde) -> double {
    return toolbox::normalize_pm180(longitude_diff(jde));
  };

  for (const std::size_t cap : std::array<std::size_t, 4> { 0, 1, 3, toolbox::NEWTON_MAX_ITERATIONS }) {
    const double conjunction = detail::newton_method<NumericPolicy::CONJUNCTION>(left, right, 0.0, cap);
    const double phase = detail::newton_method<NumericPolicy::PHASE>(left, right, 0.0, cap);
    const double expected_conjunction = toolbox::newton_method(
      conjunction_objective, left, right, toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY, cap
    );
    const double expected_phase = toolbox::newton_method(
      phase_objective, left, right, toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY, cap
    );
    ASSERT_EQ(std::bit_cast<uint64_t>(conjunction), std::bit_cast<uint64_t>(expected_conjunction));
    ASSERT_EQ(std::bit_cast<uint64_t>(phase), std::bit_cast<uint64_t>(expected_phase));
  }

  ASSERT_EQ(detail::newton_method<NumericPolicy::CONJUNCTION>(left, right, 0.0, 0), std::midpoint(left, right));
  ASSERT_NE(detail::newton_method<NumericPolicy::CONJUNCTION>(left, right, 0.0, 1), std::midpoint(left, right));
  ASSERT_EQ(
    detail::newton_method<NumericPolicy::PHASE>(left, right, 0.0),
    detail::newton_method<NumericPolicy::PHASE>(left, right, 0.0, toolbox::NEWTON_MAX_ITERATIONS)
  );
}


TEST(MoonPhaseSolver, PeekDoesNotAdvanceEitherGenerator) {
  const double start_jde = astro::julian_day::J2000;
  const auto check = [](auto gen) {
    const double first = gen.peek();
    ASSERT_EQ(std::bit_cast<uint64_t>(gen.peek()), std::bit_cast<uint64_t>(first));
    ASSERT_EQ(std::bit_cast<uint64_t>(gen.next()), std::bit_cast<uint64_t>(first));
    ASSERT_GT(gen.peek(), first);

    auto copy = gen;
    ASSERT_EQ(std::bit_cast<uint64_t>(gen.next()), std::bit_cast<uint64_t>(copy.next()));
    copy = gen;
    ASSERT_EQ(std::bit_cast<uint64_t>(gen.next()), std::bit_cast<uint64_t>(copy.next()));
  };
  check(new_moon::RootGenerator { start_jde });
  for (const auto kind : { PhaseKind::NEW_MOON, PhaseKind::FIRST_QUARTER, PhaseKind::FULL_MOON, PhaseKind::LAST_QUARTER }) {
    check(phase_moments::RootGenerator { start_jde, kind });
  }
}


TEST(MoonPhaseSolver, PolicyErrorsRetainRawAndFoldedDiagnostics) {
  const auto message = [](const auto& reject) -> std::string {
    try {
      reject();
    } catch (const std::invalid_argument& error) {
      return error.what();
    }
    throw std::logic_error { "Expected the policy diagnostic to throw" };
  };

  ASSERT_EQ(
    message([] { detail::reject_bracket<NumericPolicy::CONJUNCTION>(1.0, 340.0, 2.0, 20.0, 0.0); }),
    "No root between jde 1 (elongation 340 deg) and jde 2 (elongation 20 deg)."
  );
  ASSERT_EQ(
    message([] { detail::reject_bracket<NumericPolicy::PHASE>(1.0, 340.0, 2.0, 20.0, 0.0); }),
    "No root between jde 1 (elongation -20 deg) and jde 2 (elongation 20 deg) for target 0 deg."
  );
  ASSERT_EQ(
    message([] { detail::reject_estimate<NumericPolicy::CONJUNCTION>(1.0, 2.0, 180.0, 180.0, 0.0); }),
    "Cannot find the first root after jde 1: the estimate at jde 2 misses conjunction by 180 deg (elongation 180 deg)."
  );
  ASSERT_EQ(
    message([] { detail::reject_estimate<NumericPolicy::PHASE>(1.0, 2.0, -180.0, 180.0, 0.0); }),
    "Cannot find the first root after jde 1 for target 0 deg: the estimate at jde 2 "
    "misses the target by -180 deg (elongation 180 deg)."
  );
  ASSERT_EQ(message([] { detail::reject_seed<NumericPolicy::CONJUNCTION>(1.0, 0.0); }), "The jde 1 is not a root.");
  ASSERT_EQ(message([] { detail::reject_seed<NumericPolicy::PHASE>(1.0, 0.0); }), "The jde 1 is not a root for target 0 deg.");
}

} // namespace astro::moon_phase::test
