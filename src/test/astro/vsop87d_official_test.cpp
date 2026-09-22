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
#include <stdexcept>
#include "julian_day.hpp"
#include "toolbox.hpp"
#include "vsop87d/jupiter_coeff.hpp"
#include "vsop87d/mars_coeff.hpp"
#include "vsop87d/mercury_coeff.hpp"
#include "vsop87d/neptune_coeff.hpp"
#include "vsop87d/saturn_coeff.hpp"
#include "vsop87d/uranus_coeff.hpp"
#include "vsop87d/venus_coeff.hpp"
#include "vsop87d/vsop87d.hpp"
#include "vsop87d_check_data.hpp"

namespace astro::vsop87d::test {

namespace {

[[nodiscard]] auto evaluate_planet(const Planet planet, const double jm) -> Evaluation {
  switch (planet) {
    case Planet::EAR:
      return evaluate<Planet::EAR>(jm);
    case Planet::MER:
      return evaluate<Planet::MER>(jm);
    case Planet::VEN:
      return evaluate<Planet::VEN>(jm);
    case Planet::MAR:
      return evaluate<Planet::MAR>(jm);
    case Planet::JUP:
      return evaluate<Planet::JUP>(jm);
    case Planet::SAT:
      return evaluate<Planet::SAT>(jm);
    case Planet::URA:
      return evaluate<Planet::URA>(jm);
    case Planet::NEP:
      return evaluate<Planet::NEP>(jm);
  }
  throw std::logic_error { "Unknown VSOP87D planet" };
}

} // namespace

TEST(Vsop87dOfficial, Positions) {
  // One printed unit covers the source's ten-decimal rounding and evaluation-order drift.
  constexpr double PRINTED_VALUE_TOLERANCE = 1e-10;

  for (const auto& row : test_data::OFFICIAL_CHECK_ROWS) {
    SCOPED_TRACE(::testing::Message {} << "planet=" << static_cast<int>(row.planet) << " JD=" << row.jd);
    const auto result = evaluate_planet(row.planet, julian_day::jde_to_jm(row.jd));

    EXPECT_NEAR(toolbox::normalize_rad(result.λ), row.λ, PRINTED_VALUE_TOLERANCE);
    EXPECT_NEAR(result.β, row.β, PRINTED_VALUE_TOLERANCE);
    EXPECT_NEAR(result.r, row.r, PRINTED_VALUE_TOLERANCE);
  }
}

} // namespace astro::vsop87d::test
