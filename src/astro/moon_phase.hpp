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
#include <vector>
#include <format>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "toolbox.hpp"
#include "ymd.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"

#include "sun.hpp"
#include "moon.hpp"


namespace astro::moon_phase::new_moon {

// In our context, the conjunction is the moment when the Sun and the Moon are at the same apparent longitude,
// which is also called "New Moon". In Chinese, this is called "朔", "合朔", or "新月".


/**
 * @brief Calculate the difference between the apparent longitudes of the Moon and the Sun.
 * @param jde The Julian Ephemeris Day.
 * @return The normalized difference between the apparent longitudes of the Moon and the Sun, in degrees.
 * @see VSOP87D, ELP2000-82B, and Astronomical Algorithms, Jean Meeus, 1998.
 */
[[nodiscard]] inline auto longitude_diff(const double jde) -> double {
  const auto sun_apparent_lon = astro::sun::geocentric_coord::apparent(jde).λ;
  const auto moon_apparent_lon = astro::moon::geocentric_coord::apparent(jde).λ;
  const auto diff = moon_apparent_lon - sun_apparent_lon;
  return diff.normalize().deg();
}


/**
 * @brief How near conjunction a JDE must sit before it can serve as a bracket endpoint,
 *        measured in degrees of Moon-Sun elongation.
 * @note The same figure sets where `f` is unwrapped below, so the bracket that Newton's method
 *       accepts and the interval on which `f` is smooth are one and the same by construction.
 */
inline constexpr double BRACKET_TOLERANCE_DEG = 15.0;

/**
 * @brief Apply Newton's method to find the jde, when the Sun and Moon are at the same apparent longitude.
 * @param left_jde The left bound of the search, inclusive.
 * @param right_jde The right bound of the search, exclusive.
 * @param iterations The maximum number of iterations.
 * @return The jde of the conjunction.
 * @note It is the caller's responsibility to ensure the root exists in the range of [left_jde, right_jde).
 * @throw std::invalid_argument If no root exists in the range of [left_jde, right_jde).
 */
[[nodiscard]] inline auto newton_method(
  const double left_jde,
  const double right_jde,            // NOLINT(bugprone-easily-swappable-parameters)
  const std::size_t iterations = astro::toolbox::NEWTON_MAX_ITERATIONS
) -> double {

  // Make sure the root exists in the range of [left_jde, right_jde).
  const double left_diff = longitude_diff(left_jde);
  const double right_diff = longitude_diff(right_jde);

  if (left_diff <= 360.0 - BRACKET_TOLERANCE_DEG or right_diff >= BRACKET_TOLERANCE_DEG) [[unlikely]] {
    throw std::invalid_argument {
      std::format(
        "No root between jde {} (elongation {} deg) and jde {} (elongation {} deg).",
        left_jde, left_diff, right_jde, right_diff
      )
    };
  }

  // Define the function `f` which is differentiable.
  // We are going to find the root where `f` evaluates to 0.
  // Just before conjunction the Moon still trails the Sun by nearly a full turn, so the raw
  // difference jumps from 360 back to 0 across the root; unwrapping it keeps `f` smooth there.
  const auto f = [](const double jde) -> double {
    const double diff = longitude_diff(jde);
    if (diff > 360.0 - BRACKET_TOLERANCE_DEG) {
      return diff - 360.0;
    }
    return diff;
  };

  return astro::toolbox::newton_method(
    f, left_jde, right_jde, astro::toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY, iterations
  );
}


/**
 * @brief How far the mean-rate extrapolation may miss conjunction and still be worth refining,
 *        in degrees of Moon-Sun elongation.
 * @note Refining divides the miss by the mean rate, so a miss of this size leaves the estimate
 *       about `30 / 12.19 * 0.19` = 0.47 day short of the root — which is what makes
 *       `BRACKET_HALF_WIDTH_DAYS` the binding constraint on how large this may be.
 */
inline constexpr double ESTIMATE_TOLERANCE_DEG = 30.0;

/**
 * @brief Half the width of the bracket handed to Newton's method, in days.
 * @note Chosen between two bounds. Too narrow and the bracket stops containing the root, since
 *       refining leaves an error of up to 0.47 day at the tolerance above. Too wide and the
 *       endpoints drift past `BRACKET_TOLERANCE_DEG`: at the Moon's fastest, 14.5 deg/day, an
 *       endpoint 0.67 day out already sits 9.7 deg from conjunction.
 */
inline constexpr double BRACKET_HALF_WIDTH_DAYS = 0.5;

/**
 * @brief Approximate the range of the first root after the given jde, when the Sun and Moon are at the same apparent longitude.
 * @param jde The jde.
 * @return The range of the first root after the given `jde`.
 * @throw std::invalid_argument If the extrapolation lands further than `ESTIMATE_TOLERANCE_DEG`
 *        from conjunction, i.e. too far off to be worth refining.
 */
[[nodiscard]] inline auto first_root_range_after(const double jde) -> std::pair<double, double> {
  const double cur_diff = longitude_diff(jde);
  const double gap = 360.0 - cur_diff;

  constexpr double deg_per_day = astro::toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY;
  const double est_jde = jde + gap / deg_per_day; // Estimate the next root jde.

  // Extrapolating at the mean rate lands near the root but not on it: the Moon's true rate runs
  // between about 10.5 and 14.5 deg/day, so a month of it can miss by several degrees either way.
  // Signed, so that overshooting the conjunction reads positive and falling short reads negative.
  const double est_jde_diff = longitude_diff(est_jde);
  const double signed_miss_deg = (est_jde_diff > 180.0) ? est_jde_diff - 360.0 : est_jde_diff;

  if (std::fabs(signed_miss_deg) > ESTIMATE_TOLERANCE_DEG) [[unlikely]] {
    throw std::invalid_argument {
      std::format(
        "Cannot find the first root after jde {}: the estimate at jde {} misses conjunction "
        "by {} deg (elongation {} deg).",
        jde, est_jde, signed_miss_deg, est_jde_diff
      )
    };
  }

  // Step the estimate onto the root before bracketing it. Bracketing the estimate directly would
  // leave the endpoints however far out the extrapolation happened to miss, plus whatever the
  // rate varied over the bracket -- measured, that put an endpoint within 0.67 deg of the
  // tolerance Newton's method demands. Refining first makes the endpoints' distance from
  // conjunction a property of `BRACKET_HALF_WIDTH_DAYS` instead, so the bracket the solver
  // accepts is one this function builds by construction rather than by luck.
  const double root_est = est_jde - (signed_miss_deg / deg_per_day);
  return { root_est - BRACKET_HALF_WIDTH_DAYS, root_est + BRACKET_HALF_WIDTH_DAYS };
}


/**
 * @brief Find the next root jde.
 * @param jde The jde. This is expected to be a root.
 * @return The next root jde.
 */
[[nodiscard]] inline auto next_root(const double jde) -> double {
  const double jde_lon_diff = longitude_diff(jde);
  if (1.0 < jde_lon_diff and jde_lon_diff < 359.0) [[unlikely]] {
    throw std::invalid_argument {
      std::format("The jde {} is not a root.", jde)
    };
  }

  const auto next_root_range = first_root_range_after(jde + 1.0); // Add 1.0 in case `jde_lon_diff` falls into [359.0, 360.0).
  const auto [left, right] = next_root_range;
  return newton_method(left, right);
}

/**
 * @brief Generator for finding the roots (i.e. conjunction moments of the Sun and Moon).
 */
// TODO: Use `std::generator` once every CI leg has it (./linter.py --features).
struct RootGenerator {
private:
  double _root;

public:
  explicit RootGenerator(const double start_jde) {
    const auto [left, right] = first_root_range_after(start_jde);
    const double first_root = newton_method(left, right);
    _root = first_root;
  }

  auto next() -> double {
    const double root = _root;
    _root = next_root(_root);
    return root;
  }
};


/**
 * @brief Calculate conjunctions moments of the Sun and Moon in a given Gregorian year.
          计算某一个公历年中日月合朔的时刻。
 * @param year The Gregorian year.
 * @return The vector of the conjunction moments, in JDE (Julian Ephemeris Day).
 * @note The year runs from Jan 1 to Jan 1 in UTC; before 1972 the bounds degrade to UT1.
 * @details The Sun's position is calculated using VSOP87D,
 * @details The Moon's position is calculated using truncated ELP2000-82B.
 * @see VSOP87D, ELP2000-82B, and Astronomical Algorithms, Jean Meeus, 1998.
 */
[[nodiscard]] inline auto moments(const int32_t year) -> std::vector<double> {
  // The first moment of the year, inclusive.
  const calendar::Datetime start_moment_utc {
    util::to_ymd(year, 1, 1),
    0.0,
  };

  // The last moment of the year, exclusive.
  const calendar::Datetime end_moment_utc {
    util::to_ymd(year + 1, 1, 1),
    0.0,
  };

  // #84: the bounds are UTC, not UT1 — a conjunction between the two midnights used to be
  // attributed to the neighbouring year.
  const auto start_jde = astro::julian_day::utc_to_jde(start_moment_utc);
  const auto end_jde = astro::julian_day::utc_to_jde(end_moment_utc);

  RootGenerator gen(start_jde);
  std::vector<double> roots;

  while (true) {
    const auto root = gen.next();
    if (root >= end_jde) {
      break;
    }

    roots.push_back(root);
  }

  return roots;
}

} // namespace astro::moon_phase::new_moon
