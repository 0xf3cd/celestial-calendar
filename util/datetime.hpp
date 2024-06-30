#ifndef __UTIL_DATETIME_HPP__
#define __UTIL_DATETIME_HPP__

#include <chrono>
#include <cassert>

namespace util {

using namespace std::chrono;

constexpr inline nanoseconds ns_in_a_day = duration_cast<nanoseconds>(days { 1 });

/**
 * @brief Represents a date and time in the form of `year_month_day` and `hh_mm_ss`.
 */
struct DateTime {
private:
  year_month_day _ymd;
  hh_mm_ss<nanoseconds> _time_of_day;

public:
  DateTime() = delete;

  /**
   * @brief Constructs a `DateTime` from a `time_point`.
   * @param tp The time point. The expected clock is `system_clock`.
   */
  template <typename Duration>
  constexpr explicit DateTime(const time_point<system_clock, Duration>& tp) 
    : _ymd { floor<days>(tp) }, 
      _time_of_day { tp - floor<days>(tp) } 
  {
    static_assert(Duration::period::num == 1, "Duration period must be 1");
    static_assert(Duration::period::den <= 1000000000, "Duration precision must be at least nanoseconds");
  }

  /**
   * @brief Constructs a `DateTime` from a `year_month_day` and `hh_mm_ss`.
   * @param ymd The year, month, and day.
   * @param time_of_day The time of day.
   */
  template <typename Duration>
  constexpr explicit DateTime(const year_month_day& ymd, const hh_mm_ss<Duration>& time_of_day)
    : _ymd { ymd },
      _time_of_day { duration_cast<nanoseconds>(time_of_day.to_duration()) }
  {
    static_assert(Duration::period::num == 1, "Duration period must be 1");
    static_assert(Duration::period::den <= 1000000000, "Duration precision must be at least nanoseconds");
  }

  /**
   * @brief Constructs a `DateTime` from a `year_month_day` and a fraction of a day.
   * @param ymd The year, month, and day.
   * @param fraction_of_day The fraction of a day, in the range [0.0, 1.0).
   */
  constexpr explicit DateTime(const year_month_day& ymd, double fraction_of_day)
    : _ymd { ymd },
      _time_of_day { 
        nanoseconds { static_cast<int64_t>(
          fraction_of_day * ns_in_a_day.count()
        )} 
      }
  {
      // Runtime check to ensure `fraction_of_day` is within valid range.
      assert(fraction_of_day >= 0.0 && fraction_of_day < 1.0);
  }

  /** @brief Returns the year, month, and day. */
  constexpr year_month_day ymd() const noexcept {
    return _ymd;
  }

  /** @brief Returns the time of day. */
  constexpr hh_mm_ss<nanoseconds> time_of_day() const noexcept {
    return _time_of_day;
  }

  /** @brief Returns the fraction of a day, in the range [0.0, 1.0). */
  constexpr double fraction_of_day() const noexcept {
    const nanoseconds elapsed = _time_of_day.to_duration();
    constexpr double ns_count = ns_in_a_day.count();
    return static_cast<double>(elapsed.count()) / ns_count;
  }
};

} // namespace util

#endif // __UTIL_DATETIME_HPP__
