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

#include <array>
#include <cmath>
#include <format>
#include <ranges>
#include <cassert>
#include <optional>

#include "ymd.hpp"
#include "datetime.hpp"

/**
 * This file contains the functions to compute the dynamical time.
 * 
 * Here are some concepts:
 *  1. Universal Time (UT) is a time standard based on Earth's rotation, derived from astronomical observations. 
 *     UT relies on the Earth's rotational period and cannot be kept by atomic clocks.
 *  2. Coordinated Universal Time (UTC) is the most common time standard used globally.
 *     UTC is based on International Atomic Time (TAI), and can be kept by atomic clocks.
 *     It is required that |UT1 - UTC| < 0.9 seconds, so leap seconds are introduced to adjust UTC (UT1 is a version of UT).
 *  3. Dynamical Time (TD) is a time scale used in astronomy, representing the time experienced by an observer on Earth's surface,
 *     accounting for relativistic effects and other factors. It is based on TAI as well.
 *     Terrestrial Dynamical Time (TDT) and Terrestrial Time (TT) are equivalent to TD.
 *     In this project, TT is used.
 *  4. Delta T (△T) is the difference between Dynamical Time and Universal Time, i.e., △T = TT - UT1.
 * 
 * References for above concepts:
 * - https://eclipse.gsfc.nasa.gov/LEcat5/time.html
 * - https://en.wikipedia.org/wiki/Coordinated_Universal_Time
 */


namespace astro::delta_t {

#pragma region Algorithm 1

namespace algo1 {
// Algo1 ref: https://www.cnblogs.com/qintangtao/archive/2013/03/04/2942245.html

/** @brief The coefficients of algorithm 1. */
struct Algo1Coefficients {
  int32_t year;
  double a, b, c, d; 
};

constexpr std::array<Algo1Coefficients, 20> ALGO1_COEFFICIENTS = {{
  { -4000, 108371.7, -13036.80, 392.000,  0.0000 },
  {  -500,  17201.0,   -627.82,  16.170, -0.3413 },
  {  -150,  12200.6,   -346.41,   5.403, -0.1593 },
  {   150,   9113.8,   -328.13,  -1.647,  0.0377 },
  {   500,   5707.5,   -391.41,   0.915,  0.3145 },
  {   900,   2203.4,   -283.45,  13.034, -0.1778 },
  {  1300,    490.1,    -57.35,   2.085, -0.0072 },
  {  1600,    120.0,     -9.81,  -1.532,  0.1403 },
  {  1700,     10.2,     -0.91,   0.510, -0.0370 },
  {  1800,     13.4,     -0.72,   0.202, -0.0193 },
  {  1830,      7.8,     -1.81,   0.416, -0.0247 },
  {  1860,      8.3,     -0.13,  -0.406,  0.0292 },
  {  1880,     -5.4,      0.32,  -0.183,  0.0173 },
  {  1900,     -2.3,      2.06,   0.169, -0.0135 },
  {  1920,     21.2,      1.69,  -0.304,  0.0167 },
  {  1940,     24.2,      1.22,  -0.064,  0.0031 },
  {  1960,     33.2,      0.51,   0.231, -0.0109 },
  {  1980,     51.0,      1.29,  -0.026,  0.0032 },
  {  2000,     63.87,     0.1,    0.0,    0.0    },
  {  2005,      0.0,      0.0,    0.0,    0.0    }, // This data point will not be used. So we don't care the values of a, b, c, d here.
}};


/** @brief Get the coefficients of algorithm 1. Returns `std::nullopt` if not found. */
constexpr std::optional<
  std::pair<Algo1Coefficients, Algo1Coefficients>
> 
find_coefficients(const int32_t year) {
  // TODO: Use `std::views::pairwise` when supported.
  const auto pairwise = [](const auto& range) {
    using namespace std::ranges;
    return views::iota(0, distance(range) - 1) | views::transform([&range](auto i) {
      return std::pair { *(begin(range) + i), *(begin(range) + i + 1) };
    });
  };

  for (const auto& [start, end] : pairwise(ALGO1_COEFFICIENTS)) {
    if (year >= start.year and year < end.year) {
      return std::pair { start, end };
    }
  }

  return std::nullopt;
}


/**
 * @brief The function to compute △T of a given gregorian year, using algorithm 1.
 * @param year The year, of double type.
 * @return The delta T.
 *
 * @throw std::out_of_range if the year is < -4000.
 * @example `compute(2005.99999999....)` returns the delta T for the last moment of year 2005.
 * @example `compute(1984.0)` returns the delta T for the first moment of year 1984.
 * 
 * @ref https://www.cnblogs.com/qintangtao/archive/2013/03/04/2942245.html
 * @note The original algorithm takes integers as input. But I am using doubles here,
 *       with the hope of getting more accurate results.
 */
constexpr double compute(const double year) {
  if (year < -4000) {
    throw std::out_of_range {
      std::vformat("Year {} is not supported by algorithm 1.", std::make_format_args(year))
    };
  }

  const auto coefficients = find_coefficients(static_cast<int32_t>(year));
  if (coefficients) {
    const auto& [start, end] = *coefficients;
    assert(year >= start.year and year < end.year);

    const double t1 = (year - start.year) / (end.year - start.year) * 10.0;
    const double t2 = t1 * t1;
    const double t3 = t2 * t1;

    return start.a + start.b * t1 + start.c * t2 + start.d * t3;
  }

  assert(year >= 2005);

  const auto F = [](double year) constexpr -> double {
    return 64.7 + (year - 2005) * 0.4;
  };

  const auto f = [](double year) constexpr -> double {
    return -20.0 + 31.0 * std::pow((year - 1820) / 100.0, 2);
  };

  if (year >= 2005 and year < 2015) {
    return F(year);
  }
  if (year >= 2015 and year < 2115) {
    return f(year) + (year - 2114) * (f(2014) - F(2014)) / 100.0;
  }
  // For years >= 2115, simply use `f`.
  return f(year);
}

} // namespace algo1
#pragma endregion


#pragma region Algorithm 2

namespace algo2 {
// Algo2 ref: https://eclipse.gsfc.nasa.gov/SEcat5/deltatpoly.html

/**
 * @brief The function to compute △T of a given gregorian year, using algorithm 2.
 * @param year The year, of double type.
 * @return The delta T.
 * 
 * @example `compute(2005.99999999....)` returns the delta T for the last moment of year 2005.
 * @example `compute(1984.0)` returns the delta T for the first moment of year 1984.
 * 
 * @ref https://eclipse.gsfc.nasa.gov/SEcat5/deltatpoly.html
 */
constexpr double compute(const double year) noexcept {
  if (year < -500) {
    const double u = (year - 1820) / 100.0;
    return -20.0 + 32.0 * std::pow(u, 2);
  }

  if (year >= -500 and year < 500) {
    const double u = year / 100.0;
    return 10583.6 - 1014.41 * u + 33.78311 * std::pow(u, 2)
         - 5.952053 * std::pow(u, 3) - 0.1798452 * std::pow(u, 4)
         + 0.022174192 * std::pow(u, 5) + 0.0090316521 * std::pow(u, 6);
  }

  if (year >= 500 and year < 1600) {
    const double u = (year - 1000) / 100.0;
    return 1574.2 - 556.01 * u + 71.23472 * std::pow(u, 2)
         + 0.319781 * std::pow(u, 3) - 0.8503463 * std::pow(u, 4)
         - 0.005050998 * std::pow(u, 5) + 0.0083572073 * std::pow(u, 6);
  }

  if (year >= 1600 and year < 1700) {
    const double t = year - 1600;
    return 120.0 - 0.9808 * t - 0.01532 * std::pow(t, 2)
         + std::pow(t, 3) / 7129.0;
  }

  if (year >= 1700 and year < 1800) {
    const double t = year - 1700;
    return 8.83 + 0.1603 * t - 0.0059285 * std::pow(t, 2)
         + 0.00013336 * std::pow(t, 3) - std::pow(t, 4) / 1174000.0;
  }

  if (year >= 1800 and year < 1860) {
    const double t = year - 1800;
    return 13.72 - 0.332447 * t + 0.0068612 * std::pow(t, 2)
         + 0.0041116 * std::pow(t, 3) - 0.00037436 * std::pow(t, 4)
         + 0.0000121272 * std::pow(t, 5) - 0.0000001699 * std::pow(t, 6)
         + 0.000000000875 * std::pow(t, 7);
  }

  if (year >= 1860 and year < 1900) {
    const double t = year - 1860;
    return 7.62 + 0.5737 * t - 0.251754 * std::pow(t, 2)
         + 0.01680668 * std::pow(t, 3) - 0.0004473624 * std::pow(t, 4)
         + std::pow(t, 5) / 233174.0;
  }

  if (year >= 1900 and year < 1920) {
    const double t = year - 1900;
    return -2.79 + 1.494119 * t - 0.0598939 * std::pow(t, 2)
         + 0.0061966 * std::pow(t, 3) - 0.000197 * std::pow(t, 4);
  }

  if (year >= 1920 and year < 1941) {
    const double t = year - 1920;
    return 21.20 + 0.84493 * t - 0.076100 * std::pow(t, 2)
         + 0.0020936 * std::pow(t, 3);
  }

  if (year >= 1941 and year < 1961) {
    const double t = year - 1950;
    return 29.07 + 0.407 * t - std::pow(t, 2) / 233.0
         + std::pow(t, 3) / 2547.0;
  }

  if (year >= 1961 and year < 1986) {
    const double t = year - 1975;
    return 45.45 + 1.067 * t - std::pow(t, 2) / 260.0
         - std::pow(t, 3) / 718.0;
  }

  if (year >= 1986 and year < 2005) {
    const double t = year - 2000;
    return 63.86 + 0.3345 * t - 0.060374 * std::pow(t, 2)
         + 0.0017275 * std::pow(t, 3) + 0.000651814 * std::pow(t, 4)
         + 0.00002373599 * std::pow(t, 5);
  }

  if (year >= 2005 and year < 2050) {
    const double t = year - 2000;
    return 62.92 + 0.32217 * t + 0.005589 * std::pow(t, 2);
  }

  if (year >= 2050 and year < 2150) {
    return -20.0 + 32.0 * std::pow((year - 1820) / 100.0, 2)
         - 0.5628 * (2150 - year);
  }

  // Otherwise, year is >= 2150.
  return -20.0 + 32 * std::pow((year - 1820) / 100.0, 2);
}

} // namespace algo2
#pragma endregion


#pragma region Algorithm 3

namespace algo3 {
// Algo3 ref: https://eclipsewise.com/help/deltatpoly2014.html

/**
 * @brief The function to compute △T of a given gregorian year, using algorithm 3.
 * @param year The year, of double type.
 * @return The delta T.
 * 
 * @throw std::out_of_range if the year is >= 3000.
 * @example `compute(2005.99999999....)` returns the delta T for the last moment of year 2005.
 * @example `compute(1984.0)` returns the delta T for the first moment of year 1984.
 * 
 * @ref https://eclipsewise.com/help/deltatpoly2014.html
 */
constexpr double compute(const double year) {
  if (year >= 3000) {
    throw std::out_of_range {
      std::vformat("Year {} is not supported by algorithm 3.", std::make_format_args(year))
    };
  }

  // For year < 2005, algo3 produces the same results as algo2.
  if (year < 2005) {
    return algo2::compute(year);
  }

  if (year >= 2005 and year < 2015) {
    const double t = year - 2005;
    return 64.69 + 0.2930 * t;
  }

  { // year >= 2015
    const double t = year - 2015;
    return 67.62 + 0.3645 * t + 0.0039755 * std::pow(t, 2);
  }
}

} // namespace algo3
#pragma endregion


#pragma region Algorithm 4

namespace algo4 {

// Algo4 is a polynomial model fitting the ΔT values.
// Model training: https://github.com/0xf3cd/AstroTime-Analysis/blob/main/DeltaT/models.ipynb
// The model is based on:
//   Year 2005.0 - 2024.5: IERS's bulletin A data (https://www.iers.org/IERS/EN/Publications/Bulletins/bulletins.html). 
//   Year 2024.5 - 2035.0: USNO's "Long-term" data (https://maia.usno.navy.mil/products/deltaT).

/**
 * @brief The function to compute △T of a given gregorian year, using algorithm 4.
 * @param year The year, of double type.
 * @return The delta T.
 *
 * @throw std::out_of_range if the year is >= 2035.
 * @example `compute(2005.99999999....)` returns the delta T for the last moment of year 2005.
 * @example `compute(1984.0)` returns the delta T for the first moment of year 1984.
 * 
 * @ref Bulletin A data - https://www.iers.org/IERS/EN/Publications/Bulletins/bulletins.html
 * @ref USNO Long-term data - https://maia.usno.navy.mil/products/deltaT
 * @ref Models - https://github.com/0xf3cd/AstroTime-Analysis/blob/main/DeltaT/models.ipynb
 * 
 * @note For year < 2005.0, algo2 is used instead.
 * @note For 2005.0 <= year < 2024.0, poly model trained on Bulletin A data is used.
 * @note For 2024.0 <= year < 2035.0, poly model trained on USNO long-term data is used.
 */
constexpr double compute(const double year) {
  if (year >= 2035) {
    throw std::out_of_range {
      std::format("The year {} is out of range for algorithm 4.", year)
    };
  }

  if (year < 2005) {
    return algo2::compute(year);
  }

  if (year >= 2005 and year < 2024) {
    const double u = year - 1990;
    return -1539.5103964825782 + 7305.087465383047 / u + 116.17205714035308 * u 
         - 1.1279910329686536 * std::pow(u, 2) - 0.2754809577876994 * std::pow(u, 3) + 0.01542796862306066 * std::pow(u, 4) 
         - 0.0003332548091334704 * std::pow(u, 5) + 2.6541070013360904e-06 * std::pow(u, 6);
  }

  { // year >= 2024
    const double u = year - 2020;
    return 73.38076003516039 - 4.199766017124573 / u - 1.3053623848472002 * u 
         + 0.14136771053009262 * std::pow(u, 2) - 0.004086715638812636 * std::pow(u, 3);
  }
}

} // namespace algo4
#pragma endregion


/**
 * @brief The function to compute △T of a given gregorian year.
 * @param year The year, of double type. The year has fractional part, indicating the time elapsed in the year.
 * @return The delta T, in seconds.
 * @details Algo 4 is used, because it is the most accurate one.
 * @note Since Algo 4 is used, it throws std::out_of_range if the year is >= 2035.
 * @example `compute(2005.99999999....)` returns the delta T for the last moment of year 2005.
 * @example `compute(1984.0)` returns the delta T for the first moment of year 1984.
 * @example `compute(2015.5)` returns the delta T for the middle moment of year 2015 (roughly June 30/July 1).
 */
constexpr double compute(const double year) {
  return algo4::compute(year);
}


/**
 * @brief The function to compute △T of a given calendar datetime (UT1).
 * @param ut1_dt The calendar datetime (UT1).
 * @return The delta T, in seconds.
 * @overload This is a overload function of `compute(const double year)`.
 * @note Some caller are passing `calendar::Datetime` in UT1, and some are passing `calendar::Datetime` in TT.
 *       Considering the longness of a year, the difference between `UT1` and `TT` is ignored.
 */
constexpr double compute(const calendar::Datetime& ut1_dt) {
  using namespace std::chrono;
  using namespace util::ymd_operator;

  // Get the gregorian year.
  const auto& ut1_ymd = ut1_dt.ymd;
  const auto [ut1_year, _, __] = util::from_ymd(ut1_ymd);
  
  // Calculate how many days have already passed since in this year.
  const int32_t past_days = ut1_ymd - util::to_ymd(ut1_year, 1, 1);

  // Calculate how many days are in this year.
  const int32_t total_days = util::to_ymd(ut1_year + 1, 1, 1) - util::to_ymd(ut1_year, 1, 1);

  // Convert to a double, representing the fraction/percentage of the past time in the year.
  const double day_fraction = ut1_dt.fraction();
  const double year_fraction = (day_fraction + past_days) / total_days;

  return compute(ut1_year + year_fraction);
}


/**
 * @brief Convert a `calendar::Datetime` in UT1 to a new `calendar::Datetime` in TT.
 * @param ut1_dt The datetime in UT1.
 * @return The datetime in TT.
 */
constexpr calendar::Datetime ut1_to_tt(const calendar::Datetime& ut1_dt) {
  using namespace util::ymd_operator;
  using namespace std::chrono;

  // Calculate the delta T.
  const double delta_t = compute(ut1_dt);

  // As per the formula, TT = UT1 + ΔT.
  // Calculate the day fraction in TT, using the above ΔT.
  const double tt_day_fraction = ut1_dt.fraction()                          // UT1 day fraction
                               + (delta_t / calendar::in_a_day<seconds>()); // Fraction of ΔT in a day.

  // After adjustment, `tt_day_fraction` can be out of the range of [0.0, 1.0).

  // Find out how many days to add or substract. `days` can be either positive or negative.
  const int32_t days = static_cast<int32_t>(std::floor(tt_day_fraction));
  
  // Correct the fraction to be in the range of [0.0, 1.0).
  const double real_fraction = tt_day_fraction - days;

  // Finally return the result.
  return calendar::Datetime { ut1_dt.ymd + days, real_fraction };
}


/**
 * @brief Convert a `calendar::Datetime` in TT to a new `calendar::Datetime` in UT1.
 * @param tt_dt The datetime in TT.
 * @return The datetime in UT1.
 */
constexpr calendar::Datetime tt_to_ut1(const calendar::Datetime& tt_dt) {
  using namespace util::ymd_operator;
  using namespace std::chrono;

  // Calculate the delta T.
  const double delta_t = compute(tt_dt);

  // As per the formula, UT1 = TT - ΔT.
  // Calculate the day fraction in UT1, using the above ΔT.
  const double ut1_day_fraction = tt_dt.fraction()                           // TT day fraction
                                - (delta_t / calendar::in_a_day<seconds>()); // Fraction of ΔT in a day.

  // After adjustment, `ut1_day_fraction` can be out of the range of [0.0, 1.0).

  // Find out how many days to add or substract. `days` can be either positive or negative.
  const int32_t days = static_cast<int32_t>(std::floor(ut1_day_fraction));
  
  // Correct the fraction to be in the range of [0.0, 1.0).
  const double real_fraction = ut1_day_fraction - days;

  // Finally return the result.
  return calendar::Datetime { tt_dt.ymd + days, real_fraction };
}

} // namespace astro::delta_t
