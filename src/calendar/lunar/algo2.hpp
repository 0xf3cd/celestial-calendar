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

#include <optional>
#include <algorithm>

#include "common.hpp"
#include "jieqi.hpp"
#include "moon_phase.hpp"
#include "julian_day.hpp"


namespace calendar::lunar::algo2 {

using namespace calendar::jieqi;
using calendar::lunar::common::LunarYear;


/** @brief The metadata of a lunar month. */
struct LunarMonth {
  // Start of the month, inclusive.
  double start_jde;

  // End of the month, exclusive.
  double end_jde;

  // Jieqis that fall in this lunar month.
  std::vector<JieqiGenerator::JieqiPair> contained_jieqis;

  auto operator==(const LunarMonth& other) const -> bool {
    return start_jde == other.start_jde
       and end_jde == other.end_jde
       and contained_jieqis == other.contained_jieqis;
  }
};

/**
 * @brief A generator that generates some metadata of lunar months, including Jieqi and length of the month.
 * @note Generated lunar and jieqi information starts after the given JDE.
 * @details The new moon moments and jieqi can be put back to the generator for future use. So I don't think
 *          `std::generator` can be used here... Since `std::generator` doesn't support put back?
 */
struct LunarMonthGenerator {
private:
  astro::moon_phase::new_moon::RootGenerator _new_moon_gen;
  calendar::jieqi::JieqiGenerator _jieqi_gen;

  std::optional<double> _next_new_moon;
  std::optional<JieqiGenerator::JieqiPair> _next_jieqi;

  std::optional<LunarMonth> _next_month;

  auto next_new_moon() -> double {
    if (_next_new_moon.has_value()) {
      const double jde = *_next_new_moon;
      _next_new_moon = std::nullopt;
      return jde;
    }

    return _new_moon_gen.next();
  }

  auto put_back_new_moon(const double jde) -> void {
    assert(!_next_new_moon.has_value());
    _next_new_moon = jde;
  }

  auto next_jieqi() -> JieqiGenerator::JieqiPair {
    if (_next_jieqi.has_value()) {
      auto jieqi = *_next_jieqi;
      _next_jieqi = std::nullopt;
      return jieqi;
    }

    return _jieqi_gen.next();
  }

  auto put_back_jieqi(const JieqiGenerator::JieqiPair jieqi) -> void {
    assert(!_next_jieqi.has_value());
    _next_jieqi = jieqi;
  }

  auto next_month() -> LunarMonth {
    if (_next_month.has_value()) {
      auto month = *_next_month;
      _next_month = std::nullopt;
      return month;
    }

    // Get the bounds of the next lunar month.
    const auto start_jde = next_new_moon();
    const auto end_jde = next_new_moon();
    put_back_new_moon(end_jde);

    // Get the Jieqis that fall in this lunar month.
    std::vector<JieqiGenerator::JieqiPair> jieqis;
    while (true) {
      const auto jieqi = next_jieqi();

      // If the Jieqi is in next month, stop.
      if (jieqi.jde >= end_jde) {
        put_back_jieqi(jieqi);
        break;
      }

      // If the Jieqi is not in this month, continue going.
      if (jieqi.jde < start_jde) {
        continue;
      }

      jieqis.push_back(jieqi);
    }

    return {
      .start_jde = start_jde,
      .end_jde = end_jde,
      .contained_jieqis = jieqis
    };
  }

  auto put_back_month(const LunarMonth& month) -> void {
    assert(!_next_month.has_value());
    _next_month = month;
  }

public:
  explicit LunarMonthGenerator(const double start_jde) 
    : _new_moon_gen(start_jde),
      _jieqi_gen(start_jde),
      _next_new_moon(_new_moon_gen.next()),
      _next_jieqi(_jieqi_gen.next())
  {}

  /** @brief Get the metadata of the next lunar month. */
  auto next() -> LunarMonth {
    return next_month();
  }

  /** @brief Peek the metadata of the next lunar month, without advancing. */
  auto peek() -> LunarMonth {
    auto month = next_month();
    put_back_month(month);
    return month;
  }
};

/** 
 * @brief A chunk of lunar months
 * @details A chunk is defined as a contiguous sequence of lunar months,
 *          from 11th month in a year (inclusive), to 11th month in the next year (exclusive). 
 */
using LunarMonthChunk = std::vector<LunarMonth>;


/**
 * @brief Calculate the lunar month chunks for the given year.
 * @param year The Lunar year.
 * @return The lunar month chunks.
 *         The first chunk is from 11th month in the previous year to 11th month in the current year.
 *         The second chunk is from 11th month in the current year to 11th month in the next year.
 */
inline auto calc_lunar_month_chunks(int32_t year) -> std::pair<LunarMonthChunk, LunarMonthChunk> {
  // The lunar month where Winter Solstice (i.e. Jieqi::冬至) occurs is defined as the 11th month.
  const auto winter_solstice_last_year = jieqi_jde(year - 1, Jieqi::冬至);

  // Start from a bit earlier than the winter solstice, ensuring the entireness of the 11th lunar month.
  LunarMonthGenerator lunar_month_gen { winter_solstice_last_year - 90.0 };

  // Define a helper function to check if the month is the 11th lunar month.
  const auto is_11th = [](const auto& month) {
    const auto& jieqis = month.contained_jieqis;
    const auto is_winter_solstice = [](const auto& jq) { return jq.jieqi == Jieqi::冬至; };
    return std::any_of(cbegin(jieqis), cend(jieqis), is_winter_solstice); 
  };

  // Define a helper function to get the next chunk.
  const auto next_chunk = [&] {
    LunarMonthChunk chunk;
    while (true) {
      const auto month = lunar_month_gen.peek();
      if (is_11th(month) and (not chunk.empty())) {
        break;
      }
      chunk.push_back(lunar_month_gen.next());
    }
    return chunk;
  };

  [[maybe_unused]] const auto _ = next_chunk();
  const auto first_chunk = next_chunk();
  const auto second_chunk = next_chunk();

  return { first_chunk, second_chunk };
}


/**
 * @brief Get the leap month in the given chunk.
 * @param chunk The chunk of lunar months.
 * @return The index of the leap month in the given chunk. `std::nullopt` if there is no leap month.
 */
inline auto leap_month_in_chunk(const LunarMonthChunk& chunk) -> std::optional<int32_t> {
  assert(size(chunk) == 12 or size(chunk) == 13);

  // As per the rules, for 12-month chunks, there is no leap month.
  if (size(chunk) == 12) {
    return std::nullopt;
  }

  // As per the rules, the leap month is the first month where Qi (气/中气) does not appear.
  // TODO: Use `std::enumerate` instead when supported.
  for (const auto& [index, month] : std::views::zip(std::views::iota(0), chunk)) {
    const auto& jq_pairs = month.contained_jieqis;
    const bool has_qi = std::any_of(cbegin(jq_pairs), cend(jq_pairs), [](const auto& pair) {
      return is_qi(pair.jieqi);
    });
    if (not has_qi) {
      return index;
    }
  }

  assert(false); // Should never reach here.
  return std::nullopt;
}


/**
 * @brief Get the start jde of the lunar year.
 * @param chunk The chunk of lunar months.
 * @param leap_month The index of the leap month in the given chunk. `std::nullopt` if there is no leap month.
 * @return The start jde of the lunar year.
 */
inline auto calc_lunar_year_start_jde(const LunarMonthChunk& chunk, std::optional<int32_t> leap_month) -> double {
  if (leap_month.has_value() and (*leap_month <= 2)) {
    // The lunar year starts from the third month after the 11th month in previous year,
    // because of the leap month.
    return chunk[3].start_jde;
  }
  // Otherwise, the lunar year starts from the second month after the 11th month in previous year.
  return chunk[2].start_jde;
}


/** @brief The raw lunar year information, can be processed to `LunarYear`. */
struct LunarYearContext {
  double start_jde;
  double end_jde;

  std::optional<double> leap_month_jde;

  std::vector<LunarMonth> months;
};


/**
 * @brief Create a `LunarYearContext` for the given year, which basically contains
 *        the month info, including leap month.
 * @param year The year to create the context for.
 * @return The `LunarYearContext` for the given year.
 */
inline auto create_lunar_year_context(int32_t year) -> LunarYearContext {
  const auto& [chunk1, chunk2] = calc_lunar_month_chunks(year);

  const auto chunk1_leap_month = leap_month_in_chunk(chunk1);
  const auto chunk2_leap_month = leap_month_in_chunk(chunk2);

  // Figure out the start jde of the lunar year.
  const auto lunar_year_start_jde = calc_lunar_year_start_jde(chunk1, chunk1_leap_month);

  // Figure out the end jde of the lunar year.
  // The end jde is just the start jde of the next year.
  const auto lunar_year_end_jde = calc_lunar_year_start_jde(chunk2, chunk2_leap_month);

  // Figure out if there is a leap month in the lunar year.
  // Check if the leap month is in chunk1.
  std::optional<double> chunk1_leap_jde = std::nullopt;
  if (chunk1_leap_month.has_value()) {
    const auto& m = chunk1[*chunk1_leap_month];
    if (m.start_jde >= lunar_year_start_jde) {
      chunk1_leap_jde = m.start_jde;
    }
  }

  // Check if the leap month is in chunk2.
  std::optional<double> chunk2_leap_jde = std::nullopt;
  if (chunk2_leap_month.has_value()) {
    const auto& m = chunk2[*chunk2_leap_month];
    if (m.start_jde < lunar_year_end_jde) {
      chunk2_leap_jde = m.start_jde;
    }
  }

  // Check if there are two leap months in the lunar year. If so, throw an error.
  if (chunk1_leap_jde.has_value() and chunk2_leap_jde.has_value()) {
    throw std::runtime_error {
      std::format("Two leap months in lunar year: {} ({} and {})", 
                  year, chunk1_leap_jde.value(), chunk2_leap_jde.value())
    };
  }

  // Finally, get the start jde of the leap month.
  std::optional<double> leap_month_jde = std::nullopt;
  if (chunk1_leap_jde.has_value()) {
    leap_month_jde = chunk1_leap_jde;
  } else if (chunk2_leap_jde.has_value()) {
    leap_month_jde = chunk2_leap_jde;
  }

  // Figure out the months in the lunar year.
  // TODO: Use `std::concat` when it gets supported.
  std::vector<LunarMonth> months;

  for (const auto& m : chunk1) {
    if (m.start_jde < lunar_year_start_jde) {
      continue;
    }
    months.push_back(m);
  }

  for (const auto& m : chunk2) {
    if (m.start_jde >= lunar_year_end_jde) {
      break;
    }
    months.push_back(m);
  }

  return { lunar_year_start_jde, lunar_year_end_jde, leap_month_jde, months };
}


/**
 * @brief Calculate the lunar year information for the given year. 
          计算给定年份的阴历年信息。
 * @param year The Lunar year. 阴历年份。
 * @return The lunar year information. 阴历年信息。
 * @see https://ytliu0.github.io/ChineseCalendar/rules_simp.html
 */
// inline auto calc_lunar_year(int32_t year) -> LunarYear {
//   return {};
// }


} // namespace calendar::lunar::algo2
