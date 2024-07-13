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

#include <chrono>
#include <cassert>

#include "date.hpp"
#include "datetime.hpp"

namespace astro::julian_day {

// According to https://aa.usno.navy.mil/data/JulianDate
// > Note that the time scale that is the basis for Julian dates is Universal Time (UT1), 
//   and that 0h UT1 corresponds to a Julian date fraction of 0.5.


/**
 * @brief The julian day number of 2000-01-01, 12:00:00.0 (noon).
 */
constexpr double J2000 = 2451545.0;

/**
 * @brief The julian day number of 1858-11-17, 00:00:00.0, which is the zero point of modified julian day.
 */
constexpr double MJD0 = 2400000.5;

/**
 * @brief Converts a UT1 datetime to julian day.
 * @param dt The date and time (UT1).
 * @return The julian day number.
 */
double ut1_to_jd(const calendar::Datetime& dt) {
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

  assert(dt.ok());
  
  const auto& [g_y, g_m, g_d] = util::from_ymd(dt.ymd);
  assert(g_y > 0);

  const uint32_t Y = (g_m <= 2) ? g_y - 1 : g_y;
  const uint32_t M = (g_m <= 2) ? g_m + 12 : g_m;
  const uint32_t D = g_d;

  const uint32_t A = Y / 100;
  const uint32_t B = A / 4;
  const uint32_t C = 2 - A + B;
  const uint32_t E = 365.25 * (Y + 4716);
  const uint32_t F = 30.6001 * (M + 1);
  const double  JD = C + D + E + F - 1524.5 + dt.fraction(); // add the fractional part as well.

  assert(JD > 0);
  return JD;
}


/**
 * @brief Converts a julian day number to UT1 datetime.
 * @param jd The julian day number.
 * @return The date and time.
 */
calendar::Datetime jd_to_ut1(const double jd) {
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

  assert(jd > 0);

  // The algorithm fails if Y < 400, so we need to check it.
  if (jd < 1885420.686921) {
    // Julian day number of 450-01-01 (gregorian) is roughly 1885420.686921
    throw std::runtime_error("The estimated gregorian year is < 450.");
  }

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
 * @brief Converts a julian day number to modified julian day.
 * @param jd The julian day number.
 * @return The modified julian day.
 */
constexpr double jd_to_mjd(const double jd) {
  return jd - MJD0;
}


/**
 * @brief Converts a modified julian day to julian day number.
 * @param mjd The modified julian day.
 * @return The julian day number.
 */
constexpr double mjd_to_jd(const double mjd) {
  return mjd + MJD0;
}


/**
 * @brief Converts a julian day number to julian millennium.
 * @param jd The julian day number.
 * @return The julian millennium since J2000.
 */
constexpr double jd_to_jm(const double jd) {
  return (jd - J2000) / 365250.0;
}

/**
 * @brief Converts a julian millennium to julian day number.
 * @param jm The julian millennium since J2000.
 * @return The julian day number.
 */
constexpr double jm_to_jd(const double jm) {
  return jm * 365250.0 + J2000;
}


/**
 * @brief Converts a julian day number to julian century.
 * @param jd The julian day number.
 * @return The julian century since J2000.
 */
constexpr double jd_to_jc(const double jd) {
  return (jd - J2000) / 36525.0;
}


/**
 * @brief Converts a julian century to julian day number.
 * @param jc The julian century since J2000.
 * @return The julian day number.
 */
constexpr double jc_to_jd(const double jc) {
  return jc * 36525.0 + J2000;
}


} // namespace astro::julian_day
