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

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>

#include "delta_t.hpp"
#include "leap_second.hpp"

#include "ymd.hpp"
#include "datetime.hpp"

namespace astro::julian_day {

// According to https://aa.usno.navy.mil/data/JulianDate
// > Note that the time scale that is the basis for Julian dates is Universal Time (UT1), 
//   and that 0h UT1 corresponds to a Julian date fraction of 0.5.
//
// The Julian day number used in Jean Meeus algo is actually JDE, Julian Ephemeris Date,
// which is based on TT (Terrestrial Time, i.e. Dynamical Time).

/**
 * @brief The julian day number of 2000-01-01, 12:00:00.0 (noon).
 * @ref ERFA v2.0.1 `erfam.h`, `ERFA_DJ00`.
 */
inline constexpr double J2000 = 2451545.0;

/**
 * @brief The number of days in one Julian century (36525 days).
 * @ref ERFA v2.0.1 `erfam.h`, `ERFA_DJC`.
 */
inline constexpr double DAYS_PER_JULIAN_CENTURY = 36525.0;


/**
 * @brief Convert UT1 datetime to julian day number.
 * @param ut1_dt The datetime in UT1.
 * @return The julian day number.
 * @throw std::runtime_error if the gregorian year is < 1.
 * @note Years 1-400 convert forward, but sit below `jd_to_ut1`'s year-401 bound — round-trips
 *       only close from 401-01-01 onwards.
 * @ref ERFA v2.0.1 `src/cal2jd.c`, `eraCal2jd`.
 */
[[nodiscard]] inline auto ut1_to_jd(const calendar::Datetime& ut1_dt) -> double {
  assert(ut1_dt.ok());
  
  const auto& [g_y, g_m, g_d] = util::from_ymd(ut1_dt.ymd);

  // #77: preserve the forward conversion's public year domain.
  if (g_y < 1) {
    throw std::runtime_error {
      std::format("The year {} is < 1, not supported by this algorithm.", g_y)
    };
  }

  const auto month_offset = (static_cast<std::int64_t>(g_m) - 14) / 12;
  const auto year_with_offset = static_cast<std::int64_t>(g_y) + month_offset;
  const auto modified_julian_day =
    ((1461 * (year_with_offset + 4800)) / 4)
    + ((367 * (static_cast<std::int64_t>(g_m) - 2 - (12 * month_offset))) / 12)
    - ((3 * ((year_with_offset + 4900) / 100)) / 4)
    + static_cast<std::int64_t>(g_d) - 2432076;
  const double jd = 2400000.5 + static_cast<double>(modified_julian_day) + ut1_dt.fraction();

  assert(jd > 0);
  return jd;
}


/**
 * @brief Convert julian day number to UT1 datetime.
 * @param jd The julian day number.
 * @return The datetime in UT1.
 * @throw std::runtime_error if `jd` is not finite, beyond the `year_month_day`-representable
 *        years, or the estimated gregorian year is < 401.
 * @note `ut1_to_jd(32767-12-31, fraction ≈ 1)` rounds up to exactly the upper bound
 *       13689325.5, which this function rejects — a 1-ulp round-trip break callers must expect.
 * @ref ERFA v2.0.1 `src/jd2cal.c`, `eraJd2cal`.
 */
[[nodiscard]] inline auto jd_to_ut1(const double jd) -> calendar::Datetime {
  // #77: NaN/Inf would slip past the range check below (NaN comparisons are false) and reach
  // undefined float→int conversions. Reject them first.
  if (not std::isfinite(jd)) {
    throw std::runtime_error {
      std::format("The julian day number {} is not finite.", jd)
    };
  }

  // #77: preserve the lower bound at the first full year in the public inverse domain.
  if (jd < 1867522.5) {
    throw std::runtime_error {
      std::format("The julian day number {} is below JD 1867522.5 (401-01-01), "
                  "where the estimated gregorian year drops under 401.", jd)
    };
  }

  // #67: JD 13689325.5 is conceptually 32768-01-01, beyond `std::chrono::year`.
  if (jd >= 13689325.5) {
    throw std::runtime_error {
      std::format("The julian day number {} is beyond the representable years.", jd)
    };
  }

  const double rounded_day = std::floor(jd + 0.5);
  auto day_number = static_cast<std::int64_t>(rounded_day);
  const std::array residuals { jd - rounded_day, 0.0 };

  double sum = 0.5;
  double correction = 0.0;
  for (const double residual : residuals) {
    const double next_sum = sum + residual;
    correction += (std::fabs(sum) >= std::fabs(residual))
                    ? (sum - next_sum) + residual
                    : (residual - next_sum) + sum;
    sum = next_sum;
    if (sum >= 1.0) {
      ++day_number;
      sum -= 1.0;
    }
  }

  double fraction = sum + correction;
  correction = fraction - sum;

  if (fraction < 0.0) {
    fraction = sum + 1.0;
    correction += (1.0 - fraction) + sum;
    sum = fraction;
    fraction = sum + correction;
    correction = fraction - sum;
    --day_number;
  }

  if ((fraction - 1.0) >= -std::numeric_limits<double>::epsilon() / 4.0) {
    const double next_sum = sum - 1.0;
    correction += (sum - next_sum) - 1.0;
    sum = next_sum;
    fraction = sum + correction;
    if (-std::numeric_limits<double>::epsilon() / 2.0 < fraction) {
      ++day_number;
      fraction = (fraction > 0.0) ? fraction : 0.0;
    }
  }

  auto l = day_number + 68569;
  const auto n = (4 * l) / 146097;
  l -= ((146097 * n) + 3) / 4;
  const auto i = (4000 * (l + 1)) / 1461001;
  l -= ((1461 * i) / 4) - 31;
  const auto k = (80 * l) / 2447;
  const auto day = l - ((2447 * k) / 80);
  l = k / 11;
  const auto month = k + 2 - (12 * l);
  const auto year = (100 * (n - 49)) + i + l;

  assert(1 <= day and day <= 31);
  assert(0.0 <= fraction and fraction < 1.0);
  assert(1 <= month and month <= 12);
  assert(year > 0);

  const auto& ymd = util::to_ymd(
    static_cast<std::uint32_t>(year),
    static_cast<std::uint32_t>(month),
    static_cast<std::uint32_t>(day)
  );
  assert(ymd.ok());

  return calendar::Datetime { ymd, fraction };
}


/**
 * @brief Converts a TT datetime to julian ephemeris day number.
 * @param tt_dt The date and time (TT).
 * @return The julian ephemeris day number, which is based on TT (not UT1).
 */
[[nodiscard]] inline auto tt_to_jde(const calendar::Datetime& tt_dt) -> double {
  // In my understanding, the process of converting UT1->JD and TT->JDE is the same.
  return ut1_to_jd(tt_dt);
}


/**
 * @brief Converts a julian ephemeris day number to TT datetime.
 * @param jde The julian ephemeris day number, which is based on TT (not UT1).
 * @return The date and time, in TT.
 */
[[nodiscard]] inline auto jde_to_tt(const double jde) -> calendar::Datetime {
  // In my understanding, the process of converting UT1->JD and TT->JDE is the same.
  return jd_to_ut1(jde);
}


/**
 * @brief Converts a julian ephemeris day number to UT1 datetime.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The date and time, in UT1.
 */
[[nodiscard]] inline auto jde_to_ut1(const double jde) -> calendar::Datetime {
  const auto tt_dt = jde_to_tt(jde);
  return astro::delta_t::tt_to_ut1(tt_dt);
}


/**
 * @brief Converts a UT1 datetime to julian ephemeris day number.
 * @param ut1_dt The date and time, in UT1.
 * @return The julian ephemeris day number, which is based on TT.
 */
[[nodiscard]] inline auto ut1_to_jde(const calendar::Datetime& ut1_dt) -> double {
  const auto tt_dt = astro::delta_t::ut1_to_tt(ut1_dt);
  return tt_to_jde(tt_dt);
}


/**
 * @brief Converts a julian ephemeris day number to UTC datetime.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The date and time, in UTC.
 * @note Before modern UTC (1972-01-01) this degrades to UT1; see `astro::leap_second::tt_to_utc`.
 */
[[nodiscard]] inline auto jde_to_utc(const double jde) -> calendar::Datetime {
  return astro::leap_second::tt_to_utc(jde_to_tt(jde));
}


/**
 * @brief Converts a UTC datetime to julian ephemeris day number.
 * @param utc_dt The date and time, in UTC.
 * @return The julian ephemeris day number, which is based on TT.
 * @note Before modern UTC (1972-01-01) this degrades to UT1; see `astro::leap_second::utc_to_tt`.
 */
[[nodiscard]] inline auto utc_to_jde(const calendar::Datetime& utc_dt) -> double {
  return tt_to_jde(astro::leap_second::utc_to_tt(utc_dt));
}


/**
 * @brief Converts a julian ephemeris day number to julian millennium.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The julian millennium since J2000.
 * @ref ERFA v2.0.1 `erfam.h`, `ERFA_DJM`.
 */
[[nodiscard]] constexpr auto jde_to_jm(const double jde) -> double {
  return (jde - J2000) / 365250.0;
}

/**
 * @brief Converts a julian millennium to julian ephemeris day number.
 * @param jm The julian millennium since J2000.
 * @return The julian ephemeris day number, which is based on TT.
 * @ref ERFA v2.0.1 `erfam.h`, `ERFA_DJM`.
 */
[[nodiscard]] constexpr auto jm_to_jde(const double jm) -> double {
  return (jm * 365250.0) + J2000;
}


/**
 * @brief Converts a julian ephemeris day number to julian century.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The julian century since J2000.
 */
[[nodiscard]] constexpr auto jde_to_jc(const double jde) -> double {
  return (jde - J2000) / DAYS_PER_JULIAN_CENTURY;
}


/**
 * @brief Converts a julian century to julian ephemeris day number.
 * @param jc The julian century since J2000.
 * @return The julian ephemeris day number, which is based on TT.
 */
[[nodiscard]] constexpr auto jc_to_jde(const double jc) -> double {
  return (jc * DAYS_PER_JULIAN_CENTURY) + J2000;
}


} // namespace astro::julian_day
