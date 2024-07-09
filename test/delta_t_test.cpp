#include <gtest/gtest.h>
#include <numeric>
#include "delta_t.hpp"
#include "delta_t_test_helper.hpp"

namespace calendar::delta_t {

TEST(DynamicalTime, delta_t_algo1) {
  ASSERT_THROW(algo1::compute(-4001), std::out_of_range);

  ASSERT_NEAR(algo1::compute(500), 5710.0, 5.0);
  ASSERT_NEAR(algo1::compute(1950),  29.0, 0.1);
  ASSERT_NEAR(algo1::compute(2008),  66.0, 0.1);
}

TEST(DynamicalTime, delta_t_algo2) {
  ASSERT_THROW(algo2::compute(2024, 0), std::out_of_range);
  ASSERT_THROW(algo2::compute(2024, 13), std::out_of_range);

  ASSERT_NEAR(algo2::compute(500, 1), 5710.0, 1.0);
  ASSERT_NEAR(algo2::compute(1950, 1),  29.0, 0.1);
  ASSERT_NEAR(algo2::compute(2008, 1),  66.0, 0.15);
}

#pragma region Delta T Algorithms Statistics

TEST(DynamicalTime, delta_t_statistics) {
  using namespace std::ranges;

  { // Print results.
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

  { // Print differences from observations.
    std::println("ΔT differences from observations:");

    const auto head_line = make_line(algo_info::DELTA_T_ALGO_NAMES);
    const auto devider = std::string(head_line.size(), '-');

    std::println("{}", devider);
    std::println("{}", head_line);
    std::println("{}", devider);

    for (const auto& [year, expected_delta_t] : dataset::ACCURATE_DELTA_T_TABLE) {
      std::println("{}", make_line(
        operation::calc_diff(year, expected_delta_t)
      ));
    }

    std::println("{}", devider);
    std::println("");
  }

  { // Print statistics on differences.
  }

  { // Print statistics on ΔT values.
    // MSE, RMSE, MAE, MAD, R^2, R....
    // Standard deviation... Variance...
  } 
}

}
