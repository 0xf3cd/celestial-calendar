#include <gtest/gtest.h>
#include <chrono>
#include <print>
#include "util.hpp"
#include "jieqi.hpp"
#include "datetime.hpp"

namespace calendar::jieqi::test {

using namespace std::chrono;
using namespace std::literals;

using namespace calendar::jieqi;


using hms_type = hh_mm_ss<nanoseconds>;

struct SolarLongitude {
  const year_month_day ymd;
  const hms_type hms;
  const double lon_degs; // Degrees.
  const double epsilon;  // Tolerance.
};


// Data collected from:
// - https://scienceworld.wolfram.com/astronomy/WinterSolstice.html
// - https://jieqi.bmcx.com/
// - https://www.weather.gov/media/ind/seasons.pdf
//
// Actually some data points are in UT1, and some are UTC...
// Assume they are in UT1...
const std::vector<SolarLongitude> DATASET {
  { util::to_ymd(1984, 12, 21),              hms_type { 16h + 10min }, 270.0,  0.01    },
  { util::to_ymd(1997, 12, 21),              hms_type { 19h + 54min }, 270.0,  0.01    },
  { util::to_ymd(2000,  3, 20),         hms_type { 7h + 35min + 15s },   0.0,  0.01    },
  { util::to_ymd(2008,  6, 20), hms_type { 23h + 59min + 20s + 56ms },  90.0,  0.00002 },
  { util::to_ymd(2034,  2,  3),        hms_type { 18h + 40min + 41s }, 315.0,  0.001   },
  { util::to_ymd(2023,  3, 20),              hms_type { 21h + 24min },   0.0,  0.001   },
  { util::to_ymd(2024,  9, 22),              hms_type { 12h + 44min }, 180.0,  0.001   },
  { util::to_ymd(2026,  9, 23),                     hms_type { 5min }, 180.0,  0.001   },
  { util::to_ymd(2027,  6, 21),              hms_type { 14h + 11min },  90.0,  0.001   },
};


TEST(JieQi, solar_apparent_geocentric_longitude) {
  for (const auto& [ymd, hms, expected_lon, epsilon] : DATASET) {
    const Datetime dt { ymd, hms };
    const auto tt_dt = astro::delta_t::ut1_to_tt(dt);

    const auto jde = astro::julian_day::tt_to_jde(tt_dt);
    const auto lon = solar_apparent_geocentric_longitude(jde);

    ASSERT_NEAR(lon, expected_lon, epsilon);
  }
}

TEST(JieQi, estimate_ut1) {
  for (const auto& [ymd, hms, expected_lon, _] : DATASET) {
    const auto [y, __, ___] = util::from_ymd(ymd);

    const Datetime expected_dt { ymd, hms };
    const Datetime estimated_dt = estimate_ut1(y, expected_lon);

    // Convert the datetimes to jd, and use jd to compare.
    const double expected_jd = astro::julian_day::ut1_to_jd(expected_dt);
    const double estimated_jd = astro::julian_day::ut1_to_jd(estimated_dt);

    const double diff = std::abs(estimated_jd - expected_jd);
    ASSERT_LE(diff, 5.0);
  }
}


// TEST(JieQi, find_ut1) {
//   // Use `DATASET`.
//   for (const auto& [ymd, hms, expected_lon, _] : DATASET) {
//     const auto [y, __, ___] = util::from_ymd(ymd);

//     const Datetime expected_dt { ymd, hms };
//     const Datetime found_dt = find_ut1(y, expected_lon);

//     // Convert the datetimes to jd, and use jd to compare.
//     const double expected_jd = astro::julian_day::ut1_to_jd(expected_dt);
//     const double found_jd    = astro::julian_day::ut1_to_jd(found_dt);

//     const double diff = std::abs(found_jd - expected_jd);
//     ASSERT_NEAR(diff, 0.0, 0.01);
//   }

//   // Use random data.
//   for (auto i = 0; i < 1000; ++i) {
//     const double random_jde = astro::julian_day::J2000 + util::random(-10000.0, 10000.0);

//     const auto tt_dt  = astro::julian_day::jde_to_tt(random_jde);
//     const auto ut1_dt = astro::delta_t::tt_to_ut1(tt_dt);

//     const auto [y, _, __] = util::from_ymd(ut1_dt.ymd);
//     const auto lon = solar_apparent_geocentric_longitude(random_jde);
//     const Datetime found_ut1_dt = find_ut1(y, lon);
//     const Datetime found_tt_dt  = astro::delta_t::ut1_to_tt(found_ut1_dt);

//     const double found_jde = astro::julian_day::tt_to_jde(found_tt_dt);
//     const double diff = std::abs(found_jde - random_jde);
//     ASSERT_LE(diff, 0.01);
//   }
// }

}
