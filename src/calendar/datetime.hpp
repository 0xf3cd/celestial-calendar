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
#include <chrono>
#include <format>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <functional>

#include "ymd.hpp"
#include "hash.hpp"

namespace calendar {

// Closed set of chrono *type* names used by this header and its callers (#51).
// Replaces open `using namespace std::chrono;`. New type dependencies need an
// explicit line here — do not reopen the namespace. Function templates
// (`duration_cast` / `floor`) resolve via ADL and are intentionally not listed;
// cross-stdlib risk is covered by CI (Windows/macOS).
// Type aliases: `using X = …`. Templates imported by using-declaration:
// `hh_mm_ss` / `time_point` are class templates (CTAD applies). `sys_time` is an
// alias template in the standard library — imported in the same shape for
// consistency only; do not rely on CTAD through it.
using days            = std::chrono::days;
using seconds         = std::chrono::seconds;
using nanoseconds     = std::chrono::nanoseconds;
using microseconds    = std::chrono::microseconds;
using system_clock    = std::chrono::system_clock;
using year_month_day  = std::chrono::year_month_day;
using sys_days        = std::chrono::sys_days;

using std::chrono::sys_time;
using std::chrono::time_point;
using std::chrono::hh_mm_ss;


/** 
 * @brief Checks if a type is of `std::chrono::duration`. 
 * @tparam T The type to check.
 */
template <typename T>
concept IsDuration = requires {
  typename T::rep;
  typename T::period;
};

/** 
 * @brief Returns the number of the given duration in a day.
 * @tparam Duration The duration type.
 * @return The number of the given duration in a day.
 * @example `in_a_day<days>() == 1`
 * @example `in_a_day<seconds>() == 86400` (There are 86400 seconds in a day.)
 */
template <IsDuration Duration>
[[nodiscard]] consteval auto in_a_day() -> uint64_t {
  return duration_cast<Duration>(days { 1 }).count();
}


/** 
 * @brief Checks if a type can cast to `std::chrono::nanoseconds`,
 *         which is the assumption of function `fraction`. 
 * @tparam T The type to check.
 */
template <typename T>
concept Fractionable = requires (T t) {
  { duration_cast<nanoseconds>(t) } -> std::same_as<nanoseconds>;
};

/** 
 * @brief Returns the fraction of a day. 
 * @param elapsed The elapsed time.
 * @tparam Fractionable The type of input `elapsed`.
 * @return The fraction of a day, of type `double`.
 * @note The type of input `elapsed` should be convertible to `std::chrono::nanoseconds`.
 * @warning No check on the input `elapsed`, so it can be negative or greater than `in_a_day<nanoseconds>()`.
 *          Thus, the returned result may be < 0.0 or >= 1.0.
 */
[[nodiscard]] constexpr auto to_fraction(const Fractionable auto& elapsed) -> double {
  const auto& ns_duration = duration_cast<nanoseconds>(elapsed);
  const double ns_elapsed = ns_duration.count();
  return ns_elapsed / in_a_day<nanoseconds>();
}


/** 
 * @brief Returns the nanoseconds of `fraction` days. 
 * @param fraction The fraction.
 * @return The nanoseconds of `fraction` days.
 * @example `from_fraction(0.0) == 00:00:00.000000000`
 * @example `from_fraction(0.5) == 12:00:00.000000000`
 * @example `from_fraction(1.0) == 24:00:00.000000000`
 * @example `from_fraction(2.0) == 48:00:00.000000000`
 * @note The precision of the returned value is `std::chrono::nanoseconds`.
 */
[[nodiscard]] constexpr auto from_fraction(const double fraction) -> hh_mm_ss<nanoseconds> {
  return hh_mm_ss {
    nanoseconds {
      static_cast<int64_t>(
        fraction * in_a_day<nanoseconds>()
      )
    }
  };
}


/**
 * @brief Validate the fraction of a day, *before* any narrowing conversion happens.
 * @param fraction The fraction of a day, expected in [0.0, 1.0).
 * @return `fraction`, unchanged.
 * @throw std::invalid_argument if `fraction` is outside [0.0, 1.0).
 * @note #67: written as `not (x >= lo and x < hi)` so NaN fails the check too — a plain
 *       `fraction < 0.0 or fraction >= 1.0` test is always false for NaN and lets it through
 *       to the undefined double→int64 cast in `from_fraction`.
 */
[[nodiscard]] constexpr auto validate_fraction(const double fraction) -> double {
  if (not (fraction >= 0.0 and fraction < 1.0)) { // NOLINT(readability-simplify-boolean-expr) — NaN must fail this check (#67).
    throw std::invalid_argument {
      std::vformat(
        "Argument `fraction` out of range [0.0, 1.0), whose value is {}",
        std::make_format_args(fraction)
      )
    };
  }
  return fraction;
}


/**
 * @struct Represents a date and a time in the form of `year_month_day` and `hh_mm_ss`.
 * @note The precision of the `time_of_day` field is `std::chrono::nanoseconds`.
 * @note The `time_of_day` field (i.e. `hh_mm_ss`) is positive and less than 24:00:00.0 (i.e. 1 day).
 * @note No time zone is assumed.
 * @details This struct is used to represent UT1, UTC, and TT time in this project.
 */
struct Datetime {
  year_month_day ymd;
  hh_mm_ss<nanoseconds> time_of_day;

  Datetime() = delete;

  /**
   * @brief Constructs a `Datetime` from a `time_point`.
   * @param tp The time point. The expected clock is `system_clock`.
   */
  template <IsDuration Duration>
  constexpr explicit Datetime(const time_point<system_clock, Duration>& tp) 
    : ymd         { floor<days>(tp) }, 
      time_of_day { tp - floor<days>(tp) }
  {
    if (not ok()) {
      const double ns = static_cast<double>(
        time_of_day.to_duration().count()
      );
      throw std::runtime_error {
        std::vformat(
          "Sanity check failed, `ymd` is {} and `time_of_day` is {}ns",
          std::make_format_args(ymd, ns)
        )
      };
    }
  }

  /**
   * @brief Constructs a `Datetime` from a `year_month_day` and `hh_mm_ss`.
   * @param ymd The year, month, and day.
   * @param time_of_day The time of day.
   */
  template <IsDuration Duration>
  constexpr explicit Datetime(const year_month_day& ymd, const hh_mm_ss<Duration>& time_of_day)
    : ymd         { ymd },
      time_of_day { duration_cast<nanoseconds>(time_of_day.to_duration()) }
  {
    if (not ok()) {
      const double ns = static_cast<double>(
        this->time_of_day.to_duration().count()
      );
      throw std::runtime_error {
        std::vformat(
          "Sanity check failed, `ymd` is {} and `time_of_day` is {}ns",
          std::make_format_args(this->ymd, ns)
        )
      };
    }
  }

  /**
   * @brief Constructs a `Datetime` from a `year_month_day` and a fraction of a day.
   * @param ymd The year, month, and day.
   * @param fraction The fraction of a day, in the range [0.0, 1.0).
   */
  constexpr explicit Datetime(const year_month_day& ymd, double fraction)
    : ymd         { ymd },
      // #67: validate before the narrowing cast inside `from_fraction` — after the cast,
      // NaN/huge inputs are already UB.
      time_of_day { from_fraction(validate_fraction(fraction)) }
  {
    if (not ymd.ok()) {
      throw std::invalid_argument { 
        std::vformat(
          "Argument gregorian date `ymd` is invalid, whose value is `{}`", 
          std::make_format_args(this->ymd)
        ) 
      };
    }

    if (not ok()) {
      const double ns = static_cast<double>(
        time_of_day.to_duration().count()
      );
      throw std::runtime_error {
        std::vformat(
          "Sanity check failed, `ymd` is {} and `time_of_day` is {}ns",
          std::make_format_args(ymd, ns)
        )
      };
    }
  }

  /** 
   * @brief Checks if the underlying gregorian date and time are valid and within the supported range. 
   * @return `true` if all good, `false` otherwise.
   */
  [[nodiscard]] constexpr auto ok() const noexcept -> bool {
    // Check if the gregorian date is valid.
    if (not ymd.ok()) {
      return false;
    }
    
    // Check if the time of day (i.e. hh_mm_ss) is valid.
    // Expect that `time_of_day` is positive and is less than 24:00:00.0 (i.e. 1 day).
    if (time_of_day.is_negative()) {
      return false;
    }
    if (time_of_day.to_duration() >= days { 1 }) {
      return false;
    }
    
    return true;
  }

  /** @brief Returns the year. */
  [[nodiscard]] constexpr auto year() const noexcept -> int32_t {
    return static_cast<int32_t>(ymd.year());
  }

  /** @brief Returns the month. */
  [[nodiscard]] constexpr auto month() const noexcept -> uint32_t {
    return static_cast<uint32_t>(ymd.month());
  }

  /** @brief Returns the day. */
  [[nodiscard]] constexpr auto day() const noexcept -> uint32_t {
    return static_cast<uint32_t>(ymd.day());
  }

  /** 
   * @brief Returns the fraction of a day, in the range [0.0, 1.0).
   * @return The fraction of a day, expected to be in the range [0.0, 1.0).
   */
  [[nodiscard]] constexpr auto fraction() const noexcept -> double {
    const auto elapsed = time_of_day.to_duration();
    return to_fraction(elapsed);
  }

  // Define some operators as well in order to use `Datetime` in some STL containers.

  [[nodiscard]] constexpr auto operator==(const Datetime& other) const noexcept -> bool {
    return ymd == other.ymd and time_of_day.to_duration() == other.time_of_day.to_duration();
  }

  // `operator!=` is not spelled out: C++20 rewrites `a != b` as `!(a == b)`.

  [[nodiscard]] constexpr auto operator<=>(const Datetime& other) const noexcept {
    if (auto cmp = ymd <=> other.ymd; cmp != 0) {
      return cmp;
    }
    return time_of_day.to_duration() <=> other.time_of_day.to_duration();
  }
};


/**
 * @brief Returns `dt` shifted by `offset_sec` seconds, carrying whole days into the date part.
 * @param dt The datetime to shift.
 * @param offset_sec The offset in seconds. Can be negative, and is not limited to one day.
 * @return The shifted datetime.
 * @throw std::invalid_argument if `offset_sec` is not finite, or if the shifted date leaves
 *        `std::chrono::year_month_day`'s representable years.
 */
[[nodiscard]] constexpr auto add_seconds(const Datetime& dt, const double offset_sec) -> Datetime {
  // Beyond ±2e9 days the day carry below overflows `int32_t`; NaN fails the comparison too.
  if (not (std::fabs(offset_sec) < 2.0e9 * in_a_day<seconds>())) {
    throw std::invalid_argument {
      std::format("The offset {} seconds is not finite or beyond the day-carry range.", offset_sec)
    };
  }

  const double fraction = dt.fraction() + (offset_sec / in_a_day<seconds>());

  // The shift can leave [0.0, 1.0); carry the whole days into the date part. A shift landing
  // exactly on midnight can round the remainder up to 1.0 (the sum comes out a half-ulp below
  // a whole day) — carry that too, or the constructor rejects the fraction.
  const auto floor_carry = static_cast<int32_t>(std::floor(fraction));
  const bool midnight_carry = (fraction - floor_carry) >= 1.0;
  const auto day_carry = floor_carry + (midnight_carry ? 1 : 0);
  const double remainder = midnight_carry ? 0.0 : fraction - floor_carry;

  // `std::chrono::year` stores an unspecified value beyond ±32767 (`ok()` cannot be trusted
  // on a wrapped date) — bound the shifted date while it is still a day count.
  const auto shifted = std::chrono::sys_days { dt.ymd } + std::chrono::days { day_carry };
  if (shifted < std::chrono::sys_days { util::to_ymd(-32767, 1, 1) }
      or shifted > std::chrono::sys_days { util::to_ymd(32767, 12, 31) }) {
    throw std::invalid_argument {
      std::format("The offset {} seconds shifts the date beyond the representable years.", offset_sec)
    };
  }

  return Datetime { std::chrono::year_month_day { shifted }, remainder };
}

} // namespace calendar


namespace std {

// Define hash function for Datetime.
// This function may be an overkill though.
template <>
struct hash<calendar::Datetime> {
  [[nodiscard]] auto operator()(const calendar::Datetime& dt) const -> std::size_t {
    const auto [y, m, d] = util::from_ymd(dt.ymd);
    const double fraction = dt.fraction();
    return util::hash::hash(y, m, d, fraction);
  }
};

} // namespace std
