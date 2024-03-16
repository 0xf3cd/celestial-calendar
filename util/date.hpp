#ifndef __UTIL_DATE_HPP__
#define __UTIL_DATE_HPP__

#include <chrono>

namespace util {

/*! @brief A type that can be converted to `std::chrono::year`. */
template <typename T>
concept YearCastable = std::convertible_to<T, int32_t>;


/*! @brief Converts the input year, month, and date to a `std::chrono::year_month_day`. */
constexpr std::chrono::year_month_day to_ymd(
  const YearCastable auto year, 
  const uint32_t month, 
  const uint32_t day
) {
  const std::chrono::year __year { static_cast<int32_t>(year) };
  return std::chrono::year_month_day { __year / month / day };
}


/*! @brief A type that can be converted to `std::chrono::days`. */
template <typename T>
concept DayCastable = requires (T t) {
  { std::chrono::days { t } } -> std::same_as<std::chrono::days>;
};


constexpr std::chrono::year_month_day operator+(
  const std::chrono::year_month_day& ymd, 
  const DayCastable auto& days
) {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


constexpr std::chrono::year_month_day operator+(
  const DayCastable auto& days,
  const std::chrono::year_month_day& ymd
) {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


constexpr std::chrono::year_month_day operator-(
  const std::chrono::year_month_day& ymd, 
  const DayCastable auto& days
) {
  const auto time_point = std::chrono::sys_days { ymd } - std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}

} // namespace util

#endif // __UTIL_DATE_HPP__
