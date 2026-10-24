/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#include "lib.hpp"
#include "celestial.h"

namespace lib {

namespace {
thread_local std::string LAST_ERROR; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
thread_local ErrorKind LAST_ERROR_KIND = ErrorKind::NONE; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

auto clear_last_error() noexcept -> void {
  LAST_ERROR.clear();
  LAST_ERROR_KIND = ErrorKind::NONE;
}

auto set_last_error(const ErrorKind kind, const std::string_view message) noexcept -> void {
  LAST_ERROR_KIND = kind;
  try {
    LAST_ERROR = message;
  } catch (...) { // NOLINT(bugprone-empty-catch) — nowhere left to report; swallowing is the contract.
  }
}

auto set_unknown_error(const std::string_view operation) noexcept -> void {
  LAST_ERROR_KIND = ErrorKind::INTERNAL;
  try {
    LAST_ERROR = "Unknown error in ";
    LAST_ERROR += operation;
    LAST_ERROR += '.';
  } catch (...) { // NOLINT(bugprone-empty-catch) — nowhere left to report; swallowing is the contract.
  }
}

auto last_error_message() noexcept -> const char* {
  return LAST_ERROR.c_str();
}

auto last_error_kind() noexcept -> ErrorKind { return LAST_ERROR_KIND; }

} // namespace lib


extern "C" {

// Contract: see celestial.h.
auto celestial_last_error() -> const char* {
  return lib::last_error_message();
}

auto celestial_last_error_kind() -> uint32_t {
  return static_cast<uint32_t>(lib::last_error_kind());
}

static_assert(static_cast<uint32_t>(lib::ErrorKind::NONE) == CELESTIAL_ERROR_NONE);
static_assert(static_cast<uint32_t>(lib::ErrorKind::INVALID_INPUT) == CELESTIAL_ERROR_INVALID_INPUT);
static_assert(static_cast<uint32_t>(lib::ErrorKind::COMPUTATION) == CELESTIAL_ERROR_COMPUTATION);
static_assert(static_cast<uint32_t>(lib::ErrorKind::RESOURCE) == CELESTIAL_ERROR_RESOURCE);
static_assert(static_cast<uint32_t>(lib::ErrorKind::INTERNAL) == CELESTIAL_ERROR_INTERNAL);

}
