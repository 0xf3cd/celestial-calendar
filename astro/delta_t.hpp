// Copyright (c) 2024 Ningqi Wang (0xf3cd) <https://github.com/0xf3cd>
#pragma once

#include <array>
#include <cmath>
#include <format>
#include <ranges>
#include <cassert>
#include <optional>


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
 *  4. Delta T (△T) is the difference between Dynamical Time and Universal Time, i.e., △T = TD - UT1.
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
 * @param year The gregorian time. The year is actually a double.
 * @returns The delta T.
 * @ref https://www.cnblogs.com/qintangtao/archive/2013/03/04/2942245.html
 * @note The original algorithm takes integers as input. But I am using doubles here,
 *       with the hope of getting more accurate results.
 *       E.g., to get △T the last day of 2005, 2005.99 can be used as input.
 */
constexpr double compute(double year) {
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


#pragma region Algorithm 2

namespace algo2 {
// Algo2 ref: https://eclipse.gsfc.nasa.gov/SEcat5/deltatpoly.html

/**
 * @fn The function to compute △T of a given gregorian year and month, using algorithm 2.
 * @param year The gregorian year. The year is actually an integer.
 * @returns The delta T.
 * @ref https://eclipse.gsfc.nasa.gov/SEcat5/deltatpoly.html
 */
constexpr double compute(double year) {
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


#pragma region Algorithm 3

namespace algo3 {
// Algo3 ref: https://eclipsewise.com/help/deltatpoly2014.html

/**
 * @fn The function to compute △T of a given gregorian year and month, using algorithm 3.
 * @param year The gregorian year. The year is actually an integer.
 * @returns The delta T.
 * @ref https://eclipsewise.com/help/deltatpoly2014.html
 */
constexpr double compute(double year) {
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


#pragma region Algorithm 4

namespace algo4 {

// Algo4 is a polynomial model fitting the ΔT values.
// Model training: https://github.com/0xf3cd/AstroTime-Analysis/blob/main/DeltaT/models.ipynb
// The model is based on:
//   Year 2005.0 - 2024.5: IERS's bulletin A data (https://www.iers.org/IERS/EN/Publications/Bulletins/bulletins.html). 
//   Year 2024.5 - 2035.0: USNO's "Long-term" data (https://maia.usno.navy.mil/products/deltaT).

/**
 * @fn The function to compute △T of a given gregorian year and month, using algorithm 4.
 * @param year The gregorian year. The year is actually an integer.
 * @returns The delta T.
 * @ref Bulletin A data - https://www.iers.org/IERS/EN/Publications/Bulletins/bulletins.html
 * @ref USNO Long-term data - https://maia.usno.navy.mil/products/deltaT
 * @ref Models - https://github.com/0xf3cd/AstroTime-Analysis/blob/main/DeltaT/models.ipynb
 * @note For year < 2005.0, algo2 is used instead.
 * @note For 2005.0 <= year < 2024.0, poly model trained on Bulletin A data is used.
 * @note For 2024.0 <= year < 2035.0, poly model trained on USNO long-term data is used.
 */
constexpr double compute(double year) {
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


enum class Algorithm {
  Algo1,
  Algo2,
  Algo3,
  Algo4,
};

// As per the statistical analysis, algorithm 4 has the best accuracy.
constexpr double compute(double year, Algorithm algorithm = Algorithm::Algo4) {
  // TODO: Make this a if-constexpr?
  if (algorithm == Algorithm::Algo1) {
    return algo1::compute(year);
  }
  if (algorithm == Algorithm::Algo2) {
    return algo2::compute(year);
  }
  if (algorithm == Algorithm::Algo3) {
    return algo3::compute(year);
  }
  return algo4::compute(year);
}

} // namespace astro::delta_t
