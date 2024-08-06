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
#include <cmath>
#include <ranges>
#include <numeric>

#include "toolbox.hpp"

namespace astro::elp2000_82b::coeff {

// This namespace is mostly used to store the coefficients of the truncated ELP2000-82B model.
// Ref: Astronomical Algorithms, Jean Meeus, 1998, Chapter 47.
//
// As indicated in the book, the model is truncated for the sake of simplicity.
// So the results are not exactly the same as the original ELP2000-82B model.
// For better accuracy, please refer to Chapront's Lunar Tables and Programs, and the original ELP2000-82B model.

/** @brief Represents a coefficient term in the truncated ELP2000-82B model, used to calculate Longitude and Radius. */
struct LRCoefficients {
  int32_t D;
  int32_t M;
  int32_t Mp;
  int32_t F;
  int32_t argL;
  int32_t argR;
};

/** @brief Represents a coefficient term in the truncated ELP2000-82B model, used to calculate Latitude. */
struct BCoefficients {
  int32_t D;
  int32_t M;
  int32_t Mp;
  int32_t F;
  int32_t argB;
};

/** @brief Represents coefficients of the LR table. */
constexpr std::array<LRCoefficients, 60> LR {{
  { 0,  0,  1,  0, 6288774, -20905355 },
  { 2,  0, -1,  0, 1274027,  -3699111 },
  { 2,  0,  0,  0,  658314,  -2955968 },
  { 0,  0,  2,  0,  213618,   -569925 },
  { 0,  1,  0,  0, -185116,     48888 },
  { 0,  0,  0,  2, -114332,     -3149 },
  { 2,  0, -2,  0,   58793,    246158 },
  { 2, -1, -1,  0,   57066,   -152138 },
  { 2,  0,  1,  0,   53322,   -170733 },
  { 2, -1,  0,  0,   45758,   -204586 },
  { 0,  1, -1,  0,  -40923,   -129620 },
  { 1,  0,  0,  0,  -34720,    108743 },
  { 0,  1,  1,  0,  -30383,    104755 },
  { 2,  0,  0, -2,   15327,     10321 },
  { 0,  0,  1,  2,  -12528,         0 },
  { 0,  0,  1, -2,   10980,     79661 },
  { 4,  0, -1,  0,   10675,    -34782 },
  { 0,  0,  3,  0,   10034,    -23210 },
  { 4,  0, -2,  0,    8548,    -21636 },
  { 2,  1, -1,  0,   -7888,     24208 },
  { 2,  1,  0,  0,   -6766,     30824 },
  { 1,  0, -1,  0,   -5163,     -8379 },
  { 1,  1,  0,  0,    4987,    -16675 },
  { 2, -1,  1,  0,    4036,    -12831 },
  { 2,  0,  2,  0,    3994,    -10445 },
  { 4,  0,  0,  0,    3861,    -11650 },
  { 2,  0, -3,  0,    3665,     14403 },
  { 0,  1, -2,  0,   -2689,     -7003 },
  { 2,  0, -1,  2,   -2602,         0 },
  { 2, -1, -2,  0,    2390,     10056 },
  { 1,  0,  1,  0,   -2348,      6322 },
  { 2, -2,  0,  0,    2236,     -9884 },
  { 0,  1,  2,  0,   -2120,      5751 },
  { 0,  2,  0,  0,   -2069,         0 },
  { 2, -2, -1,  0,    2048,     -4950 },
  { 2,  0,  1, -2,   -1773,      4130 },
  { 2,  0,  0,  2,   -1595,         0 },
  { 4, -1, -1,  0,    1215,     -3958 },
  { 0,  0,  2,  2,   -1110,         0 },
  { 3,  0, -1,  0,    -892,      3258 },
  { 2,  1,  1,  0,    -810,      2616 },
  { 4, -1, -2,  0,     759,     -1897 },
  { 0,  2, -1,  0,    -713,     -2117 },
  { 2,  2, -1,  0,    -700,      2354 },
  { 2,  1, -2,  0,     691,         0 },
  { 2, -1,  0, -2,     596,         0 },
  { 4,  0,  1,  0,     549,     -1423 },
  { 0,  0,  4,  0,     537,     -1117 },
  { 4, -1,  0,  0,     520,     -1571 },
  { 1,  0, -2,  0,    -487,     -1739 },
  { 2,  1,  0, -2,    -399,         0 },
  { 0,  0,  2, -2,    -381,     -4421 },
  { 1,  1,  1,  0,     351,         0 },
  { 3,  0, -2,  0,    -340,         0 },
  { 4,  0, -3,  0,     330,         0 },
  { 2, -1,  2,  0,     327,         0 },
  { 0,  2,  1,  0,    -323,      1165 },
  { 1,  1, -1,  0,     299,         0 },
  { 2,  0,  3,  0,     294,         0 },
  { 2,  0, -1, -2,       0,      8752 },
}};

/** @brief Represents coefficients of the B table. */
constexpr std::array<BCoefficients, 60> B {{
  { 0,  0,  0,  1, 5128122 },
  { 0,  0,  1,  1,  280602 },
  { 0,  0,  1, -1,  277693 },
  { 2,  0,  0, -1,  173237 },
  { 2,  0, -1,  1,   55413 },
  { 2,  0, -1, -1,   46271 },
  { 2,  0,  0,  1,   32573 },
  { 0,  0,  2,  1,   17198 },
  { 2,  0,  1, -1,    9266 },
  { 0,  0,  2, -1,    8822 },
  { 2, -1,  0, -1,    8216 },
  { 2,  0, -2, -1,    4324 },
  { 2,  0,  1,  1,    4200 },
  { 2,  1,  0, -1,   -3359 },
  { 2, -1, -1,  1,    2463 },
  { 2, -1,  0,  1,    2211 },
  { 2, -1, -1, -1,    2065 },
  { 0,  1, -1, -1,   -1870 },
  { 4,  0, -1, -1,    1828 },
  { 0,  1,  0,  1,   -1794 },
  { 0,  0,  0,  3,   -1749 },
  { 0,  1, -1,  1,   -1565 },
  { 1,  0,  0,  1,   -1491 },
  { 0,  1,  1,  1,   -1475 },
  { 0,  1,  1, -1,   -1410 },
  { 0,  1,  0, -1,   -1344 },
  { 1,  0,  0, -1,   -1335 },
  { 0,  0,  3,  1,    1107 },
  { 4,  0,  0, -1,    1021 },
  { 4,  0, -1,  1,     833 },
  { 0,  0,  1, -3,     777 },
  { 4,  0, -2,  1,     671 },
  { 2,  0,  0, -3,     607 },
  { 2,  0,  2, -1,     596 },
  { 2, -1,  1, -1,     491 },
  { 2,  0, -2,  1,    -451 },
  { 0,  0,  3, -1,     439 },
  { 2,  0,  2,  1,     422 },
  { 2,  0, -3, -1,     421 },
  { 2,  1, -1,  1,    -366 },
  { 2,  1,  0,  1,    -351 },
  { 4,  0,  0,  1,     331 },
  { 2, -1,  1,  1,     315 },
  { 2, -2,  0, -1,     302 },
  { 0,  0,  1,  3,    -283 },
  { 2,  1,  1, -1,    -229 },
  { 1,  1,  0, -1,     223 },
  { 1,  1,  0,  1,     223 },
  { 0,  1, -2, -1,    -220 },
  { 2,  1, -1, -1,    -220 },
  { 1,  0,  1,  1,    -185 },
  { 2, -1, -2, -1,     181 },
  { 0,  1,  2,  1,    -177 },
  { 4,  0, -2, -1,     176 },
  { 4, -1, -1, -1,     166 },
  { 1,  0,  1, -1,    -164 },
  { 4,  0,  1, -1,     132 },
  { 1,  0, -1, -1,    -119 },
  { 4, -1,  0, -1,     115 },
  { 2, -2,  0,  1,     107 },
}};

} // namespace astro::elp2000_82b::coeff


namespace astro::elp2000_82b {

using coeff::LR;
using coeff::B;

using toolbox::Angle;
using toolbox::AngleUnit::DEG;
using toolbox::AngleUnit::RAD;


/**
 * @brief The scaling factor used in the Jean Meeus's Astronomical Algorithms, for the longitude and latitude.
 *        The unit of the raw evaluation result of longitude and latitude is 0.000001 degree.
 */
constexpr double LON_LAT_SCALING_FACTOR = 1e6;

/**
 * @brief The scaling factor used in the Jean Meeus's Astronomical Algorithms, for the radius/distance.
 *        The unit of the raw evaluation result of radius/distance is 0.001 kilometer.
 */
constexpr double RADIUS_SCALING_FACTOR = 1e3;

/**
 * @struct The context (arguments) for a given julian century.
 * @note This struct is expected to only hold the arguments from the ELP2000-82B model.
 */
struct Context {
  double     jc; // The julian century

  Angle<DEG> Lp; // The argument for the mean longitude of the Moon from the Sun
  Angle<DEG> D;  // The argument for the mean elongation of the Moon from the Sun
  Angle<DEG> M;  // The argument for the mean anomaly of the Sun
  Angle<DEG> Mp; // The argument for the mean anomaly of the Moon
  Angle<DEG> F;  // The argument for the distance of the Moon from its Ascending Node
  Angle<DEG> A1; // Further argument 1, as indicated in Astronomical Algorithms, Jean Meeus, 1998, Chapter 47
  Angle<DEG> A2; // Further argument 2, as indicated in Astronomical Algorithms, Jean Meeus, 1998, Chapter 47
  Angle<DEG> A3; // Further argument 3, as indicated in Astronomical Algorithms, Jean Meeus, 1998, Chapter 47
  double E;      // The argument for the eccentricity of the Earth's orbit around the Sun
};


/**
 * @brief Create context for the given julian century.
 * @param jc The julian century.
 * @return The created context.
 * @see Astronomical Algorithms, Jean Meeus, 1998, Chapter 47.
 */
inline auto create_context(const double jc) -> Context {
  const double jc2 = jc * jc;
  const double jc3 = jc2 * jc;
  const double jc4 = jc3 * jc;

  const Angle<DEG> Lp { 218.3164477 + 481267.88123421 * jc - 0.0015786 * jc2 + jc3 / 538841 - jc4 / 65194000 };
  const Angle<DEG> D  { 297.8501921 + 445267.1114034 * jc - 0.0018819 * jc2 + jc3 / 545868 - jc4 / 113065000 };
  const Angle<DEG> M  { 357.5291092 + 35999.0502909 * jc - 0.0001536 * jc2 - jc3 / 24490000 };
  const Angle<DEG> Mp { 134.9633964 + 477198.8675055 * jc + 0.0087414 * jc2 + jc3 / 69699 - jc4 / 147120000 };
  const Angle<DEG> F  { 93.2720950 + 483202.0175233 * jc - 0.0036539 * jc2 - jc3 / 3526000 + jc4 / 863310000 };

  const Angle<DEG> A1 { 119.75 + 131.849 * jc };
  const Angle<DEG> A2 { 53.09 + 479264.290 * jc };
  const Angle<DEG> A3 { 313.45 + 481266.484 * jc };

  const double E = 1 - 0.002516 * jc - 0.0000074 * jc2;

  return {
    .jc = jc,
    .Lp = Lp.normalize(),
    .D  = D.normalize(),
    .M  = M.normalize(),
    .Mp = Mp.normalize(),
    .F  = F.normalize(),
    .A1 = A1.normalize(),
    .A2 = A2.normalize(),
    .A3 = A3.normalize(),
    .E  = E
  };
};


/**
 * @struct The result of the ELP2000-82B evaluation.
 */
struct Evaluation {
  double Σl; // Unit is 0.000001 degrees
  double Σb; // Unit is 0.000001 degrees
  double Σr; // Unit is 0.001 kilometers

  Context ctx; // The context for the Jean Meeus's algorithm
};


/**
 * @brief Evaluate ELP2000-82B on the given parameters.
 * @param jc The julian century.
 * @return The evaluated result.
 * @see Astronomical Algorithms, Jean Meeus, 1998, Chapter 47.
 */
inline auto evaluate(const double jc) -> Evaluation {
  using namespace std::ranges;

  const auto ctx = create_context(jc);

  // Calculate the longitude periodic terms.
  const auto lon_terms = LR | views::transform([&](const coeff::LRCoefficients& coeff) {
    const Angle<DEG> θ {
      coeff.D  * ctx.D.deg()  +
      coeff.M  * ctx.M.deg()  +
      coeff.Mp * ctx.Mp.deg() +
      coeff.F  * ctx.F.deg()
    };

    const auto M_correction = std::pow(ctx.E, std::abs(coeff.M));
    return coeff.argL * std::sin(θ.rad()) * M_correction;
  });

  // Calculate the distance/radius periodic terms.
  const auto rad_terms = LR | views::transform([&](const coeff::LRCoefficients& coeff) {
    const Angle<DEG> θ {
      coeff.D  * ctx.D.deg()  +
      coeff.M  * ctx.M.deg()  +
      coeff.Mp * ctx.Mp.deg() +
      coeff.F  * ctx.F.deg()
    };

    const auto M_correction = std::pow(ctx.E, std::abs(coeff.M));
    return coeff.argR * std::cos(θ.rad()) * M_correction;
  });

  // Calculate the latitude periodic terms.
  const auto lat_terms = B | views::transform([&](const coeff::BCoefficients& coeff) {
    const Angle<DEG> θ {
      coeff.D  * ctx.D.deg()  +
      coeff.M  * ctx.M.deg()  +
      coeff.Mp * ctx.Mp.deg() +
      coeff.F  * ctx.F.deg()
    };

    const auto M_correction = std::pow(ctx.E, std::abs(coeff.M));
    return coeff.argB * std::sin(θ.rad()) * M_correction;
  });

  return {
    .Σl  = std::reduce(cbegin(lon_terms), cend(lon_terms)),
    .Σb  = std::reduce(cbegin(lat_terms), cend(lat_terms)),
    .Σr  = std::reduce(cbegin(rad_terms), cend(rad_terms)),
    .ctx = ctx
  };
};

} // namespace astro::elp2000_82b
