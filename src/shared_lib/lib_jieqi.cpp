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

#include <format>
#include <string_view>

#include "lib.hpp"
#include "celestial.h"

#include "jieqi.hpp"

extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

auto celestial_query_jieqi_moment(const int32_t year, const uint8_t jq_idx) -> CelestialJieqiMomentQuery { // NOLINT(bugprone-easily-swappable-parameters): year/index is the published query order.
  return lib::wrap_export(__func__, [=]() -> CelestialJieqiMomentQuery {
    if (jq_idx >= calendar::jieqi::JIEQI_COUNT) [[unlikely]] {
      throw std::invalid_argument {
        std::format("Argument `jq_idx` must be in [0, 23], got {}", jq_idx)
      };
    }

    using namespace calendar::jieqi;
    const auto jq = from_index(jq_idx);

    const calendar::Datetime ut1_dt = jieqi_ut1_moment(year, jq);

    const auto [y, m, d] = util::from_ymd(ut1_dt.ymd);
    const double fraction = ut1_dt.fraction();

    return {
      .valid  = true,
      .jq_idx = jq_idx,
      .y      = y,
      .m      = m,
      .d      = d,
      .frac   = fraction,
    };
  });
}


auto celestial_get_jieqi_name(const uint8_t jq_idx, char * const buf, const uint32_t buf_size) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    if (jq_idx >= calendar::jieqi::JIEQI_COUNT) [[unlikely]] {
      throw std::invalid_argument {
        std::format("Argument `jq_idx` must be in [0, 23], got {}", jq_idx)
      };
    }
    using namespace calendar::jieqi;
    const std::string_view name = name_of(from_index(jq_idx));
    return lib::copy_text(name, buf, buf_size);
  });
} // extern "C"

}
