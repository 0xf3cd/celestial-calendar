/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
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

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "earth.hpp"


namespace astro::sidereal {

/**
 * @brief Compute the Greenwich Mean Sidereal Time (GMST) for a UT1 instant.
 * @param jd_ut1 The julian day number, on the **UT1** scale — NOT the JDE (TT) used by most other
 *        algorithms in this library. Feeding a TT value here silently shifts the result by
 *        ΔT ≈ 69 s of time (≈ 0.29° of rotation, at epoch ~2026).
 * @return The GMST (Greenwich hour angle of the mean vernal equinox), normalized to [0°, 360°).
 * @note Meeus (12.4) gives GMST as a polynomial in julian centuries of *universal* time; its daily
 *       advance is only the ~0.9856° 0h-to-0h drift — the 360°/day rotation drops out mod 360°
 *       **only on the 0h UT grid** (Meeus's Example 12.a is such a 0h case). The completed form
 *       used here folds the full sidereal rate back in and stays valid at any UT1 instant:
 *       θ₀ = 280.46061837° + 360.98564736629°·d + 0.000387933°·T² − T³/38710000 (mod 360°),
 *       where the constant picks up 180° because J2000.0 falls at noon UT. On the 0h grid this
 *       coincides with (12.4) exactly; `julian_day::jde_to_jc` is not reused since its contract
 *       is TT-based.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 12, Formulas (12.2)-(12.4).
 */
inline auto greenwich_mean(const double jd_ut1) -> astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> {
  using astro::toolbox::Angle;
  using astro::toolbox::AngleUnit::DEG;

  // Days and julian centuries of *universal* time since J2000.0 UT.
  const double d_ut1 = jd_ut1 - astro::julian_day::J2000;
  const double t_ut1 = d_ut1 / 36525.0;

  const double θ0 = 280.46061837
                  + astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY * d_ut1
                  + 0.000387933 * t_ut1 * t_ut1
                  - (t_ut1 * t_ut1 * t_ut1) / 38710000.0;

  return Angle<DEG> { θ0 }.normalize();
}

/**
 * @brief Compute the Greenwich Apparent Sidereal Time (GAST) for a UT1 instant.
 * @param jd_ut1 The julian day number, on the **UT1** scale (see `greenwich_mean`'s warning).
 * @param jde_tt The julian ephemeris day, on the **TT** scale, of the same instant; used only for
 *        the nutation terms (Δψ and ε), which are ephemeris-time quantities.
 * @param model The nutation model to use. Defaults to `earth::nutation::Model::IAU_1980`.
 * @return The GAST (Greenwich hour angle of the true vernal equinox), normalized to [0°, 360°).
 * @note Meeus: apparent sidereal time = mean sidereal time + Δψ·cos ε, where Δψ is the nutation
 *       in longitude and ε the TRUE obliquity (ε₀ + Δε), both evaluated at `jde_tt`.
 *       The correction (the equation of the equinoxes) is small: |Δψ| ≲ 17".3.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 12 (and Chapter 22 for Δψ, ε).
 */
inline auto greenwich_apparent(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the jd_ut1/jde_tt naming is the UT1/TT guard (issue #41).
  const double jd_ut1,
  const double jde_tt,
  const astro::earth::nutation::Model model = astro::earth::nutation::Model::IAU_1980
) -> astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> {
  using astro::toolbox::Angle;
  using astro::toolbox::AngleUnit::DEG;

  // The nutation correction in right ascension: Δψ·cos ε. Nutation is computed on the TT scale.
  const auto Δψ = astro::earth::nutation::longitude(jde_tt, model);
  const auto ε  = astro::earth::obliquity::true_obliquity(jde_tt, model);
  const double correction_deg = Δψ.deg() * std::cos(ε.rad());

  return (greenwich_mean(jd_ut1) + correction_deg).normalize();
}

/**
 * @brief Compute the Local Apparent Sidereal Time (LAST) for an observer.
 * @param jd_ut1 The julian day number, on the **UT1** scale (see `greenwich_mean`'s warning).
 * @param jde_tt The julian ephemeris day, on the **TT** scale, of the same instant (nutation).
 * @param longitude The observer's geographic longitude, measured **positive west** from Greenwich
 *        (Meeus's convention; e.g. Washington USNO ≈ +77°.0656). East-positive longitudes must
 *        be negated before calling.
 * @param model The nutation model to use. Defaults to `earth::nutation::Model::IAU_1980`.
 * @return The LAST, normalized to [0°, 360°).
 * @note With the west-positive convention the relation reads θ = θ₀(GAST) − longitude, consistent
 *       with Meeus's hour-angle formula H = θ₀ − L − α used from Chapter 13 onward.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 12.
 */
inline auto local_apparent(
  const double jd_ut1,
  const double jde_tt,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& longitude,
  const astro::earth::nutation::Model model = astro::earth::nutation::Model::IAU_1980
) -> astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> {
  return (greenwich_apparent(jd_ut1, jde_tt, model) - longitude.deg()).normalize();
}

} // namespace astro::sidereal
