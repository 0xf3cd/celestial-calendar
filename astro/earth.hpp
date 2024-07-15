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

} // namespace astro::earth::heliocentric_coord