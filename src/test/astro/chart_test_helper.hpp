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

#pragma once

#include <array>
#include <string_view>

#include "ephemeris.hpp"
#include "geolocation.hpp"
#include "house.hpp"
#include "moon.hpp"
#include "planet.hpp"
#include "sun.hpp"
#include "toolbox.hpp"

#include "delta_t.hpp"

namespace astro::chart::test {

/** @brief Build a test location from north-positive latitude and east-positive longitude. */
[[nodiscard]] inline auto location(const double latitude_deg, const double longitude_deg) -> astro::GeoLocation {
  return {
    .latitude = astro::toolbox::AngleDeg { latitude_deg },
    .longitude = astro::toolbox::AngleDeg { longitude_deg },
  };
}

// The seven composition roles exercised by both chart_test.cpp (core semantics) and
// chart_cabi_test.cpp (C ABI parity): one table, so the two layers never drift apart.
struct CompositionRole {
  std::string_view role;
  astro::delta_t::Model model;
  astro::house::System system;
  astro::GeoLocation observer;
};

inline const std::array COMPOSITION_ROLES {
  CompositionRole { .role = "algo1-equal",       .model = astro::delta_t::Model::ALGO1, .system = astro::house::System::EQUAL,      .observer = location(51.5, 0.0) },
  CompositionRole { .role = "algo2-whole",       .model = astro::delta_t::Model::ALGO2, .system = astro::house::System::WHOLE_SIGN, .observer = location(51.5, 0.0) },
  CompositionRole { .role = "algo3-south",       .model = astro::delta_t::Model::ALGO3, .system = astro::house::System::PLACIDUS,   .observer = location(-33.87, 151.21) },
  CompositionRole { .role = "algo4-placidus",    .model = astro::delta_t::Model::ALGO4, .system = astro::house::System::PLACIDUS,   .observer = location(51.5, 0.0) },
  CompositionRole { .role = "algo5-equal",       .model = astro::delta_t::Model::ALGO5, .system = astro::house::System::EQUAL,      .observer = location(-33.87, 151.21) },
  CompositionRole { .role = "algo5-whole-south", .model = astro::delta_t::Model::ALGO5, .system = astro::house::System::WHOLE_SIGN, .observer = location(-33.87, 151.21) },
  CompositionRole { .role = "algo5-placidus",    .model = astro::delta_t::Model::ALGO5, .system = astro::house::System::PLACIDUS,   .observer = location(51.5, 0.0) },
};

/**
 * @brief Replay lower-level positions independently of @ref astro::ephemeris::target_facts.
 * @note Nodes have no spherical distance.
 */
[[nodiscard]] inline auto expected_coordinates(
  const astro::ephemeris::Target target,
  const double jde_tt
) -> astro::toolbox::SphericalCoordinate {
  using astro::planet::Planet;
  using enum astro::ephemeris::Target;
  switch (target) {
    case SUN: return astro::sun::geocentric_coord::apparent(jde_tt);
    case MOON: return astro::moon::geocentric_coord::apparent(jde_tt);
    case MERCURY: return astro::planet::geocentric_coord::apparent(Planet::MERCURY, jde_tt);
    case VENUS: return astro::planet::geocentric_coord::apparent(Planet::VENUS, jde_tt);
    case MARS: return astro::planet::geocentric_coord::apparent(Planet::MARS, jde_tt);
    case JUPITER: return astro::planet::geocentric_coord::apparent(Planet::JUPITER, jde_tt);
    case SATURN: return astro::planet::geocentric_coord::apparent(Planet::SATURN, jde_tt);
    case URANUS: return astro::planet::geocentric_coord::apparent(Planet::URANUS, jde_tt);
    case NEPTUNE: return astro::planet::geocentric_coord::apparent(Planet::NEPTUNE, jde_tt);
    case PLUTO: return astro::planet::geocentric_coord::apparent(Planet::PLUTO, jde_tt);
    default: throw std::logic_error { "Nodes have no test spherical coordinate" };
  }
}

} // namespace astro::chart::test
