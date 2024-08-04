/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
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

#include "earth.hpp"
#include "julian_day.hpp"
#include "toolbox.hpp"
#include "elp2000_82b.hpp"


namespace astro::moon::geocentric_coord {

using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;
using astro::toolbox::AngleUnit::RAD;
using astro::toolbox::DistanceUnit::KM;
using astro::toolbox::Distance;
using astro::toolbox::SphericalCoordinate;

using astro::elp2000_82b::evaluate;
using astro::elp2000_82b::LON_LAT_SCALING_FACTOR;
using astro::elp2000_82b::RADIUS_SCALING_FACTOR;


/**
 * @brief Calculate the apparent geocentric position of the Moon, using truncated ELP2000-82B.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The geocentric ecliptic position of the Moon, calculated using truncated ELP2000-82B.
 */
inline auto apparent(const double jde) -> SphericalCoordinate {
  const double jc = astro::julian_day::jde_to_jc(jde);

  const auto evaluated = evaluate(jc);

  // Longitude
  const auto Σl = evaluated.Σl + evaluated.perturbation_l;
  const auto lon_nutation = astro::earth::nutation::longitude(jde);
  const Angle<DEG> lon = evaluated.Lp + (Σl / LON_LAT_SCALING_FACTOR) + lon_nutation; 

  // Latitude
  const auto Σb = evaluated.Σb + evaluated.perturbation_b;
  const Angle<DEG> lat { Σb / LON_LAT_SCALING_FACTOR };

  // Distance
  const Distance<KM> r { 385000.56 + evaluated.Σr / RADIUS_SCALING_FACTOR };

  return {
    .λ = lon,
    .β = lat,
    .r = r
  };
}


/**
 * @brief Calculate the equatorial horizontal parallax of the Moon.
 * @param coord The geocentric ecliptic position of the Moon.
 * @return The equatorial horizontal parallax of the Moon.
 */
inline auto equatorial_horizontal_parallax(const SphericalCoordinate& coord) -> Angle<RAD> {
  const auto& r = coord.r;
  const auto ppi_rad = std::asin(6378.14 / r.km());
  return { ppi_rad };
}

} // namespace astro::moon::geocentric_coord
