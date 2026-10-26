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

#include <cmath>
#include <format>
#include <stdexcept>
#include <string_view>

#include "toolbox.hpp"

namespace astro {

/**
 * @brief An observer's geographic location on the Earth.
 * @note `latitude` is positive north in [-90°, 90°]. `longitude` is positive east of Greenwich
 *       in [-180°, 180°], following the modern/ISO 6709 convention.
 */
struct GeoLocation {
  toolbox::AngleDeg latitude;
  toolbox::AngleDeg longitude;
};

namespace geolocation {

/** @brief Reject a non-finite geographic longitude or one outside [-180°, 180°], preserving the caller's name. */
inline auto validate_longitude(const toolbox::AngleDeg longitude, const std::string_view name) -> void {
  const double longitude_deg = longitude.deg();
  if (not std::isfinite(longitude_deg) or longitude_deg < -180.0 or longitude_deg > 180.0) {
    throw std::invalid_argument {
      std::format("Argument `{}` out of range [-180, 180], got {}", name, longitude_deg)
    };
  }
}

} // namespace geolocation

} // namespace astro
