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

#include <chrono>
#include <cstdint>
#include <format>
#include <stdexcept>

#include "celestial.h"
#include "datetime.hpp"
#include "julian_day.hpp"
#include "ymd.hpp"

namespace lib::civil {

/** @brief Validate raw civil fields before chrono narrows month/day to their stored representation. */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): year/month/day/fraction is the published civil-date order.
inline auto validate(const int32_t year, const uint32_t month, const uint32_t day, const double fraction) -> void {
  const auto civil_year = util::checked_year(year);
  if (month < 1 or month > 12) {
    throw std::invalid_argument { std::format("Argument `month` must be in [1, 12], got {}", month) };
  }

  const auto month_end = civil_year / std::chrono::month { month } / std::chrono::last;
  const uint32_t last_day = static_cast<unsigned>(month_end.day());
  if (day < 1 or day > last_day) {
    throw std::invalid_argument {
      std::format("Argument `day` must be in [1, {}] for {}-{}, got {}", last_day, year, month, day)
    };
  }

  static_cast<void>(calendar::validate_fraction(fraction));
}

/** @brief Decode the explicit civil input scale without imposing a consumer's date window. */
[[nodiscard]] inline auto scale(const uint32_t code) -> astro::julian_day::Scale {
  switch (code) {
    case CELESTIAL_CIVIL_SCALE_UTC: return astro::julian_day::Scale::UTC;
    case CELESTIAL_CIVIL_SCALE_UT1: return astro::julian_day::Scale::UT1;
    default: throw std::invalid_argument { std::format("Unknown `civil_scale_code` {}", code) };
  }
}

} // namespace lib::civil
