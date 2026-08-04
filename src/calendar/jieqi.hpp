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

#pragma once

#include <array>
#include <format>
#include <ranges>
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <string_view>

#include "sun.hpp"
#include "util.hpp"
#include "astro.hpp"
#include "cache.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"


namespace calendar::jieqi {

/** @enum The Chinese Jieqi (节气) */
enum class Jieqi : uint8_t {
  立春, 雨水, 惊蛰, 春分, 清明, 谷雨, 立夏, 小满, 芒种, 夏至, 小暑, 大暑,
  立秋, 处暑, 白露, 秋分, 寒露, 霜降, 立冬, 小雪, 大雪, 冬至, 小寒, 大寒,

  COUNT,

  /* English aliases. */
  LICHUN = 立春, YUSHUI = 雨水, JINGZHE = 惊蛰, CHUNFEN = 春分, QINGMING = 清明, GUYU = 谷雨,
  LIXIA = 立夏, XIAOMAN = 小满, MANGZHONG = 芒种, XIAZHI = 夏至, XIAOSHU = 小暑, DASHU = 大暑,
  LIQIU = 立秋, CHUSHU = 处暑, BAILU = 白露, QIUFEN = 秋分, HANLU = 寒露, SHUANGJIANG = 霜降,
  LIDONG = 立冬, XIAOXUE = 小雪, DAXUE = 大雪, DONGZHI = 冬至, XIAOHAN = 小寒, DAHAN = 大寒,
};

inline constexpr uint8_t JIEQI_COUNT = std::to_underlying(Jieqi::COUNT);
static_assert(24U == JIEQI_COUNT);


/**
 * @brief Check if the given `jq` is a Jie (节).
 * @param jq The jieqi.
 * @return `true` if the given `jq` is a Jie (节), `false` otherwise.
 */
[[nodiscard]] constexpr auto is_jie(const Jieqi jq) -> bool {
  const auto index = std::to_underlying(jq);
  return index % 2 == 0;
}


/**
 * @brief Check if the given `jq` is a Qi (气).
 * @param jq The jieqi.
 * @return `true` if the given `jq` is a Qi (气), `false` otherwise.
 */
[[nodiscard]] constexpr auto is_qi(const Jieqi jq) -> bool {
  const auto index = std::to_underlying(jq);
  return index % 2 == 1;
}


/**
 * @brief Get the index of the given `jq`.
 * @param jq The jieqi.
 * @return The index of the given `jq`.
 */
[[nodiscard]] constexpr auto to_index(const Jieqi jq) -> uint8_t {
  return std::to_underlying(jq);
}


/**
 * @brief Get the Jieqi at a 0-based index counted from 立春.
 * @param index The index, in `[0, JIEQI_COUNT)`.
 * @return The jieqi.
 * @throw std::out_of_range if `index` is not a valid index.
 */
[[nodiscard]] constexpr auto from_index(const uint8_t index) -> Jieqi {
  if (index >= JIEQI_COUNT) {
    throw std::out_of_range { "index must be less than 24" };
  }
  return static_cast<Jieqi>(index);
}


/** @brief A view of all enum values of `Jieqi`. */
inline constexpr auto JIEQI_LIST = std::views::iota(uint8_t { 0 }, JIEQI_COUNT)
                          | std::views::transform([](const auto i) { return from_index(i); });

/** @brief A view of all enum values of `Jieqi`, but ordered by their occurrence in a gregorian year.
 *         That means the first value is "小寒", since it is the first Jieqi in any gregorian year.
 */
inline constexpr auto GREGORIAN_YEAR_JIEQI_LIST = std::views::iota(uint8_t { 0 }, JIEQI_COUNT)
                                         | std::views::transform([](const auto i) { return (i + to_index(Jieqi::小寒)) % JIEQI_COUNT; })
                                         | std::views::transform([](const auto i) { return from_index(i); });


/**
 * @brief Name of each Jieqi, positioned by `to_index`.
 * @note Read it through `name_of`, not directly -- a bare index is the wrong key here. See `name_of`.
 */
inline constexpr std::array<std::string_view, JIEQI_COUNT> JIEQI_NAME {{
  "立春", "雨水", "惊蛰",
  "春分", "清明", "谷雨",
  "立夏", "小满", "芒种",
  "夏至", "小暑", "大暑",
  "立秋", "处暑", "白露",
  "秋分", "寒露", "霜降",
  "立冬", "小雪", "大雪",
  "冬至", "小寒", "大寒",
}};


/**
 * @brief Solar longitude (in degrees) at which each Jieqi begins, positioned by `to_index`.
 * @note Read it through `longitude_of` -- see `name_of`.
 */
inline constexpr std::array<double, JIEQI_COUNT> JIEQI_SOLAR_LONGITUDE {{
  315.0, 330.0, 345.0,
    0.0,  15.0,  30.0,
   45.0,  60.0,  75.0,
   90.0, 105.0, 120.0,
  135.0, 150.0, 165.0,
  180.0, 195.0, 210.0,
  225.0, 240.0, 255.0,
  270.0, 285.0, 300.0,
}};


// The tables are position-keyed, so a mis-ordered edit is silent. Pin the order at compile time.
//
// The longitudes are pinned exactly: they advance 15 degrees per Jieqi starting from 315 at 立春,
// so the whole table has a closed form and any reorder breaks it.
static_assert([] {
  for (uint8_t i = 0; i < JIEQI_COUNT; ++i) {
    if (JIEQI_SOLAR_LONGITUDE.at(i) != static_cast<double>((i * 15 + 315) % 360)) {
      return false;
    }
  }
  return true;
}());

// The names have no closed form, so these pin the ends and one interior point. That catches a
// shift (an entry added or dropped), which is the likely edit accident; it does not catch two
// interior names swapped with each other.
static_assert("立春" == JIEQI_NAME.at(to_index(Jieqi::立春)));
static_assert("夏至" == JIEQI_NAME.at(to_index(Jieqi::夏至)));
static_assert("大寒" == JIEQI_NAME.at(to_index(Jieqi::大寒)));


// Both tables are read through these two, never by bare subscript. Two different index spaces
// live in this codebase -- `to_index` counts from 立春, while the HKO almanac and
// `GREGORIAN_YEAR_JIEQI_LIST` count from 小寒 -- and a subscript cannot tell them apart, so
// indexing the wrong one yields a plausible wrong answer in silence. Taking a `Jieqi` makes
// that a compile error again.
//
// `.at()` stays on the read path: it throws `std::out_of_range` for a `Jieqi` outside `[0, 24)`,
// which the enum's fixed underlying type makes reachable from outside the library
// (`static_cast<Jieqi>(25)`, or `Jieqi::COUNT` itself).

/**
 * @brief Get the name of the given `jq`.
 * @param jq The jieqi.
 * @return The name.
 * @throw std::out_of_range if `jq` is not a valid Jieqi.
 */
[[nodiscard]] constexpr auto name_of(const Jieqi jq) -> std::string_view {
  return JIEQI_NAME.at(to_index(jq));
}


/**
 * @brief Get the solar longitude (in degrees) at which the given `jq` begins.
 * @param jq The jieqi.
 * @return The solar longitude.
 * @throw std::out_of_range if `jq` is not a valid Jieqi.
 */
[[nodiscard]] constexpr auto longitude_of(const Jieqi jq) -> double {
  return JIEQI_SOLAR_LONGITUDE.at(to_index(jq));
}


/**
 * @brief Get the JDE for the given `year` and `jieqi`.
 * @param year The year, in gregorian calendar.
 * @param jq The jieqi.
 * @return The JDE (Julian Ephemeris Day).
 */
[[nodiscard]] inline auto calc_jieqi_jde(const int32_t year, const Jieqi jq) -> double {
  const auto lon = longitude_of(jq);
  const auto roots = astro::sun::geocentric_coord::math::find_roots(year, lon);

  if (roots.size() != 1) {
    // `make_format_args` binds lvalue references, so the name needs a home of its own.
    const auto name = name_of(jq);
    throw std::runtime_error {
      std::vformat("Unexpected roots size for year {}, jieqi {}",
                   std::make_format_args(year, name))
    };
  }

  return roots[0];
}


/** @brief Simply a cached version of `calc_jieqi_jde`. */
const inline auto jieqi_jde = util::cache::cache_func(calc_jieqi_jde);


/**
 * @brief Get the UT1 moment of the given `year` and `jieqi`.
 * @param year The year, in gregorian calendar.
 * @param jq The jieqi.
 * @return The UT1 (Universal Time 1).
 * @details This is just a thin wrapper around `jieqi_jde`.
 * @note UT1, not UTC — the lunar-calendar rules render civil moments through the
 *       leap-second-aware path instead (#84). Remaining consumers: the C-ABI
 *       `query_jieqi_moment` (contract: `celestial.h`) and the HKO golden axis; UT1 stays
 *       for contract stability. The UT1/UTC gap is DUT1 (≤ 0.9 s while leap seconds are
 *       applied); past the ΔAT table freeze it follows ΔT−(ΔAT+32.184) (#115).
 */
[[nodiscard]] inline auto jieqi_ut1_moment(const int32_t year, const Jieqi jq) -> calendar::Datetime {
  return astro::julian_day::jde_to_ut1(jieqi_jde(year, jq));
}


/** @brief A generator that generates consecutive Jieqis and their moments (in JDE), 
 *         starting from a given JDE (exclusive). */
// TODO: Use `std::generator` once every CI leg has it (./linter.py --features).
struct JieqiGenerator {
private:
  int32_t _year;
  uint8_t _jq_index;

public:
  explicit JieqiGenerator(const double start_jde) {
    // #115: the start year is a civil concept — resolve it in UTC, matching `moments()` (#84).
    const auto start_utc = astro::julian_day::jde_to_utc(start_jde);
    const auto start_year = start_utc.year();

    // Find the first Jieqi after the given JDE.
    _year = start_year;
    for (const auto jq : GREGORIAN_YEAR_JIEQI_LIST) {
      const auto jde = jieqi_jde(_year, jq);
      if (jde > start_jde) {
        _jq_index = to_index(jq);
        return;
      }
    }

    // Otherwise, the next Jieqi falls into next year.
    // The first Jieqi in a Gregorian year is `Jieqi::小寒`.
    ++_year;
    _jq_index = to_index(Jieqi::小寒); // NOLINT(cppcoreguidelines-prefer-member-initializer)
  }

  struct JieqiPair { 
    Jieqi jieqi;
    double jde; 
    [[nodiscard]] auto operator==(const JieqiPair& rhs) const -> bool = default;
  };

  auto next() -> JieqiPair {
    const auto jq = from_index(_jq_index);
    const auto jde = jieqi_jde(_year, jq);

    // Update the Jieqi index.
    _jq_index = (_jq_index + 1) % JIEQI_COUNT;

    // If next Jieqi is `Jieqi::小寒`, then we know the next Gregorian year comes.
    if (_jq_index == to_index(Jieqi::小寒)) {
      ++_year;
    }

    return { jq, jde };
  }
};

} // namespace calendar::jieqi
