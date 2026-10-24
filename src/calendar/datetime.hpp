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

#pragma once

#include <cmath>
#include <chrono>
#include <format>
#include <cassert>
#include <cstdint>
#include <concepts>
#include <limits>
#include <ratio>
#include <stdexcept>
#include <type_traits>

#include "ymd.hpp"

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
 * @par Example
 * `in_a_day<days>() == 1`
 * @par Example
 * `in_a_day<seconds>() == 86400` (There are 86400 seconds in a day.)
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

namespace detail {

/** @brief Cast an arithmetic duration to a signed integral duration without overflowing intermediates. */
template <IsDuration To, IsDuration From>
[[nodiscard]] constexpr auto checked_duration_cast(const From elapsed) -> To {
  using Rep = From::rep;
  using Out = To::rep;
  static_assert(std::is_integral_v<Out> and std::is_signed_v<Out>);
  if constexpr (std::is_floating_point_v<Rep>) {
    const auto count = std::chrono::duration<long double, typename To::period> { elapsed }.count();
    constexpr auto LOWER = static_cast<long double>(std::numeric_limits<Out>::min());
    const bool representable = count >= LOWER and count < -LOWER;
    if (not representable) {
      throw std::invalid_argument { "Duration does not fit the requested integral representation" };
    }
    const auto native_count = std::chrono::duration<Rep, typename To::period> { elapsed }.count();
    const bool native_representable = native_count >= LOWER and native_count < -LOWER;
    if (not native_representable) {
      throw std::invalid_argument { "Rounded duration does not fit the requested integral representation" };
    }
    return To { static_cast<Out>(native_count) };
  } else {
    static_assert(std::is_integral_v<Rep> and std::numeric_limits<Rep>::digits <= 64);
    using Scale = std::ratio_divide<typename From::period, typename To::period>;
    constexpr auto NUM = static_cast<uint64_t>(Scale::num);
    constexpr auto DEN = static_cast<uint64_t>(Scale::den);
    const bool negative = std::is_signed_v<Rep> and elapsed.count() < 0;
    const auto magnitude = negative ? uint64_t { 0 } - static_cast<uint64_t>(elapsed.count())
                                    : static_cast<uint64_t>(elapsed.count());
    const auto limit = negative ? uint64_t { 0 } - static_cast<uint64_t>(std::numeric_limits<Out>::min())
                                : static_cast<uint64_t>(std::numeric_limits<Out>::max());
    const auto quotient = magnitude / DEN;
    const auto remainder = magnitude % DEN;
    // Divide first: even a representable result need not have a representable count * NUM.
    if (quotient > limit / NUM) {
      throw std::invalid_argument { "Duration conversion exceeds its integral representation" };
    }
    const auto whole = quotient * NUM;
    const auto part = [remainder] {
      if (remainder <= std::numeric_limits<uint64_t>::max() / NUM) {
        return remainder * NUM / DEN;
      }
      // Each reduced remainder is < DEN <= INTMAX_MAX, so doubling and adding fit uint64_t.
      uint64_t value = 0;
      uint64_t rest = 0;
      for (uint64_t mask = uint64_t { 1 } << 63; mask != 0; mask >>= 1) {
        value *= 2;
        rest *= 2;
        if (rest >= DEN) {
          rest -= DEN;
          ++value;
        }
        if ((NUM & mask) != 0) {
          rest += remainder;
          if (rest >= DEN) {
            rest -= DEN;
            ++value;
          }
        }
      }
      return value;
    }();
    if (part > limit - whole) {
      throw std::invalid_argument { "Duration does not fit the requested integral representation" };
    }
    const auto result = whole + part;
    if (negative and result == limit) {
      return To { std::numeric_limits<Out>::min() };
    }
    const auto positive = static_cast<Out>(result);
    return To { negative ? -positive : positive };
  }
}

/** @brief Validate the original day duration and return its nanosecond count. */
template <IsDuration Duration>
[[nodiscard]] constexpr auto checked_time_of_day(const Duration elapsed) -> nanoseconds {
  const bool in_day = [elapsed] {
    if constexpr (std::is_integral_v<typename Duration::rep>) {
      return elapsed.count() >= 0 and checked_duration_cast<seconds>(elapsed).count() < 86400;
    } else {
      const auto seconds_elapsed = std::chrono::duration<long double> { elapsed }.count();
      return seconds_elapsed >= 0.0L and seconds_elapsed < 86400.0L;
    }
  }();
  if (not in_day) {
    throw std::invalid_argument { "Time of day must be finite and in [0, 24h)" };
  }
  return checked_duration_cast<nanoseconds>(elapsed);
}

} // namespace detail

/** 
 * @brief Returns the fraction of a day. 
 * @param elapsed The elapsed time.
 * @tparam Fractionable The type of input `elapsed`.
 * @return The fraction of a day, of type `double`.
 * @note The type of input `elapsed` should be convertible to `std::chrono::nanoseconds`.
 * @note Negative and multi-day durations are allowed; the result may be < 0.0 or >= 1.0.
 * @throw std::invalid_argument if the nanosecond count is not representable or the input is not finite.
 */
[[nodiscard]] constexpr auto to_fraction(const Fractionable auto& elapsed) -> double {
  const auto ns_duration = detail::checked_duration_cast<nanoseconds>(elapsed);
  const double ns_elapsed = ns_duration.count();
  return ns_elapsed / in_a_day<nanoseconds>();
}


/** 
 * @brief Returns the nanoseconds of `fraction` days. 
 * @param fraction The fraction.
 * @return The nanoseconds of `fraction` days.
 * @par Example
 * `from_fraction(0.0) == 00:00:00.000000000`
 * @par Example
 * `from_fraction(0.5) == 12:00:00.000000000`
 * @par Example
 * `from_fraction(1.0) == 24:00:00.000000000`
 * @par Example
 * `from_fraction(2.0) == 48:00:00.000000000`
 * @note The precision of the returned value is `std::chrono::nanoseconds`.
 * @throw std::invalid_argument if `fraction` days do not fit an `int64_t` count of nanoseconds
 *        (which includes NaN and either infinity).
 * @note Unlike `validate_fraction` this does not require [0.0, 1.0) — whole days are the
 *       documented contract. What it requires is that the product survives the cast, which is
 *       undefined outside int64's range; the guard's negated form is `validate_fraction`'s
 *       NaN-safe shape (#67, #86).
 */
[[nodiscard]] constexpr auto from_fraction(const double fraction) -> hh_mm_ss<nanoseconds> {
  // The largest whole-day count whose nanoseconds still fit an int64_t, kept a hair inside the
  // boundary: the exact ratio is not representable, so compare against a value known to convert.
  constexpr double MAX_DAYS = 9.2e18 / static_cast<double>(in_a_day<nanoseconds>());

  if (not (-MAX_DAYS < fraction and fraction < MAX_DAYS)) { // NOLINT(readability-simplify-boolean-expr) — NaN must fail this check (#86).
    throw std::invalid_argument {
      std::vformat(
        "Argument `fraction` does not fit an int64 nanosecond count (|fraction| < {}), "
        "got {}",
        std::make_format_args(MAX_DAYS, fraction)
      )
    };
  }

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
        "Argument `fraction` out of range [0.0, 1.0), got {}",
        std::make_format_args(fraction)
      )
    };
  }
  return fraction;
}


/**
 * @brief Represents a date and a time in the form of `year_month_day` and `hh_mm_ss`.
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
   * @throw std::invalid_argument If the input is non-finite, outside the Gregorian year range,
   *        or cannot be represented during conversion.
   * @note Floating-point rounding can make the day remainder unrepresentable near midnight;
   *       such inputs are rejected.
   */
  template <IsDuration Duration>
  constexpr explicit Datetime(const time_point<system_clock, Duration>& tp) 
    : ymd {}
  {
    const auto elapsed = tp.time_since_epoch();
    constexpr auto FIRST = sys_days { std::chrono::year::min() / 1 / 1 }.time_since_epoch().count();
    constexpr auto END = sys_days { std::chrono::year::max() / 12 / 31 }.time_since_epoch().count() + 1;
    if constexpr (std::is_integral_v<typename Duration::rep>) {
      constexpr auto SECONDS_PER_DAY = static_cast<int64_t>(in_a_day<seconds>());
      const auto count = detail::checked_duration_cast<seconds>(elapsed).count();
      const bool below_first = count < FIRST * SECONDS_PER_DAY or
        (count == FIRST * SECONDS_PER_DAY and elapsed.count() % Duration::period::den != 0);
      if (below_first or count >= END * SECONDS_PER_DAY) {
        throw std::invalid_argument { "Time point is outside the representable Gregorian years" };
      }
      using Scale = std::ratio_divide<typename Duration::period, days::period>;
      auto day_count = detail::checked_duration_cast<days>(elapsed).count();
      const auto remainder = elapsed.count() % Scale::den;
      if (elapsed.count() < 0 and remainder != 0) {
        --day_count;
      }
      ymd = year_month_day { sys_days { days { day_count } } };
      if constexpr (Scale::num == 1) {
        // At the minimum tick count, floor<days> can put the day start below the input range (#379).
        const auto ticks = static_cast<int64_t>(remainder);
        const auto positive_ticks = ticks < 0 ? ticks + Scale::den : ticks;
        time_of_day = hh_mm_ss { detail::checked_time_of_day(
          std::chrono::duration<int64_t, typename Duration::period> { positive_ticks }
        ) };
      } else {
        using Common = std::common_type_t<Duration, days>;
        using Integral = std::chrono::duration<int64_t, typename Common::period>;
        const auto input = detail::checked_duration_cast<Integral>(elapsed);
        const auto start = detail::checked_duration_cast<Integral>(days { day_count });
        if (start.count() < 0 and input.count() > std::numeric_limits<int64_t>::max() + start.count()) {
          throw std::invalid_argument { "Time-point day remainder is not representable" };
        }
        time_of_day = hh_mm_ss { detail::checked_time_of_day(input - start) };
      }
    } else {
      const auto day_value = std::chrono::duration<long double, days::period> { elapsed }.count();
      const bool within_years = day_value >= FIRST and day_value < END;
      if (not within_years) {
        throw std::invalid_argument { "Time point is non-finite or outside the representable Gregorian years" };
      }
      const auto day = floor<days>(tp);
      const auto day_count = day.time_since_epoch().count();
      if (day_count < FIRST or day_count >= END) {
        throw std::invalid_argument { "Time-point day cannot be represented within the Gregorian year range" };
      }
      ymd = year_month_day { day };
      try {
        time_of_day = hh_mm_ss { detail::checked_time_of_day(tp - day) };
      } catch (const std::invalid_argument&) {
        throw std::invalid_argument { "Floating time-point day remainder is not representable" };
      }
    }
  }

  /**
   * @brief Constructs a `Datetime` from a date and a raw duration since the start of that day.
   * @param ymd The year, month, and day.
   * @param elapsed The finite time of day in [0, 24h), validated before conversion to nanoseconds.
   * @throw std::invalid_argument If the date is invalid or the duration is non-finite or outside [0, 24h).
   */
  template <IsDuration Duration>
  constexpr explicit Datetime(const year_month_day& ymd, const Duration elapsed)
    : ymd         { ymd },
      time_of_day { detail::checked_time_of_day(elapsed) }
  {
    if (not ymd.ok()) {
      throw std::invalid_argument {
        std::format("Argument Gregorian date `ymd` is invalid, got `{}`", ymd)
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
          "Argument gregorian date `ymd` is invalid, got `{}`", 
          std::make_format_args(this->ymd)
        ) 
      };
    }
    // No `ok()` re-check: `ymd` was just validated, and `validate_fraction` bounds the fraction
    // to [0.0, 1.0), which `from_fraction` maps inside [0, 24h) — the branch would be dead.
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
    return static_cast<double>(elapsed.count()) / in_a_day<nanoseconds>();
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
