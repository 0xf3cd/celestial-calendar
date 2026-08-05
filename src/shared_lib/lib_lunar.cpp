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

#include <cassert>

#include "lib.hpp"
#include "celestial.h"

#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"
#include "lunar/algo3.hpp"
#include "lunar/converter.hpp"


namespace {

// #128: the C ABI speaks traditional month numbering + leap flag, while the converter core
// speaks positional month indices (#130). The translation lives here, on the library side
// of the boundary — the positional semantics stay out of the published ABI.

template <calendar::lunar::common::Algo algo>
auto convert_to_lunar(const int32_t year, const uint8_t month, const uint8_t day) -> std::optional<LunarDate> {
  const auto converted = calendar::lunar::converter::Converter<algo>::gregorian_to_lunar(util::to_ymd(year, month, day));
  if (not converted) {
    return std::nullopt;
  }

  const auto [y, m, d] = util::from_ymd(*converted);
  const auto& info = calendar::lunar::common::AlgoMetadata<algo>::get_info_for_year(y);
  const auto traditional = calendar::lunar::common::month_at_position(info, static_cast<uint8_t>(m));
  if (not traditional) {
    return std::nullopt;
  }

  return LunarDate {
    .valid   = true,
    .year    = y,
    .month   = traditional->month,
    .is_leap = traditional->is_leap,
    .day     = static_cast<uint8_t>(d),
  };
}

template <calendar::lunar::common::Algo algo>
auto convert_to_gregorian(const int32_t year, const uint8_t month, const bool is_leap, const uint8_t day) -> std::optional<GregorianDate> {
  // An out-of-window year throws here; the export's catch turns that into `valid = false`.
  const auto& info = calendar::lunar::common::AlgoMetadata<algo>::get_info_for_year(year);

  const auto position = calendar::lunar::common::month_position(info, month, is_leap);
  if (not position) {
    return std::nullopt;
  }

  const auto converted = calendar::lunar::converter::Converter<algo>::lunar_to_gregorian(util::to_ymd(year, *position, day));
  if (not converted) {
    return std::nullopt;
  }

  const auto [y, m, d] = util::from_ymd(*converted);
  return GregorianDate {
    .valid = true,
    .year  = y,
    .month = static_cast<uint8_t>(m),
    .day   = static_cast<uint8_t>(d),
  };
}

} // namespace


extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

auto get_supported_lunar_year_range(const uint8_t algo) -> SupportedLunarYearRange {
  if (algo == 1) {
    return {
      .valid = true,
      .start = calendar::lunar::algo1::START_YEAR,
      .end   = calendar::lunar::algo1::END_YEAR,
    };
  }

  if (algo == 2) {
    return {
      .valid = true,
      .start = calendar::lunar::algo2::START_YEAR,
      .end   = calendar::lunar::algo2::END_YEAR,
    };
  }

  if (algo == 3) {
    return {
      .valid = true,
      .start = calendar::lunar::algo3::START_YEAR,
      .end   = calendar::lunar::algo3::END_YEAR,
    };
  }

  return {};
}

auto get_lunar_year_info(const uint8_t algo, const int32_t year) -> LunarYearInfo { // NOLINT(bugprone-easily-swappable-parameters)
  using namespace std::views;
  
  try {
    if (algo != 1 and algo != 2 and algo != 3) {
      throw std::runtime_error {
        std::format("Unsupported algorithm: {}", algo)
      };
    }

    const auto raw = std::invoke([=] {
      if (algo == 1) {
        return calendar::lunar::algo1::calc_lunar_year(year);
      }
      if (algo == 2) {
        return calendar::lunar::algo2::get_info_for_year(year);
      }
      return calendar::lunar::algo3::calc_lunar_year(year);
    });

    const auto [y, m, d] = util::from_ymd(raw.date_of_first_day);

    uint16_t month_len = 0;
    // TODO: Use `std::views::enumerate` once every CI leg has it (./linter.py --features).
    for (const auto& [i, days] : zip(iota(0), raw.month_lengths)) {
      assert(days == 29 or days == 30);
      const uint16_t bit = (days == 29) ? 0 : 1;
      month_len |= (bit << i);
    }

    return {
      .valid      = true,
      .year       = y,
      .month      = static_cast<uint8_t>(m),
      .day        = static_cast<uint8_t>(d),
      .leap_month = raw.leap_month,
      .month_len  = month_len,
    };

  } catch (const std::exception& e) {
    // #67: `e.what()` is a message, not a format string — pass it as an argument.
    lib::info("Exception raised during execution of get_lunar_year_info, algo = {}, year = {}", algo, year);
    lib::info("{}", e.what());
    return {};
  } catch (...) {
    return {};
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d is the natural date order, as in every neighbour.
auto gregorian_to_lunar(const uint8_t algo, const int32_t year, const uint8_t month, const uint8_t day) -> LunarDate {
  try {
    if (algo != 1 and algo != 2 and algo != 3) {
      throw std::runtime_error {
        std::format("Unsupported algorithm: {}", algo)
      };
    }

    const auto converted = std::invoke([=]() -> std::optional<LunarDate> {
      if (algo == 1) {
        return convert_to_lunar<calendar::lunar::common::Algo::ALGO_1>(year, month, day);
      }
      if (algo == 2) {
        return convert_to_lunar<calendar::lunar::common::Algo::ALGO_2>(year, month, day);
      }
      return convert_to_lunar<calendar::lunar::common::Algo::ALGO_3>(year, month, day);
    });

    if (not converted) {
      return {};
    }
    return *converted;

  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of gregorian_to_lunar, algo = {}, year = {}, month = {}, day = {}", algo, year, month, day);
    lib::info("{}", e.what());
    return {};
  } catch (...) {
    return {};
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d is the natural date order, as in every neighbour.
auto lunar_to_gregorian(const uint8_t algo, const int32_t year, const uint8_t month, const bool is_leap, const uint8_t day) -> GregorianDate {
  try {
    if (algo != 1 and algo != 2 and algo != 3) {
      throw std::runtime_error {
        std::format("Unsupported algorithm: {}", algo)
      };
    }

    const auto converted = std::invoke([=]() -> std::optional<GregorianDate> {
      if (algo == 1) {
        return convert_to_gregorian<calendar::lunar::common::Algo::ALGO_1>(year, month, is_leap, day);
      }
      if (algo == 2) {
        return convert_to_gregorian<calendar::lunar::common::Algo::ALGO_2>(year, month, is_leap, day);
      }
      return convert_to_gregorian<calendar::lunar::common::Algo::ALGO_3>(year, month, is_leap, day);
    });

    if (not converted) {
      return {};
    }
    return *converted;

  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of lunar_to_gregorian, algo = {}, year = {}, month = {}, is_leap = {}, day = {}", algo, year, month, is_leap, day);
    lib::info("{}", e.what());
    return {};
  } catch (...) {
    return {};
  }
}

}
