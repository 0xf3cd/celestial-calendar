#include <gtest/gtest.h>
#include <print>
#include <ranges>
#include "util.hpp"
#include "astro.hpp"

namespace astro::syzygy::test {

using namespace astro::syzygy::conjunction::sun_moon;

TEST(SyzygyConjunction, RootGenerator) {
  using namespace std::ranges;

  const auto jde = astro::julian_day::J2000 + util::random(-200000.0, 200000.0);
  
  const auto roots = std::invoke([&] {
    RootGenerator gen(jde);
    std::vector<double> roots;
    for (int i = 0; i < 500; ++i) {
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
  for (auto it = begin(roots); it != end(roots); ++it) {
    const auto next = std::next(it);
    if (next == end(roots)) {
      break;
    }

    // Ensure next > current
    ASSERT_GT(*next, *it);

    // Ensure the gap is around 29.5 days, which is the avg length of a lunar month.
    const double day_diff = *next - *it;
    ASSERT_NEAR(day_diff, 29.5, 0.75);
  }
}

} // namespace astro::syzygy::test
