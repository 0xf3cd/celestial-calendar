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
#include "earth.hpp"

namespace astro::sun::geocentric_coord {

using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;
using astro::toolbox::AngleUnit::RAD;
using astro::toolbox::SphericalCoordinate;


/**
 * @brief Calculate the geocentric position of the Sun, using VSOP87D.
 * @param jd The Julian Day.
 * @return The geocentric ecliptic position of the Sun, calculated using VSOP87D.
 * @details The function invokes `astro::earth::heliocentric_coord::vsop87d`, and
 *          transforms the heliocentric coordinates to geocentric coordinates.
 */
SphericalCoordinate vsop87d(const double jd) {
  const auto& [λ_helio, β_helio, r_helio] = astro::earth::heliocentric_coord::vsop87d(jd);
  return {
    // Convert the heliocentric ecliptic longitude of Earth to geocentric ecliptic longitude of Sun.
    // The formula is: λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180∘
    .λ = std::invoke([&] {
      using namespace astro::toolbox::literals;
      const auto sum = λ_helio + 180.0_deg;
      return sum.normalize();
    }),

    // Convert the heliocentric ecliptic latitude of Earth to geocentric ecliptic latitude of Sun.
    // The formula is: β_sun_geocentric_deg = -β_earth_heliocentric_deg
    .β = -β_helio,

    // The distance (radius) is the same for both Sun and Earth.
    .r = r_helio 
  };
}


/** @brief The FK5 correction for the coordinate calculated using VSOP87D. */
struct Fk5Correction {
  const Angle<DEG> Δλ;
  const Angle<DEG> Δβ;
};


/**
 * @brief Calculate the correction for the VSOP87D coordinate, in order to convert it to FK5 system.
 * @param jd The Julian Day.
 * @return The correction (i.e. Δlongitude and Δlatitude).
 * @details As per Jean Meeus's Astronomical Algorithms, this correction is applied for accuracy.
 */
Fk5Correction fk5_correction(const double jd, const SphericalCoordinate& vsop87d_coord) {
  const double jc = astro::julian_day::jd_to_jc(jd);
  const auto& [vsop_λ, vsop_β, vsop_r] = vsop87d_coord;

  // Calculate the deltas for longitude and latitude, in arcsec.
  const Angle λ_dash = vsop_λ - Angle<DEG> { (1.397 + 0.00031 * jc) * jc };
  const double λ_dash_rad = λ_dash.as<RAD>();

  const double delta_λ_arcsec = -0.09033 + 0.03916 * (cos(λ_dash_rad) + sin(λ_dash_rad)) * tan(vsop_β.as<RAD>());
  const double delta_β_arcsec = 0.03916 * (cos(λ_dash_rad) - sin(λ_dash_rad));

  return {
    .Δλ = Angle<DEG>::from_arcsec(delta_λ_arcsec),
    .Δβ = Angle<DEG>::from_arcsec(delta_β_arcsec),
  };
}


/**
 * @brief Calculate the geocentric position of the Sun, using VSOP87D. 
 *        The position is corrected to FK5 system, considering nutation and aberration. 
 * @param jd The Julian Day.
 * @return The geocentric ecliptic position of the Sun, after correction.
 */
SphericalCoordinate corrected_position(const double jd) {
  // Use VSOP87D to calculate the geocentric ecliptic position of the Sun.
  const auto vsop_coord = vsop87d(jd);

  // Calculate the correction for the VSIO87D result, in order to convert it to FK5 system.
  const auto correction = fk5_correction(jd, vsop_coord);

  // Calculate the Earth's nutation in longtitude.
  const auto nutation = astro::earth::nutation::longitude(jd);

  // Calculate the Solar aberration.
  const auto aberration = astro::earth::aberration::compute(vsop_coord.r);

  // Calculate the adjusted longitude.
  const auto λ = vsop_coord.λ + correction.Δλ + nutation - aberration;

  // Calculate the adjusted latitude.
  const auto β = vsop_coord.β + correction.Δβ;

  return {
    .λ = λ,
    .β = β,
    .r = vsop_coord.r
  };
}

} // namespace astro::sun::geocentric_coord
