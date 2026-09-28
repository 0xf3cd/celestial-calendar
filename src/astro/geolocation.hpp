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

#include "toolbox.hpp"

namespace astro {

/**
 * @brief An observer's geographic location on the Earth.
 * @note `latitude` is positive north in [-90°, 90°]. `longitude` is positive east of Greenwich
 *       in [-180°, 180°], following the modern/ISO 6709 convention.
 */
struct GeoLocation {
  astro::toolbox::AngleDeg latitude;
  astro::toolbox::AngleDeg longitude;
};

} // namespace astro
