// Copyright (c) 2024 Ningqi Wang (0xf3cd) <https://github.com/0xf3cd>
#pragma once

#include <array>
#include <cmath>
#include <format>
#include <ranges>
#include <cassert>
#include <optional>

namespace calendar::dynamical_time {

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
 *     In this project, TD is used.
 *  4. Delta T (△T) is the difference between Dynamical Time and Universal Time, i.e., △T = TD - UT.
 * 
 * References for above concepts:
 * - https://eclipse.gsfc.nasa.gov/LEcat5/time.html
 * - https://en.wikipedia.org/wiki/Coordinated_Universal_Time
 */


#pragma region Delta T

namespace delta_t {

namespace algo1 {
// Algo1 ref: https://www.cnblogs.com/qintangtao/archive/2013/03/04/2942245.html
//            https://blog.sciencenet.cn/blog-255662-604045.html

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
  {  2000,     63.87,     0.1,    0,      0      },
  {  2005,     64.0,      0,      0,      0      }, // This data point will not be used. So we don't care the values of a, b, c, d here.
}};


/** @brief Get the coefficients of algorithm 1. Returns `std::nullopt` if not found. */
constexpr std::optional<
  std::pair<Algo1Coefficients, Algo1Coefficients>
> 
find_coefficients(int32_t year) {
  // TODO: Use `std::views::pairwise` when supported.
  const auto pairwise = [](auto&& range) {
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
 * @fn The function to compute △T of a given gregorian year, using algorithm 1.
 * @param year The gregorian year.
 * @returns The delta T.
 * @ref https://www.cnblogs.com/qintangtao/archive/2013/03/04/2942245.html
 */
constexpr double compute(int32_t year) {
  if (year < -4000) {
    throw std::out_of_range {
      std::vformat("Year {} is not supported by algorithm 1.", std::make_format_args(year))
    };
  }

  const auto&& coefficients = find_coefficients(year);
  if (coefficients) {
    const auto& [ start, end ] = *coefficients;
    assert(year >= start.year and year < end.year);

    const double t1 = static_cast<double>(year - start.year) / (end.year - start.year) * 10.0;
    const double t2 = t1 * t1;
    const double t3 = t2 * t1;

    return start.a + start.b * t1 + start.c * t2 + start.d * t3;
  }

  assert(year >= 2005);

  const auto F = [](int32_t year) constexpr -> double {
    return 64.7 + (year - 2005) * 0.4;
  };

  const auto f = [](int32_t year) constexpr -> double {
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

/**
 * @enum Algorithms to 
 */
enum class Algorithm {
  Algo1,
};

} // namespace delta_t

} // namespace calendar::dynamical_time
