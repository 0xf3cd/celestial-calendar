/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
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
#include <chrono>
#include <format>
#include <iterator>
#include <algorithm>
#include <stdexcept>

#include "delta_t.hpp"

#include "ymd.hpp"
#include "datetime.hpp"

namespace astro::leap_second {

// UTC differs from the uniform atomic scale TAI by an integer number of leap seconds (ΔAT),
// stepped by IERS so that |UTC - UT1| stays under 0.9 s. TT is defined as TAI + 32.184 s.
// So TT - UTC = ΔAT + 32.184 s exactly — no model involved, unlike ΔT = TT - UT1.

/**
 * @brief TT − TAI in seconds. Exact by definition: TT = TAI + 32.184 s.
 * @ref IAU 1991 Resolution A4.
 */
inline constexpr double TT_MINUS_TAI_SEC = 32.184;


/** @brief One step of the ΔAT table: from `start_utc` (inclusive), TAI − UTC is `tai_minus_utc` seconds. */
struct LeapSecondEntry {
  std::chrono::year_month_day start_utc;
  double tai_minus_utc;
};

/**
 * @brief The ΔAT (TAI − UTC) step table, from the start of modern UTC (1972-01-01) onwards.
 *        Each new entry is one inserted leap second.
 * @details No leap second has been announced since 2017, and CGPM Resolution 4 (2022) decided
 *          to discontinue them by 2035 — past the last entry, ΔAT is held at its final value.
 * @ref IERS Bulletin C; the transcription is pinned by `LeapSecond.TableMatchesIERS`.
 */
inline constexpr std::array<LeapSecondEntry, 28> LEAP_SECOND_TABLE {{
  { util::to_ymd(1972, 1, 1), 10.0 },
  { util::to_ymd(1972, 7, 1), 11.0 },
  { util::to_ymd(1973, 1, 1), 12.0 },
  { util::to_ymd(1974, 1, 1), 13.0 },
  { util::to_ymd(1975, 1, 1), 14.0 },
  { util::to_ymd(1976, 1, 1), 15.0 },
  { util::to_ymd(1977, 1, 1), 16.0 },
  { util::to_ymd(1978, 1, 1), 17.0 },
  { util::to_ymd(1979, 1, 1), 18.0 },
  { util::to_ymd(1980, 1, 1), 19.0 },
  { util::to_ymd(1981, 7, 1), 20.0 },
  { util::to_ymd(1982, 7, 1), 21.0 },
  { util::to_ymd(1983, 7, 1), 22.0 },
  { util::to_ymd(1985, 7, 1), 23.0 },
  { util::to_ymd(1988, 1, 1), 24.0 },
  { util::to_ymd(1990, 1, 1), 25.0 },
  { util::to_ymd(1991, 1, 1), 26.0 },
  { util::to_ymd(1992, 7, 1), 27.0 },
  { util::to_ymd(1993, 7, 1), 28.0 },
  { util::to_ymd(1994, 7, 1), 29.0 },
  { util::to_ymd(1996, 1, 1), 30.0 },
  { util::to_ymd(1997, 7, 1), 31.0 },
  { util::to_ymd(1999, 1, 1), 32.0 },
  { util::to_ymd(2006, 1, 1), 33.0 },
  { util::to_ymd(2009, 1, 1), 34.0 },
  { util::to_ymd(2012, 7, 1), 35.0 },
  { util::to_ymd(2015, 7, 1), 36.0 },
  { util::to_ymd(2017, 1, 1), 37.0 },
}};


/**
 * @brief The first day of modern UTC. Before it this project has no UTC to model: civil
 *        datetimes are treated as UT1 and converted through ΔT instead.
 */
inline constexpr std::chrono::year_month_day MODERN_UTC_START = LEAP_SECOND_TABLE.front().start_utc;


/**
 * @brief Look up ΔAT = TAI − UTC for the given UTC calendar date.
 * @param utc_ymd The UTC calendar date.
 * @return ΔAT, in seconds.
 * @throw std::invalid_argument if the date predates modern UTC (1972-01-01).
 */
[[nodiscard]] constexpr auto tai_minus_utc(const std::chrono::year_month_day& utc_ymd) -> double {
  if (utc_ymd < MODERN_UTC_START) {
    throw std::invalid_argument {
      std::vformat("ΔAT is undefined before modern UTC (1972-01-01), got {}",
                   std::make_format_args(utc_ymd))
    };
  }

  // The last entry whose start is on or before the date.
  return std::prev(
    std::ranges::upper_bound(LEAP_SECOND_TABLE, utc_ymd, {}, &LeapSecondEntry::start_utc)
  )->tai_minus_utc;
}


/**
 * @brief TT − UTC in seconds for the given UTC calendar date: ΔAT + 32.184 s, exact by definition.
 * @param utc_ymd The UTC calendar date.
 * @return TT − UTC, in seconds.
 * @throw std::invalid_argument if the date predates modern UTC (1972-01-01).
 */
[[nodiscard]] constexpr auto tt_minus_utc(const std::chrono::year_month_day& utc_ymd) -> double {
  return TT_MINUS_TAI_SEC + tai_minus_utc(utc_ymd);
}


/**
 * @brief Convert a `calendar::Datetime` in UTC to a new `calendar::Datetime` in TT.
 * @param utc_dt The datetime in UTC.
 * @return The datetime in TT.
 * @details TT = UTC + ΔAT + 32.184 s. Before modern UTC (1972-01-01) the input is treated
 *          as UT1 and converted through ΔT instead — the rubber-second UTC of 1960-1971 is
 *          not modelled. The seam at 1972-01-01 is ΔT(1972.0) − 42.184 s, well under a second.
 *          Past the table's last entry ΔAT is held at 37 s (see `LEAP_SECOND_TABLE`).
 * @note A `Datetime` cannot carry an inserted second 23:59:60 itself; the conversion is
 *       defined on the representable instants around it.
 */
[[nodiscard]] constexpr auto utc_to_tt(const calendar::Datetime& utc_dt) -> calendar::Datetime {
  if (utc_dt.ymd < MODERN_UTC_START) {
    return delta_t::ut1_to_tt(utc_dt);
  }
  return calendar::add_seconds(
    utc_dt,
    tt_minus_utc(utc_dt.ymd)
  );
}


/**
 * @brief Convert a `calendar::Datetime` in TT to a new `calendar::Datetime` in UTC.
 * @param tt_dt The datetime in TT.
 * @return The datetime in UTC.
 * @details ΔAT steps on the UTC date, which is only known once the conversion is done — so
 *          guess with the TT date, then refine once with the resulting UTC date. One
 *          refinement settles any step crossing: consecutive steps are at least half a year
 *          apart, while TT − UTC is under two minutes. Before modern UTC (1972-01-01) this
 *          falls back to UT1 through ΔT, mirroring `utc_to_tt`; past the table's last entry
 *          ΔAT is held at 37 s (see `LEAP_SECOND_TABLE`).
 * @note Instants inside an inserted leap second (the unrepresentable 23:59:60) land in the
 *       first second of the following UTC day, duplicating it — on that 1 s TT interval the
 *       conversion is not invertible, and `utc_to_tt(tt_to_utc(tt))` returns `tt` plus one
 *       second.
 */
[[nodiscard]] constexpr auto tt_to_utc(const calendar::Datetime& tt_dt) -> calendar::Datetime {
  if (tt_dt.ymd < MODERN_UTC_START) {
    return delta_t::tt_to_ut1(tt_dt);
  }

  const auto guessed_utc = calendar::add_seconds(
    tt_dt,
    -tt_minus_utc(tt_dt.ymd)
  );

  // The first ~42.184 s of 1972-01-01 (TT) still map below the table start.
  if (guessed_utc.ymd < MODERN_UTC_START) {
    return delta_t::tt_to_ut1(tt_dt);
  }

  return calendar::add_seconds(
    tt_dt,
    -tt_minus_utc(guessed_utc.ymd)
  );
}

} // namespace astro::leap_second
