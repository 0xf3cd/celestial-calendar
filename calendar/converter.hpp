#ifndef __CALENDAR_CONVERTER_HPP__
#define __CALENDAR_CONVERTER_HPP__

#include <chrono>
#include <numeric>

#include "date.hpp"
#include "lunardata.hpp"

namespace calendar::converter {

using std::chrono::year_month_day;
using namespace calendar::lunardata;

/*! @brief The first supported lunar date. */
const inline year_month_day FIRST_LUNAR_DATE = util::to_ymd(START_YEAR, 1, 1);

/*! @brief The last supported lunar date. */
const inline year_month_day LAST_LUNAR_DATE = std::invoke([] {
  const LunarYearInfo info = lunardata_cache.get(END_YEAR);
  return util::to_ymd(END_YEAR, info.month_lengths.size(), info.month_lengths.back());
});

/*! @brief The first supported solar date. */
const inline year_month_day FIRST_SOLAR_DATE = std::invoke([] {
  const LunarYearInfo info = lunardata_cache.get(START_YEAR);
  return info.date_of_first_day;
});

/*! @brief The last supported solar date. */
const inline year_month_day LAST_SOLAR_DATE = std::invoke([] {
  using namespace util;
  const LunarYearInfo info = lunardata_cache.get(END_YEAR);
  const auto [ b, e ] = std::pair { info.month_lengths.begin(), info.month_lengths.end() };
  const uint32_t days_count = std::accumulate(b, e, 0);
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
  const auto info = lunardata_cache.get(y);
  const auto& ml = info.month_lengths;
  
  if (m < 1 || m > ml.size()) {
    return false;
  }
  if (d < 1 || d > ml[m - 1]) {
    return false;
  }

  return true;
}


/*! @fn Converts solar date to lunar date. 将公历日期转换为阴历日期。 */
year_month_day solar_to_lunar(const year_month_day& solar_date) {
  return solar_date;
}


/*! @fn Converts lunar date to solar date. 将阴历日期转换为公历日期。 */
year_month_day lunar_to_solar(const year_month_day& lunar_date) {
  return lunar_date;
}

} // namespace calendar::converter

#endif // __CALENDAR_CONVERTER_HPP__
