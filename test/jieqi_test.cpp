#include <gtest/gtest.h>
#include <chrono>
#include "util.hpp"
#include "jieqi.hpp"
#include "datetime.hpp"

namespace calendar::jieqi {

using namespace std::chrono;
using namespace std::literals;

TEST(JieQi, solar_apparent_geocentric_longitude) {
  { // Winter Solstice of year 1984 (ref: https://scienceworld.wolfram.com/astronomy/WinterSolstice.html)
    // Expected time: 1984-12-21 16:10 UT
    const auto ymd = util::to_ymd(1984, 12, 21);
    const auto hms = hh_mm_ss<nanoseconds> { 16h + 10min };
    const auto ut_dt = Datetime { ymd, hms };

    const auto lon = solar_apparent_geocentric_longitude(ut_dt);
    ASSERT_NEAR(lon, 270.0, 0.01); // Actual diff is 0.009..., which is still acceptable.
  }
  { // Winter Solstice of year 1997 (ref: https://scienceworld.wolfram.com/astronomy/WinterSolstice.html)
    // Expected time: 1997-12-21 19:54 UT
    const auto ymd = util::to_ymd(1997, 12, 21);
    const auto hms = hh_mm_ss<nanoseconds> { 19h + 54min };
    const auto ut_dt = Datetime { ymd, hms };

    const auto lon = solar_apparent_geocentric_longitude(ut_dt);
    ASSERT_NEAR(lon, 270.0, 0.01); // Actual diff is 0.009..., which is still acceptable.
  }
  { // Spring Equinox of year 2000 (ref: https://jieqi.bmcx.com/)
    // Expected time: 2000-03-20 07:35:15 UTC
    const auto ymd = util::to_ymd(2000, 3, 20);
    const auto hms = hh_mm_ss<nanoseconds> { 7h + 35min + 15s };
    const auto utc_dt = Datetime { ymd, hms };

    // Ignore the difference between UTC and UT1.
    const auto lon = solar_apparent_geocentric_longitude(utc_dt);
    ASSERT_NEAR(lon, 0.0, 0.001);
  }
  { // Summer Solstice of year 2008 (ref: https://jieqi.bmcx.com/)
    // Expected time: 2008-06-20 23:59:21 UTC
    const auto ymd = util::to_ymd(2008, 6, 20);
    const auto hms = hh_mm_ss<nanoseconds> { 23h + 59min + 21s };
    const auto utc_dt = Datetime { ymd, hms };

    // Ignore the difference between UTC and UT1.
    const auto lon = solar_apparent_geocentric_longitude(utc_dt);
    ASSERT_NEAR(lon, 90.0, 0.001);
  }
  { // 立春 of year 2034 (ref: https://jieqi.bmcx.com/)
    // Expected time: 2034-02-03 18:40:41 UTC
    const auto ymd = util::to_ymd(2034, 2, 3);
    const auto hms = hh_mm_ss<nanoseconds> { 18h + 40min + 41s };
    const auto utc_dt = Datetime { ymd, hms };

    // Ignore the difference between UTC and UT1.
    const auto lon = solar_apparent_geocentric_longitude(utc_dt);
    ASSERT_NEAR(lon, 315.0, 0.001);
  }
  { // Spring Equinox of year 2023
    // Expected time: 2023-March-20 21:24 UTC
    const auto ymd = util::to_ymd(2023, 3, 20);
    const auto hms = hh_mm_ss<nanoseconds> { 21h + 24min };
    const auto utc_dt = Datetime { ymd, hms };

    // Ignore the difference between UTC and UT1.
    const auto lon = solar_apparent_geocentric_longitude(utc_dt);
    ASSERT_NEAR(lon, 0.0, 0.001);
  }
  { // Autumn Equinox of year 2024 (ref: https://www.weather.gov/media/ind/seasons.pdf)
    // Expected time: 2024-Sept-22 12:44 UTC
    const auto ymd = util::to_ymd(2024, 9, 22);
    const auto hms = hh_mm_ss<nanoseconds> { 12h + 44min };
    const auto utc_dt = Datetime { ymd, hms };

    // Ignore the difference between UTC and UT1.
    const auto lon = solar_apparent_geocentric_longitude(utc_dt);
    ASSERT_NEAR(lon, 180.0, 0.001);
  }
  { // Autumn Equinox of year 2026 (ref: https://www.weather.gov/media/ind/seasons.pdf)
    // Expected time: 2026-09-23 00:05 UTC
    const auto ymd = util::to_ymd(2026, 9, 23);
    const auto hms = hh_mm_ss<nanoseconds> { 5min };
    const auto utc_dt = Datetime { ymd, hms };

    // Ignore the difference between UTC and UT1.
    const auto lon = solar_apparent_geocentric_longitude(utc_dt);
    ASSERT_NEAR(lon, 180.0, 0.001);
  }
  { // Sumer Solstice of year 2027 (ref: https://www.weather.gov/media/ind/seasons.pdf)
    // Expected time: 2027-06-21 14:11 UTC
    const auto ymd = util::to_ymd(2027, 6, 21);
    const auto hms = hh_mm_ss<nanoseconds> { 14h + 11min };
    const auto utc_dt = Datetime { ymd, hms };

    // Ignore the difference between UTC and UT1.
    const auto lon = solar_apparent_geocentric_longitude(utc_dt);
    ASSERT_NEAR(lon, 90.0, 0.001);
  }
}

}
