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
#include <cassert>
#include <format>
#include <stdexcept>

#include "delta_t.hpp"

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
 */
constexpr double J2000 = 2451545.0;


/**
 * @brief Convert UT1 datetime to julian day number.
 * @param ut1_dt The datetime in UT1.
 * @return The julian day number.
 * @throw std::runtime_error if the gregorian year is < 1.
 * @note Years 1-400 convert forward, but sit below `jd_to_ut1`'s year-401 bound — round-trips
 *       only close from 401-01-01 onwards.
 */
inline auto ut1_to_jd(const calendar::Datetime& ut1_dt) -> double {
  /*
    Ref: https://quasar.as.utexas.edu/BillInfo/JulianDatesG.html
    The algorithm is as follows:
      Y, M, D = year, month, day
      if M <= 2:
        Y = Y - 1
        M = M + 12 
      A = Y/100
      B = A/4
      C = 2-A+B
      E = 365.25x(Y+4716)
      F = 30.6001x(M+1)
      JD= C+D+E+F-1524.5

    All above variables except JD are integers (dropping the fractional part).
   */

  assert(ut1_dt.ok());
  
  const auto& [g_y, g_m, g_d] = util::from_ymd(ut1_dt.ymd);

  // #77: the unsigned arithmetic below wraps for year < 1 (`g_y - 1` as `uint32_t`) and would
  // silently return a garbage JD. Throw instead, mirroring `jd_to_ut1`'s error contract.
  if (g_y < 1) {
    throw std::runtime_error {
      std::format("The year {} is < 1, not supported by this algorithm.", g_y)
    };
  }

  // NOLINTBEGIN
  // The following code is doing narrowing-conversions. 
  // But keep it as-is for matching the original algorithm expressions.
  const uint32_t Y = (g_m <= 2) ? g_y - 1 : g_y;
  const uint32_t M = (g_m <= 2) ? g_m + 12 : g_m;
  const uint32_t D = g_d;

  const uint32_t A = Y / 100;
  const uint32_t B = A / 4;
  const uint32_t C = 2 - A + B;
  const uint32_t E = 365.25 * (Y + 4716);
  const uint32_t F = 30.6001 * (M + 1);
  const double  JD = C + D + E + F - 1524.5 + ut1_dt.fraction(); // add the fractional part as well.
  // NOLINTEND

  assert(JD > 0);
  return JD;
}


/**
 * @brief Convert julian day number to UT1 datetime.
 * @param jd The julian day number.
 * @return The datetime in UT1.
 * @throw std::runtime_error if `jd` is not finite, or the estimated gregorian year is < 401.
 */
inline auto jd_to_ut1(const double jd) -> calendar::Datetime {
  /*
    Ref: https://quasar.as.utexas.edu/BillInfo/JulianDatesG.html
    The algorithm is as follows:
      Q = JD+0.5
      Z = Integer part of Q
      W = (Z - 1867216.25)/36524.25
      X = W/4
      A = Z+1+W-X
      B = A+1524
      C = (B-122.1)/365.25
      D = 365.25xC
      E = (B-D)/30.6001
      F = 30.6001xE
      Day of month = B-D-F+(Q-Z)
      Month = E-1 or E-13 (must get number less than or equal to 12)
      Year = C-4715 (if Month is January or February) or C-4716 (otherwise)
   
     It is mentioned that "dropping the fractional part of all multiplicatons and divisions",
     so all above variables except JD are integers.

     It is also mentioned that "the method fails if Y<400".
   */

  // #77: NaN/Inf would slip past the range check below (NaN comparisons are false) and reach
  // undefined float→int conversions. Reject them first.
  if (not std::isfinite(jd)) {
    throw std::runtime_error {
      std::format("The julian day number {} is not finite.", jd)
    };
  }

  assert(jd > 0);

  // The reference says the method fails if Y < 400, so reject everything below year 401 — the
  // smallest full year safely inside the method's domain. 401-01-01 (gregorian) is exactly
  // JD 1867522.5 (#77: the old cutoff 1867524.457118 sat ~2 days high and wrongly rejected
  // the first two days of year 401).
  if (jd < 1867522.5) {
    throw std::runtime_error("The estimated gregorian year is < 401.");
  }

  // NOLINTBEGIN
  // The following code is doing narrowing-conversions. 
  // But keep it as-is for matching the original algorithm expressions.
  const double   Q = jd + 0.5;
  const uint32_t Z = Q;
  const uint32_t W = (Z - 1867216.25) / 36524.25;
  const uint32_t X = W / 4;
  const uint32_t A = Z + 1 + W - X;
  const uint32_t B = A + 1524;
  const uint32_t C = (B - 122.1) / 365.25;
  const uint32_t D = 365.25 * C;
  const uint32_t E = (B - D) / 30.6001;
  const uint32_t F = 30.6001 * E;

  const uint32_t day = B - D - F + (Q - Z);
  const double fraction = B - D - F + (Q - Z) - day;
  // NOLINTEND

  assert(1 <= day and day <= 31);
  assert(0.0 <= fraction and fraction < 1.0);

  const uint32_t month = (E > 13) ? (E - 13) : (E - 1);
  assert(1 <= month and month <= 12);

  const uint32_t year = (month <= 2) ? C - 4715 : C - 4716;
  assert(year > 0);

  const auto& ymd = util::to_ymd(year, month, day);
  assert(ymd.ok());

  return calendar::Datetime { ymd, fraction };
}


/**
 * @brief Converts a TT datetime to julian ephemeris day number.
 * @param tt_dt The date and time (TT).
 * @return The julian ephemeris day number, which is based on TT (not UT1).
 */
inline auto tt_to_jde(const calendar::Datetime& tt_dt) -> double {
  // In my understanding, the process of converting UT1->JD and TT->JDE is the same.
  return ut1_to_jd(tt_dt);
}


/**
 * @brief Converts a julian ephemeris day number to TT datetime.
 * @param jde The julian ephemeris day number, which is based on TT (not UT1).
 * @return The date and time, in TT.
 */
inline auto jde_to_tt(const double jde) -> calendar::Datetime {
  // In my understanding, the process of converting UT1->JD and TT->JDE is the same.
  return jd_to_ut1(jde);
}


/**
 * @brief Converts a julian ephemeris day number to UT1 datetime.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The date and time, in UT1.
 */
inline auto jde_to_ut1(const double jde) -> calendar::Datetime {
  const auto tt_dt = jde_to_tt(jde);
  return astro::delta_t::tt_to_ut1(tt_dt);
}


/**
 * @brief Converts a UT1 datetime to julian ephemeris day number.
 * @param ut1_dt The date and time, in UT1.
 * @return The julian ephemeris day number, which is based on TT.
 */
inline auto ut1_to_jde(const calendar::Datetime& ut1_dt) -> double {
  const auto tt_dt = astro::delta_t::ut1_to_tt(ut1_dt);
  return tt_to_jde(tt_dt);
}


/**
 * @brief Converts a julian day number to julian millennium.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The julian millennium since J2000.
 */
constexpr auto jde_to_jm(const double jde) -> double {
  return (jde - J2000) / 365250.0;
}

/**
 * @brief Converts a julian millennium to julian day number.
 * @param jm The julian millennium since J2000.
 * @return The julian ephemeris day number, which is based on TT.
 */
constexpr auto jm_to_jde(const double jm) -> double {
  return jm * 365250.0 + J2000;
}


/**
 * @brief Converts a julian day number to julian century.
 * @param jde The julian day number.
 * @return The julian century since J2000.
 */
constexpr auto jde_to_jc(const double jde) -> double {
  return (jde - J2000) / 36525.0;
}


/**
 * @brief Converts a julian century to julian day number.
 * @param jc The julian century since J2000.
 * @return The julian ephemeris day number, which is based on TT.
 */
constexpr auto jc_to_jde(const double jc) -> double {
  return jc * 36525.0 + J2000;
}


} // namespace astro::julian_day
