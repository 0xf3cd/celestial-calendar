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

#include <optional>
#include <stdexcept>

namespace util::test {

/** @brief Unwrap an expected value with a guard visible to clang-tidy, unlike a gtest assertion. */
template <typename T>
[[nodiscard]] inline auto req(const std::optional<T>& value) -> const T& {
  if (not value.has_value()) {
    throw std::logic_error { "expected optional to hold a value" };
  }
  return *value;
}

} // namespace util::test
