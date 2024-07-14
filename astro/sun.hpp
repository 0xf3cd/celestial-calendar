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

#include "math.hpp"
#include "julian_day.hpp"
#include "vsop87d/vsop87d.hpp"

namespace astro::sun {

/**
 * @brief Calculate the geocentric ecliptic longitude of the Sun, using VSOP87D.
 * @param jd The Julian Day.
 * @return The geocentric ecliptic longitude of the Sun (in degree).
 * @ref https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）
 */
double vsop87d_longitude(const double jd) {
  using namespace astro::vsop87d;
  using namespace astro::math;
  using astro::julian_day::jd_to_jm;

  // Use VSOP87D model to calculate the heliocentric ecliptic longitude of Earth.
  const double τ = jd_to_jm(jd);

  // Get the heliocentric ecliptic longitude of Earth (in radians).
  const double λ_earth_heliocentric_rad = evaluate(earth_coeff::L, τ);

  // Convert the radians to degrees.
  const double λ_earth_heliocentric_deg = ensure_positive_deg(rad_to_deg(λ_earth_heliocentric_rad));

  // Convert the heliocentric ecliptic longitude of Earth to geocentric ecliptic longitude of Sun.
  // The formula is: λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180∘
  const double λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180.0;
  return ensure_positive_deg(λ_sun_geocentric_deg);
}

/**
 * @brief Calculate the geocentric ecliptic latitude of the Sun, using VSOP87D.
 * @param jd The Julian Day.
 * @return The geocentric ecliptic latitude of the Sun (in degree).
 * @ref https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）
 */
double vsop87d_latitude(const double jd) {
  using namespace astro::vsop87d;
  using namespace astro::math;
  using astro::julian_day::jd_to_jm;

  // Use VSOP87D model to calculate the heliocentric ecliptic latitude of Earth.
  const double τ = jd_to_jm(jd);

  // Get the heliocentric ecliptic latitude of Earth (in radians).  
  const auto β_earth_heliocentric_rad = evaluate(earth_coeff::B, τ);

  // Convert the radians to degrees.
  const double β_earth_heliocentric_deg = rad_to_deg(β_earth_heliocentric_rad);
  
  // Convert the heliocentric ecliptic latitude of Earth to geocentric ecliptic latitude of Sun.
  // The formula is: β_sun_geocentric_deg = -β_earth_heliocentric_deg
  const double β_sun_geocentric_deg = -β_earth_heliocentric_deg;

  // Extra sanity check. The expected range is [-90, 90] degrees.
  if (β_sun_geocentric_deg < -90 or β_sun_geocentric_deg > 90) {
    throw std::runtime_error {
      std::format("β_sun_geocentric_deg = {} is unexpectedly out of range", β_sun_geocentric_deg)
    };
  }

  return β_sun_geocentric_deg;
}


// The returned ecliptic coordinates returned by VSOP87D are based on mean equinox and equater of J2000.
// TODO: Project the coordinates to other coordinate systems.


} // namespace astro::sun
