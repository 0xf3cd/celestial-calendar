#ifndef __CALENDAR_JULIAN_DAY_HPP__
#define __CALENDAR_JULIAN_DAY_HPP__

#include <chrono>
#include <cassert>

#include "date.hpp"

namespace calendar::julian_day {

enum class Algo {
  Algo1, // Ref: https://quasar.as.utexas.edu/BillInfo/JulianDatesG.html
};


#pragma region Algo1

/**
 * @brief Converts a gregorian date to julian day. Using algorithm 1.
 * @param gregorian_ymd The gregorian date.
 * @returns The julian day number.
 */
double to_jd_algo1(const std::chrono::year_month_day& gregorian_ymd) {
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

  assert(gregorian_ymd.ok());
  
  const auto& [g_y, g_m, g_d] = util::from_ymd(gregorian_ymd);
  assert(g_y > 0);

  const uint32_t Y = (g_m <= 2) ? g_y - 1 : g_y;
  const uint32_t M = (g_m <= 2) ? g_m + 12 : g_m;
  const uint32_t D = g_d;

  const uint32_t A = Y / 100;
  const uint32_t B = A / 4;
  const uint32_t C = 2 - A + B;
  const uint32_t E = 365.25 * (Y + 4716);
  const uint32_t F = 30.6001 * (M + 1);
  const double JD = C + D + E + F - 1524.5;

  assert(JD > 0);
  return JD;
}


/**
 * @brief Converts a julian day number to gregorian date. Using algorithm 1.
 * @param jd The julian day number.
 * @returns The gregorian date.
 */
std::chrono::year_month_day from_jd_algo1(const double jd) {
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
  assert(1 <= day && day <= 31);

  const uint32_t month = (E > 13) ? (E - 13) : (E - 1);
  assert(1 <= month && month <= 12);

  const uint32_t year = (month <= 2) ? C - 4715 : C - 4716;
  assert(year > 0);
  
  const auto&& ymd = util::to_ymd(year, month, day);
  assert(ymd.ok());

  return ymd;
}

} // namespace calendar::julian_day

#endif // __CALENDAR_JULIAN_DAY_HPP__
