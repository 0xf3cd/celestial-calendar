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

#pragma once

#include "astro.hpp"
#include "datetime.hpp"


namespace calendar::jieqi {


/**
 * @brief Calculate the apparent geocentric longitude of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The apparent geocentric longitude of the Sun in degrees.
 */
double solar_apparent_geocentric_longitude(const double jde) {
  // Calculate the apparent geocentric longitude of the Sun.
  const auto coord = astro::sun::geocentric_coord::apparent(jde);

  // Return in degrees.
  return coord.λ.deg();
}


// /**
//  * @brief Return the estimated time when the Sun will reach the given solar longitude.
//  * @param gregorian_year The Gregorian year.
//  * @param solar_longitude The solar longitude in degrees.
//  * @return The estimated datetime, in UT1.
//  * @note The method only does estimation. The returned value is not exact.
//  * @example The moment of Summer Solstice in 2008 is at 2008-06-20 23:59:20.56209 UT1.
//  *          The estimation returned by this function falls into the interval [a, b], where:
//  *          - a = 2023-03-15 21:24:20.56209 UT1 (actual moment - 5 days)
//  *          - b = 2023-03-25 21:24:20.56209 UT1 (actual moment + 5 days)
//  */
// calendar::Datetime estimate_ut1(const int32_t gregorian_year, const double solar_longitude) {
//   using astro::toolbox::AngleUnit::DEG;
//   const astro::toolbox::Angle<DEG> angle { solar_longitude };
//   const astro::toolbox::Angle<DEG> normalized = angle.normalize(); // Now in [0, 360).

//   // Spring Equinox in a gregorian year is roughly March 20, when the solar longitude is 0.
//   const calendar::Datetime spring_equinox { util::to_ymd(gregorian_year, 3, 20), 0.0 };

//   // Suppose there are 365.25 days in a year.
//   constexpr auto degs_per_day = 360.0 / 365.25;
//   const double est_days = normalized.deg() / degs_per_day;

//   const int32_t full_days = static_cast<int32_t>(est_days);
//   const double fraction = est_days - full_days;

//   using namespace util::ymd_operator;
//   const calendar::Datetime roughly_estimated { spring_equinox.ymd + full_days, fraction };

//   std::println("> roughly_estimated.ymd: {}", roughly_estimated.ymd);

//   const auto [y, m, d] = util::from_ymd(roughly_estimated.ymd);
//   assert(y == gregorian_year or y == gregorian_year + 1);

//   // Check if the roughly estimated result is in the next year.
//   // If so, correct it to this year.
//   if (y == gregorian_year + 1) [[unlikely]] {
//     return calendar::Datetime { util::to_ymd(gregorian_year, m, d), roughly_estimated.fraction() };
//   }

//   // Otherwise, return the result.
//   return roughly_estimated;
// }


// /**
//  * @brief Find the time when the Sun will reach the given solar longitude. This function returns accurate results.
//  * @param gregorian_year The Gregorian year.
//  * @param solar_longitude The solar longitude in degrees.
//  * @param epsilon The tolerance. Default is 1e-6.
//  * @return The datetime, in UT1.
//  * @details Newton's method is used to find the time.
//  * @ref https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（下）
//  */
// calendar::Datetime find_ut1(const int32_t gregorian_year, const double solar_longitude, const double epsilon = 1e-6) {
//   const auto f = [&](const double jde) -> double {
//     const double lon = solar_apparent_geocentric_longitude(jde);
//     return lon - solar_longitude;
//   };

//   // Newton's method is defined as:
//   // Xn+1 = Xn - f(Xn) / f'(Xn)
//   //
//   // And we can use the finite difference to approximate the derivative, which is defined as:
//   // f'(x) = (f(x+h) - f(x-h)) / (2*h), where h is a small number, usally [1e-8, 1e-5].

//   // `next_jde` is used to find the next jde, using the finite difference approximation and Newton's method.
//   const auto next_jde = [&](const double jde) -> double {
//     constexpr double h       = 0.000005;
//     const double f_jde       = f(jde);
//     const double f_prime_jde = (f(jde + h) - f(jde - h)) / (2 * h);
//     return jde - f_jde / f_prime_jde;
//   };

//   const auto est_ut1_dt = estimate_ut1(gregorian_year, solar_longitude);
//   const auto est_jde    = ut1_to_jde(est_ut1_dt);

//   double jde = est_jde;
//   while (std::abs(f(jde)) > epsilon) {
//     jde = next_jde(jde);
//   }

//   return jde_to_ut1(jde);
// }



/** @enum The Chinese Jieqi (节气) */
enum class Jieqi {
  立春, 雨水, 惊蛰, 春分, 清明, 谷雨, 立夏, 小满, 芒种, 夏至, 小暑, 大暑,
  立秋, 处暑, 白露, 秋分, 寒露, 霜降, 立冬, 小雪, 大雪, 冬至, 小寒, 大寒,

  /* English aliases. */
  LICHUN = 立春, YUSHUI = 雨水, JINGZHE = 惊蛰, CHUNFEN = 春分, QINGMING = 清明, GUYU = 谷雨,
  LIXIA = 立夏, XIAOMAN = 小满, MANGZHONG = 芒种, XIAZHI = 夏至, XIAOSHU = 小暑, DASHU = 大暑,
  LIQIU = 立秋, CHUSHU = 处暑, BAILU = 白露, QIUFEN = 秋分, HANLU = 寒露, SHUANGJIANG = 霜降,
  LIDONG = 立冬, XIAOXUE = 小雪, DAXUE = 大雪, DONGZHI = 冬至, XIAOHAN = 小寒, DAHAN = 大寒,
};

} // namespace calendar::jieqi
