#include <gtest/gtest.h>
#include <numeric>
#include "delta_t.hpp"
#include "delta_t_test_helper.hpp"

namespace astro::delta_t {

TEST(DynamicalTime, delta_t_algo1) {
  ASSERT_THROW(algo1::compute(-4001), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo1::compute(500.0), 5710.0, 5.0);
  ASSERT_NEAR(algo1::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo1::compute(2008.0),  66.0, 0.1);
}

TEST(DynamicalTime, delta_t_algo2) {
  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo2::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo2::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo2::compute(2008.0),  66.0, 0.15);
}

TEST(DynamicalTime, delta_t_algo3) {
  ASSERT_THROW(algo3::compute(3000.1), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo3::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo3::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo3::compute(2008.0),  66.0, 0.5);
}

TEST(DynamicalTime, delta_t_algo4) {
  ASSERT_THROW(algo4::compute(2035.1), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo4::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo4::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo4::compute(2008.0),  66.0, 0.6);
}

#pragma region Delta T Algorithms Statistics

TEST(DynamicalTime, delta_t_statistics) {
  using namespace std::ranges;

  {
    std::println("ΔT results:");

    const auto head_line = make_line(std::vector { "year", "expected" }, algo_info::DELTA_T_ALGO_NAMES);
    const auto devider = std::string(head_line.size(), '-');

    std::println("{}", devider);
    std::println("{}", head_line);
    std::println("{}", devider);

    for (const auto& [year, expected_delta_t] : dataset::ACCURATE_DELTA_T_TABLE) {
      const auto datapoint_line = make_line(
        std::vector { pad(year), pad(expected_delta_t) }, 
        operation::evaluate(year)
      );

      std::println("{}", datapoint_line);
    }

    std::println("{}", devider);
    std::println("");
  }

  {
    std::println("ΔT differences from observations:");

    const auto head_line = make_line(std::vector { "year" }, algo_info::DELTA_T_ALGO_NAMES);
    const auto devider = std::string(head_line.size(), '-');

    std::println("{}", devider);
    std::println("{}", head_line);
    std::println("{}", devider);

    for (const auto& [year, expected_delta_t] : dataset::ACCURATE_DELTA_T_TABLE) {
      std::println("{}", make_line(
        std::vector { year },
        operation::calc_diff(year, expected_delta_t)
      ));
    }

    std::println("{}", devider);
    std::println("");
  }
}

}
