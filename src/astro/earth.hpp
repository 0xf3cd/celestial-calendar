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

#include <span>
#include <array>
#include <ranges>
#include <functional>

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "vsop87d/vsop87d.hpp"


namespace astro::earth {

using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;
using astro::toolbox::AngleUnit::RAD;
using astro::toolbox::SphericalCoordinate;

using astro::vsop87d::Planet;

}


namespace astro::earth::heliocentric_coord {

/**
 * @brief Calculate the heliocentric position of the Earth, using VSOP87D.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The heliocentric ecliptic position of the Earth, calculated using VSOP87D.
 */
inline auto vsop87d(const double jde) -> SphericalCoordinate {
  const double jm = astro::julian_day::jde_to_jm(jde);
  const auto evaluated = astro::vsop87d::evaluate<Planet::EAR>(jm);

  return {
    // As per the algorithm, the longitude is normalized to [0, 2π).
    .λ = Angle<RAD>(evaluated.λ).normalize(),
    .β = Angle<RAD>(evaluated.β),
    .r = evaluated.r
  };
}

} // namespace astro::earth::heliocentric_coord


namespace astro::earth::nutation {

// Nutation is a periodic oscillation of the axis of rotation of a rotating body, such as Earth, that superimposes on its precessional motion.
// For Earth, this means that while its axis precesses (moves in a slow, conical motion) due to gravitational forces exerted by the Moon and the Sun, 
// there are also smaller, shorter-term variations in the tilt of the axis known as nutation.

struct θCoeffs {
  int32_t D;
  int32_t M;
  int32_t Mp;
  int32_t F;
  int32_t Ω;
};

struct ψCoeffs {
  double coeff1;
  double coeff2;
};

struct εCoeffs {
  double coeff1;
  double coeff2;
};

struct NutationCoeffs {
  θCoeffs θ;  // Including D, M, Mp, F, Ω (Meeus's expressions); or, l,l',F,D,Om (IAU 1980's expressions).
  ψCoeffs Δψ; // Coefficients for the Earth's nutation in longitude.
  εCoeffs Δε; // Coefficients for the Earth's nutation in obliquity.
};

// The following data was collected from Jean Meeus, "Astronomical Algorithms", 2nd ed, Table 22.A in Ch. 22.
// This table is based on IAU 1980 nutation model, and some terms are omitted.
constexpr std::array<NutationCoeffs, 63> MEEUS_NUTATION_COEFFS {{
  { {  0,  0,  0,  0,  1 }, { -171996.0, -174.2 }, { 92025.0,  8.9 } },
  { { -2,  0,  0,  2,  2 }, {  -13187.0,   -1.6 }, {  5736.0, -3.1 } },
  { {  0,  0,  0,  2,  2 }, {   -2274.0,   -0.2 }, {   977.0, -0.5 } },
  { {  0,  0,  0,  0,  2 }, {    2062.0,    0.2 }, {  -895.0,  0.5 } },
  { {  0,  1,  0,  0,  0 }, {    1426.0,   -3.4 }, {    54.0, -0.1 } },
  { {  0,  0,  1,  0,  0 }, {     712.0,    0.1 }, {    -7.0,  0.0 } },
  { { -2,  1,  0,  2,  2 }, {    -517.0,    1.2 }, {   224.0, -0.6 } },
  { {  0,  0,  0,  2,  1 }, {    -386.0,   -0.4 }, {   200.0,  0.0 } },
  { {  0,  0,  1,  2,  2 }, {    -301.0,    0.0 }, {   129.0, -0.1 } },
  { { -2, -1,  0,  2,  2 }, {     217.0,   -0.5 }, {   -95.0,  0.3 } },
  { { -2,  0,  1,  0,  0 }, {    -158.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  0,  2,  1 }, {     129.0,    0.1 }, {   -70.0,  0.0 } },
  { {  0,  0, -1,  2,  2 }, {     123.0,    0.0 }, {   -53.0,  0.0 } },
  { {  2,  0,  0,  0,  0 }, {      63.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  1,  0,  1 }, {      63.0,    0.1 }, {   -33.0,  0.0 } },
  { {  2,  0, -1,  2,  2 }, {     -59.0,    0.0 }, {    26.0,  0.0 } },
  { {  0,  0, -1,  0,  1 }, {     -58.0,   -0.1 }, {    32.0,  0.0 } },
  { {  0,  0,  1,  2,  1 }, {     -51.0,    0.0 }, {    27.0,  0.0 } },
  { { -2,  0,  2,  0,  0 }, {      48.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0, -2,  2,  1 }, {      46.0,    0.0 }, {   -24.0,  0.0 } },
  { {  2,  0,  0,  2,  2 }, {     -38.0,    0.0 }, {    16.0,  0.0 } },
  { {  0,  0,  2,  2,  2 }, {     -31.0,    0.0 }, {    13.0,  0.0 } },
  { {  0,  0,  2,  0,  0 }, {      29.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  1,  2,  2 }, {      29.0,    0.0 }, {   -12.0,  0.0 } },
  { {  0,  0,  0,  2,  0 }, {      26.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  0,  2,  0 }, {     -22.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0, -1,  2,  1 }, {      21.0,    0.0 }, {   -10.0,  0.0 } },
  { {  0,  2,  0,  0,  0 }, {      17.0,   -0.1 }, {     0.0,  0.0 } },
  { {  2,  0, -1,  0,  1 }, {      16.0,    0.0 }, {    -8.0,  0.0 } },
  { { -2,  2,  0,  2,  2 }, {     -16.0,    0.1 }, {     7.0,  0.0 } },
  { {  0,  1,  0,  0,  1 }, {     -15.0,    0.0 }, {     9.0,  0.0 } },
  { { -2,  0,  1,  0,  1 }, {     -13.0,    0.0 }, {     7.0,  0.0 } },
  { {  0, -1,  0,  0,  1 }, {     -12.0,    0.0 }, {     6.0,  0.0 } },
  { {  0,  0,  2, -2,  0 }, {      11.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  0, -1,  2,  1 }, {     -10.0,    0.0 }, {     5.0,  0.0 } },
  { {  2,  0,  1,  2,  2 }, {      -8.0,    0.0 }, {     3.0,  0.0 } },
  { {  0,  1,  0,  2,  2 }, {       7.0,    0.0 }, {    -3.0,  0.0 } },
  { { -2,  1,  1,  0,  0 }, {      -7.0,    0.0 }, {     0.0,  0.0 } },
  { {  0, -1,  0,  2,  2 }, {      -7.0,    0.0 }, {     3.0,  0.0 } },
  { {  2,  0,  0,  2,  1 }, {      -7.0,    0.0 }, {     3.0,  0.0 } },
  { {  2,  0,  1,  0,  0 }, {       6.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  2,  2,  2 }, {       6.0,    0.0 }, {    -3.0,  0.0 } },
  { { -2,  0,  1,  2,  1 }, {       6.0,    0.0 }, {    -3.0,  0.0 } },
  { {  2,  0, -2,  0,  1 }, {      -6.0,    0.0 }, {     3.0,  0.0 } },
  { {  2,  0,  0,  0,  1 }, {      -6.0,    0.0 }, {     3.0,  0.0 } },
  { {  0, -1,  1,  0,  0 }, {       5.0,    0.0 }, {     0.0,  0.0 } },
  { { -2, -1,  0,  2,  1 }, {      -5.0,    0.0 }, {     3.0,  0.0 } },
  { { -2,  0,  0,  0,  1 }, {      -5.0,    0.0 }, {     3.0,  0.0 } },
  { {  0,  0,  2,  2,  1 }, {      -5.0,    0.0 }, {     3.0,  0.0 } },
  { { -2,  0,  2,  0,  1 }, {       4.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  1,  0,  2,  1 }, {       4.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  1, -2,  0 }, {       4.0,    0.0 }, {     0.0,  0.0 } },
  { { -1,  0,  1,  0,  0 }, {      -4.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  1,  0,  0,  0 }, {      -4.0,    0.0 }, {     0.0,  0.0 } },
  { {  1,  0,  0,  0,  0 }, {      -4.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  1,  2,  0 }, {       3.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0, -2,  2,  2 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { { -1, -1,  1,  0,  0 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  1,  1,  0,  0 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { {  0, -1,  1,  2,  2 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { {  2, -1, -1,  2,  2 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  3,  2,  2 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { {  2, -1,  0,  2,  2 }, {      -3.0,    0.0 }, {     0.0,  0.0 } }
}};


// The following IAU 1980 Nutation Model data was collected from https://www.iausofa.org/2021_0512_C/sofa/nut80.c.
// Compared to Meeus's omitted version, this table contains all terms.
constexpr std::array<NutationCoeffs, 106> IAU1980_NUTATION_COEFFS {{
  { {  0,  0,  0,  0,  1 }, { -171996.0, -174.2 }, { 92025.0,  8.9 } },
  { {  0,  0,  0,  0,  2 }, {    2062.0,    0.2 }, {  -895.0,  0.5 } },
  { {  0,  0, -2,  2,  1 }, {      46.0,    0.0 }, {   -24.0,  0.0 } },
  { {  0,  0,  2, -2,  0 }, {      11.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0, -2,  2,  2 }, {      -3.0,    0.0 }, {     1.0,  0.0 } },
  { { -1, -1,  1,  0,  0 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { { -2, -2,  0,  2,  1 }, {      -2.0,    0.0 }, {     1.0,  0.0 } },
  { {  0,  0,  2, -2,  1 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  0,  2,  2 }, {  -13187.0,   -1.6 }, {  5736.0, -3.1 } },
  { {  0,  1,  0,  0,  0 }, {    1426.0,   -3.4 }, {    54.0, -0.1 } },
  { { -2,  1,  0,  2,  2 }, {    -517.0,    1.2 }, {   224.0, -0.6 } },
  { { -2, -1,  0,  2,  2 }, {     217.0,   -0.5 }, {   -95.0,  0.3 } },
  { { -2,  0,  0,  2,  1 }, {     129.0,    0.1 }, {   -70.0,  0.0 } },
  { { -2,  0,  2,  0,  0 }, {      48.0,    0.0 }, {     1.0,  0.0 } },
  { { -2,  0,  0,  2,  0 }, {     -22.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  2,  0,  0,  0 }, {      17.0,   -0.1 }, {     0.0,  0.0 } },
  { {  0,  1,  0,  0,  1 }, {     -15.0,    0.0 }, {     9.0,  0.0 } },
  { { -2,  2,  0,  2,  2 }, {     -16.0,    0.1 }, {     7.0,  0.0 } },
  { {  0, -1,  0,  0,  1 }, {     -12.0,    0.0 }, {     6.0,  0.0 } },
  { {  2,  0, -2,  0,  1 }, {      -6.0,    0.0 }, {     3.0,  0.0 } },
  { { -2, -1,  0,  2,  1 }, {      -5.0,    0.0 }, {     3.0,  0.0 } },
  { { -2,  0,  2,  0,  1 }, {       4.0,    0.0 }, {    -2.0,  0.0 } },
  { { -2,  1,  0,  2,  1 }, {       4.0,    0.0 }, {    -2.0,  0.0 } },
  { { -1,  0,  1,  0,  0 }, {      -4.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  1,  2,  0,  0 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  0,  0, -2,  1 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  1,  0, -2,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  1,  0,  0,  2 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { {  1,  0, -1,  0,  1 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  1,  0,  2,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  0,  2,  2 }, {   -2274.0,   -0.2 }, {   977.0, -0.5 } },
  { {  0,  0,  1,  0,  0 }, {     712.0,    0.1 }, {    -7.0,  0.0 } },
  { {  0,  0,  0,  2,  1 }, {    -386.0,   -0.4 }, {   200.0,  0.0 } },
  { {  0,  0,  1,  2,  2 }, {    -301.0,    0.0 }, {   129.0, -0.1 } },
  { { -2,  0,  1,  0,  0 }, {    -158.0,    0.0 }, {    -1.0,  0.0 } },
  { {  0,  0, -1,  2,  2 }, {     123.0,    0.0 }, {   -53.0,  0.0 } },
  { {  2,  0,  0,  0,  0 }, {      63.0,    0.0 }, {    -2.0,  0.0 } },
  { {  0,  0,  1,  0,  1 }, {      63.0,    0.1 }, {   -33.0,  0.0 } },
  { {  0,  0, -1,  0,  1 }, {     -58.0,   -0.1 }, {    32.0,  0.0 } },
  { {  2,  0, -1,  2,  2 }, {     -59.0,    0.0 }, {    26.0,  0.0 } },
  { {  0,  0,  1,  2,  1 }, {     -51.0,    0.0 }, {    27.0,  0.0 } },
  { {  2,  0,  0,  2,  2 }, {     -38.0,    0.0 }, {    16.0,  0.0 } },
  { {  0,  0,  2,  0,  0 }, {      29.0,    0.0 }, {    -1.0,  0.0 } },
  { { -2,  0,  1,  2,  2 }, {      29.0,    0.0 }, {   -12.0,  0.0 } },
  { {  0,  0,  2,  2,  2 }, {     -31.0,    0.0 }, {    13.0,  0.0 } },
  { {  0,  0,  0,  2,  0 }, {      26.0,    0.0 }, {    -1.0,  0.0 } },
  { {  0,  0, -1,  2,  1 }, {      21.0,    0.0 }, {   -10.0,  0.0 } },
  { {  2,  0, -1,  0,  1 }, {      16.0,    0.0 }, {    -8.0,  0.0 } },
  { { -2,  0,  1,  0,  1 }, {     -13.0,    0.0 }, {     7.0,  0.0 } },
  { {  2,  0, -1,  2,  1 }, {     -10.0,    0.0 }, {     5.0,  0.0 } },
  { { -2,  1,  1,  0,  0 }, {      -7.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  1,  0,  2,  2 }, {       7.0,    0.0 }, {    -3.0,  0.0 } },
  { {  0, -1,  0,  2,  2 }, {      -7.0,    0.0 }, {     3.0,  0.0 } },
  { {  2,  0,  1,  2,  2 }, {      -8.0,    0.0 }, {     3.0,  0.0 } },
  { {  2,  0,  1,  0,  0 }, {       6.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  2,  2,  2 }, {       6.0,    0.0 }, {    -3.0,  0.0 } },
  { {  2,  0,  0,  0,  1 }, {      -6.0,    0.0 }, {     3.0,  0.0 } },
  { {  2,  0,  0,  2,  1 }, {      -7.0,    0.0 }, {     3.0,  0.0 } },
  { { -2,  0,  1,  2,  1 }, {       6.0,    0.0 }, {    -3.0,  0.0 } },
  { { -2,  0,  0,  0,  1 }, {      -5.0,    0.0 }, {     3.0,  0.0 } },
  { {  0, -1,  1,  0,  0 }, {       5.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  2,  2,  1 }, {      -5.0,    0.0 }, {     3.0,  0.0 } },
  { { -2,  1,  0,  0,  0 }, {      -4.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  1, -2,  0 }, {       4.0,    0.0 }, {     0.0,  0.0 } },
  { {  1,  0,  0,  0,  0 }, {      -4.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  1,  1,  0,  0 }, {      -3.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  1,  2,  0 }, {       3.0,    0.0 }, {     0.0,  0.0 } },
  { {  0, -1,  1,  2,  2 }, {      -3.0,    0.0 }, {     1.0,  0.0 } },
  { {  2, -1, -1,  2,  2 }, {      -3.0,    0.0 }, {     1.0,  0.0 } },
  { {  0,  0, -2,  0,  1 }, {      -2.0,    0.0 }, {     1.0,  0.0 } },
  { {  0,  0,  3,  2,  2 }, {      -3.0,    0.0 }, {     1.0,  0.0 } },
  { {  2, -1,  0,  2,  2 }, {      -3.0,    0.0 }, {     1.0,  0.0 } },
  { {  0,  1,  1,  2,  2 }, {       2.0,    0.0 }, {    -1.0,  0.0 } },
  { { -2,  0, -1,  2,  1 }, {      -2.0,    0.0 }, {     1.0,  0.0 } },
  { {  0,  0,  2,  0,  1 }, {       2.0,    0.0 }, {    -1.0,  0.0 } },
  { {  0,  0,  1,  0,  2 }, {      -2.0,    0.0 }, {     1.0,  0.0 } },
  { {  0,  0,  3,  0,  0 }, {       2.0,    0.0 }, {     0.0,  0.0 } },
  { {  1,  0,  0,  2,  2 }, {       2.0,    0.0 }, {    -1.0,  0.0 } },
  { {  0,  0, -1,  0,  2 }, {       1.0,    0.0 }, {    -1.0,  0.0 } },
  { { -4,  0,  1,  0,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  0, -2,  2,  2 }, {       1.0,    0.0 }, {    -1.0,  0.0 } },
  { {  4,  0, -1,  2,  2 }, {      -2.0,    0.0 }, {     1.0,  0.0 } },
  { { -4,  0,  2,  0,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  1,  1,  2,  2 }, {       1.0,    0.0 }, {    -1.0,  0.0 } },
  { {  2,  0,  1,  2,  1 }, {      -1.0,    0.0 }, {     1.0,  0.0 } },
  { {  4,  0, -2,  2,  2 }, {      -1.0,    0.0 }, {     1.0,  0.0 } },
  { {  0,  0, -1,  4,  2 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2, -1,  1,  0,  0 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  2,  2,  1 }, {       1.0,    0.0 }, {    -1.0,  0.0 } },
  { {  2,  0,  2,  2,  2 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  0,  1,  0,  1 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  0,  4,  2 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  3,  2,  2 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  1,  2,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  1,  0,  2,  1 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2, -1, -1,  0,  1 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { {  0,  0,  0, -2,  1 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { { -1,  0,  0,  2,  2 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  1,  0,  0,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  0,  1, -2,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  0, -1,  0,  2,  1 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { { -2,  1,  1,  0,  1 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  0,  1, -2,  0 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  2,  0,  2,  0,  0 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
  { {  4,  0,  0,  2,  2 }, {      -1.0,    0.0 }, {     0.0,  0.0 } },
  { {  1,  1,  0,  0,  0 }, {       1.0,    0.0 }, {     0.0,  0.0 } },
}};


/** @enum Specify which model to use when calculating Earth's nutation. */
enum class Model : uint8_t { MEEUS, IAU_1980 };

/** @brief Find the nutation coefficients for the given model. */
inline auto find_model(const Model model) -> std::span<const NutationCoeffs> {
  switch (model) {
    case Model::MEEUS:    return { MEEUS_NUTATION_COEFFS };
    case Model::IAU_1980: return { IAU1980_NUTATION_COEFFS };
    default:              throw std::runtime_error { "Unknown nutation model" };
  }
}


/**
 * @brief Return the function to calculate the θ values, for the given julian century.
 * @param jc The julian century since J2000.
 * @return The function to calculate the θ values, which takes `θParams` as input and returns the θ value in degrees.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22.
 */
inline auto gen_eval_θ(const double jc) -> std::function<Angle<DEG>(θCoeffs)> {
  const double jc2 = jc * jc;
  const double jc3 = jc * jc2;

  // D is the mean elongation of the Moon from the Sun in degrees.
  const double D  = 297.85036 + 445267.111480 * jc - 0.0019142 * jc2 + jc3 / 189474.0;
  // M is the mean anomaly of the Sun (Earth) in degrees.
  const double M  = 357.52772 + 35999.050340  * jc - 0.0001603 * jc2 - jc3 / 300000.0;
  // Mp is the mean anomaly of the Moon in degrees.
  const double Mp = 134.96298 + 477198.867398 * jc + 0.0086972 * jc2 + jc3 / 56250.0;
  // F is the Moon's argument of latitude in degrees.
  const double F  = 93.27191  + 483202.017538 * jc - 0.0036825 * jc2 + jc3 / 327270.0;
  // Ω is the longitude of the ascending node of the Moon's mean orbit on the ecliptic in degrees.
  const double Ω  = 125.04452 - 1934.136261   * jc + 0.0020708 * jc2 + jc3 / 450000.0;

  return [=](const θCoeffs& coeffs) -> Angle<DEG> {
    const double degrees = D * coeffs.D + M * coeffs.M + Mp * coeffs.Mp + F * coeffs.F + Ω * coeffs.Ω;
    return Angle<DEG> { degrees };
  };
};


/**
 * @brief Calculates the nutation in longitude (Δψ) for the given julian day.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param model The model to use when calculating the nutation. Defaults to `Model::IAU_1980`.
 * @return The nutation in longitude (Δψ) in degrees.
 * @note By default, the IAU 1980 model is used, since it is more accurate.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22.
 */
inline auto longitude(const double jde, const Model model = Model::IAU_1980) -> Angle<DEG> {
  // Get the Julian century since J2000.
  const double jc = astro::julian_day::jde_to_jc(jde);

  // Create the function to calculate the θ values.
  const auto eval_θ = gen_eval_θ(jc);

  // Select the coefficient terms to use.
  const auto& coeff_terms = find_model(model);

  // Evaluate each term.
  const auto results = coeff_terms | std::views::transform([&](const NutationCoeffs& coeffs) {
    const Angle<DEG> θ = eval_θ(coeffs.θ);
    const auto& [a, b] = coeffs.Δψ;
    return (a + b * jc) * std::sin(θ.rad());
  });

  // Accumulate the results of all the terms.
  // The unit is 0".0001.
  const auto sum_results = std::reduce(begin(results), end(results));
  const auto Δψ_arcsec = sum_results * 0.0001;

  // Convert the result to degrees.
  return Angle<DEG>::from_arcsec(Δψ_arcsec);
}


/**
 * @brief Calculates the nutation in obliquity (Δε) for the given julian day.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param model The model to use when calculating the nutation. Defaults to `Model::IAU_1980`.
 * @return The nutation in obliquity (Δε) in degrees.
 * @note By default, the IAU 1980 model is used, since it is more accurate.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22.
 */
inline auto obliquity(const double jde, const Model model = Model::IAU_1980) -> Angle<DEG> {
  // Get the Julian century since J2000.
  const double jc = astro::julian_day::jde_to_jc(jde);

  // Create the function to calculate the θ values.
  const auto eval_θ = gen_eval_θ(jc);

  // Select the coefficient terms to use.
  const auto& coeff_terms = find_model(model);

  // Evaluate each term.
  const auto results = coeff_terms | std::views::transform([&](const NutationCoeffs& coeffs) {
    const Angle<DEG> θ = eval_θ(coeffs.θ);
    const auto& [a, b] = coeffs.Δε;
    return (a + b * jc) * std::cos(θ.rad());
  });

  // Accumulate the results of all the terms.
  // The unit is 0".0001.
  const auto sum_results = std::reduce(begin(results), end(results));
  const auto Δε_arcsec = sum_results * 0.0001;

  // Convert the result to degrees.
  return Angle<DEG>::from_arcsec(Δε_arcsec);
}

} // namespace astro::earth::nutation


namespace astro::earth::aberration {

/**
 * @ref https://en.wikipedia.org/wiki/Aberration_(astronomy)
 * According to wikipedia:
 * "Annual aberration is caused by the motion of an observer on Earth as the planet revolves around the Sun. Due to"... 
 * "Its accepted value is 20.49552 arcseconds (sec) or 0.000099365 radians (rad) (at J2000)."
 */

/** @brief The constant for the annual aberration, at J2000.0. */
constexpr double ANNUAL_CONSTANT = 20.49552;


/**
 * @brief Compute the aberration for the given radius (in AU).
 * @param r The radius (in AU).
 * @return The aberration (in degrees).
 */
inline auto compute(const double r) -> Angle<DEG> {
  const double aberration_arcsec = ANNUAL_CONSTANT / r;
  return Angle<DEG>::from_arcsec(aberration_arcsec);
}

} // namespace astro::earth::aberration

