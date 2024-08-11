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

#include <unordered_map>

#include "util.hpp"
#include "astro.hpp"
#include "datetime.hpp"


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

constexpr uint8_t JIEQI_COUNT = static_cast<uint8_t>(Jieqi::COUNT);
static_assert(24U == JIEQI_COUNT);


/**
 * @brief Check if the given `jq` is a Jie (节).
 * @param jq The jieqi.
 * @return `true` if the given `jq` is a Jie (节), `false` otherwise.
 */
constexpr auto is_jie(const Jieqi jq) -> bool {
  const auto index = static_cast<uint8_t>(jq);
  return index % 2 == 0;
}


/**
 * @brief Check if the given `jq` is a Qi (气).
 * @param jq The jieqi.
 * @return `true` if the given `jq` is a Qi (气), `false` otherwise.
 */
constexpr auto is_qi(const Jieqi jq) -> bool {
  const auto index = static_cast<uint8_t>(jq);
  return index % 2 == 1;
}


/**
 * @brief Get the index of the given `jq`.
 * @param jq The jieqi.
 * @return The index of the given `jq`.
 */
constexpr auto to_index(const Jieqi jq) -> uint8_t {
  return static_cast<uint8_t>(jq);
}


/**
 * @brief Get the index of the given `jq`.
 * @param jq The jieqi.
 * @return The index of the given `jq`.
 */
constexpr auto from_index(const uint8_t index) -> Jieqi {
  if (index >= JIEQI_COUNT) {
    throw std::out_of_range { "index must be less than 24" };
  }
  return static_cast<Jieqi>(index);
}


/** @brief A view of all enum values of `Jieqi`. */
constexpr auto JIEQI_LIST = std::views::iota(0, static_cast<int8_t>(JIEQI_COUNT)) 
                          | std::views::transform([](const auto i) { return from_index(i); });

/** @brief A view of all enum values of `Jieqi`, but ordered by their occurrence in a gregorian year.
 *         That means the first value is "小寒", since it is the first Jieqi in any gregorian year.
 */
constexpr auto GREGORIAN_YEAR_JIEQI_LIST = std::views::iota(0, static_cast<int8_t>(JIEQI_COUNT)) 
                                         | std::views::transform([](const auto i) { return (i + to_index(Jieqi::小寒)) % JIEQI_COUNT; })
                                         | std::views::transform([](const auto i) { return from_index(i); });


/** @brief Mapping table to get the name of the given `jieqi`. */
const inline std::unordered_map<Jieqi, std::string_view> JIEQI_NAME = {
  { Jieqi::立春, "立春" }, { Jieqi::雨水, "雨水" }, { Jieqi::惊蛰, "惊蛰" },
  { Jieqi::春分, "春分" }, { Jieqi::清明, "清明" }, { Jieqi::谷雨, "谷雨" },
  { Jieqi::立夏, "立夏" }, { Jieqi::小满, "小满" }, { Jieqi::芒种, "芒种" },
  { Jieqi::夏至, "夏至" }, { Jieqi::小暑, "小暑" }, { Jieqi::大暑, "大暑" },
  { Jieqi::立秋, "立秋" }, { Jieqi::处暑, "处暑" }, { Jieqi::白露, "白露" },
  { Jieqi::秋分, "秋分" }, { Jieqi::寒露, "寒露" }, { Jieqi::霜降, "霜降" },
  { Jieqi::立冬, "立冬" }, { Jieqi::小雪, "小雪" }, { Jieqi::大雪, "大雪" },
  { Jieqi::冬至, "冬至" }, { Jieqi::小寒, "小寒" }, { Jieqi::大寒, "大寒" },
};


/** @brief Mapping table to get the solar longitude of the given `Jieqi`. */
const inline std::unordered_map<Jieqi, double> JIEQI_SOLAR_LONGITUDE = {
  { Jieqi::立春, 315.0 }, { Jieqi::雨水, 330.0 }, { Jieqi::惊蛰, 345.0 },
  { Jieqi::春分,   0.0 }, { Jieqi::清明,  15.0 }, { Jieqi::谷雨,  30.0 },
  { Jieqi::立夏,  45.0 }, { Jieqi::小满,  60.0 }, { Jieqi::芒种,  75.0 },
  { Jieqi::夏至,  90.0 }, { Jieqi::小暑, 105.0 }, { Jieqi::大暑, 120.0 },
  { Jieqi::立秋, 135.0 }, { Jieqi::处暑, 150.0 }, { Jieqi::白露, 165.0 },
  { Jieqi::秋分, 180.0 }, { Jieqi::寒露, 195.0 }, { Jieqi::霜降, 210.0 },
  { Jieqi::立冬, 225.0 }, { Jieqi::小雪, 240.0 }, { Jieqi::大雪, 255.0 },
  { Jieqi::冬至, 270.0 }, { Jieqi::小寒, 285.0 }, { Jieqi::大寒, 300.0 },
};

/**
 * @brief Get the JDE for the given `year` and `jieqi`.
 * @param year The year, in gregorian calendar.
 * @param jq The jieqi.
 * @return The JDE (Julian Ephemeris Day).
 */
inline auto calc_jieqi_jde(const int32_t year, const Jieqi jq) -> double {
  const auto lon = JIEQI_SOLAR_LONGITUDE.at(jq);
  const auto roots = astro::sun::geocentric_coord::math::find_roots(year, lon);

  if (roots.size() != 1) {
    throw std::runtime_error {
      std::vformat("Unexpected roots size for year {}, jieqi {}", 
                   std::make_format_args(year, JIEQI_NAME.at(jq)))
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
 */
inline auto jieqi_ut1_moment(const int32_t year, const Jieqi jq) -> calendar::Datetime {
  return astro::julian_day::jde_to_ut1(jieqi_jde(year, jq));
}


/** @brief A generator that generates consecutive Jieqis and their moments (in JDE), 
 *         starting from a given JDE (exclusive). */
// TODO: Use `std::generator` when supported.
struct JieqiGenerator {
private:
  int32_t _year;
  uint8_t _jq_index;

public:
  explicit JieqiGenerator(const double start_jde) {
    const auto start_ut1 = astro::julian_day::jde_to_ut1(start_jde);
    const auto start_year = start_ut1.year();

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
    auto operator==(const JieqiPair& rhs) const -> bool { 
      return jieqi == rhs.jieqi and jde == rhs.jde; 
    }
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
