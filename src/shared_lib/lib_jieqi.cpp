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
#include "jieqi.hpp"

extern "C" {


struct Discriminant {
  bool     valid; // Indicates if the result is valid.
  uint32_t count; // The count of the roots, which is 0, 1, or 2.
};

/**
 * @brief Calculate the JDE discriminant, which is the count of the roots for the given `year` and `lon`.
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @returns The count of the roots, which is 0, 1, or 2.
 *          0 indicates that Sun won't reach the given geocentric longitude in the given year.
 *          1 indicates that Sun will reach the given geocentric longitude once in the given year.
 *          2 indicates that Sun will reach the given geocentric longitude twice in the given year.
 */
auto root_discriminant(const int32_t year, const double longitude) -> Discriminant {
  try {
    return {
      .valid = true,
      .count = calendar::jieqi::math::discriminant(year, longitude),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of root_discriminant");
    lib::debug("root_discriminant: year = {}, lon = {}, error = {}", year, longitude, e.what());
    return {};
  }
}


/**
 * @brief Find the JDE(s) at which the Sun reaches the given `longitude` in the given `year`.
 *        The result is written to the provided slots. It's caller's responsibility to allocate and free the slots.
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @param slots The slots. It's caller's responsibility to allocate and free the slots.
 * @param slot_count The count of slots.
 * @return How many slots are written.
 */
auto copy_roots(
  const int32_t year, 
  const double longitude, 
  double * const slots, 
  const uint32_t slot_count
) -> uint32_t {
  using namespace calendar::jieqi::math;

  try {
    auto roots = find_roots(year, longitude);

    // Some sanity check...
    const auto root_count = discriminant(year, longitude);
    if (roots.size() != root_count) [[unlikely]] {
      lib::info("Error in copy_roots: roots.size() is {}, but expected size is {}", roots.size(), root_count);
      lib::info("No root will be written to the slots.");

      return 0;
    }

    const auto num_written = std::min(static_cast<uint32_t>(roots.size()), slot_count);
    std::copy(roots.begin(), roots.begin() + num_written, slots);

    return num_written;
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of copy_roots");
    lib::debug("copy_roots: year = {}, lon = {}, error = {}", year, longitude, e.what());

    return 0;
  }
}


struct JieqiMomentQuery {
  bool     valid;  // Indicates if the result is valid.

  uint8_t  jq_idx; // The index of the Jieqi. Expected to be in the range [0, 24). This is the enum value of `Jieqi`.

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
auto query_jieqi_moment(const int32_t year, const uint8_t jq_idx) -> JieqiMomentQuery { // NOLINT(bugprone-easily-swappable-parameters)
  // Validate the input.
  if (jq_idx >= 24) [[unlikely]] {
    return {};
  }

  using namespace calendar::jieqi;

  try {
    const auto jq = static_cast<Jieqi>(jq_idx);

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
  }
}


/**
 * @brief Get the Chinese name of the Jieqi.
 * @param jq_idx The index of the Jieqi. Expected to be in the range [0, 24). This is the enum value of `Jieqi`.
 * @param buf The name memory. It's caller's responsibility to allocate and free the memory.
 * @param buf_size Maximum bytes that can be written to `buf`.
 * @returns `true` if the name is successfully written to `buf`.
 */
auto get_jieqi_name(const uint8_t jq_idx, char * const buf, const uint32_t buf_size) -> bool {
  if (jq_idx >= 24) [[unlikely]] {
    lib::info("Error in get_jieqi_name: jq_idx is {}, but expected to be in the range [0, 24).", jq_idx);
    return false;
  }

  using namespace calendar::jieqi;
  const std::string_view name = JIEQI_NAME.at(static_cast<Jieqi>(jq_idx));

  // Check if the buffer is large enough to hold the name and the null terminator
  if (buf_size < name.size() + 1) {
    lib::info("Error in get_jieqi_name: provided buffer is too small. Required {}, actual {}.", name.size() + 1, buf_size);
    return false;
  }

  // Copy the name to the buffer
  std::memcpy(buf, name.data(), name.size());
  buf[name.size()] = '\0'; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  return true;
}

}
