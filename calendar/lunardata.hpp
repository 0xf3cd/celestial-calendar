#ifndef __CALENDAR_LUNARDATA_HPP__
#define __CALENDAR_LUNARDATA_HPP__

#include <array>
#include <vector>
#include <unordered_map>
#include <chrono>

#include "formatter.hpp"
#include "date.hpp"

namespace calendar::lunardata {

/*! @brief The first supported lunar year. */
constexpr uint32_t START_YEAR = 1901;

/*! @brief The last supported lunar year. */
constexpr uint32_t END_YEAR = 2099;

/*! @brief The encoded binary data for each lunar year. */
constexpr std::array<uint32_t, 199> LUNAR_DATA = {
  0x620752, 0x4c0ea5, 0x38b64a, 0x5c064b, 0x440a9b, 0x309556, 0x56056a, 0x400b59, 0x2a5752, 0x500752, 
  0x3adb25, 0x600b25, 0x480a4b, 0x32b4ab, 0x5802ad, 0x42056b, 0x2c4b69, 0x520da9, 0x3efd92, 0x640e92, 
  0x4c0d25, 0x36ba4d, 0x5c0a56, 0x4602b6, 0x2e95b5, 0x5606d4, 0x400ea9, 0x2c5e92, 0x500e92, 0x3acd26, 
  0x5e052b, 0x480a57, 0x32b2b6, 0x580b5a, 0x4406d4, 0x2e6ec9, 0x520749, 0x3cf693, 0x620a93, 0x4c052b, 
  0x34ca5b, 0x5a0aad, 0x46056a, 0x309b55, 0x560ba4, 0x400b49, 0x2a5a93, 0x500a95, 0x38f52d, 0x5e0536, 
  0x480aad, 0x34b5aa, 0x5805b2, 0x420da5, 0x2e7d4a, 0x540d4a, 0x3d0a95, 0x600a97, 0x4c0556, 0x36cab5, 
  0x5a0ad5, 0x4606d2, 0x308ea5, 0x560ea5, 0x40064a, 0x286c97, 0x4e0a9b, 0x3af55a, 0x5e056a, 0x480b69, 
  0x34b752, 0x5a0b52, 0x420b25, 0x2c964b, 0x520a4b, 0x3d14ab, 0x6002ad, 0x4a056d, 0x36cb69, 0x5c0da9, 
  0x460d92, 0x309d25, 0x560d25, 0x415a4d, 0x640a56, 0x4e02b6, 0x38c5b5, 0x5e06d5, 0x480ea9, 0x34be92, 
  0x5a0e92, 0x440d26, 0x2c6a56, 0x500a57, 0x3d14d6, 0x62035a, 0x4a06d5, 0x36b6c9, 0x5c0749, 0x460693, 
  0x2e952b, 0x54052b, 0x3e0a5b, 0x2a555a, 0x4e056a, 0x38fb55, 0x600ba4, 0x4a0b49, 0x32ba93, 0x580a95, 
  0x42052d, 0x2c8aad, 0x500ab5, 0x3d35aa, 0x6205d2, 0x4c0da5, 0x36dd4a, 0x5c0d4a, 0x460c95, 0x30952e, 
  0x540556, 0x3e0ab5, 0x2a55b2, 0x5006d2, 0x38cea5, 0x5e0725, 0x48064b, 0x32ac97, 0x560cab, 0x42055a, 
  0x2c6ad6, 0x520b69, 0x3d7752, 0x620b52, 0x4c0b25, 0x36da4b, 0x5a0a4b, 0x4404ab, 0x2ea55b, 0x5405ad, 
  0x3e0b6a, 0x2a5b52, 0x500d92, 0x3afd25, 0x5e0d25, 0x480a55, 0x32b4ad, 0x5804b6, 0x4005b5, 0x2c6daa, 
  0x520ec9, 0x3f1e92, 0x620e92, 0x4c0d26, 0x36ca56, 0x5a0a57, 0x440556, 0x2e86d5, 0x540755, 0x400749, 
  0x286e93, 0x4e0693, 0x38f52b, 0x5e052b, 0x460a5b, 0x32b55a, 0x58056a, 0x420b65, 0x2c974a, 0x520b4a, 
  0x3d1a95, 0x620a95, 0x4a052d, 0x34caad, 0x5a0ab5, 0x4605aa, 0x2e8ba5, 0x540da5, 0x400d4a, 0x2a7c95, 
  0x4e0c96, 0x38f94e, 0x5e0556, 0x480ab5, 0x32b5b2, 0x5806d2, 0x420ea5, 0x2e8e4a, 0x50068b, 0x3b0c97, 
  0x6004ab, 0x4a055b, 0x34cad6, 0x5a0b6a, 0x460752, 0x309725, 0x540b45, 0x3e0a8b, 0x28549b,
};


/*! 
 @struct LunarYearInfo 
 @brief  Information of the lunar year. 阴历年信息。
 */
struct LunarYearInfo {
  /*! @brief The date of the first day of the lunar year in gregorian calendar. 
             本阴历年第一天对应的公历日期。 */
  std::chrono::year_month_day date_of_first_day;

  /*! @brief The month of the leap month (1 <= leap_month <= 12). If 0, there is no leap month.
             闰月的月份 (1-12)，如果为 0 则没有闰月。 */
  uint8_t leap_month;

  /*! @brief The number of days in all months in the lunar year.
             There are 12 elements if there is no leap year, otherwise there are 13 elements.
             本阴历年每个月的天数。
             如果没有闰月，那么有 12 个元素；如果有闰月，那么有 13 个元素。 */
  std::vector<uint8_t> month_lengths;
};


/*! 
 @fn Get the lunar year information for the given year. 
     返回给定年份的阴历年信息。
 @param year The year. 年份。
 @return The lunar year information. 阴历年信息。
 */
LunarYearInfo get_lunar_year_info(uint32_t year) {
  // Validate the input year.
  if (year < START_YEAR || year > END_YEAR) {
    throw std::out_of_range { util::format("year must be between %u and %u.", START_YEAR, END_YEAR) };
  }

  const uint32_t bin_data       = LUNAR_DATA[year - START_YEAR];
  const uint32_t days_offset    = bin_data >> 17;
  const uint8_t  leap_month     = (bin_data >> 13) & 0xf;
  const uint16_t month_len_info = bin_data & 0x1fff;

  const auto get_first_day = [&] {
    using namespace util;
    return to_ymd(year, 1, 1) + days_offset;
  };

  const auto get_month_lengths = [&] {
    std::vector<uint8_t> lengths;
    const uint8_t month_count = 12 + (leap_month != 0);
    for (uint8_t i = 0; i < month_count; ++i) {
      // There can be either 30 or 29 days in a lunar month.
      if (month_len_info >> i & 0x1) {
        lengths.push_back(30);
      } else {
        lengths.push_back(29);
      }
    }
    return lengths;
  };

  return {
    .date_of_first_day = get_first_day(),
    .leap_month        = leap_month,
    .month_lengths     = get_month_lengths(),
  };  
}


/*!
 @class LunarYearInfoCache 
 @brief Cache the lunar year information. 
        缓存阴历年信息。
 */
struct LunarYearInfoCache {
private:
  std::unordered_map<uint32_t, LunarYearInfo> cache;

public:
  /*! 
   @fn Return the cached lunar year info. 
       返回缓存的阴历年信息。
   @attention The input year should be in the range of [START_YEAR, END_YEAR].
              输入的年份需要在所支持的范围内。
   @param year The year. 年份。
   @return The lunar year info. 阴历年信息。
   */
  LunarYearInfo get(uint32_t year) {
    assert(year >= START_YEAR && year <= END_YEAR);

    if (cache.find(year) == cache.end()) {
      cache[year] = get_lunar_year_info(year);
    }
    return cache[year];
  }
};


/*! @brief The global lunar year information cache. 全局阴历年信息缓存。 */
const inline LunarYearInfoCache g_lunar_year_info_cache {};

} // namespace calendar::lunardata

#endif // __CALENDAR_LUNARDATA_HPP__
