/*
 * CelestialCalendar: 
 *   A C++23-style library for date conversions and astronomical calculations for various calendars,
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

#include <print>

#include "jieqi.hpp"


extern "C" {

using namespace calendar::jieqi;

/**
 * @brief Calculate the JDE discriminant, which is the count of the roots for the given `year` and `lon`.
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @returns The count of the roots, which is 0, 1, or 2.
 *          0 indicates that Sun won't reach the given geocentric longitude in the given year.
 *          1 indicates that Sun will reach the given geocentric longitude once in the given year.
 *          2 indicates that Sun will reach the given geocentric longitude twice in the given year.
 */
uint32_t jde_discriminant(const int32_t year, const double longitude) {
  return math::discriminant(year, longitude);
}


/**
 * @brief Find the JDE(s) at which the Sun reaches the given `longitude` in the given `year`.
 *        The result is written to the provided slots. It's caller's responsibility to allocate and free the slots.
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @param slots The slots. It's caller's responsibility to allocate and free the slots.
 * @param slot_count The count of slots.
 */
void find_jdes(
  const int32_t year, 
  const double longitude, 
  double * const slots, 
  const uint32_t slot_count
) {
  const uint32_t root_count = math::discriminant(year, longitude);
  const auto roots = math::find_roots(year, longitude);

  if (roots.size() != root_count) [[unlikely]] {
    std::println("Error in find_jdes: roots.size() is {}, but expected size is {}", roots.size(), root_count);
    std::println("No root will be written to the slots.");
    return;
  }
  
  for (uint32_t i = 0; i < std::min(slot_count, root_count); i++) {
    slots[i] = roots[i];
  }
}


struct JieqiMomentQuery {
  bool     valid;  // Indicates if the result is valid.

  int32_t  jq_idx; // The index of the Jieqi. Expected to be in the range [0, 24). This is the enum value of `Jieqi`.

  int32_t  y;      // The year.
  uint32_t m;      // The month.
  uint32_t d;      // The day.
  double   frac;   // The fraction of the day. Expected to be in the range [0.0, 1.0).
};


/**
 * @brief Query the accurate moment of the Jieqi in the given `year`.
 * @param year The year, in gregorian calendar.
 * @param jq_idx The index of the Jieqi. Expected to be in the range [0, 24). This is the enum value of `Jieqi`.
 * @returns A `JieqiMomentQuery` struct.
 */
JieqiMomentQuery query_jieqi(const int32_t year, const int32_t jq_idx) {
  // Validate the input.
  if (jq_idx < 0 || jq_idx >= 24) [[unlikely]] {
    return {
      .valid  = false,
      .jq_idx = 0,
      .y      = 0,
      .m      = 0,
      .d      = 0,
      .frac   = 0.0,
    };
  }

  try {
    const Jieqi jq = static_cast<Jieqi>(jq_idx);

    const calendar::Datetime ut1_dt = ut1_for_jieqi(year, jq);

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
    std::println("Error in query_jieqi: {}", e.what());

    return {
      .valid  = false,
      .jq_idx = 0,
      .y      = 0,
      .m      = 0,
      .d      = 0,
      .frac   = 0.0,
    };
  }
}


/**
 * @brief Get the Chinese name of the Jieqi.
 * @param jq_idx The index of the Jieqi. Expected to be in the range [0, 24). This is the enum value of `Jieqi`.
 * @param name_mem The name memory. It's caller's responsibility to allocate and free the memory.
 * @param mem_size Maximum bytes that can be written to `name_mem`.
 */
void get_jieqi_chinese(const int32_t jq_idx, char * const mem, const uint32_t mem_size) {
  if (jq_idx < 0 || jq_idx >= 24) [[unlikely]] {
    std::println("Error in get_jieqi_chinese: jq_idx is {}, but expected to be in the range [0, 24).", jq_idx);
    return;
  }

  const std::string_view name = JIEQI_NAME.at(static_cast<Jieqi>(jq_idx));

  // Check if the buffer is large enough to hold the name and the null terminator
  if (mem_size < name.size() + 1) {
    std::println("Error in get_jieqi_chinese: provided buffer is too small.");
    return;
  }

  std::memcpy(mem, name.data(), name.size());
  mem[name.size()] = '\0';
}

}
