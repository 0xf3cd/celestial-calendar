#ifndef __UTIL_DATE_HPP__
#define __UTIL_DATE_HPP__

#include <chrono>

namespace util {

/*! @brief A type that can be converted to `std::chrono::year`. */
template <typename T>
concept YearCastable = std::convertible_to<T, int32_t>;


/*! @brief Converts the input year, month, and date to a `std::chrono::year_month_day`. */
const auto to_date = [](
  YearCastable auto year, 
  uint32_t month, 
  uint32_t day
) -> std::chrono::year_month_day {
  const auto __year = std::chrono::year { static_cast<int32_t>(year) };
  return std::chrono::year_month_day { __year / month / day };
};


/*! @brief A type that can be converted to `std::chrono::days`. */
template <typename T>
concept DayCastable = requires (T t) {
  { std::chrono::days { t } } -> std::same_as<std::chrono::days>;
};


template <typename T>
  requires DayCastable<T>
constexpr std::chrono::year_month_day operator+(const std::chrono::year_month_day& ymd, const T& days) {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


template <typename T>
  requires DayCastable<T>
constexpr std::chrono::year_month_day operator+(const T& days, const std::chrono::year_month_day& ymd) {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


template <typename T>
  requires DayCastable<T>
constexpr std::chrono::year_month_day operator-(const std::chrono::year_month_day& ymd, const T& days) {
  const auto time_point = std::chrono::sys_days { ymd } - std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}

}

#endif // __UTIL_DATE_HPP__
