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

#include <stdexcept>

#include "ephemeris.hpp"
#include "geolocation.hpp"
#include "moon.hpp"
#include "planet.hpp"
#include "sun.hpp"
#include "toolbox.hpp"

namespace astro::chart::test {

/** @brief Build a test location from north-positive latitude and east-positive longitude. */
[[nodiscard]] inline auto location(const double latitude_deg, const double longitude_deg) -> astro::GeoLocation {
  return {
    .latitude = astro::toolbox::AngleDeg { latitude_deg },
    .longitude = astro::toolbox::AngleDeg { longitude_deg },
  };
}

/** @brief Test-only target mapping to lower-level position APIs; nodes have no spherical distance. */
[[nodiscard]] inline auto expected_coordinates(const astro::ephemeris::Target target, const double jde_tt)
  -> astro::toolbox::SphericalCoordinate {
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
