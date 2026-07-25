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

#include <vector>
#include <format>

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
inline auto longitude_diff(const double jde) -> double {
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
constexpr double BRACKET_TOLERANCE_DEG = 15.0;

/**
 * @brief Apply Newton's method to find the jde, when the Sun and Moon are at the same apparent longitude.
 * @param left_jde The left bound of the search, inclusive.
 * @param right_jde The right bound of the search, exclusive.
 * @param iterations The maximum number of iterations.
 * @return The jde of the conjunction.
 * @note It is the caller's responsibility to ensure the root exists in the range of [left_jde, right_jde).
 * @throw std::invalid_argument If no root exists in the range of [left_jde, right_jde).
 */
inline auto newton_method(
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
 * @brief Approximate the range of the first root after the given jde, when the Sun and Moon are at the same apparent longitude.
 * @param jde The jde.
 * @return The range of the first root after the given `jde`.
 */
inline auto first_root_range_after(const double jde) -> std::pair<double, double> {
  const double cur_diff = longitude_diff(jde);
  const double gap = 360.0 - cur_diff;

  constexpr double deg_per_day = astro::toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY;
  const double est_jde = jde + gap / deg_per_day; // Estimate the next root jde.

  // Extrapolating at the mean rate lands near the root but not on it: the Moon's true rate runs
  // between about 10.5 and 14.5 deg/day, so a month of extrapolation can be off by a few degrees
  // either way. The branches below accept anything within 30 deg of conjunction.
  const double est_jde_diff = longitude_diff(est_jde);

  if (est_jde_diff == 0.0) [[unlikely]] {
    return { est_jde - 0.1, est_jde + 0.1 };
  } 
  
  if (est_jde_diff < 30.0) {
    const double left_bound = est_jde - (est_jde_diff * 2 / deg_per_day);
    return { left_bound, est_jde };
  }

  if (est_jde_diff > 330.0) {
    const double right_bound = est_jde + ((360.0 - est_jde_diff) * 2 / deg_per_day);
    return { est_jde, right_bound };
  }

  throw std::invalid_argument {
    std::format(
      "Cannot find the first root after jde {}: the estimate at jde {} sits {} deg from conjunction.",
      jde, est_jde, est_jde_diff
    )
  };
}


/**
 * @brief Find the next root jde.
 * @param jde The jde. This is expected to be a root.
 * @return The next root jde.
 */
inline auto next_root(const double jde) -> double {
  const double jde_lon_diff = longitude_diff(jde);
  if (1.0 < jde_lon_diff and jde_lon_diff < 359.0) [[unlikely]] {
    throw std::invalid_argument {
      std::format("The jde {} is not a root.", jde)
    };
  }

  const auto next_root_range = first_root_range_after(jde + 1.0); // Add 1.0 in case `jde_lon_diff` falls into [359.0, 360.0).
  const auto [left, right] = next_root_range;
  return newton_method(left, right);
};

/**
 * @brief Generator for finding the roots (i.e. conjunction moments of the Sun and Moon).
 */
// TODO: Use `std::generator` when supported.
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
 * @details The Sun's position is calculated using VSOP87D, 
 * @details The Moon's position is calculated using truncated ELP2000-82B.
 * @see VSOP87D, ELP2000-82B, and Astronomical Algorithms, Jean Meeus, 1998.
 */
inline auto moments(const int32_t year) -> std::vector<double> {
  // The first moment of the year, inclusive.
  const calendar::Datetime start_moment {
    util::to_ymd(year, 1, 1),
    0.0,
  };

  // The last moment of the year, exclusive.
  const calendar::Datetime end_moment {
    util::to_ymd(year + 1, 1, 1),
    0.0,
  };

  // Convert to JDEs.
  // TODO: Use `utc_to_jde` when supported.
  const auto start_jde = astro::julian_day::ut1_to_jde(start_moment);
  const auto end_jde = astro::julian_day::ut1_to_jde(end_moment);

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
