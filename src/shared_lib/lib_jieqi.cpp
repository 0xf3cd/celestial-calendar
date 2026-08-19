/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
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

#include <cstring>

#include "lib.hpp"
#include "celestial.h"

#include "jieqi.hpp"

extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

auto query_jieqi_moment(const int32_t year, const uint8_t jq_idx) -> JieqiMomentQuery { // NOLINT(bugprone-easily-swappable-parameters)
  return lib::wrap_export("query_jieqi_moment", [=]() -> JieqiMomentQuery {
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


auto get_jieqi_name(const uint8_t jq_idx, char * const buf, const uint32_t buf_size) -> bool {
  return lib::wrap_export("get_jieqi_name", [=] {
    if (jq_idx >= calendar::jieqi::JIEQI_COUNT) [[unlikely]] {
      throw std::invalid_argument {
        std::format("Argument `jq_idx` must be in [0, 23], got {}", jq_idx)
      };
    }
    if (buf == nullptr) {
      throw std::invalid_argument { "Argument `buf` is null." };
    }

    using namespace calendar::jieqi;
    const std::string_view name = name_of(from_index(jq_idx));

    if (buf_size < name.size() + 1) {
      throw std::invalid_argument {
        std::format("Argument `buf_size` must be at least {}, got {}", name.size() + 1, buf_size)
      };
    }

    std::memcpy(buf, name.data(), name.size());
    buf[name.size()] = '\0'; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    return true;
  });
}

}
