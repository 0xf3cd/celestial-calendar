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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "ephemeris_events.hpp"
#include "../provenance/ephemeris-events/libm_reference.hpp"

namespace astro::ephemeris::test {

namespace {

/** @brief Evaluate the deployed elementary function at the frozen reference's actual inputs. */
auto evaluate(const libm_reference::Row& row) -> double {
  // Volatile inputs exercise deployed elementary functions rather than constant folding the frozen rows.
  volatile const double runtime_x = row.inputs.at(0);
  volatile const double runtime_y = row.inputs.at(1);
  volatile const double runtime_z = row.inputs.at(2);
  const double x = runtime_x;
  const double y = runtime_y;
  const double z = runtime_z;
  using enum libm_reference::Function;
  switch (row.function) {
    case SIN: return std::sin(x);
    case COS: return std::cos(x);
    case TAN: return std::tan(x);
    case ATAN2: return std::atan2(x, y);
    case ASIN: return std::asin(x);
    case HYPOT2: return std::hypot(x, y);
    case HYPOT3: return std::hypot(x, y, z);
    case POW: return std::pow(x, y);
  }
  throw std::runtime_error { "Unknown elementary-function reference" };
}

/** @brief Include reference-to-binary64 rounding without treating the reference as an exact real oracle. */
auto allowance(const libm_reference::Row& row) -> double {
  using enum libm_reference::Function;
  const bool relative = row.function == TAN or row.function == HYPOT2 or row.function == HYPOT3 or row.function == POW;
  const double scale = relative ? std::fabs(row.expected) : 1.0;
  const double lower_spacing = row.expected - std::nextafter(row.expected, -std::numeric_limits<double>::infinity());
  const double upper_spacing = std::nextafter(row.expected, std::numeric_limits<double>::infinity()) - row.expected;
  const double oracle_rounding = std::max(lower_spacing, upper_spacing) / 2.0;
  return std::nextafter((0x1p-48 * scale) + oracle_rounding, std::numeric_limits<double>::infinity());
}

} // namespace

TEST(EphemerisEventMath, ElementaryFunctionPremisesHaveIndependentHighPrecisionWitnesses) {
  detail::validate_event_environment();
  std::size_t checked = 0;
  for (const auto& row : libm_reference::ROWS) {
    SCOPED_TRACE(checked);
    const double got = evaluate(row);
    ASSERT_TRUE(std::isfinite(got));
    ASSERT_LE(std::fabs(got - row.expected), allowance(row));
    ++checked;
  }
  ASSERT_EQ(checked, 516);
}

TEST(EphemerisEventMath, ARelativeZeroPremiseRequiresAnExactZero) {
  for (const auto& row : libm_reference::ROWS) {
    using enum libm_reference::Function;
    const bool relative = row.function == TAN or row.function == HYPOT2 or row.function == HYPOT3 or row.function == POW;
    if (relative and row.expected == 0.0) {
      ASSERT_EQ(evaluate(row), 0.0);
    }
  }
}

TEST(EphemerisEventMath, FrozenZeroReferencesPreserveTheIeeeSign) {
  for (const auto& row : libm_reference::ROWS) {
    if (row.expected == 0.0) {
      ASSERT_EQ(std::signbit(evaluate(row)), std::signbit(row.expected));
    }
  }
}

} // namespace astro::ephemeris::test
