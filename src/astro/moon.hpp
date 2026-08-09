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
#include <format>
#include <stdexcept>

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
[[nodiscard]] inline auto longitude(const elp2000_82b::Context& ctx) -> double {
  return (3958.0 * std::sin(ctx.A1.rad())) 
       + (1962.0 * std::sin(ctx.Lp.rad() - ctx.F.rad())) 
       + (318.0 * std::sin(ctx.A2.rad()));
}


/**
 * @brief Calculate perturbation of the Moon's geocentric latitude.
 * @details As per Astronomical Algorithms, Jean Meeus, 1998, Chapter 47, 
 *          the Moon is perturbed by Venus, Jupiter, and Earth.
 * @param ctx The context.
 * @return The perturbation of the Moon's geocentric latitude. Unit is 0.000001 degrees.
 * @see Astronomical Algorithms, Jean Meeus, 1998, Chapter 47.
 */
[[nodiscard]] inline auto latitude(const elp2000_82b::Context& ctx) -> double {
  return (-2235.0 * std::sin(ctx.Lp.rad()))
       + (382.0 * std::sin(ctx.A3.rad()))
       + (175.0 * std::sin(ctx.A1.rad() - ctx.F.rad()))
       + (175.0 * std::sin(ctx.A1.rad() + ctx.F.rad()))
       + (127.0 * std::sin(ctx.Lp.rad() - ctx.Mp.rad()))
       - (115.0 * std::sin(ctx.Lp.rad() + ctx.Mp.rad()));
}

} // namespace astro::moon::perturbation


namespace astro::moon::geocentric_coord {

/**
 * @brief Earth's equatorial radius, in kilometers.
 * @ref Astronomical Algorithms, Jean Meeus, 1998, Chapter 47 -- the equatorial horizontal parallax
 *      is `asin(6378.14 / r)`, with `r` the Earth-Moon distance in the same unit.
 */
inline constexpr double EARTH_EQUATORIAL_RADIUS_KM = 6378.14;

/**
 * @brief Calculate the apparent geocentric position of the Moon, using truncated ELP2000-82B.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The geocentric ecliptic position of the Moon, calculated using truncated ELP2000-82B.
 */
[[nodiscard]] inline auto apparent(const double jde) -> toolbox::SphericalCoordinate {
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
  const toolbox::DistanceKm r { 385000.56 + (evaluated.Σr / elp2000_82b::RADIUS_SCALING_FACTOR) };

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
 * @throw std::invalid_argument if `distance` is not finite, or is not greater than Earth's
 *        equatorial radius.
 * @note The guard is the contract, not a reachability claim: the real Moon never comes near,
 *       but this is a public function over a `Distance<KM>` that is open to any value, and
 *       `asin` of an argument above 1 returns a silent NaN rather than failing (#86).
 */
[[nodiscard]] inline auto equatorial_horizontal_parallax(const toolbox::DistanceKm& distance) -> toolbox::AngleRad {
  const double km = distance.km();

  // Written as `not (km > r)` so that a NaN distance fails the check too — `km <= r` is always
  // false for NaN and would let it through to `asin` (#67's lesson, same shape). NaN is not the
  // whole job: `inf > r` is true, and `asin(r / inf)` is a well-formed 0 rad — a plausible-looking
  // answer for a nonsense input — so finiteness needs its own check.
  if (not std::isfinite(km) or not (km > EARTH_EQUATORIAL_RADIUS_KM)) {
    throw std::invalid_argument {
      std::format("Argument `distance` must exceed Earth's equatorial radius {} km, got {}",
                  EARTH_EQUATORIAL_RADIUS_KM, km)
    };
  }

  const auto ppi_rad = std::asin(EARTH_EQUATORIAL_RADIUS_KM / km);
  return toolbox::AngleRad { ppi_rad };
}

} // namespace astro::moon::geocentric_coord
