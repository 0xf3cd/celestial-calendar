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

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "vsop87d/vsop87d.hpp"

namespace astro::earth::heliocentric_coord {

using std::cos;
using std::sin;
using std::tan;

using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;
using astro::toolbox::AngleUnit::RAD;
using astro::toolbox::SphericalCoordinate;

using astro::vsop87d::Planet;

/**
 * @brief Calculate the heliocentric position of the Earth, using VSOP87D.
 * @param jd The Julian Day.
 * @return The heliocentric ecliptic position of the Earth, calculated using VSOP87D.
 */
SphericalCoordinate vsop87d(const double jd) {
  const double jm = astro::julian_day::jd_to_jm(jd);
  const auto evaluated = astro::vsop87d::evaluate<Planet::EAR>(jm);

  return {
    // As per the algorithm, the longitude is normalized to [0, 360).
    .lon = Angle<RAD>(evaluated.lon).normalize(),
    .lat = Angle<RAD>(evaluated.lat),
    .r   = evaluated.r
  };
}

/**
 * @brief Apply the correction to VSOP87D position, to convert it to the FK5 system.
 * @param jd The Julian Day.
 * @return The adjusted VSOP87D-calculated position in the FK5 system.
 * @details As per Jean Meeus's Astronomical Algorithms, this correction is applied for accuracy.
 */
SphericalCoordinate to_fk5(const double jd, const SphericalCoordinate& vsop87d_result) {
  const double jc = astro::julian_day::jd_to_jc(jd);
  const auto& [vsop_λ, vsop_β, vsop_r] = vsop87d_result;

  // Calculate the deltas for longitude and latitude, in arcsec.
  const Angle λ_dash = vsop_λ - ((1.397 + 0.00031 * jc) * jc);
  const double λ_dash_rad = λ_dash.as<RAD>();

  const double delta_λ_arcsec = -0.09033 + 0.03916 * (cos(λ_dash_rad) + sin(λ_dash_rad)) * tan(vsop_β.as<RAD>());
  const double delta_β_arcsec = 0.03916 * (cos(λ_dash_rad) - sin(λ_dash_rad));

  return {
    .lon = vsop_λ + Angle<DEG>::from_arcsec(delta_λ_arcsec),
    .lat = vsop_β + Angle<DEG>::from_arcsec(delta_β_arcsec),
    .r   = vsop_r,
  };
}

} // namespace astro::earth::heliocentric_coord
