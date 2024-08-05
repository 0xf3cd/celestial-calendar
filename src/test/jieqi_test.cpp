#include <gtest/gtest.h>
#include <chrono>
#include <ranges>
#include "util.hpp"
#include "jieqi.hpp"
#include "datetime.hpp"

namespace calendar::jieqi::test {

using namespace std::chrono;
using namespace std::literals;

using namespace calendar::jieqi;
using namespace calendar::jieqi::math;


using hms_type = hh_mm_ss<nanoseconds>;

struct JieqiData {
  const year_month_day ymd;
  const hms_type hms;
  const double lon_degs; // Degrees.
  const double epsilon;  // Tolerance.
  const Jieqi jieqi;
};


// Data collected from:
// - https://scienceworld.wolfram.com/astronomy/WinterSolstice.html
// - https://jieqi.bmcx.com/
// - https://www.weather.gov/media/ind/seasons.pdf
//
// Actually some data points are in UT1, and some are in UTC...
// Assume they are in UT1...
const std::vector<JieqiData> DATASET {
  { util::to_ymd(1984, 12, 21),              hms_type { 16h + 10min }, 270.0,  0.01,    Jieqi::冬至 },
  { util::to_ymd(1997, 12, 21),              hms_type { 19h + 54min }, 270.0,  0.01,    Jieqi::冬至 },
  { util::to_ymd(2000,  3, 20),         hms_type { 7h + 35min + 15s },   0.0,  0.01,    Jieqi::春分 },
  { util::to_ymd(2008,  6, 20), hms_type { 23h + 59min + 20s + 56ms },  90.0,  0.00002, Jieqi::夏至 },
  { util::to_ymd(2023,  3, 20),              hms_type { 21h + 24min },   0.0,  0.001,   Jieqi::春分 },
  { util::to_ymd(2024,  9, 22),              hms_type { 12h + 44min }, 180.0,  0.001,   Jieqi::秋分 },
  { util::to_ymd(2026,  9, 23),                     hms_type { 5min }, 180.0,  0.001,   Jieqi::秋分 },
  { util::to_ymd(2027,  6, 21),              hms_type { 14h + 11min },  90.0,  0.001,   Jieqi::夏至 },
};


TEST(JieQiMath, SolarLongitude) {
  for (const auto& [ymd, hms, expected_lon, epsilon, _] : DATASET) {
    const Datetime dt { ymd, hms };
    const auto tt_dt = astro::delta_t::ut1_to_tt(dt);

    const auto jde = astro::julian_day::tt_to_jde(tt_dt);
    const auto lon = solar_longitude(jde);

    const auto lon_diff = std::fabs(std::fmod(lon - expected_lon, 360.0));
    ASSERT_TRUE((lon_diff < epsilon) or (lon_diff > 360.0 - epsilon));
  }
}

TEST(JieQiMath, FindRoots) {
  for (const auto& [ymd, hms, expected_lon, _ignored1, _ignored2] : DATASET) {
    const Datetime dt { ymd, hms };
    const double jde = astro::julian_day::ut1_to_jde(dt);

    const auto [y, _ignored3, _ignored4] = util::from_ymd(ymd);
    const auto roots = find_roots(y, expected_lon);

    // For the above dataset, we should only find 1 root for every data point.
    ASSERT_EQ(roots.size(), 1);
    ASSERT_NEAR(roots[0], jde, 0.01);
  }

  // Test random data.
  for (auto i = 0; i < 64; i++) {
    const double jde = astro::julian_day::J2000 + util::random(-300 * 365.25, 33 * 365.25);
    const auto ut1_dt = astro::julian_day::jde_to_ut1(jde);

    const auto expected_lon = solar_longitude(jde);
    const auto [y, _ignored1, _ignored2] = util::from_ymd(ut1_dt.ymd);

    const auto roots = find_roots(y, expected_lon);
    ASSERT_TRUE(roots.size() <= 2);
    ASSERT_EQ(roots.size(), discriminant(y, expected_lon));

    // In theory, if there are 2 roots, then `expected_lon` must be around 280.0
    if (roots.size() == 2) {
      ASSERT_NEAR(expected_lon, 280.0, 5.0);
    }

    for (const auto root : roots) {
      const auto calculated_lon = solar_longitude(root);
      ASSERT_NEAR(calculated_lon, expected_lon, 1e-8);
    }
  }
}

TEST(JieQi, NameQuery) {
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::立春), 315.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::雨水), 330.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::惊蛰), 345.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::春分), 0.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::清明), 15.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::秋分), 180.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::小雪), 240.0);

  // Test couple English aliases as well.
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::LICHUN),  315.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::CHUNFEN), 0.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::XIAOHAN), 285.0);
  ASSERT_EQ(JIEQI_SOLAR_LONGITUDE.at(Jieqi::DAHAN),   300.0);
}


TEST(JieQi, JDE) {
  // Random pick some years, to avoid test time being too long.
  auto years = std::views::iota(1800, 2034) | std::views::filter([](int32_t) {
    // If running on ARM, then test less cases.
    // Mainly because the test run is too slow in ARM dockers on the GitHub CI.
#if defined(__arm__) or defined(__aarch64__)
    return util::random(0.0, 1.0) < 0.042;
#else
    // Otherwise, randomly pick some years.
    return util::random(0.0, 1.0) < 0.25;
#endif
  });

  for (const auto year : years) {
    using UnderType = std::underlying_type_t<Jieqi>;
    for (int32_t jq_idx = 0; jq_idx < static_cast<UnderType>(Jieqi::COUNT); jq_idx++) {
      const auto jq = static_cast<Jieqi>(jq_idx);

      const auto jde = jieqi_jde(year, jq); // Use Newton's method to find the root.

      const auto jde_lon = solar_longitude(jde);
      const auto expected_lon = JIEQI_SOLAR_LONGITUDE.at(jq);

      const auto lon_diff = std::fabs(std::fmod(jde_lon - expected_lon, 360.0));
      ASSERT_TRUE((lon_diff < 1e-9) or (lon_diff > 360.0 - 1e-9));
    }
  }
}

TEST(JieQi, UT1Moment) {
  for (const auto& [ymd, hms, _ignored1, _ignored2, jq] : DATASET) {
    const Datetime real_dt { ymd, hms };
    const auto [y, _ignored3, _ignored4] = util::from_ymd(ymd);
    
    const auto est_dt = jieqi_ut1_moment(y, jq);

    ASSERT_EQ(est_dt.ymd, real_dt.ymd);
    ASSERT_NEAR(est_dt.fraction(), real_dt.fraction(), 0.01);
  }
}

} // namespace calendar::jieqi::test
