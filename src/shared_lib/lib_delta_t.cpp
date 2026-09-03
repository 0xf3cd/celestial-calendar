/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
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

extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

auto delta_t_algo1(double year) -> DeltaT {
  return lib::wrap_export("delta_t_algo1", [=]() -> DeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument {
        std::format("Argument `year` is not finite, got {}", year)
      };
    }

    return {
      .valid = true,
      .value = astro::delta_t::algo1::compute(year),
    };
  });
}

auto delta_t_algo2(double year) -> DeltaT {
  return lib::wrap_export("delta_t_algo2", [=]() -> DeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument {
        std::format("Argument `year` is not finite, got {}", year)
      };
    }

    return {
      .valid = true,
      .value = astro::delta_t::algo2::compute(year),
    };
  });
}

auto delta_t_algo3(double year) -> DeltaT {
  return lib::wrap_export("delta_t_algo3", [=]() -> DeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument {
        std::format("Argument `year` is not finite, got {}", year)
      };
    }

    return {
      .valid = true,
      .value = astro::delta_t::algo3::compute(year),
    };
  });
}

auto delta_t_algo4(double year) -> DeltaT {
  return lib::wrap_export("delta_t_algo4", [=]() -> DeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument {
        std::format("Argument `year` is not finite, got {}", year)
      };
    }

    return {
      .valid = true,
      .value = astro::delta_t::algo4::compute(year),
    };
  });
}

auto delta_t_algo5(double year) -> DeltaT {
  return lib::wrap_export("delta_t_algo5", [=]() -> DeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument {
        std::format("Argument `year` is not finite, got {}", year)
      };
    }

    return {
      .valid = true,
      .value = astro::delta_t::algo5::compute(year),
    };
  });
}

auto delta_t(double year) -> DeltaT {
  return lib::wrap_export("delta_t", [=]() -> DeltaT {
    if (not std::isfinite(year)) {
      throw std::invalid_argument {
        std::format("Argument `year` is not finite, got {}", year)
      };
    }

    return {
      .valid = true,
      .value = astro::delta_t::compute(year),
    };
  });
}   

}
