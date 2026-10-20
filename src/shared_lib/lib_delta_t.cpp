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

#include "delta_t.hpp"

namespace {

using enum astro::delta_t::Model;

// The six exports share one body; only the model template argument differs.
template <astro::delta_t::Model model>
[[nodiscard]] auto delta_t_export(const char* const operation, const double year) -> DeltaT {
  return lib::wrap_export(operation, [=]() -> DeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument {
        std::format("Argument `year` is not finite, got {}", year)
      };
    }

    return {
      .valid = true,
      .value = astro::delta_t::compute(model, year),
    };
  });
}

} // namespace

extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

auto delta_t_algo1(double year) -> DeltaT {
  return delta_t_export<ALGO1>(__func__, year);
}

auto delta_t_algo2(double year) -> DeltaT {
  return delta_t_export<ALGO2>(__func__, year);
}

auto delta_t_algo3(double year) -> DeltaT {
  return delta_t_export<ALGO3>(__func__, year);
}

auto delta_t_algo4(double year) -> DeltaT {
  return delta_t_export<ALGO4>(__func__, year);
}

auto delta_t_algo5(double year) -> DeltaT {
  return delta_t_export<ALGO5>(__func__, year);
}

auto delta_t(double year) -> DeltaT {
  return delta_t_export<ALGO5>(__func__, year);
}

}
