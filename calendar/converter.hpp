#ifndef __CALENDAR_CONVERTER_HPP__
#define __CALENDAR_CONVERTER_HPP__

#include <chrono>

#include "date.hpp"

namespace calendar::converter {

using std::chrono::year_month_day;

/*! @fn Checks if the input solar date is valid and within the supported range. 
        检查输入的公历日期是否有效，且在支持的范围内。 */
bool is_valid_solar(const year_month_day& solar_date) {
  if (!solar_date.ok()) {
    return false;
  }
  return true;
}


/*! @fn Checks if the input lunar date is valid and within the supported range. 
        检查输入的阴历日期是否有效，且在支持的范围内。 */
bool is_valid_lunar(const year_month_day& lunar_date) {
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
