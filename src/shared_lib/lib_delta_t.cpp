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
#include "lib_codes.hpp"
#include "celestial.h"

#include "delta_t.hpp"

extern "C" {

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Evaluation year precedes its model selector in the published ABI.
auto celestial_delta_t(const double year, const uint32_t model_code) -> CelestialDeltaT {
  return lib::wrap_export(__func__, [=]() -> CelestialDeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument { std::format("Argument `year` is not finite, got {}", year) };
    }
    const auto model = lib::codes::delta_t_model(model_code);
    return {
      .valid = true,
      .model_identity_code = lib::codes::code_of(model),
      .value = astro::delta_t::compute(model, year),
    };
  });
}

} // extern "C"
