/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
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

// Shared helpers for the rise_set test family (#62: four test files had grown four copies
// of these, and `cell_minutes` had already diverged — one parsed HH:MM:SS, another only
// HH:MM). Pure functions only; per-file constants stay in their own anonymous namespaces.

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "rise_set.hpp"

namespace astro::rise_set::test {

/** @brief Build a GeoLocation from degrees (east-positive longitude). */
constexpr auto loc(const double lat_deg, const double lon_deg) -> GeoLocation {
  return { .latitude = astro::toolbox::AngleDeg { lat_deg },
           .longitude = astro::toolbox::AngleDeg { lon_deg } };
}

/**
 * @brief Checked unwrap for assertions. clang-tidy's bugprone-unchecked-optional-access cannot
 *        see through gtest's ASSERT_TRUE, so tests unwrap through this provably-guarded helper
 *        rather than NOLINT-ing every access.
 */
template <typename T>
inline auto req(const std::optional<T>& opt) -> const T& {
  if (not opt.has_value()) {
    throw std::logic_error { "expected optional to hold a value" };
  }
  return *opt;
}

/** @brief Parse "HH:MM" / "HH:MM:SS" into minutes-of-day; blank cell → `nullopt`. */
inline auto cell_minutes(const std::string_view cell) -> std::optional<double> {
  const auto first = cell.find_first_not_of(' ');
  if (first == std::string_view::npos) {
    return std::nullopt;
  }

  if (cell.size() - first < 5 or cell[first + 2] != ':') {
    throw std::invalid_argument { "malformed golden cell: " + std::string { cell } };
  }

  const auto digits = [&](const size_t pos) { return (10.0 * (cell[pos] - '0')) + (cell[pos + 1] - '0'); };
  double minutes = (60.0 * digits(first)) + digits(first + 3);

  const auto second_colon = cell.find(':', first + 3);
  if (second_colon != std::string_view::npos) {
    if (second_colon != first + 5 or cell.size() - first < 8) {
      throw std::invalid_argument { "malformed golden cell: " + std::string { cell } };
    }
    minutes += digits(first + 6) / 60.0;
  }

  return minutes;
}

/** @brief |a − b| in minutes on the 24h circle, so a source's day and ours may differ. */
inline auto clock_diff(const double a, const double b) -> double {
  const double diff = std::fabs(a - b);
  return std::min(diff, 1440.0 - diff);
}

} // namespace astro::rise_set::test
