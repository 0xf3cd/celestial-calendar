#ifndef __UTIL_DATETIME_HPP__
#define __UTIL_DATETIME_HPP__

#include <chrono>
#include <cassert>

namespace util::datetime {

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
 * @example `in_a_day<seconds>() == 86400`
 */
template <IsDuration Duration>
consteval uint64_t in_a_day() {
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
constexpr double to_fraction(const Fractionable auto& elapsed) {
  const double ns_elapsed = std::invoke([&] constexpr {
    const auto&& __ns = duration_cast<nanoseconds>(elapsed);
    return __ns.count();
  });

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
constexpr hh_mm_ss<nanoseconds> from_fraction(const double fraction) {
  return hh_mm_ss {
    nanoseconds {
      static_cast<int64_t>(
        fraction * in_a_day<nanoseconds>()
      )
    }
  };
}


/**
 * @brief Represents a gregorian date and time in the form of `year_month_day` and `hh_mm_ss`.
 */
struct DateTime {
private:
  const year_month_day _ymd;
  const hh_mm_ss<nanoseconds> _time_of_day;

public:
  DateTime() = delete;

  /**
   * @brief Constructs a `DateTime` from a `time_point`.
   * @param tp The time point. The expected clock is `system_clock`.
   */
  template <IsDuration Duration>
  constexpr explicit DateTime(const time_point<system_clock, Duration>& tp) 
    : _ymd { floor<days>(tp) }, 
      _time_of_day { tp - floor<days>(tp) } 
  {
    if (not ok()) {
      throw std::runtime_error("Unexpected invalid check result after constructing from time_point");
    }
  }

  /**
   * @brief Constructs a `DateTime` from a `year_month_day` and `hh_mm_ss`.
   * @param ymd The year, month, and day.
   * @param time_of_day The time of day.
   */
  template <IsDuration Duration>
  constexpr explicit DateTime(const year_month_day& ymd, const hh_mm_ss<Duration>& time_of_day)
    : _ymd { ymd },
      _time_of_day { duration_cast<nanoseconds>(time_of_day.to_duration()) }
  {
    if (not ok()) {
      throw std::runtime_error("Unexpected invalid check result after constructing from year_month_day and hh_mm_ss");
    }
  }

  /**
   * @brief Constructs a `DateTime` from a `year_month_day` and a fraction of a day.
   * @param ymd The year, month, and day.
   * @param fraction_of_day The fraction of a day, in the range [0.0, 1.0).
   */
  constexpr explicit DateTime(const year_month_day& ymd, double fraction_of_day)
    : _ymd { ymd },
      _time_of_day { from_fraction(fraction_of_day) }
  {
      if (not ymd.ok()) {
        throw std::invalid_argument("Argument gregorian date `ymd` is invalid");
      }

      if (fraction_of_day < 0.0 or fraction_of_day >= 1.0) {
        throw std::invalid_argument("Argument `fraction_of_day` out of range");
      }

      if (not ok()) {
        throw std::runtime_error("Unexpected invalid check result after constructing from year_month_day and fraction");
      }
  }

  /** 
   * @brief Checks if the underlying gregorian date and time are valid and within the supported range. 
   * @return `true` if all good, `false` otherwise.
   */
  constexpr bool ok() const noexcept {
    if (not _ymd.ok()) {
      return false;
    }
    /*
     * Expect that `_time_of_day` is positive and is less than 24:00:00.0000000000 (i.e. 1 day).
     */
    if (_time_of_day.is_negative()) {
      return false;
    }
    if (_time_of_day.to_duration() >= days { 1 }) {
      return false;
    }
    return true;
  }

  /** 
   * @brief Returns the year, month, and day.
   * @return The year, month, and day, in the form of `std::chrono::year_month_day`.
   */
  constexpr year_month_day ymd() const noexcept {
    return _ymd;
  }

  /** 
   * @brief Returns the time of day. 
   * @return The time of day, in the form of `std::chrono::hh_mm_ss<nanoseconds>`.
   */
  constexpr hh_mm_ss<nanoseconds> time_of_day() const noexcept {
    return _time_of_day;
  }

  /** 
   * @brief Returns the fraction of a day, in the range [0.0, 1.0).
   * @return The fraction of a day, expected to be in the range [0.0, 1.0).
   */
  constexpr double fraction_of_day() const noexcept {
    const nanoseconds&& elapsed = _time_of_day.to_duration();
    return to_fraction(elapsed);
  }
};

} // namespace util::datetime

#endif // __UTIL_DATETIME_HPP__
