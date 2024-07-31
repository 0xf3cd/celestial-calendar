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
#include <numbers>

#include "vsop87d/defines.hpp"

namespace astro::toolbox {

#pragma region Angle Stuff

/** @brief Normalize degree to [0, 360). */
constexpr auto normalize_deg(const double deg) -> double {
  const double _deg = std::remainder(deg, 360.0);
  return _deg < 0.0 ? _deg + 360.0 : _deg; 
}

/** @brief Normalize radian to [0, 2π). */
constexpr auto normalize_rad(const double rad) -> double {
  const double _rad = std::remainder(rad, 2.0 * std::numbers::pi);
  return _rad < 0.0 ? _rad + 2.0 * std::numbers::pi : _rad;
}

/** @brief The number of degrees in a radian. */
constexpr double DEG_PER_RAD = 180.0 / std::numbers::pi;

/** @brief Convert degree to radian. */
constexpr auto deg_to_rad(const double deg) -> double {
  return deg / DEG_PER_RAD;
}

/** @brief Convert radian to degree. */
constexpr auto rad_to_deg(const double rad) -> double {
  return rad * DEG_PER_RAD;
}

/** @brief The number of minutes in a degree. */
constexpr uint32_t MIN_PER_DEG = 60;

/** @brief The number of seconds in a minute. */
constexpr uint32_t SEC_PER_MIN = 60;

/** @brief The number of seconds in a degree. */
constexpr uint32_t SEC_PER_DEG = SEC_PER_MIN * MIN_PER_DEG;

/** @brief Convert minutes to degrees. */
constexpr auto arcmin_to_deg(const double arcmin) -> double {
  return arcmin / MIN_PER_DEG;
}

/** @brief Convert seconds to degrees. */
constexpr auto arcsec_to_deg(const double arcsec) -> double {
  return arcsec / SEC_PER_DEG;
}


/** @enum AngleUnit the angle's unit, either degree or radian. */
enum class AngleUnit : uint8_t { RAD, DEG };

/** 
 * @struct Represents an angle. 
 * @tparam Unit The angle's unit, either degree or radian.
 */
template <AngleUnit Unit>
struct Angle {
  const double _value;

  constexpr Angle(const double value) : _value { value } {} // NOLINT(google-explicit-constructor)

  constexpr static auto from_arcmin(const double value) -> Angle<AngleUnit::DEG> {
    return { arcmin_to_deg(value) };
  }

  constexpr static auto from_arcsec(const double value) -> Angle<AngleUnit::DEG> {
    return { arcsec_to_deg(value) };
  }

  /** @brief Allow implicit conversion to the other unit. */
  template <AngleUnit As>
  constexpr operator Angle<As>() const { // NOLINT(google-explicit-constructor)
    return { as<As>() };
  }

  constexpr auto operator+(const Angle<Unit>& other) const -> Angle<Unit> {
    return { _value + other._value };
  }

  constexpr auto operator+(const double other) const -> Angle<Unit> {
    return { _value + other };
  }

  constexpr auto operator-(const Angle<Unit>& other) const -> Angle<Unit> {
    return { _value - other._value };
  }

  constexpr auto operator-(const double other) const -> Angle<Unit> {
    return { _value - other };
  }

  constexpr auto operator-() const -> Angle<Unit> {
    return { -_value };
  }

  constexpr auto operator*(const double other) const -> Angle<Unit> {
    return { _value * other };
  }

  constexpr auto operator/(const double other) const -> Angle<Unit> {
    if (other == 0.0) {
      throw std::runtime_error { "Division by zero." };
    }
    return { _value / other };
  }

  /**
   * @brief Convert the angle to another unit.
   * @param As The unit to convert to.
   * @return The converted angle.
   */
  template <AngleUnit As>
  [[nodiscard]] [[nodiscard]] [[nodiscard]] constexpr auto as() const -> double {
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
  [[nodiscard]] constexpr auto normalize() const -> Angle<Unit> {
    if constexpr (Unit == AngleUnit::DEG) {
      return { normalize_deg(_value) };
    } else {
      return { normalize_rad(_value) };
    }
  }

  /** @brief Return the angle in degrees. */
  [[nodiscard]] constexpr auto deg() const -> double {
    return as<AngleUnit::DEG>();
  }

  /** @brief Return the angle in radians. */
  [[nodiscard]] constexpr auto rad() const -> double {
    return as<AngleUnit::RAD>();
  }
};

#pragma endregion


#pragma region Literals

namespace literals {

inline auto operator"" _deg(const long double value) -> Angle<AngleUnit::DEG> {
  return { static_cast<double>(value) };
}

inline auto operator"" _arcmin(const long double value) -> Angle<AngleUnit::DEG> {
  return Angle<AngleUnit::DEG>::from_arcmin(static_cast<double>(value));
}

inline auto operator"" _arcsec(const long double value) -> Angle<AngleUnit::DEG> {
  return Angle<AngleUnit::DEG>::from_arcsec(static_cast<double>(value));
}

inline auto operator"" _rad(const long double value) -> Angle<AngleUnit::RAD> {
  return { static_cast<double>(value) };
}

}  // namespace literals

#pragma endregion


#pragma region Coordinate Definitions

/**
 * @brief Represents a position in a spherical coordinate system.
 */
struct SphericalCoordinate {
  const Angle<AngleUnit::DEG> λ; // Longitude
  const Angle<AngleUnit::DEG> β; // Latitude
  const double r;                // Radius, In AU
};

#pragma endregion

} // namespace astro::toolbox
