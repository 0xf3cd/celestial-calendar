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
using namespace astro::sun::geocentric_coord::math;


using hms_type = hh_mm_ss<nanoseconds>;

struct JieqiData {
  const year_month_day ymd;
  const hms_type hms;
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
  { util::to_ymd(1984, 12, 21),              hms_type { 16h + 10min }, Jieqi::冬至 },
  { util::to_ymd(1997, 12, 21),              hms_type { 19h + 54min }, Jieqi::冬至 },
  { util::to_ymd(2000,  3, 20),         hms_type { 7h + 35min + 15s }, Jieqi::春分 },
  { util::to_ymd(2008,  6, 20), hms_type { 23h + 59min + 20s + 56ms }, Jieqi::夏至 },
  { util::to_ymd(2023,  3, 20),              hms_type { 21h + 24min }, Jieqi::春分 },
  { util::to_ymd(2024,  9, 22),              hms_type { 12h + 44min }, Jieqi::秋分 },
  { util::to_ymd(2026,  9, 23),                     hms_type { 5min }, Jieqi::秋分 },
  { util::to_ymd(2027,  6, 21),              hms_type { 14h + 11min }, Jieqi::夏至 },
};

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


TEST(JieQi, IsJieOrQi) {
  ASSERT_TRUE(is_jie(Jieqi::立春));
  ASSERT_FALSE(is_qi(Jieqi::立春));

  ASSERT_TRUE(is_jie(Jieqi::小寒));
  ASSERT_FALSE(is_qi(Jieqi::小寒));

  ASSERT_TRUE(is_qi(Jieqi::雨水));
  ASSERT_FALSE(is_jie(Jieqi::雨水));

  ASSERT_TRUE(is_qi(Jieqi::大寒));
  ASSERT_FALSE(is_jie(Jieqi::大寒));
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
    for (int32_t jq_idx = 0; jq_idx < to_index(Jieqi::COUNT); jq_idx++) {
      const auto jq = from_index(jq_idx);

      const auto jde = jieqi_jde(year, jq); // Use Newton's method to find the root.

      const auto jde_lon = solar_longitude(jde);
      const auto expected_lon = JIEQI_SOLAR_LONGITUDE.at(jq);

      const auto lon_diff = std::fabs(std::fmod(jde_lon - expected_lon, 360.0));
      ASSERT_TRUE((lon_diff < 1e-9) or (lon_diff > 360.0 - 1e-9));
    }
  }
}

TEST(JieQi, JDEOrder) {
  const auto year = util::random(1900, 2050);
  
  const auto jdes = GREGORIAN_YEAR_JIEQI_LIST
                  | std::views::transform([&](const auto& jq) { return jieqi_jde(year, jq); })
                  | std::ranges::to<std::vector>();

  ASSERT_TRUE(std::is_sorted(cbegin(jdes), cend(jdes)));

  // Ensure the JDEs are in the given year.
  for (const auto jde : jdes) {
    const auto ut1 = astro::julian_day::jde_to_ut1(jde);
    ASSERT_EQ(ut1.year(), year);
  }
}

TEST(JieQi, UT1Moment) {
  for (const auto& [ymd, hms, jq] : DATASET) {
    const Datetime real_dt { ymd, hms };
    const auto [y, _, __] = util::from_ymd(ymd);

    const auto est_dt = jieqi_ut1_moment(y, jq);

    ASSERT_EQ(est_dt.ymd, real_dt.ymd);
    ASSERT_NEAR(est_dt.fraction(), real_dt.fraction(), 0.01);
  }
}

TEST(JieQi, Generator) {
  const auto random_year = util::random(1500, 2200);
  const auto random_jq_index = util::random(0, JIEQI_COUNT - 1);
  const auto random_jq = from_index(random_jq_index);

  const auto jde = jieqi_jde(random_year, random_jq);
  const auto start_jde = util::random(-10.0, 0.0) + jde;

  auto year = random_year;
  auto jq_index = random_jq_index;

  std::vector<double> jdes;
  JieqiGenerator jieqi_gen { start_jde };

  for (auto _ = 0; _ < 360; ++_) {
    const auto [jq, jde] = jieqi_gen.next();
    ASSERT_EQ(jq, from_index(jq_index));
    ASSERT_EQ(jde, jieqi_jde(year, jq));

    // Update the Jieqi index.
    ++jq_index;
    if (jq_index >= JIEQI_COUNT) {
      jq_index = 0;
    }

    // Update the year.
    // If current Jieqi is `Jieqi::冬至`, then we know this is the last Jieqi in a Gregorian year,
    // and it's time to move to the next year.
    if (jq == Jieqi::冬至) {
      ++year;
    }

    // Finally, store the JDE.
    jdes.push_back(jde);
  }

  // Check if the JDEs are in order.
  ASSERT_TRUE(std::is_sorted(cbegin(jdes), cend(jdes)));
}

} // namespace calendar::jieqi::test
