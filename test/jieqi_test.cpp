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

}
