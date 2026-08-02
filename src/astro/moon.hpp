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

#include <cmath>

#include "earth.hpp"
#include "julian_day.hpp"
#include "toolbox.hpp"
#include "elp2000_82b.hpp"


namespace astro::moon::perturbation {


/**
 * @brief Calculate perturbation of the Moon's geocentric longitude.
 * @details As per Astronomical Algorithms, Jean Meeus, 1998, Chapter 47, 
 *          the Moon is perturbed by Venus, Jupiter, and Earth.
 * @param ctx The context.
 * @return The perturbation of the Moon's geocentric longitude. Unit is 0.000001 degrees.
 * @see Astronomical Algorithms, Jean Meeus, 1998, Chapter 47.
 */
inline auto longitude(const elp2000_82b::Context& ctx) -> double {
  return 3958.0 * std::sin(ctx.A1.rad()) 
       + 1962.0 * std::sin(ctx.Lp.rad() - ctx.F.rad()) 
       + 318.0 * std::sin(ctx.A2.rad());
}


/**
 * @brief Calculate perturbation of the Moon's geocentric latitude.
 * @details As per Astronomical Algorithms, Jean Meeus, 1998, Chapter 47, 
 *          the Moon is perturbed by Venus, Jupiter, and Earth.
 * @param ctx The context.
 * @return The perturbation of the Moon's geocentric latitude. Unit is 0.000001 degrees.
 * @see Astronomical Algorithms, Jean Meeus, 1998, Chapter 47.
 */
inline auto latitude(const elp2000_82b::Context& ctx) -> double {
  return -2235.0 * std::sin(ctx.Lp.rad())
       + 382.0 * std::sin(ctx.A3.rad())
       + 175.0 * std::sin(ctx.A1.rad() - ctx.F.rad())
       + 175.0 * std::sin(ctx.A1.rad() + ctx.F.rad())
       + 127.0 * std::sin(ctx.Lp.rad() - ctx.Mp.rad())
       - 115.0 * std::sin(ctx.Lp.rad() + ctx.Mp.rad());
}

} // namespace astro::moon::perturbation


namespace astro::moon::geocentric_coord {




/**
 * @brief Calculate the apparent geocentric position of the Moon, using truncated ELP2000-82B.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The geocentric ecliptic position of the Moon, calculated using truncated ELP2000-82B.
 */
inline auto apparent(const double jde) -> toolbox::SphericalCoordinate {
  const double jc = astro::julian_day::jde_to_jc(jde);

  const auto evaluated = elp2000_82b::evaluate(jc);

  // Longitude, considering the perturbation and nutation.
  const auto Σl = evaluated.Σl + perturbation::longitude(evaluated.ctx);
  const auto lon_nutation = astro::earth::nutation::longitude(jde);
  const toolbox::AngleDeg lon = evaluated.ctx.Lp + toolbox::AngleDeg { Σl / elp2000_82b::LON_LAT_SCALING_FACTOR } + lon_nutation;

  // Latitude, considering the perturbation.
  const auto Σb = evaluated.Σb + perturbation::latitude(evaluated.ctx);
  const toolbox::AngleDeg lat { Σb / elp2000_82b::LON_LAT_SCALING_FACTOR };

  // Distance, in KM.
  const toolbox::DistanceKm r { 385000.56 + evaluated.Σr / elp2000_82b::RADIUS_SCALING_FACTOR };

  return {
    .λ = lon.normalize(),
    .β = lat,
    .r = toolbox::DistanceAu { r }
  };
}


/**
 * @brief Calculate the equatorial horizontal parallax of the Moon.
 * @param distance The geocentric distance of the Moon.
 * @return The equatorial horizontal parallax of the Moon.
 */
inline auto equatorial_horizontal_parallax(const toolbox::DistanceKm& distance) -> toolbox::AngleRad {
  const auto ppi_rad = std::asin(6378.14 / distance.km());
  return toolbox::AngleRad { ppi_rad };
}

} // namespace astro::moon::geocentric_coord
