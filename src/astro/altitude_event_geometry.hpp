/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace astro::detail::altitude_bounds::observer_c2 {

using Derivatives = std::array<double, 3>;

/** @brief Uniform source bounds through order two, per TT day; distance is AU. */
struct Source {
  Derivatives direction;
  Derivatives distance_au;
  double distance_lower_au;
  double rotation_first_rad;
  double rotation_second_rad;
  double earth_radius_au;
};

/** @brief WGS84 site/latitude magnitude uppers and a positive dimensionless translated-norm lower. */
struct Site {
  double equatorial_upper;
  double polar_upper;
  double cos_latitude_upper;
  double sin_latitude_upper;
  double q_lower;
};

/** @brief Local-vector and altitude-residual derivative bounds, per TT day. */
struct Bounds {
  Derivatives local_vector;
  double first;
  double second;
};

/** @brief Outward sum of nonnegative uppers, preserving exact zero. */
[[nodiscard]] inline auto add(const double left, const double right) -> double {
  return left == 0.0 and right == 0.0 ? 0.0 : std::nextafter(left + right, std::numeric_limits<double>::infinity());
}

/** @brief Outward product of nonnegative uppers, preserving exact zero. */
[[nodiscard]] inline auto multiply(const double left, const double right) -> double {
  return left == 0.0 or right == 0.0 ? 0.0 : std::nextafter(left * right, std::numeric_limits<double>::infinity());
}

/** @brief Divide a nonnegative upper by a positive lower, rounding outward. */
[[nodiscard]] inline auto divide(const double value, const double lower) -> double {
  return value == 0.0 ? 0.0 : std::nextafter(value / lower, std::numeric_limits<double>::infinity());
}

/** @brief Scale three nonnegative derivative uppers outward. */
[[nodiscard]] inline auto scale(const double factor, const Derivatives& values) -> Derivatives {
  return { multiply(factor, values.at(0)), multiply(factor, values.at(1)), multiply(factor, values.at(2)) };
}

/** @brief Earth-radius/distance derivatives use only positive lowers in denominators. */
[[nodiscard]] inline auto parallax(const Source& source) -> Derivatives {
  const double reciprocal = divide(1.0, source.distance_lower_au);
  const double first = divide(multiply(source.distance_au.at(1), reciprocal), source.distance_lower_au);
  const double second = divide(
    add(multiply(source.distance_au.at(2), reciprocal), multiply(2.0, multiply(source.distance_au.at(1), first))),
    source.distance_lower_au
  );
  return scale(source.earth_radius_au, { reciprocal, first, second });
}

/**
 * @brief Bound up-s*q and the translated vector on the complete source box, where s is constant.
 * @note q²=1+p²|k|²-2p(k·w) removes daily rotation from the untranslated unit-vector norm.
 * @see statistics/altitude_bounds/observer_geometry_majorants.py, projected_objective_bounds.
 */
[[nodiscard]] inline auto compose(const Source& source, const Site& site, const double sine_upper) -> Bounds {
  const auto nonnegative = [](const double value) { return std::isfinite(value) and value >= 0.0; };
  const bool scalars_valid = std::ranges::all_of(
    std::array { source.rotation_first_rad, source.rotation_second_rad, source.earth_radius_au,
      site.equatorial_upper, site.polar_upper, site.cos_latitude_upper, site.sin_latitude_upper, sine_upper },
    nonnegative
  );
  const bool source_valid = std::ranges::all_of(source.direction, nonnegative)
    and std::ranges::all_of(source.distance_au, nonnegative) and source.direction.at(0) == 1.0
    and std::isfinite(source.distance_lower_au) and source.distance_lower_au > 0.0
    and source.distance_au.at(0) >= source.distance_lower_au;
  const bool site_valid = std::isfinite(site.q_lower) and site.q_lower > 0.0
    and site.cos_latitude_upper <= 1.0 and site.sin_latitude_upper <= 1.0 and sine_upper <= 1.0;
  if (not scalars_valid or not source_valid or not site_valid) {
    throw std::runtime_error { "The observer C2 source certificate is invalid" };
  }

  const double d1 = source.direction.at(1);
  const double d2 = source.direction.at(2);
  const double theta1 = source.rotation_first_rad;
  const double rotated1 = add(theta1, d1);
  const double rotated2 = add(add(source.rotation_second_rad, multiply(theta1, theta1)), add(multiply(2.0, multiply(theta1, d1)), d2));
  const auto p = parallax(source);
  const double p0 = p.at(0);
  const double p1 = p.at(1);
  const double p2 = p.at(2);
  const double a = site.equatorial_upper;
  const double b = site.polar_upper;
  const double site_l1 = add(a, b);
  const double vector0 = add(1.0, multiply(site_l1, p0));
  const double vector1 = add(rotated1, multiply(site_l1, p1));
  const double vector2 = add(rotated2, multiply(site_l1, p2));
  if (site.q_lower > vector0) {
    throw std::runtime_error { "The observer norm lower exceeds its complete vector upper" };
  }
  const double generic1 = multiply(add(1.0, sine_upper), vector1);
  const double generic2 = add(multiply(add(1.0, sine_upper), vector2), multiply(sine_upper, divide(multiply(vector1, vector1), site.q_lower)));

  const double c = site.cos_latitude_upper;
  const double s = site.sin_latitude_upper;
  const double projected_site = add(multiply(c, a), multiply(s, b));
  const double up1 = add(add(multiply(c, rotated1), multiply(s, d1)), multiply(projected_site, p1));
  const double up2 = add(add(multiply(c, rotated2), multiply(s, d2)), multiply(projected_site, p2));
  const double k1 = add(multiply(a, rotated1), multiply(b, d1));
  const double k2 = add(multiply(a, rotated2), multiply(b, d2));
  const double site_squared = add(multiply(a, a), multiply(b, b));
  const double squared1 = add(
    multiply(site_squared, multiply(2.0, multiply(p0, p1))),
    multiply(2.0, add(multiply(p1, site_l1), multiply(p0, k1)))
  );
  const double squared2 = add(
    multiply(site_squared, multiply(2.0, add(multiply(p0, p2), multiply(p1, p1)))),
    multiply(2.0, add(add(multiply(p2, site_l1), multiply(2.0, multiply(p1, k1))), multiply(p0, k2)))
  );
  const double norm1 = divide(divide(squared1, site.q_lower), 2.0);
  const double norm2 = add(
    divide(divide(squared2, site.q_lower), 2.0),
    divide(divide(divide(divide(multiply(squared1, squared1), site.q_lower), site.q_lower), site.q_lower), 4.0)
  );
  const double first = std::min(generic1, add(up1, multiply(sine_upper, norm1)));
  const double second = std::min(generic2, add(up2, multiply(sine_upper, norm2)));
  const Derivatives vector { vector0, vector1, vector2 };
  if (not std::isfinite(first) or not std::isfinite(second) or not std::ranges::all_of(vector, nonnegative)) {
    throw std::runtime_error { "The observer C2 certificate overflowed" };
  }
  return { .local_vector = vector, .first = first, .second = second };
}

} // namespace astro::detail::altitude_bounds::observer_c2
