#ifndef __CALENDAR_CONVERTER_HPP__
#define __CALENDAR_CONVERTER_HPP__

#include <chrono>
#include <numeric>
#include <cassert>
#include <optional>

#include "date.hpp"
#include "lunardata.hpp"

namespace calendar::converter {

using std::chrono::year_month_day;
using std::chrono::sys_days;
using namespace calendar::lunardata;

/*! @brief The first supported lunar date. */
const inline year_month_day FIRST_LUNAR_DATE = util::to_ymd(START_YEAR, 1, 1);

/*! @brief The last supported lunar date. */
const inline year_month_day LAST_LUNAR_DATE = std::invoke([] {
  const LunarYearInfo info = LUNARDATA_CACHE.get(END_YEAR);
  return util::to_ymd(END_YEAR, info.month_lengths.size(), info.month_lengths.back());
});

/*! @brief The first supported solar date. */
const inline year_month_day FIRST_SOLAR_DATE = std::invoke([] {
  const LunarYearInfo info = LUNARDATA_CACHE.get(START_YEAR);
  return info.date_of_first_day;
});

/*! @brief The last supported solar date. */
const inline year_month_day LAST_SOLAR_DATE = std::invoke([] {
  using namespace util;
  const LunarYearInfo info = LUNARDATA_CACHE.get(END_YEAR);
  const auto [ b, e ] = std::pair { info.month_lengths.begin(), info.month_lengths.end() };
  const uint32_t days_count = std::reduce(b, e, 0);
  return (days_count - 1) + info.date_of_first_day;
});


/*! @fn Checks if the input solar date is valid and within the supported range. 
        检查输入的公历日期是否有效，且在支持的范围内。 */
bool is_valid_solar(const year_month_day& solar_date) {
  if (!solar_date.ok()) {
    return false;
  }
  if (solar_date < FIRST_SOLAR_DATE || solar_date > LAST_SOLAR_DATE) {
    return false;
  }
  return true;
}


/*! @fn Checks if the input lunar date is valid and within the supported range. 
        检查输入的阴历日期是否有效，且在支持的范围内。 */
bool is_valid_lunar(const year_month_day& lunar_date) {
  if (lunar_date < FIRST_LUNAR_DATE || lunar_date > LAST_LUNAR_DATE) {
    return false;
  }

  const auto [ y, m, d ] = util::from_ymd(lunar_date);
  const auto info = LUNARDATA_CACHE.get(y);
  const auto& ml = info.month_lengths;
  
  if (m < 1 || m > ml.size()) {
    return false;
  }
  if (d < 1 || d > ml[m - 1]) {
    return false;
  }

  return true;
}


/*! 
 @fn solar_to_lunar 
 @brief Converts solar date to lunar date. 将公历日期转换为阴历日期。 
 @param solar_date The solar date. 公历日期。
 @return The optional lunar date. 阴历日期（optional）。
 @attention The input date should be in the range of [FIRST_SOLAR_DATE, LAST_SOLAR_DATE].
            输入的日期需要在所支持的范围内。
 @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
            输入的日期无效时返回 `std::nullopt`。不会抛出异常。
 */
std::optional<year_month_day> solar_to_lunar(const year_month_day& solar_date) {
  if (!is_valid_solar(solar_date)) {
    return std::nullopt;
  }

  const auto [ solar_y, solar_m, solar_d ] = util::from_ymd(solar_date);

  const auto find_out_lunar_date = [&](const uint32_t lunar_y) -> year_month_day {
    const auto& info = LUNARDATA_CACHE.get(lunar_y);
    const auto& ml = info.month_lengths;

    // Calculate how many days have past in the lunar year.
    const uint32_t past_days_count = (sys_days { solar_date } - sys_days { info.date_of_first_day }).count();

    uint32_t lunar_m_idx = 0;
    uint32_t rest_days_count = past_days_count;
    while (rest_days_count >= ml[lunar_m_idx]) {
      rest_days_count -= ml[lunar_m_idx];
      ++lunar_m_idx;
    }

    const uint32_t lunar_m = lunar_m_idx + 1;
    assert(1 <= lunar_m && lunar_m <= 13);

    const uint32_t lunar_d = rest_days_count + 1;
    assert(1 <= lunar_d && lunar_d <= 30);

    return util::to_ymd(lunar_y, lunar_m, lunar_d);
  }; // find_out_lunar_date
  
  // The lunar year can either be the same as the solar year, or the previous year.
  // Example: a solar date in solar year 2024 can be in lunar year 2023 or 2024.

  // First, check if lunar date and solar date are in the same year.
  if (solar_y <= END_YEAR) {
    using namespace util;
    const auto& info = LUNARDATA_CACHE.get(solar_y);
    const auto& ml = info.month_lengths;
    const uint32_t lunar_year_days_count = std::reduce(ml.begin(), ml.end(), 0);

    // Calculate the solar date of the last day in the lunar year.
    const year_month_day last_lunar_day = info.date_of_first_day + (lunar_year_days_count - 1);
    if (solar_date >= info.date_of_first_day && solar_date <= last_lunar_day) { // Yeah! We found the lunar year.
      return find_out_lunar_date(solar_y);
    }
  }

  // Otherwise, the lunar date falls into the previous year.
  return find_out_lunar_date(solar_y - 1);
}


/*! 
 @fn lunar_to_solar 
 @brief Converts lunar date to solar date. 将阴历日期转换为公历日期。 
 @param lunar_date The lunar date. 阴历日期。
 @return The optional solar date. 公历日期（optional）。
 @attention The input date should be in the range of [FIRST_LUNAR_DATE, LAST_LUNAR_DATE].
            输入的日期需要在所支持的范围内。
 @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
            输入的日期无效时返回 `std::nullopt`。不会抛出异常。
 */
std::optional<year_month_day> lunar_to_solar(const year_month_day& lunar_date) {
  if (!is_valid_lunar(lunar_date)) {
    return std::nullopt;
  }

  const auto [ y, m, d ] = util::from_ymd(lunar_date);
  const auto& info = LUNARDATA_CACHE.get(y);
  const auto& ml = info.month_lengths;

  const uint32_t past_days_count = d + std::reduce(ml.begin(), ml.begin() + m - 1, 0);

  using namespace util;
  return info.date_of_first_day + (past_days_count - 1);
}

} // namespace calendar::converter

#endif // __CALENDAR_CONVERTER_HPP__
