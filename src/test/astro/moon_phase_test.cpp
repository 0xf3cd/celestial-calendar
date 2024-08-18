#include <gtest/gtest.h>
#include <print>
#include <vector>
#include <chrono>
#include <ranges>
#include "julian_day.hpp"
#include "util.hpp"
#include "astro.hpp"
#include "ymd.hpp"

namespace astro::moon_phase::test {

using namespace astro::moon_phase::new_moon;

TEST(NewMoon, RootGenerator) {
  using namespace std::ranges;
  const auto jde = astro::julian_day::J2000 + util::random(-200000.0, 200000.0);
  
  const auto roots = std::invoke([&] {
    RootGenerator gen(jde);
    std::vector<double> roots;
    for (int i = 0; i < 64; ++i) {
      roots.push_back(gen.next()); // NOLINT(performance-inefficient-vector-operation)
    }
    return roots;
  });

  for (const auto root : roots) {
    ASSERT_GT(root, jde);
    const double diff = longitude_diff(root);

    constexpr double epsilon = 0.00001;
    ASSERT_TRUE(diff < epsilon or diff > 360.0 - epsilon);
  }

  // TODO: Use `std::views::pairwise` when it gets supported.
  for (auto it = cbegin(roots); it != cend(roots); ++it) {
    const auto next = std::next(it);
    if (next == cend(roots)) {
      break;
    }

    // Ensure next > current
    ASSERT_GT(*next, *it);
    // Ensure the gap is around 29.5 days, which is the avg length of a lunar month.
    const double day_diff = *next - *it;
    ASSERT_NEAR(day_diff, 29.5, 0.75);
  }
}


TEST(NewMoon, Moments) {
  // Find roots in consecutive 10 years, and ensure the roots are consecutive as expected.
  const int32_t year = util::random(1700, 2050);

  std::vector<double> roots;
  for (int i = 0; i < 10; ++i) {
    const auto roots_in_year = moments(year + i);

    // Ensure all roots in this year are really in this year.
    for (const auto root : roots_in_year) {
      const auto dt = astro::julian_day::jde_to_ut1(root);
      const auto [y, _, __] = util::from_ymd(dt.ymd);
      ASSERT_EQ(y, year + i);
    }

    roots.insert(end(roots), cbegin(roots_in_year), cend(roots_in_year));
  }

  // Ensure `roots` are in order and consecutive.
  for (auto it = cbegin(roots); it != cend(roots); ++it) {
    const auto next = std::next(it);
    if (next == cend(roots)) {
      break;
    }
    // Ensure next root is greater than current
    ASSERT_GT(*next, *it);
    // Ensure the gap is around 29.5 days, which is the avg length of a lunar month.
    const double day_diff = *next - *it;
    ASSERT_NEAR(day_diff, 29.5, 0.75);
  }
}


TEST(NewMoon, DiffTest1) {
  using namespace std::ranges;
  using namespace std::chrono_literals;
  using hms = std::chrono::hh_mm_ss<std::chrono::nanoseconds>;

  // Datetimes in UTC+8 (Beijing time).
  // Data source: https://github.com/leetcola/nong/wiki/算法系列之十九：用天文方法计算日月合朔（新月）
  const std::vector<calendar::Datetime> datetimes {
    calendar::Datetime { util::to_ymd(2011, 11, 25), hms { 14h +  9min + 41s + 250ms } },
    calendar::Datetime { util::to_ymd(2011, 12, 25), hms {  2h +  6min + 27s + 250ms } },
    calendar::Datetime { util::to_ymd(2012,  1, 23), hms { 15h + 39min + 24s + 160ms } },
    calendar::Datetime { util::to_ymd(2012,  2, 22), hms {  6h + 34min + 40s + 840ms } },
    calendar::Datetime { util::to_ymd(2012,  3, 22), hms { 22h + 37min +  8s + 910ms } },
    calendar::Datetime { util::to_ymd(2012,  4, 21), hms { 15h + 18min + 22s + 120ms } },
    calendar::Datetime { util::to_ymd(2012,  5, 21), hms {  7h + 46min + 59s + 970ms } },
    calendar::Datetime { util::to_ymd(2012,  6, 19), hms { 23h +  2min +  6s + 390ms } },
    calendar::Datetime { util::to_ymd(2012,  7, 19), hms { 12h + 24min +  2s + 830ms } },
    calendar::Datetime { util::to_ymd(2012,  8, 17), hms { 23h + 54min + 28s +  30ms } },
    calendar::Datetime { util::to_ymd(2012,  9, 16), hms { 10h + 10min + 36s + 990ms } },
    calendar::Datetime { util::to_ymd(2012, 10, 15), hms { 20h +  2min + 30s + 980ms } },
    calendar::Datetime { util::to_ymd(2012, 11, 14), hms {  6h +  8min +  5s + 900ms } },
    calendar::Datetime { util::to_ymd(2012, 12, 13), hms { 16h + 41min + 37s + 600ms } },
    calendar::Datetime { util::to_ymd(2013,  1, 12), hms {  3h + 43min + 31s + 340ms } },
  };

  const auto jdes = datetimes 
                  | views::transform(astro::julian_day::ut1_to_jde) // Actually the data is in UTC, but ignore that here.
                  | views::transform([&](const double jde) { return jde - 8.0 / 24.0; }) // Back to UTC+0.
                  | to<std::vector>();

  RootGenerator gen(jdes[0] - 0.5); // Start from a little before the first root.
  for (const auto expected_root : jdes) {
    const auto actual_root = gen.next();
    ASSERT_NEAR(expected_root, actual_root, 0.00002);
  }
}


TEST(NewMoon, DiffTest2) {
  using namespace std::ranges;
  using namespace std::chrono_literals;
  using hms = std::chrono::hh_mm_ss<std::chrono::nanoseconds>;

  // Datetimes in UTC+8 (Hong Kong time).
  // Data source: https://www.hko.gov.hk/tc/gts/astronomy/Moon_Phase.htm
  const std::vector<calendar::Datetime> datetimes {
    calendar::Datetime { util::to_ymd(2024,  1, 11), hms { 19h + 57min } },
    calendar::Datetime { util::to_ymd(2024,  2, 10), hms {  6h + 59min } },
    calendar::Datetime { util::to_ymd(2024,  3, 10), hms { 17h +  0min } },
    calendar::Datetime { util::to_ymd(2024,  4,  9), hms {  2h + 21min } },
    calendar::Datetime { util::to_ymd(2024,  5,  8), hms { 11h + 22min } },
    calendar::Datetime { util::to_ymd(2024,  6,  6), hms { 20h + 38min } },
    calendar::Datetime { util::to_ymd(2024,  7,  6), hms {  6h + 57min } },
    calendar::Datetime { util::to_ymd(2024,  8,  4), hms { 19h + 13min } },
    calendar::Datetime { util::to_ymd(2024,  9,  3), hms {  9h + 56min } },
    calendar::Datetime { util::to_ymd(2024, 10,  3), hms {  2h + 49min } },
    calendar::Datetime { util::to_ymd(2024, 11,  1), hms { 20h + 47min } },
    calendar::Datetime { util::to_ymd(2024, 12,  1), hms { 14h + 21min } },
    calendar::Datetime { util::to_ymd(2024, 12, 31), hms {  6h + 27min } },
  };

  const auto jdes = datetimes 
                  | views::transform(astro::julian_day::ut1_to_jde) // Actually the data is in UTC, but ignore that here.
                  | views::transform([&](const double jde) { return jde - 8.0 / 24.0; }) // Back to UTC+0.
                  | to<std::vector>();

  const auto actual_roots = moments(2024);

  ASSERT_EQ(size(actual_roots), size(jdes));

  for (const auto [root, expected_root] : views::zip(actual_roots, jdes)) {
    ASSERT_NEAR(root, expected_root, 0.0005);
  }
}

} // namespace astro::moon_phase::test
