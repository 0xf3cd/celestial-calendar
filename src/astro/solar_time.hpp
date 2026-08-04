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
#include <format>
#include <stdexcept>

#include "toolbox.hpp"
#include "sun.hpp"
#include "earth.hpp"
#include "julian_day.hpp"

#include "datetime.hpp"

namespace astro::solar_time {

// Apparent (true) solar time is what a sundial reads: it follows the real Sun's hour angle.
// Mean solar time ticks uniformly. Their difference is the equation of time (Meeus ch. 28),
// driven by the orbit's eccentricity and the obliquity of the ecliptic.

/** @brief Seconds of clock time per degree of hour angle. */
inline constexpr double SECONDS_OF_TIME_PER_DEGREE = 86400.0 / 360.0;


namespace detail {

/**
 * @brief Computes the Sun's mean longitude L0, referred to the mean equinox of the date.
 * @param jde_tt The julian ephemeris day number, which is based on TT.
 * @return L0, unnormalized.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Formula (28.2).
 */
[[nodiscard]] constexpr auto sun_mean_longitude(const double jde_tt) -> astro::toolbox::AngleDeg {
  using astro::toolbox::AngleDeg;

  const double τ = astro::julian_day::jde_to_jm(jde_tt);
  const double L0 = 280.4664567
                  + τ * (360007.6982779
                  + τ * (0.03032028
                  + τ * (1.0 / 49931.0
                  + τ * (-1.0 / 15300.0
                  + τ * (-1.0 / 2000000.0)))));
  return AngleDeg { L0 };
}

} // namespace detail


/**
 * @brief Computes the equation of time E = apparent solar time − mean solar time.
 * @param jde_tt The julian ephemeris day number, which is based on TT.
 * @return E as an angle in [−180°, 180°); multiply by `SECONDS_OF_TIME_PER_DEGREE` for
 *         seconds of time. Positive when the true Sun culminates before mean noon;
 *         |E| stays under 5° (20 min of time).
 * @details The wrap matters near the equinoxes, where the apparent right ascension α and
 *          the mean longitude L0 can sit on opposite sides of the 0°/360° seam.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Formula (28.1).
 */
[[nodiscard]] inline auto equation_of_time(const double jde_tt) -> astro::toolbox::AngleDeg {
  using astro::toolbox::AngleDeg;
  using astro::toolbox::normalize_pm180;

  const auto L0 = detail::sun_mean_longitude(jde_tt);
  const auto α  = astro::sun::equatorial_coord::apparent(jde_tt).α;
  const auto Δψ = astro::earth::nutation::longitude(jde_tt);
  const auto ε  = astro::earth::obliquity::true_obliquity(jde_tt);

  // (28.1); the constant 0°.0057183 folds aberration and the FK5 correction into L0.
  const double e = L0.deg() - 0.0057183 - α.deg() + Δψ.deg() * std::cos(ε.rad());
  return AngleDeg { normalize_pm180(e) };
}


/**
 * @brief Converts a civil UTC moment to local apparent (true) solar time (真太阳时).
 * @param utc_dt The date and time, in UTC.
 * @param longitude The observer's geographic longitude, positive east.
 * @return The local apparent solar Datetime: UTC + longitude in time + E.
 * @throw std::invalid_argument if `longitude` is not finite or outside [−180°, 180°];
 *        also propagated from `calendar::add_seconds` when the shifted result falls
 *        outside the representable year range.
 * @note Apparent solar time follows Earth rotation (UT1); feeding UTC leaves the
 *       |UT1 − UTC| < 0.9 s steering gap in the result, and before 1972 the input
 *       degrades to UT1 (see `astro::leap_second`). E is evaluated at the input
 *       instant — the output relabels that same physical moment (the up-to-±12 h
 *       longitude offset is a label shift, not a time shift), so no fixed-point
 *       evaluation is needed.
 */
[[nodiscard]] inline auto apparent(
  const calendar::Datetime& utc_dt,
  const astro::toolbox::AngleDeg& longitude
) -> calendar::Datetime {
  const double lon = longitude.deg();
  if (not std::isfinite(lon) or lon < -180.0 or lon > 180.0) {
    throw std::invalid_argument {
      std::format("Argument `longitude` out of range [-180, 180], whose value is {}", lon)
    };
  }

  const auto e = equation_of_time(astro::julian_day::utc_to_jde(utc_dt));
  return calendar::add_seconds(
    utc_dt,
    (lon + e.deg()) * SECONDS_OF_TIME_PER_DEGREE
  );
}

} // namespace astro::solar_time
