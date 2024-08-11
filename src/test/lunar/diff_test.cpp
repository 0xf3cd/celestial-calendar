#include <gtest/gtest.h>
#include <ranges>
#include <print>
#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"

// In this file, we test that algo1 and glao2 generates the same lunar month data.

namespace calendar::lunar::test {

auto pick_random_years() -> std::vector<int32_t> {
  using namespace std::ranges;

  // The two algoritms produce different results on some years.
  // Algo1 is using the hard-coded values, collected from Hong Kong Observatory.
  // Algo2 is based on VSOP87 and ELP2000 theories.
  // TODO: Check the correctness of these years.
  const auto drop_list = std::vector<int32_t> {
    1914, 1915, 1916, 1920,
  };

  const auto should_keep = [&](int32_t year) {
    return not std::any_of(cbegin(drop_list), cend(drop_list), [&](auto drop_year) {
      return year == drop_year;
    });
  };

#if defined(__arm__) or defined(__aarch64__)
  // If running on ARM, then test less cases.
  // Mainly because the test run is too slow in ARM dockers on the GitHub CI.
  constexpr double threshold = 0.042;
#else
  // Otherwise, randomly pick some years.
  constexpr double threshold = 0.25;
#endif

  // Algo2 doesn't really have year limits. So use algo1's.
  return views::iota(algo1::START_YEAR, algo1::END_YEAR + 1)
        | views::filter(should_keep)
        | views::filter([](auto) { return util::random(0.0, 1.0) < threshold; })
        | to<std::vector>();
}


TEST(LunarAlgoDiff, Perf) {
  using namespace std::ranges;
  const auto years = pick_random_years();

  const auto tracked_run = [](const std::function<void()>& f) -> double {
    const auto start = std::chrono::steady_clock::now();
    f();
    const auto end = std::chrono::steady_clock::now();
    const auto ns_gap = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(ns_gap);
  };

  std::vector<double> algo1_results;
  std::vector<double> algo2_results;

  for (const auto year : years) {
    algo1_results.push_back(tracked_run([&] { algo1::get_info_for_year(year); }));
    algo2_results.push_back(tracked_run([&] { algo2::get_info_for_year(year); }));
  }

  const double algo1_sum = std::reduce(cbegin(algo1_results), cend(algo1_results));
  const double algo2_sum = std::reduce(cbegin(algo2_results), cend(algo2_results));

  std::println("algo1: {} ns", algo1_sum / static_cast<double>(years.size()));
  std::println("algo2: {} ns", algo2_sum / static_cast<double>(years.size()));
  std::println("algo1 is {}x faster", algo2_sum / algo1_sum);
}

TEST(LunarAlgoDiff, Correctness) {
  // This test ensures that algo1 and algo2 have the same result on leap months.
  // Use algo1's result as the benchmark.

  using namespace std::ranges;
  const auto years = pick_random_years();

  for (const auto year : years) {
    std::println("year: {}", year);

    const auto info1 = algo1::get_info_for_year(year);
    const auto info2 = algo2::get_info_for_year(year);

    ASSERT_EQ(info1.date_of_first_day, info2.date_of_first_day);
    ASSERT_EQ(info1.leap_month, info2.leap_month);
    ASSERT_EQ(info1.month_lengths, info2.month_lengths);
  }
}

} // namespace calendar::lunar::test
