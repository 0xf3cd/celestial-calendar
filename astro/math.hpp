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

#include <cmath>
#include <numbers>

namespace astro::math {

/** @brief Normalize degree to [0, 360). */
constexpr double normalize_deg(const double deg) {
  const double __deg = std::remainder(deg, 360.0);
  return __deg < 0.0 ? __deg + 360.0 : __deg; 
}

/** @brief Normalize radian to [0, 2π). */
constexpr double normalize_rad(const double rad) {
  const double __rad = std::remainder(rad, 2.0 * std::numbers::pi);
  return __rad < 0.0 ? __rad + 2.0 * std::numbers::pi : __rad;
}

/** @brief Convert degree to radian. */
constexpr double deg_to_rad(const double deg) {
  return deg * std::numbers::pi / 180.0;
}

/** @brief Convert radian to degree. */
constexpr double rad_to_deg(const double rad) {
  return rad * 180.0 / std::numbers::pi;
}


/** @enum AngleUnit the angle's unit, either degree or radian. */
enum class AngleUnit { DEG, RAD, };


/** 
 * @struct Represents an angle. 
 * @tparam Unit The angle's unit, either degree or radian.
 */
template <AngleUnit Unit>
struct Angle {
  const double _value;

  constexpr Angle(const double value) : _value { value } {}

  /** @brief Allow implicit conversion to the other unit. */
  template <AngleUnit As>
  constexpr operator Angle<As>() const {
    return Angle<As> { as<As>() };
  }

  constexpr Angle<Unit> operator+(const Angle<Unit>& other) const {
    return Angle<Unit> { _value + other._value };
  }

  constexpr Angle<Unit> operator+(const double other) const {
    return Angle<Unit> { _value + other };
  }

  constexpr Angle<Unit> operator-(const Angle<Unit>& other) const {
    return Angle<Unit> { _value - other._value };
  }

  constexpr Angle<Unit> operator-(const double other) const {
    return Angle<Unit> { _value - other };
  }

  constexpr Angle<Unit> operator-() const {
    return Angle<Unit> { -_value };
  }

  /**
   * @brief Convert the angle to another unit.
   * @param As The unit to convert to.
   * @return The converted angle.
   */
  template <AngleUnit As>
  constexpr double as() const {
    if constexpr (Unit == As) { // No conversion needed.
      return _value;
    }

    if constexpr (As == AngleUnit::DEG) {
      return rad_to_deg(_value);
    } else {
      return deg_to_rad(_value);
    }
  }

  /**
   * @brief Normalize the angle to [0, 360) / [0, 2π), depending on the angle's unit.
   * @return The normalized angle. The returned angle is of the same unit as the original angle.
   */
  constexpr Angle<Unit> normalize() const {
    if constexpr (Unit == AngleUnit::DEG) {
      return { normalize_deg(_value) };
    } else {
      return { normalize_rad(_value) };
    }
  }
};


/**
 * @brief Represents a position in a spherical coordinate system.
 */
struct SphericalPosition {
  const Angle<AngleUnit::DEG> lon; // Longitude
  const Angle<AngleUnit::DEG> lat; // Latitude
  const double r;                  // Radious, In AU
};


} // namespace astro::math
