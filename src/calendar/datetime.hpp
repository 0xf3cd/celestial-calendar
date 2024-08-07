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

#include <chrono>
#include <format>
#include <cassert>

#include "ymd.hpp"


namespace calendar {

using namespace std::chrono;


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
consteval auto in_a_day() -> uint64_t {
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
constexpr auto to_fraction(const Fractionable auto& elapsed) -> double {
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
constexpr auto from_fraction(const double fraction) -> hh_mm_ss<nanoseconds> {
  return hh_mm_ss {
    nanoseconds {
      static_cast<int64_t>(
        fraction * in_a_day<nanoseconds>()
      )
    }
  };
}


/**
 * @struct Represents a date and a time in the form of `year_month_day` and `hh_mm_ss`.
 * @note The precision of the `time_of_day` field is `std::chrono::nanoseconds`.
 * @note The `time_of_day` field (i.e. `hh_mm_ss`) is positive and less than 24:00:00.0 (i.e. 1 day).
 * @note No time zone is assumed.
 * @details This struct is used to represent UT1 and UTC time in this project.
 */
struct Datetime {
  const year_month_day ymd;
  const hh_mm_ss<nanoseconds> time_of_day;

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
   * @param fraction The time of day.
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
      time_of_day { from_fraction(fraction) }
  {
    if (not ymd.ok()) {
      throw std::invalid_argument { 
        std::vformat(
          "Argument gregorian date `ymd` is invalid, whose value is `{}`", 
          std::make_format_args(this->ymd)
        ) 
      };
    }

    if (fraction < 0.0 or fraction >= 1.0) {
      throw std::invalid_argument {
        std::vformat(
          "Argument `fraction` out of range [0.0, 1.0), whose value is {}",
          std::make_format_args(fraction)
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

  /** 
   * @brief Returns the fraction of a day, in the range [0.0, 1.0).
   * @return The fraction of a day, expected to be in the range [0.0, 1.0).
   */
  [[nodiscard]] constexpr auto fraction() const noexcept -> double {
    const nanoseconds&& elapsed = time_of_day.to_duration();
    return to_fraction(elapsed);
  }

  // Define some operators as well in order to use `Datetime` in some STL containers.

  auto operator==(const Datetime& other) const noexcept -> bool {
    return ymd == other.ymd and time_of_day.to_duration() == other.time_of_day.to_duration();
  }

  auto operator!=(const Datetime& other) const noexcept -> bool {
    return not (*this == other);
  }

  auto operator<=>(const Datetime& other) const noexcept {
    if (auto cmp = ymd <=> other.ymd; cmp != 0) {
      return cmp;
    }
    return time_of_day.to_duration() <=> other.time_of_day.to_duration();
  };
};

} // namespace calendar


namespace std {

// Define hash function for Datetime.
// This function may be an overkill though.
template <>
struct hash<calendar::Datetime> {
  auto operator()(const calendar::Datetime& dt) const -> std::size_t {
    const auto [y, m, d] = util::from_ymd(dt.ymd);
    const double fraction = dt.fraction();

    std::size_t hash = 0xc2b2ae35;

    // Combine year, month, day, and fraction using prime numbers and bitwise operations
    hash ^= 0xa54ff53a5f1d36f1 * (std::hash<int32_t>{}(y)  <<  7);
    hash ^= 0x9e3779b97f4a7c55 * (std::hash<uint32_t>{}(m) <<  3);
    hash ^= 0xb492b66fbe98f273 * (std::hash<uint32_t>{}(d) <<  5);

    // Hash the fraction part and combine using XOR and a prime number
    std::size_t fraction_hash = std::hash<double>{}(fraction);
    hash ^= fraction_hash * 0x9e3779b9;

    // Final mixing step to ensure good distribution
    hash ^= (hash >> 17);
    hash *= 0xff51afd7ed558ccd;
    hash ^= (hash >> 13);
    hash *= 0xc4ceb9fe1a85ec53;
    hash ^= (hash >> 11);
    hash *= 0xc3a5c85c97cb3127;

    return hash;
  }
};

} // namespace std
