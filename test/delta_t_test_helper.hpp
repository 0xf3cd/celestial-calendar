#include <vector>
#include <map>
#include <unordered_map>

#include <ranges>
#include <functional>

#include <print>
#include <format>

#include "delta_t.hpp"


namespace calendar::delta_t {

#pragma region Datasets

namespace dataset {

using DatasetType = std::map<int32_t, double>; // { year, ΔT }

// Ref: https://eclipse.gsfc.nasa.gov/LEcat5/deltat.html
// Ref: https://www.eclipsewise.com/help/deltat.html
// Recent values of ΔT from direct observations
const DatasetType ACCURATE_DELTA_T_TABLE {
  { 1955, 31.1 },
  { 1960, 33.2 },
  { 1965, 35.7 },
  { 1970, 40.2 },
  { 1975, 45.5 },
  { 1980, 50.5 },
  { 1985, 54.3 },
  { 1990, 56.9 },
  { 1995, 60.8 },
  { 2000, 63.8 },
  { 2005, 64.7 },
  { 2010, 66.1 },
  { 2014, 67.3 },
};

} // namespace dataset


#pragma region Algorithm Info

namespace algo_info {

using delta_t_func = std::function<double(int32_t)>;

/** @brief Bind the `month` parameter to `algo2::compute`. */
delta_t_func algo2_partial(int32_t month) {
  using namespace std::placeholders;
  return std::bind(algo2::compute, _1, month);
}

const std::array<std::string, 6> DELTA_T_ALGO_NAMES {
  "algo1", "algo2_m1", "algo2_m4", "algo2_m7", "algo2_m10", "algo2_m12"
};

const std::array<delta_t_func, 6> DELTA_T_ALGO_FUNCS {
  algo1::compute,
  algo2_partial(1),
  algo2_partial(4),
  algo2_partial(7),
  algo2_partial(10),
  algo2_partial(12),
};  

} // namespace algo_list


#pragma region Operation

namespace operation {

using namespace std::ranges;

/** @brief Evaluate the ΔT values for the given year on all algorithms. */
auto evaluate(const int32_t year) {
  return algo_info::DELTA_T_ALGO_FUNCS | views::transform([year](auto func) {
    return func(year);
  });
}

/** @brief Calculate the differences between:
 *         - the expected ΔT value of the given year 
 *         - and the calculated ΔT values of all algorithms of the given year */
auto calc_diff(const int32_t year, const double expected_delta_t) {
  return evaluate(year) | views::transform([expected_delta_t](auto delta_t) {
    return delta_t - expected_delta_t;
  });
}

} // namespace operation


#pragma region Other Helper Functions

// TODO: Use `std::views::join_with` when it gets supported.
std::string join_with(
  const std::ranges::range auto& view, 
  const std::string& separator
) {
  // Low performance implementation...
  std::string str { "" };
  for (auto&& substr : view) {
    str += substr + separator;
  }
  if (view.empty()) {
    return str;
  }
  return str.substr(0, str.size() - separator.size());
}

constexpr int32_t PAD_WIDTH = 10;

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


std::string make_line(const std::ranges::range auto& range) {
  using namespace std::views;
  return join_with(range | transform(pad), " | ");
}

std::string make_line(
  const std::ranges::range auto& range1, 
  const std::ranges::range auto& range2
) {
  const std::string separator { " | " };

  // TODO: Use `std::views::concat` when it gets supported.
  using namespace std::views;
  return join_with(range1 | transform(pad), separator)
       + separator
       + join_with(range2 | transform(pad), separator);
}

} // namespace calendar::delta_t
