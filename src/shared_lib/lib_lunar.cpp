/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#include <cassert>
#include <format>
#include <functional>
#include <utility>

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
auto convert_to_lunar(const int32_t year, const uint8_t month, const uint8_t day) -> std::optional<CelestialLunarDate> {
  const auto converted = calendar::lunar::converter::Converter<algo>::gregorian_to_lunar(
    util::to_ymd(
      util::checked_year(year),
      month,
      day
    )
  );
  if (not converted) {
    return std::nullopt;
  }

  const auto [y, m, d] = util::from_ymd(*converted);
  const auto& info = calendar::lunar::common::AlgoMetadata<algo>::get_info_for_year(y);
  const auto traditional = calendar::lunar::common::month_at_position(info, static_cast<uint8_t>(m));
  if (not traditional) {
    return std::nullopt;
  }

  return CelestialLunarDate {
    .valid   = true,
    .year    = y,
    .month   = traditional->month,
    .is_leap = traditional->is_leap,
    .day     = static_cast<uint8_t>(d),
  };
}

template <calendar::lunar::common::Algo algo>
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d(+leap) is the natural date order, as in every neighbour.
auto convert_to_gregorian(const int32_t year, const uint8_t month, const bool is_leap, const uint8_t day) -> std::optional<CelestialGregorianDate> {
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
  return CelestialGregorianDate {
    .valid = true,
    .year  = y,
    .month = static_cast<uint8_t>(m),
    .day   = static_cast<uint8_t>(d),
  };
}

// The single validation point for the `algo` byte: every export rejects the same values
// with the same message.
[[nodiscard]] auto to_algo(const uint8_t algo) -> calendar::lunar::common::Algo {
  using enum calendar::lunar::common::Algo;
  switch (algo) {
    case 1: return ALGO_1;
    case 2: return ALGO_2;
    case 3: return ALGO_3;
    default:
      throw std::invalid_argument {
        std::format("Unsupported algorithm: {}", algo)
      };
  }
}

// Dispatches on the validated algorithm; `function` is a template lambda that receives it
// as a compile-time constant.
template <typename Function>
constexpr auto dispatch(const calendar::lunar::common::Algo algo, Function&& function) -> decltype(auto) {
  using enum calendar::lunar::common::Algo;
  switch (algo) {
    case ALGO_1: return std::forward<Function>(function).template operator()<ALGO_1>();
    case ALGO_2: return std::forward<Function>(function).template operator()<ALGO_2>();
    case ALGO_3: return std::forward<Function>(function).template operator()<ALGO_3>();
  }
  std::unreachable();
}

} // namespace


extern "C" {

// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

auto celestial_get_supported_lunar_year_range(const uint8_t algo) -> CelestialSupportedLunarYearRange {
  return lib::wrap_export(__func__, [=]() -> CelestialSupportedLunarYearRange {
    return dispatch(to_algo(algo), []<calendar::lunar::common::Algo algorithm> {
      using enum calendar::lunar::common::Algo;
      if constexpr (algorithm == ALGO_1) {
        return CelestialSupportedLunarYearRange {
          .valid = true,
          .start = calendar::lunar::algo1::START_YEAR,
          .end   = calendar::lunar::algo1::END_YEAR,
        };
      } else if constexpr (algorithm == ALGO_2) {
        return CelestialSupportedLunarYearRange {
          .valid = true,
          .start = calendar::lunar::algo2::START_YEAR,
          .end   = calendar::lunar::algo2::END_YEAR,
        };
      } else {
        return CelestialSupportedLunarYearRange {
          .valid = true,
          .start = calendar::lunar::algo3::START_YEAR,
          .end   = calendar::lunar::algo3::END_YEAR,
        };
      }
    });
  });
}

auto celestial_get_lunar_year_info(const uint8_t algo, const int32_t year) -> CelestialLunarYearInfo { // NOLINT(bugprone-easily-swappable-parameters): algorithm/year is the published query order.
  return lib::wrap_export(__func__, [=]() -> CelestialLunarYearInfo {
    using namespace std::views;

    const auto raw = dispatch(to_algo(algo), [year]<calendar::lunar::common::Algo algorithm> {
      using enum calendar::lunar::common::Algo;
      if constexpr (algorithm == ALGO_1) {
        return calendar::lunar::algo1::calc_lunar_year(year);
      } else if constexpr (algorithm == ALGO_2) {
        return calendar::lunar::algo2::get_info_for_year(year);
      } else {
        return calendar::lunar::algo3::calc_lunar_year(year);
      }
    });

    const auto [y, m, d] = util::from_ymd(raw.date_of_first_day);

    uint16_t month_len = 0;
    // TODO: Use `std::views::enumerate` once every CI leg has it (./checks.py --features).
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
  });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d is the natural date order, as in every neighbour.
auto celestial_gregorian_to_lunar(const uint8_t algo, const int32_t year, const uint8_t month, const uint8_t day) -> CelestialLunarDate {
  return lib::wrap_export(__func__, [=]() -> CelestialLunarDate {
    const auto converted = dispatch(to_algo(algo), [=]<calendar::lunar::common::Algo algorithm> {
      return convert_to_lunar<algorithm>(year, month, day);
    });

    if (not converted) {
      throw std::invalid_argument { "The Gregorian date cannot be represented by the selected algorithm." };
    }
    return *converted;
  });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d is the natural date order, as in every neighbour.
auto celestial_lunar_to_gregorian(const uint8_t algo, const int32_t year, const uint8_t month, const bool is_leap, const uint8_t day) -> CelestialGregorianDate {
  return lib::wrap_export(__func__, [=]() -> CelestialGregorianDate {
    const auto converted = dispatch(to_algo(algo), [=]<calendar::lunar::common::Algo algorithm> {
      return convert_to_gregorian<algorithm>(year, month, is_leap, day);
    });

    if (not converted) {
      throw std::invalid_argument { "The Lunar date does not exist in the selected algorithm." };
    }
    return *converted;
  });
}

}
