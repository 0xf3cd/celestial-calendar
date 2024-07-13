/**
 * ChineseCalendar: A C++ library that deals with conversions between calendar systems.
 * Copyright (C) 2024 Ningqi Wang (0xf3cd) <https://github.com/0xf3cd>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cmath>
#include <format>
#include <numbers>

#include "julian_day.hpp"
#include "earth_vsop87d.hpp"


namespace astro::helper {

/** @brief Normalize degree to [0, 360). */
constexpr double normalize_deg(const double deg) {
  const double __deg = std::remainder(deg, 360.0);
  return __deg < 0.0 ? __deg + 360.0 : __deg; 
}

/** @brief Convert degree to radian. */
constexpr double deg_to_rad(const double deg) {
  return deg * std::numbers::pi / 180.0;
}

/** @brief Convert radian to degree. */
constexpr double rad_to_deg(const double rad) {
  return rad * 180.0 / std::numbers::pi;
}

} // namespace astro::helper


namespace astro::ecliptic::sun {

/**
 * @brief Calculate the geocentric ecliptic longitude of the Sun, using VSOP87D.
 * @param jd The Julian Day.
 * @return The geocentric ecliptic longitude of the Sun (in degree).
 * @ref https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）
 */
double vsop87d_longitude(const double jd) {
  // Use VSOP87D model to calculate the heliocentric ecliptic longitude of Earth.
  const double τ = astro::julian_day::jd_to_jm(jd);

  const double L0 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::L0, τ);
  const double L1 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::L1, τ);
  const double L2 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::L2, τ);
  const double L3 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::L3, τ);
  const double L4 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::L4, τ);
  const double L5 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::L5, τ);

  // Get the heliocentric ecliptic longitude of Earth (in radians).
  const double λ_earth_heliocentric_rad = (L0 + L1 * τ + L2 * std::pow(τ, 2) + L3 * std::pow(τ, 3) 
                                              + L4 * std::pow(τ, 4) + L5 * std::pow(τ, 5)) / 10e8;

  // Convert the radians to degrees.
  using namespace astro::helper;
  const double λ_earth_heliocentric_deg = normalize_deg(rad_to_deg(λ_earth_heliocentric_rad));

  // Convert the heliocentric ecliptic longitude of Earth to geocentric ecliptic longitude of Sun.
  // The formula is: λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180∘
  const double λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180.0;
  return normalize_deg(λ_sun_geocentric_deg);
}

/**
 * @brief Calculate the geocentric ecliptic latitude of the Sun, using VSOP87D.
 * @param jd The Julian Day.
 * @return The geocentric ecliptic latitude of the Sun (in degree).
 * @ref https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）
 */
double vsop87d_latitude(const double jd) {
  // Use VSOP87D model to calculate the heliocentric ecliptic latitude of Earth.
  const double τ = astro::julian_day::jd_to_jm(jd);

  const double B0 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::B0, τ);
  const double B1 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::B1, τ);
  const double B2 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::B2, τ);
  const double B3 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::B3, τ);
  const double B4 = astro::vsop87d::earth::evaluate(astro::vsop87d::earth::B4, τ);

  // Get the heliocentric ecliptic latitude of Earth (in radians).
  const double β_earth_heliocentric_rad = (B0 + B1 * τ + B2 * std::pow(τ, 2) + B3 * std::pow(τ, 3) 
                                              + B4 * std::pow(τ, 4)) / 10e8;

  // Convert the radians to degrees.
  using namespace astro::helper;
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

} // namespace astro::ecliptic::sun
