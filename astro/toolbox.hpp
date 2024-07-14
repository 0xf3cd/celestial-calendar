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

#include "math.hpp"
#include "vsop87d/defines.hpp"

namespace astro::toolbox {

using astro::math::Angle;
using astro::math::AngleUnit::DEG;
using astro::math::AngleUnit::RAD;

/**
 * @brief Represents a position in a spherical coordinate system.
 */
struct SphericalPosition {
  const Angle<DEG> lon; // Longitude
  const Angle<DEG> lat; // Latitude
  const double r;       // Radious, In AU
};


/**
 * @brief Apply a small correction on VSOP87D's result.
 *        The correction will convert the position to FK5 system.
 * @param vspo The position calculated by VSOP87D.
 * @return The position in the FK5 system.
 * @ref https://github.com/architest/pymeeus/blob/master/pymeeus/Coordinates.py
 * @ref https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）
 */
SphericalPosition vspo87d_to_fk5(const astro::vsop87d::Evaluation& vspo) {
  // TODO: Implement this.
  (void)vspo;
  return { 0,0,0 };
}


} // namespace astro::toolbox
