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
#include <cmath>
#include <ranges>
#include <cstdint>
#include <numeric>

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "vsop87d/vsop87d.hpp"
#include "vsop87d/defines.hpp"


namespace astro::earth::heliocentric_coord {

/**
 * @brief Calculate the heliocentric position of the Earth, using VSOP87D.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The heliocentric ecliptic position of the Earth, calculated using VSOP87D.
 */
[[nodiscard]] inline auto vsop87d(const double jde) -> toolbox::SphericalCoordinate {
  const double jm = astro::julian_day::jde_to_jm(jde);
  const auto evaluated = astro::vsop87d::evaluate<vsop87d::Planet::EAR>(jm);

  return {
    // As per the algorithm, the longitude is normalized to [0, 2π).
    .λ = toolbox::AngleDeg { toolbox::AngleRad { evaluated.λ }.normalize() },
    .β = toolbox::AngleDeg { toolbox::AngleRad { evaluated.β } },
    .r = toolbox::DistanceAu { evaluated.r }
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
// NOLINTBEGIN(modernize-use-designated-initializers)
inline constexpr std::array<NutationCoeffs, 63> MEEUS_NUTATION_COEFFS {{
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
// NOLINTEND(modernize-use-designated-initializers)


// The following IAU 1980 Nutation Model data was collected from https://www.iausofa.org/2021_0512_C/sofa/nut80.c.
// Compared to Meeus's omitted version, this table contains all terms.
// NOLINTBEGIN(modernize-use-designated-initializers)
inline constexpr std::array<NutationCoeffs, 106> IAU1980_NUTATION_COEFFS {{
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
// NOLINTEND(modernize-use-designated-initializers)


/** @enum Specify which model to use when calculating Earth's nutation. */
enum class Model : uint8_t { MEEUS, IAU_1980 };

/** @brief Find the nutation coefficients for the given model. */
[[nodiscard]] inline auto find_model(const Model model) -> std::span<const NutationCoeffs> {
  switch (model) {
    case Model::MEEUS:    return { MEEUS_NUTATION_COEFFS };
    case Model::IAU_1980: return { IAU1980_NUTATION_COEFFS };
    default:              throw std::runtime_error { "Unknown nutation model" };
  }
}


/**
 * @brief Return the function to calculate the θ values, for the given julian century.
 * @param jc The julian century since J2000.
 * @return The function to calculate the θ values, which takes `θCoeffs` as input and returns the θ value in degrees.
 * @note Handed back as `auto`, not `std::function`: this evaluator runs once per coefficient row
 *       of the nutation table, so type erasure here buys an indirect call on every single term
 *       of every nutation evaluation (#98).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22.
 */
[[nodiscard]] inline auto gen_eval_θ(const double jc) {
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

  return [=](const θCoeffs& coeffs) -> toolbox::AngleDeg {
    const double degrees = D * coeffs.D + M * coeffs.M + Mp * coeffs.Mp + F * coeffs.F + Ω * coeffs.Ω;
    return toolbox::AngleDeg { degrees };
  };
}


/**
 * @brief Calculates the nutation in longitude (Δψ) for the given julian day.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param model The model to use when calculating the nutation. Defaults to `Model::IAU_1980`.
 * @return The nutation in longitude (Δψ) in degrees.
 * @note By default, the IAU 1980 model is used, since it is more accurate.
 * @note Twin of `nutation::obliquity`, which sums the same table and differs in exactly two
 *       places: it reads `coeffs.Δε` and it takes the cosine. The duplication is deliberate —
 *       each body mirrors its own Meeus summation — so fix both or neither (#49).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22.
 */
[[nodiscard]] inline auto longitude(const double jde, const Model model = Model::IAU_1980) -> toolbox::AngleDeg {
  // Get the Julian century since J2000.
  const double jc = astro::julian_day::jde_to_jc(jde);

  // Create the function to calculate the θ values.
  const auto eval_θ = gen_eval_θ(jc);

  // Select the coefficient terms to use.
  const auto& coeff_terms = find_model(model);

  // Evaluate each term.
  const auto results = coeff_terms | std::views::transform([&](const NutationCoeffs& coeffs) {
    const toolbox::AngleDeg θ = eval_θ(coeffs.θ);
    const auto& [a, b] = coeffs.Δψ;
    return (a + b * jc) * std::sin(θ.rad());
  });

  // Accumulate the results of all the terms.
  // The unit is 0".0001.
  const auto sum_results = std::reduce(cbegin(results), cend(results));
  const auto Δψ_arcsec = sum_results * 0.0001;

  // Convert the result to degrees.
  return toolbox::AngleDeg::from_arcsec(Δψ_arcsec);
}


/**
 * @brief Calculates the nutation in obliquity (Δε) for the given julian day.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param model The model to use when calculating the nutation. Defaults to `Model::IAU_1980`.
 * @return The nutation in obliquity (Δε) in degrees.
 * @note By default, the IAU 1980 model is used, since it is more accurate.
 * @note Twin of `nutation::longitude`, which sums the same table and differs in exactly two
 *       places: it reads `coeffs.Δψ` and it takes the sine. The duplication is deliberate —
 *       each body mirrors its own Meeus summation — so fix both or neither (#49).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22.
 */
[[nodiscard]] inline auto obliquity(const double jde, const Model model = Model::IAU_1980) -> toolbox::AngleDeg {
  // Get the Julian century since J2000.
  const double jc = astro::julian_day::jde_to_jc(jde);

  // Create the function to calculate the θ values.
  const auto eval_θ = gen_eval_θ(jc);

  // Select the coefficient terms to use.
  const auto& coeff_terms = find_model(model);

  // Evaluate each term.
  const auto results = coeff_terms | std::views::transform([&](const NutationCoeffs& coeffs) {
    const toolbox::AngleDeg θ = eval_θ(coeffs.θ);
    const auto& [a, b] = coeffs.Δε;
    return (a + b * jc) * std::cos(θ.rad());
  });

  // Accumulate the results of all the terms.
  // The unit is 0".0001.
  const auto sum_results = std::reduce(cbegin(results), cend(results));
  const auto Δε_arcsec = sum_results * 0.0001;

  // Convert the result to degrees.
  return toolbox::AngleDeg::from_arcsec(Δε_arcsec);
}

} // namespace astro::earth::nutation


namespace astro::earth::obliquity {

// The obliquity of the ecliptic is the angle between the ecliptic and the celestial equator.
// It is needed whenever converting between ecliptic and equatorial coordinates.

/**
 * @brief Calculates the mean obliquity of the ecliptic (ε₀) for the given julian day.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The mean obliquity (ε₀) in degrees.
 * @details Accuracy ~1" over ±2000 years from J2000; the polynomial degrades farther out.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22, Formula (22.2).
 */
[[nodiscard]] inline auto mean(const double jde) -> toolbox::AngleDeg {
  // Get the Julian century since J2000.
  const double jc = astro::julian_day::jde_to_jc(jde);

  // IAU 1980: ε₀ = 23°26'21".448 - 46".8150T - 0".00059T² + 0".001813T³
  // The polynomial is evaluated in arcseconds.
  const double ε0_arcsec = 84381.448 + jc * (-46.8150 + jc * (-0.00059 + jc * 0.001813));

  return toolbox::AngleDeg::from_arcsec(ε0_arcsec);
}

/**
 * @brief Calculates the true obliquity of the ecliptic (ε) for the given julian day.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param model The nutation model to use. Defaults to `nutation::Model::IAU_1980`.
 * @return The true obliquity (ε = ε₀ + Δε) in degrees.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 22.
 */
[[nodiscard]] inline auto true_obliquity(
  const double jde,
  const nutation::Model model = nutation::Model::IAU_1980
) -> toolbox::AngleDeg {
  return mean(jde) + nutation::obliquity(jde, model);
}

} // namespace astro::earth::obliquity


namespace astro::earth::aberration {

/**
 * @brief A term of the Δλ series: `amplitude × τ^tau_power × sin(phase + rate × τ)`,
 *        with τ the Julian millennia since J2000.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, p. 168.
 */
struct DailyVariationTerm {
  double amplitude;   // In arcseconds per day
  double phase;       // In degrees
  double rate;        // In degrees per Julian millennium
  uint8_t tau_power;
};

/**
 * @brief The Δλ series (daily variation of the Sun's geocentric longitude), p. 168.
 * @note Terms with rate 359993/719987/1079981 are due to the eccentricity, 4452671/9224659/4092677
 *       to the Moon, 450368/225184/315559/675553 to Venus, 329644/659289/299295 to Jupiter, 337181 to Mars.
 */
// NOLINTBEGIN(modernize-use-designated-initializers)
inline constexpr std::array<DailyVariationTerm, 21> MEEUS_DAILY_VARIATION_TERMS {{
  { 118.568,  87.5287,  359993.7286, 0 },
  {   2.476,  85.0561,  719987.4571, 0 },
  {   1.376,  27.8502, 4452671.1152, 0 },
  {   0.119,  73.1375,  450368.8564, 0 },
  {   0.114, 337.2264,  329644.6718, 0 },
  {   0.086, 222.5400,  659289.3436, 0 },
  {   0.078, 162.8136, 9224659.7915, 0 },
  {   0.054,  82.5823, 1079981.1857, 0 },
  {   0.052, 171.5189,  225184.4282, 0 },
  {   0.034,  30.3214, 4092677.3866, 0 },
  {   0.033, 119.8105,  337181.4711, 0 },
  {   0.023, 247.5418,  299295.6151, 0 },
  {   0.023, 325.1526,  315559.5560, 0 },
  {   0.021, 155.1241,  675553.2846, 0 },
  {   7.311, 333.4515,  359993.7286, 1 },
  {   0.305, 330.9814,  719987.4571, 1 },
  {   0.010, 328.5170, 1079981.1857, 1 },
  {   0.309, 241.4518,  359993.7286, 2 },
  {   0.021, 205.0482,  719987.4571, 2 },
  {   0.004, 297.8610, 4452671.1152, 2 },
  {   0.010, 154.7066,  359993.7286, 3 },
}};
// NOLINTEND(modernize-use-designated-initializers)

/**
 * @brief The daily variation Δλ of the Sun's geocentric longitude, mean equinox of the date.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return Δλ in arcseconds per day.
 * @note The constant term is 3548.330 for the mean equinox of the date;
 *       3548.193 is for the fixed J2000 frame (p. 168 note).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, p. 168.
 */
[[nodiscard]] inline auto daily_λ_variation(const double jde) -> double {
  using namespace std::ranges;
  const double τ = astro::julian_day::jde_to_jm(jde);
  const auto terms = MEEUS_DAILY_VARIATION_TERMS | views::transform([τ](const DailyVariationTerm& t) {
    const toolbox::AngleDeg θ { t.phase + t.rate * τ };
    return t.amplitude * std::pow(τ, t.tau_power) * std::sin(θ.rad());
  });
  return 3548.330 + std::reduce(cbegin(terms), cend(terms));
}

/** @brief The light-time for unit distance, in days per AU (= 499.00478 s ≈ 8.3 min).
 *  @note (25.11) prints 0.005775518; the 8th significant digit here is from τ_A = 499.004784 s / 86400. */
inline constexpr double LIGHT_TIME_DAYS_PER_AU = 0.0057755183;

/**
 * @brief Compute the aberration correction to the Sun's geometric longitude, Meeus (25.11).
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param r The Sun's radius vector.
 * @return The aberration (in degrees); subtract it from the geometric longitude.
 * @note Meeus (25.11) applies the correction −(light-time) × R × Δλ to the geometric longitude;
 *       this function returns the positive magnitude, which the caller subtracts (`sun.hpp`).
 *       The variable form accounts for the perturbations of the Earth's
 *       orbit (mainly lunar) that the fixed form (25.10) −20.4898″/R ignores:
 *       error < 0.001″ vs up to 0.01″ for (25.10). The (25.10) numerator is
 *       κ(1−e²), not the bare aberration constant κ = 20.49552″ (#66).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, (25.10)-(25.11), p. 167-168.
 */
[[nodiscard]] inline auto compute(const double jde, const toolbox::DistanceAu r) -> toolbox::AngleDeg {
  const double aberration_arcsec = LIGHT_TIME_DAYS_PER_AU * r.au() * daily_λ_variation(jde);
  return toolbox::AngleDeg::from_arcsec(aberration_arcsec);
}

} // namespace astro::earth::aberration

