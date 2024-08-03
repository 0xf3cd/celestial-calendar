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

#include <cmath>
#include <format>
#include <ranges>
#include <functional>

#include <vector>
#include <unordered_map>

#include "astro.hpp"
#include "datetime.hpp"


namespace calendar::jieqi::math {

// In this namespace, we use Newton's method to approximate the JDE,
// at which time the Sun reaches a given geocentric longitude in a year.
//
// The following codes are implementing the ideas illustrated in:
// https://github.com/0xf3cd/celestial-calendar/blob/main/statistics/sun_longitude.ipynb
//
// Given a year and a geocentric longitude, our goal is to find the JDE(s) that satisfy the following condition:
// 1. The JDE(s) must fall in the given year.
// 2. At the JDE(s), the Sun's geocentric longitude is equal to the given longitude.
// 
// In our context, a JDE that satisfies the above conditions is called "a root".
//
// For a given year and a given geocentric longitude, there can be 0, 1, or 2 roots.

/** 
 * @see https://github.com/0xf3cd/celestial-calendar/blob/main/statistics/sun_longitude.ipynb
 * @see https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（下）
 */


/**
 * @brief Calculate the apparent geocentric longitude of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The apparent geocentric longitude of the Sun in degrees.
 */
inline auto solar_longitude(const double jde) -> double {
  // Calculate the apparent geocentric longitude of the Sun.
  const auto coord = astro::sun::geocentric_coord::apparent(jde);

  // Return in degrees.
  return coord.λ.deg();
}

/** @brief Return the JDE of the start of the year. */
inline auto get_start_jde(const int32_t year) -> double{
  return astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year, 1, 1), 0.0 });
}

/** @brief Return the JDE of the end of the year. */
inline auto get_end_jde(const int32_t year) -> double {
  return astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year + 1, 1, 1), 0.0 });
}

/** @brief Return the apparent geocentric longitude of the Sun at the start of the year. */
inline auto get_start_lon(const int32_t year) -> double {
  return solar_longitude(get_start_jde(year));
}

/** @brief Return the apparent geocentric longitude of the Sun at the end of the year. */
inline auto get_end_lon(const int32_t year) -> double {
  return solar_longitude(get_end_jde(year));
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)

/** @brief Return true if the given year has a root for the given `lon` before the spring equinox. */
inline auto has_root_before_spring_equinox(const int32_t year, const double lon) -> bool {
  const double start_lon = get_start_lon(year);
  return start_lon <= lon and lon < 360.0;
}

/** @brief Return true if the given year has a root for the given `lon` after the spring equinox. */
inline auto has_root_after_spring_equinox(const int32_t year, const double lon) -> bool {
  const double end_lon = get_end_lon(year);
  return 0.0 <= lon and lon < end_lon;
}

/** @brief Return the count of the roots for the given `year` and `lon`. */
inline auto discriminant(const int32_t year, const double lon) -> uint32_t {
  uint32_t count = 0;

  if (has_root_before_spring_equinox(year, lon)) {
    count++;
  }
  if (has_root_after_spring_equinox(year, lon)) {
    count++;
  }

  return count;
}


// In Newton's method, we will approximate the root with the previous root, iteratively.
// The formula is: Xn+1 = Xn - f(Xn) / f'(Xn).
// Where we can use the following formula to approximate f'(x):
// f'(x) = (f(x+h) - f(x-h)) / (2*h), where h is a small number, usally [1e-8, 1e-5].
//
// As mentioned before, our goal is to find the root (JDE) at which the Sun reaches the expected longitude in a given year.
// In our context, f is defined as:
// f(jde) = solar_longitude(jde) - expected_lon,
// and f is defined on a half-open interval [start_jde(year), end_jde(year)).
//
// Newton's method requires f to be differentiable (smooth).
// So we need to modify the function of `solar_longitude`.
// Bacause the beginning position of Sun in a year is roughly 280.0 degrees, and we need to make it negative.
// Actually, for any JDE befire Spring Equinox this year, we need to substract 360 from `solar_longitude`'s result to make f smooth.
// 
// So the actual f is defined as:
// f(jde) = modified_solar_longitude(jde) - expected_lon

using FuncType = std::function<double(const double)>;

/**@brief Return a `f` that we can apply Newton's method to. */
inline auto make_f(const int32_t year, const double expected_lon) -> FuncType {
  const double apr_1st_jde = astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year, 4, 1), 0.0 });

  const auto modified_solar_longitude = [=](const double jde) -> double {
    const double raw_value = solar_longitude(jde);

    // We mostly want to substract 360.0 from those JDEs before Spring Equinox.
    //
    // We are here using the fact that the beginning of the year is roughly 280.0 degrees,
    // and it continues growing to 360 degrees (which is also 0 degrees) until Spring Equinox.
    // 
    // After Spring Equinox, it grows from 0 to ~280.0 degrees again (the last day of the year), 
    // and then next year comes.

    if (jde < apr_1st_jde and raw_value >= 250.0) {
      return raw_value - 360.0;
    }

    return raw_value; 
  };

  return [=](const double jde) -> double {
    return modified_solar_longitude(jde) - expected_lon;
  };
}

/** 
 * @brief Apply Newton's method to find the root.
 * @param f The function to find the root.
 * @param start_jde The left bound of JDE's range, inclusive.
 * @param end_jde The right bound of JDE's range, exclusive.
 * @param episilon The tolerance. Default is 1e-10.
 * @param max_iter The maximum number of iterations. Default is 20.
 * @returns The approximated root (i.e. JDE). */
inline auto newton_method(
  const FuncType& f,              // The f function to find root(s) for.
  const double start_jde,         // The left bound of JDE's range, inclusive.
  const double end_jde,           // The right bound of JDE's range, exclusive.
  const double episilon = 1e-10,  // The tolerance.
  const std::size_t max_iter = 20 // The maximum number of iterations.
) -> double {
  // `pull_back` ensures the returned JDE is in valid range.
  const auto pull_back = [&](const double jde) -> double {
    if (jde < start_jde) {
      return start_jde;
    }
    if (jde >= end_jde) {
      return end_jde - 1e-20;
    }
    return jde;
  };

  // `next` returns (has_next, next_jde)
  const auto next = [&](const double jde) -> std::pair<bool, double> {
    const double f_jde = f(jde);

    if (std::abs(f_jde) < episilon) { // We find the root!!
      return { false, jde }; // Converged. No more iterations needed.
    }

    // Do the Newton's method.
    constexpr double h = 1e-8;
    const double f_prime_jde = (f(jde + h) - f(jde - h)) / (2.0 * h); // Approximate the derivative.
    const double next_jde = jde - f_jde / f_prime_jde;                // Approximate our next guess.

    return { true, pull_back(next_jde) }; // Don't forget to pull the jde back to valid range.
  };

  // Start approximating the root.

  double jde = (start_jde + end_jde) / 2.0; // Initial guess.

  for (std::size_t iter_count = 0; iter_count < max_iter; iter_count++) {
    // Do the Newton's method.
    const auto& [has_next, next_jde] = next(jde);
    jde = next_jde;

    if (!has_next) {
      break;
    }
  }

  // We cannot find the accurate root in the above iterations.
  // Return our best estimation.
  return jde;
}


/** 
 * @brief Find the roots (i.e. JDEs) for the given `year` and `expected_lon`. 
 * @param year The year, in gregorian calendar.
 * @param expected_lon The expected solar longitude, in degrees.
 * @return The roots (i.e. JDEs). There can be 0, 1 or 2 roots.
 */
inline auto find_roots(const int32_t year, const double expected_lon) -> std::vector<double> {
  if (discriminant(year, expected_lon) == 0) { // No root.
    return {};
  }

  std::vector<FuncType> f_vec;

  // If there is a root before Spring Equinox, it means that
  // after modification (for the sake of differentiability of f),
  // the solar longitudes before spring equinox will be negative.
  // And accordingly, we need to subtract 360.0 from the expected_lon.
  if (has_root_before_spring_equinox(year, expected_lon)) {
    f_vec.emplace_back(make_f(year, expected_lon - 360.0));
  }

  // If there is a root after Spring Equinox, it means that
  // after modification (for the sake of differentiability of f),
  // the solar longitudes after spring equinox will be positive.
  // And accordingly, we have no need to modify the expected_lon.
  if (has_root_after_spring_equinox(year, expected_lon)) {
    f_vec.emplace_back(make_f(year, expected_lon));
  }

  // "nm" here denotes "newton_method".
  auto apply_nm = [&](const FuncType& f) {
    const double start_jde = get_start_jde(year);
    const double end_jde   = get_end_jde(year);
    return newton_method(f, start_jde, end_jde);
  };

  using namespace std::ranges;
  return f_vec | views::transform(apply_nm) | to<std::vector>();
}

// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace calendar::jieqi::math



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

static_assert(24U == static_cast<uint8_t>(Jieqi::COUNT));


/** @brief A view of all enum values of `Jieqi`. */
constexpr auto JIEQI_LIST = std::views::iota(0, static_cast<int8_t>(Jieqi::COUNT)) 
                          | std::views::transform([](const auto i) { return static_cast<Jieqi>(i); });


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


/** @brief Mapping table to get the solar longitude of the given `jieqi`. */
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
inline auto jieqi_jde(const int32_t year, const Jieqi jq) -> double {
  const auto lon = JIEQI_SOLAR_LONGITUDE.at(jq);
  const auto roots = calendar::jieqi::math::find_roots(year, lon);

  if (roots.size() != 1) {
    throw std::runtime_error {
      std::vformat("Unexpected roots size for year {}, jieqi {}", 
                   std::make_format_args(year, JIEQI_NAME.at(jq)))
    };
  }

  return roots[0];
}


/**
 * @brief Get the UT1 moment of the given `year` and `jieqi`.
 * @param year The year, in gregorian calendar.
 * @param jq The jieqi.
 * @return The UT1 (Universal Time 1).
 * @details This is just a thin wrapper around `jde_for_jieqi()`.
 */
inline auto jieqi_ut1_moment(const int32_t year, const Jieqi jq) -> calendar::Datetime {
  return astro::julian_day::jde_to_ut1(jieqi_jde(year, jq));
}


} // namespace calendar::jieqi
