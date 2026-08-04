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
  // Validate the input.
  if (jq_idx >= calendar::jieqi::JIEQI_COUNT) [[unlikely]] {
    return {};
  }

  using namespace calendar::jieqi;

  try {
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
    
  } catch (const std::exception& e) {
    lib::info("Error in query_jieqi_moment: {}", e.what());

    return {};
  } catch (...) {
    return {};
  }
}


auto get_jieqi_name(const uint8_t jq_idx, char * const buf, const uint32_t buf_size) -> bool {
  // Validate the input.
  if (jq_idx >= calendar::jieqi::JIEQI_COUNT) [[unlikely]] {
    lib::info("Error in get_jieqi_name: jq_idx is {}, but expected to be in the range [0, 24).", jq_idx);
    return false;
  }
  if (buf == nullptr) {
    lib::info("Error in get_jieqi_name: `buf` is null.");
    return false;
  }

  try {
    using namespace calendar::jieqi;
    const std::string_view name = name_of(from_index(jq_idx));

    // Check if the buffer is large enough to hold the name and the null terminator
    if (buf_size < name.size() + 1) {
      lib::info("Error in get_jieqi_name: provided buffer is too small. Required {}, actual {}.", name.size() + 1, buf_size);
      return false;
    }

    // Copy the name to the buffer
    std::memcpy(buf, name.data(), name.size());
    buf[name.size()] = '\0'; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    return true;
  } catch (...) {
    return false;
  }
}

}
