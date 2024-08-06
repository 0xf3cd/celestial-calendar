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

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "earth.hpp"

namespace astro::sun::geocentric_coord {

using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;
using astro::toolbox::AngleUnit::RAD;
using astro::toolbox::SphericalCoordinate;


/**
 * @brief Calculate the geocentric position of the Sun, using VSOP87D.
 * @param jde The Julian Ephemeris Day.
 * @return The geocentric ecliptic position of the Sun, calculated using VSOP87D.
 * @details The function invokes `astro::earth::heliocentric_coord::vsop87d`, and
 *          transforms the heliocentric coordinates to geocentric coordinates.
 */
inline auto vsop87d(const double jde) -> SphericalCoordinate {
  const auto& [λ_helio, β_helio, r_helio] = astro::earth::heliocentric_coord::vsop87d(jde);
  return {
    // Convert the heliocentric ecliptic longitude of Earth to geocentric ecliptic longitude of Sun.
    // The formula is: λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180∘
    .λ = std::invoke([&] {
      using namespace astro::toolbox::literals;
      const auto sum = λ_helio + 180.0_deg;
      return sum.normalize();
    }),

    // Convert the heliocentric ecliptic latitude of Earth to geocentric ecliptic latitude of Sun.
    // The formula is: β_sun_geocentric_deg = -β_earth_heliocentric_deg
    .β = -β_helio,

    // The distance (radius) is the same for both Sun and Earth.
    .r = r_helio 
  };
}


/** @brief The FK5 correction for the coordinate calculated using VSOP87D. */
struct Fk5Correction {
  const Angle<DEG> Δλ;
  const Angle<DEG> Δβ;
};


/**
 * @brief Calculate the correction for the VSOP87D coordinate, in order to convert it to FK5 system.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The correction (i.e. Δlongitude and Δlatitude).
 * @details As per Jean Meeus's Astronomical Algorithms, this correction is applied for accuracy.
 */
inline auto fk5_correction(const double jde, const SphericalCoordinate& vsop87d_coord) -> Fk5Correction {
  const double jc = astro::julian_day::jde_to_jc(jde);
  const auto& [vsop_λ, vsop_β, vsop_r] = vsop87d_coord;

  // Calculate the deltas for longitude and latitude, in arcsec.
  const Angle λ_dash = vsop_λ - Angle<DEG> { (1.397 + 0.00031 * jc) * jc };
  const double λ_dash_rad = λ_dash.rad();

  const double delta_λ_arcsec = -0.09033 + 0.03916 * (cos(λ_dash_rad) + sin(λ_dash_rad)) * tan(vsop_β.rad());
  const double delta_β_arcsec = 0.03916 * (cos(λ_dash_rad) - sin(λ_dash_rad));

  return {
    .Δλ = Angle<DEG>::from_arcsec(delta_λ_arcsec),
    .Δβ = Angle<DEG>::from_arcsec(delta_β_arcsec),
  };
}


/**
 * @brief Calculate the apparent geocentric position of the Sun, using VSOP87D. 
 *        The position is corrected to FK5 system, considering nutation and aberration. 
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The geocentric ecliptic position of the Sun, after correction.
 */
inline auto apparent(const double jde) -> SphericalCoordinate {
  // Use VSOP87D to calculate the geocentric ecliptic position of the Sun.
  const auto vsop_coord = vsop87d(jde);

  // Calculate the correction for the VSIO87D result, in order to convert it to FK5 system.
  const auto correction = fk5_correction(jde, vsop_coord);

  // Calculate the Earth's nutation in longtitude.
  const auto nutation = astro::earth::nutation::longitude(jde);

  // Calculate the Solar aberration.
  const auto aberration = astro::earth::aberration::compute(vsop_coord.r.au());

  // Calculate the adjusted longitude.
  const auto λ = vsop_coord.λ + correction.Δλ + nutation - aberration;

  // Calculate the adjusted latitude.
  const auto β = vsop_coord.β + correction.Δβ;

  return {
    .λ = λ.normalize(),
    .β = β,
    .r = vsop_coord.r
  };
}

} // namespace astro::sun::geocentric_coord


namespace astro::sun::geocentric_coord::math {

// In this namespace, we use Newton's method to approximate the JDE,
// at which time the Sun reaches a given geocentric longitude in a year.
//
// The following codes are implementing the ideas illustrated in:
// https://github.com/0xf3cd/celestial-calendar/blob/main/statistics/sun_longitude.ipynb
//
// Given a year and a geocentric longitude, our goal is to find the JDE(s) that satisfy the following condition:
// 1. The JDE(s) must fall in the given year.
// 2. At the JDE(s), the Sun's geocentric longitude is equal to the given longitude.
// 
// In our context, a JDE that satisfies the above conditions is called "a root".
//
// For a given year and a given geocentric longitude, there can be 0, 1, or 2 roots.

/** 
 * @see https://github.com/0xf3cd/celestial-calendar/blob/main/statistics/sun_longitude.ipynb
 * @see https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（下）
 */


/**
 * @brief Calculate the apparent geocentric longitude of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The apparent geocentric longitude of the Sun in degrees.
 */
inline auto solar_longitude(const double jde) -> double {
  // Calculate the apparent geocentric longitude of the Sun.
  const auto coord = astro::sun::geocentric_coord::apparent(jde);

  // Return in degrees.
  return coord.λ.deg();
}

/** @brief Return the JDE of the start of the year. */
inline auto get_start_jde(const int32_t year) -> double{
  return astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year, 1, 1), 0.0 });
}

/** @brief Return the JDE of the end of the year. */
inline auto get_end_jde(const int32_t year) -> double {
  return astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year + 1, 1, 1), 0.0 });
}

/** @brief Return the apparent geocentric longitude of the Sun at the start of the year. */
inline auto get_start_lon(const int32_t year) -> double {
  return solar_longitude(get_start_jde(year));
}

/** @brief Return the apparent geocentric longitude of the Sun at the end of the year. */
inline auto get_end_lon(const int32_t year) -> double {
  return solar_longitude(get_end_jde(year));
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)

/** @brief Return true if the given year has a root for the given `lon` before the spring equinox. */
inline auto has_root_before_spring_equinox(const int32_t year, const double lon) -> bool {
  const double start_lon = get_start_lon(year);
  return start_lon <= lon and lon < 360.0;
}

/** @brief Return true if the given year has a root for the given `lon` after the spring equinox. */
inline auto has_root_after_spring_equinox(const int32_t year, const double lon) -> bool {
  const double end_lon = get_end_lon(year);
  return 0.0 <= lon and lon < end_lon;
}

/** @brief Return the count of the roots for the given `year` and `lon`. */
inline auto discriminant(const int32_t year, const double lon) -> uint32_t {
  uint32_t count = 0;

  if (has_root_before_spring_equinox(year, lon)) {
    count++;
  }
  if (has_root_after_spring_equinox(year, lon)) {
    count++;
  }

  return count;
}


// In Newton's method, we will approximate the root with the previous root, iteratively.
// The formula is: Xn+1 = Xn - f(Xn) / f'(Xn).
// Where we can use the following formula to approximate f'(x):
// f'(x) = (f(x+h) - f(x-h)) / (2*h), where h is a small number, usally [1e-8, 1e-5].
//
// As mentioned before, our goal is to find the root (JDE) at which the Sun reaches the expected longitude in a given year.
// In our context, f is defined as:
// f(jde) = solar_longitude(jde) - expected_lon,
// and f is defined on a half-open interval [start_jde(year), end_jde(year)).
//
// Newton's method requires f to be differentiable (smooth).
// So we need to modify the function of `solar_longitude`.
// Bacause the beginning position of Sun in a year is roughly 280.0 degrees, and we need to make it negative.
// Actually, for any JDE befire Spring Equinox this year, we need to substract 360 from `solar_longitude`'s result to make f smooth.
// 
// So the actual f is defined as:
// f(jde) = modified_solar_longitude(jde) - expected_lon

using FuncType = std::function<double(const double)>;

/**@brief Return a `f` that we can apply Newton's method to. */
inline auto make_f(const int32_t year, const double expected_lon) -> FuncType {
  const double apr_1st_jde = astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year, 4, 1), 0.0 });

  const auto modified_solar_longitude = [=](const double jde) -> double {
    const double raw_value = solar_longitude(jde);

    // We mostly want to substract 360.0 from those JDEs before Spring Equinox.
    //
    // We are here using the fact that the beginning of the year is roughly 280.0 degrees,
    // and it continues growing to 360 degrees (which is also 0 degrees) until Spring Equinox.
    // 
    // After Spring Equinox, it grows from 0 to ~280.0 degrees again (the last day of the year), 
    // and then next year comes.

    if (jde < apr_1st_jde and raw_value >= 250.0) {
      return raw_value - 360.0;
    }

    return raw_value; 
  };

  return [=](const double jde) -> double {
    return modified_solar_longitude(jde) - expected_lon;
  };
}

/** 
 * @brief Apply Newton's method to find the root.
 * @param f The function to find the root.
 * @param start_jde The left bound of JDE's range, inclusive.
 * @param end_jde The right bound of JDE's range, exclusive.
 * @param episilon The tolerance. Default is 1e-10.
 * @param max_iter The maximum number of iterations. Default is 20.
 * @returns The approximated root (i.e. JDE). */
inline auto newton_method(
  const FuncType& f,              // The f function to find root(s) for.
  const double start_jde,         // The left bound of JDE's range, inclusive.
  const double end_jde,           // The right bound of JDE's range, exclusive.
  const double episilon = 1e-10,  // The tolerance.
  const std::size_t max_iter = 20 // The maximum number of iterations.
) -> double {
  // `pull_back` ensures the returned JDE is in valid range.
  const auto pull_back = [&](const double jde) -> double {
    if (jde < start_jde) {
      return start_jde;
    }
    if (jde >= end_jde) {
      return end_jde - 1e-20;
    }
    return jde;
  };

  // `next` returns (has_next, next_jde)
  const auto next = [&](const double jde) -> std::pair<bool, double> {
    const double f_jde = f(jde);

    if (std::abs(f_jde) < episilon) { // We find the root!!
      return { false, jde }; // Converged. No more iterations needed.
    }

    // Do the Newton's method.
    constexpr double h = 1e-8;
    const double f_prime_jde = (f(jde + h) - f(jde - h)) / (2.0 * h); // Approximate the derivative.
    const double next_jde = jde - f_jde / f_prime_jde;                // Approximate our next guess.

    return { true, pull_back(next_jde) }; // Don't forget to pull the jde back to valid range.
  };

  // Start approximating the root.

  double jde = (start_jde + end_jde) / 2.0; // Initial guess.

  for (std::size_t iter_count = 0; iter_count < max_iter; iter_count++) {
    // Do the Newton's method.
    const auto& [has_next, next_jde] = next(jde);
    jde = next_jde;

    if (!has_next) {
      break;
    }
  }

  // We cannot find the accurate root in the above iterations.
  // Return our best estimation.
  return jde;
}


/** 
 * @brief Find the roots (i.e. JDEs) for the given `year` and `expected_lon`. 
 * @param year The year, in gregorian calendar.
 * @param expected_lon The expected solar longitude, in degrees.
 * @return The roots (i.e. JDEs). There can be 0, 1 or 2 roots.
 */
inline auto find_roots(const int32_t year, const double expected_lon) -> std::vector<double> {
  if (discriminant(year, expected_lon) == 0) { // No root.
    return {};
  }

  std::vector<FuncType> f_vec;

  // If there is a root before Spring Equinox, it means that
  // after modification (for the sake of differentiability of f),
  // the solar longitudes before spring equinox will be negative.
  // And accordingly, we need to subtract 360.0 from the expected_lon.
  if (has_root_before_spring_equinox(year, expected_lon)) {
    f_vec.emplace_back(make_f(year, expected_lon - 360.0));
  }

  // If there is a root after Spring Equinox, it means that
  // after modification (for the sake of differentiability of f),
  // the solar longitudes after spring equinox will be positive.
  // And accordingly, we have no need to modify the expected_lon.
  if (has_root_after_spring_equinox(year, expected_lon)) {
    f_vec.emplace_back(make_f(year, expected_lon));
  }

  // "nm" here denotes "newton_method".
  auto apply_nm = [&](const FuncType& f) {
    const double start_jde = get_start_jde(year);
    const double end_jde   = get_end_jde(year);
    return newton_method(f, start_jde, end_jde);
  };

  using namespace std::ranges;
  return f_vec | views::transform(apply_nm) | to<std::vector>();
}

// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace astro::sun::geocentric_coord::math
