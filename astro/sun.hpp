/*
 * CelestialCalendar: 
 *   A C++23-style library for date conversions and astronomical calculations for various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
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

#include <format>

#include "toolbox.hpp"
#include "julian_day.hpp"

#include "vsop87d/vsop87d.hpp"
#include "earth.hpp"

namespace astro::sun {

using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;
using astro::toolbox::SphericalCoordinate;


/**
 * @brief Calculate the geocentric position of the Sun, using VSOP87D.
 * @param jd The Julian Day.
 * @return The geocentric position of the Sun.
 * @details The function invokes `astro::earth::vsop87d_heliocentric_position`, and
 *          transforms the heliocentric coordinates to geocentric coordinates.
 */
SphericalCoordinate vsop87d_geocentric_position(const double jd) {
  const auto& [λ_helio, β_helio, r_helio] = astro::earth::heliocentric_coord::vsop87d(jd);
  return {
    // Convert the heliocentric ecliptic longitude of Earth to geocentric ecliptic longitude of Sun.
    // The formula is: λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180∘
    .lon = std::invoke([&] {
      using namespace astro::toolbox::literals;
      const auto&& sum = λ_helio + 180.0_deg;
      return sum.normalize();
    }),

    // Convert the heliocentric ecliptic latitude of Earth to geocentric ecliptic latitude of Sun.
    // The formula is: β_sun_geocentric_deg = -β_earth_heliocentric_deg
    .lat = -β_helio,

    // The distance (radius) is the same for both Sun and Earth.
    .r = r_helio 
  };
}

} // namespace astro::sun
