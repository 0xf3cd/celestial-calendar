#include <gtest/gtest.h>
#include <algorithm>
#include "lunar/algo2.hpp"

namespace calendar::lunar::algo2::test {

using namespace calendar::lunar::algo2;
using namespace calendar::lunar::common;


TEST(LunarAlgo2, LunarMonthIterator) {
  const double random_jde = astro::julian_day::J2000 + util::random(-365250.0, 365250.0);

  LunarMonthIterator lunar_iter { random_jde };

  std::vector<LunarMonthIterator::LunarMonth> lunar_months;
  std::generate_n(std::back_inserter(lunar_months), 200, [&]() { return lunar_iter.next(); });

  // TODO: Use `std::views::pairwise` when supported.
  const auto lunar_month_pairs = std::views::zip(lunar_months, lunar_months | std::views::drop(1));

  std::vector<JieqiGenerator::JieqiPair> jieqi_pairs;
  for (const auto &[a, b] : lunar_month_pairs) {
    ASSERT_EQ(a.end_jde, b.start_jde);
    ASSERT_EQ(a.end_moment, b.start_moment);

    for (const auto jq_pair : a.contained_jieqis) {
      ASSERT_GE(jq_pair.jde, a.start_jde);
      ASSERT_LT(jq_pair.jde, a.end_jde);

      jieqi_pairs.push_back(jq_pair);
    }
  }

  // Ensure the Jieqis are in order.
  const auto jieqi_jdes = jieqi_pairs | std::views::transform([](const auto& jq) { return jq.jde; });
  ASSERT_TRUE(std::is_sorted(cbegin(jieqi_jdes), cend(jieqi_jdes)));

  // Ensure the first Jieqi in `jieqi_pairs` is reasonable.
  ASSERT_FALSE(jieqi_pairs.empty());
  const double first_jieqi_jde = jieqi_pairs[0].jde;
  ASSERT_GT(first_jieqi_jde, random_jde);
  ASSERT_LT(first_jieqi_jde, random_jde + 45.0);

  // Ensure the Jieqis are consecutive and correct.
  std::vector<JieqiGenerator::JieqiPair> expected_jieqi_pairs;
  JieqiGenerator jieqi_gen { random_jde };

  while (true) {
    const auto jq_pair = jieqi_gen.next();
    if (jq_pair.jde < jieqi_pairs.front().jde) {
      continue;
    }

    expected_jieqi_pairs.push_back(jq_pair);
    if (jq_pair.jde >= jieqi_pairs.back().jde) {
      break;
    }
  }

  ASSERT_EQ(size(expected_jieqi_pairs), size(jieqi_pairs));
  for (const auto& [a, b] : std::views::zip(jieqi_pairs, expected_jieqi_pairs)) {
    ASSERT_EQ(a.jde, b.jde);
    ASSERT_EQ(a.jieqi, b.jieqi);
  }
}

} // namespace calendar::lunar::algo2::test
