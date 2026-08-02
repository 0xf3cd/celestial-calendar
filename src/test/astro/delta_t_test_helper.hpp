#include <vector>
#include <map>
#include <unordered_map>

#include <ranges>
#include <functional>

#include <print>
#include <format>

#include "delta_t.hpp"


namespace astro::delta_t {

#pragma region Datasets

namespace dataset::test {

using namespace dataset;

using DatasetType = std::map<double, double>; // { year, ΔT }

// Ref: https://eclipse.gsfc.nasa.gov/LEcat5/deltat.html
// Ref: https://www.eclipsewise.com/help/deltat.html
// Recent values of ΔT from direct observations.
// 2015+ entries: IERS Bulletin A final values (AstroTime-Analysis @ ddf3be1),
// median of ±15 days around each year boundary.
const DatasetType ACCURATE_DELTA_T_TABLE {
  { 1955.0, 31.1 },
  { 1960.0, 33.2 },
  { 1965.0, 35.7 },
  { 1970.0, 40.2 },
  { 1975.0, 45.5 },
  { 1980.0, 50.5 },
  { 1985.0, 54.3 },
  { 1990.0, 56.9 },
  { 1995.0, 60.8 },
  { 2000.0, 63.8 },
  { 2005.0, 64.7 },
  { 2010.0, 66.1 },
  { 2014.0, 67.3 },
  { 2015.0, 67.64 },
  { 2016.0, 68.10 },
  { 2017.0, 68.59 },
  { 2018.0, 68.97 },
  { 2019.0, 69.22 },
  { 2020.0, 69.36 },
  { 2021.0, 69.36 },
  { 2022.0, 69.29 },
  { 2023.0, 69.20 },
  { 2024.0, 69.18 },
  { 2025.0, 69.14 },
  { 2026.0, 69.11 },
};

}  // namespace dataset::test

#pragma endregion


#pragma region Algorithm Info

namespace algo_info {

// #64: was `double(int32_t)`, which truncated fractional years.
using delta_t_func = std::function<double(double)>;

const std::array<std::string, 5> DELTA_T_ALGO_NAMES {
  "algo1", "algo2", "algo3", "algo4", "algo5"
};

const std::array<delta_t_func, 5> DELTA_T_ALGO_FUNCS {
  algo1::compute,
  algo2::compute,
  algo3::compute,
  algo4::compute,
  algo5::compute
};

}  // namespace algo_info

#pragma endregion


#pragma region Operation

namespace operation {

using namespace std::ranges;

/** @brief Evaluate the ΔT values for the given year on all algorithms. */
auto evaluate(const double year) {
  return algo_info::DELTA_T_ALGO_FUNCS | views::transform([year](auto func) {
    return func(year);
  });
}

/** @brief Calculate the differences between:
 *         - the expected ΔT value of the given year 
 *         - and the calculated ΔT values of all algorithms of the given year */
auto calc_diff(const double year, const double expected_delta_t) {
  return evaluate(year) | views::transform([expected_delta_t](auto delta_t) {
    return delta_t - expected_delta_t;
  });
}

} // namespace operation

#pragma endregion


#pragma region Other Helper Functions

// TODO: Use `std::views::join_with` when it gets supported.
auto join_with(
  const std::ranges::range auto& view, 
  const std::string& separator
) -> std::string {
  // Low performance implementation...
  std::string str;
  for (const auto& substr : view) {
    str += substr + separator;
  }
  if (view.empty()) {
    return str;
  }
  return str.substr(0, str.size() - separator.size());
}

inline constexpr int32_t PAD_WIDTH = 10;

/** @brief Pad the string with spaces. Use generic lambda here
 *         since template function cannot be implicitly instantiated
 *         when using with views/ranges.
 */
const auto pad = []<typename T>(T result) -> std::string {
  if constexpr (std::floating_point<T>) {
    return std::format("{:^{}.3f}", result, PAD_WIDTH);
  }
  return std::vformat("{:^{}}", std::make_format_args(result, PAD_WIDTH));
};

auto make_line(
  const std::ranges::range auto& range1, 
  const std::ranges::range auto& range2
) -> std::string {
  const std::string separator { " | " };

  // TODO: Use `std::views::concat` when it gets supported.
  using namespace std::views;
  return join_with(range1 | transform(pad), separator)
       + separator
       + join_with(range2 | transform(pad), separator);
}

#pragma endregion

} // namespace astro::delta_t
