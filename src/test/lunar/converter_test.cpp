/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#include <set>
#include <vector>

#include <gtest/gtest.h>
#include "lunar/converter.hpp"
#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"
#include "lunar/algo3.hpp"

namespace calendar::lunar::converter::test {

TEST(Converter, IsValidGregorian) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

    ASSERT_FALSE(Converter::is_valid_gregorian(1901y / 2 / 18));
    ASSERT_TRUE(Converter::is_valid_gregorian(1901y / 2 / 19));
    ASSERT_TRUE(Converter::is_valid_gregorian(1901y / 2 / 20));

    ASSERT_TRUE(Converter::is_valid_gregorian(2024y / 3 / 17));

    ASSERT_TRUE(Converter::is_valid_gregorian(2099y / 1 / 21));
    ASSERT_TRUE(Converter::is_valid_gregorian(2100y / 2 / 8));
    ASSERT_FALSE(Converter::is_valid_gregorian(2100y / 2 / 9));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;

    ASSERT_FALSE(Converter::is_valid_gregorian(400y / 2 / 18));
    ASSERT_TRUE(Converter::is_valid_gregorian(411y / 2 / 19));

    ASSERT_TRUE(Converter::is_valid_gregorian(2024y / 3 / 17));

    ASSERT_TRUE(Converter::is_valid_gregorian(2500y / 2 / 8));
    ASSERT_FALSE(Converter::is_valid_gregorian(2502y / 2 / 9));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_3>;

    ASSERT_FALSE(Converter::is_valid_gregorian(1599y / 2 / 18));
    ASSERT_TRUE(Converter::is_valid_gregorian(1600y / 2 / 18));

    ASSERT_TRUE(Converter::is_valid_gregorian(2199y / 12 / 31));
    ASSERT_TRUE(Converter::is_valid_gregorian(2200y / 1 / 1));
    ASSERT_FALSE(Converter::is_valid_gregorian(2200y / 3 / 1));
  }
}

TEST(Converter, IsValidLunar) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

    ASSERT_FALSE(Converter::is_valid_lunar(1900y / 12 / 29));
    ASSERT_FALSE(Converter::is_valid_lunar(1901y / 1 / 0));
    ASSERT_TRUE(Converter::is_valid_lunar(1901y / 1 / 1));
    ASSERT_TRUE(Converter::is_valid_lunar(1901y / 1 / 2));

    ASSERT_TRUE(Converter::is_valid_lunar(2024y / 3 / 17));
    ASSERT_FALSE(Converter::is_valid_lunar(2024y / 0 / 0));
    ASSERT_FALSE(Converter::is_valid_lunar(2024y / 0 / 1));
    ASSERT_FALSE(Converter::is_valid_lunar(2024y / 0 / 28));
    ASSERT_FALSE(Converter::is_valid_lunar(2024y / 1 / 0));
    ASSERT_FALSE(Converter::is_valid_lunar(2024y / 12 / 0));
    ASSERT_FALSE(Converter::is_valid_lunar(2024y / 14 / 0));

    ASSERT_TRUE(Converter::is_valid_lunar(2099y / 1 / 1));
    ASSERT_TRUE(Converter::is_valid_lunar(2099y / 12 / 29));
    ASSERT_FALSE(Converter::is_valid_lunar(2099y / 12 / 30));
    ASSERT_TRUE(Converter::is_valid_lunar(2099y / 13 / 30));
    ASSERT_FALSE(Converter::is_valid_lunar(2099y / 14 / 0));
    ASSERT_FALSE(Converter::is_valid_lunar(2099y / 14 / 1));
    ASSERT_FALSE(Converter::is_valid_lunar(2100y / 1 / 1));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;
    
    ASSERT_FALSE(Converter::is_valid_lunar(409y / 12 / 29));
    ASSERT_TRUE(Converter::is_valid_lunar(410y / 1 / 1));

    ASSERT_TRUE(Converter::is_valid_lunar(2500y / 1 / 1));
    ASSERT_FALSE(Converter::is_valid_lunar(2501y / 1 / 1));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_3>;

    ASSERT_FALSE(Converter::is_valid_lunar(1599y / 12 / 29));
    ASSERT_TRUE(Converter::is_valid_lunar(1600y / 1 / 1));
    ASSERT_TRUE(Converter::is_valid_lunar(1600y / 3 / 1));
  }
}

TEST(Converter, GregorianToLunarNegative) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2024y / 0 / 29));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(1901y / 1 / 0));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(1901y / 2 / 18));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2099y / 12 / 32));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2100y / 0 / 1));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2100y / 0 / 1));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2100y / 2 / 9));

    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(1901y / 2 / 19));
    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(2100y / 2 / 8));
    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(2024y / 3 / 18));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;

    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(400y / 12 / 29));
    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(411y / 1 / 1));

    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(2500y / 1 / 1));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2502y / 1 / 1));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_3>;

    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(1599y / 12 / 29));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(1600y / 1 / 1));
    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(1600y / 3 / 1));

    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(2199y / 12 / 31));
    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(2200y / 1 / 1));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2200y / 3 / 1));
  }
}

TEST(Converter, LunarToGregorianNegative) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2024y / 0 / 29));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(1901y / 1 / 0));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2099y / 12 / 30));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2100y / 1 / 1));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2100y / 14 / 1));

    ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(1901y / 1 / 1));
    ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(2099y / 13 / 30));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;

    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(409y / 12 / 29));
    ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(410y / 1 / 1));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_3>;

    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(1599y / 12 / 29));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(1599y / 12 / 31));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(1599y / 13 / 29));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(1599y / 13 / 31));
    ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(1600y / 1 / 1));
  }
}

// The nine walks below differ in three parameters and nothing else: which algorithm, which
// years, and how many round-trip draws. The bodies are shared; each test states its own
// parameters, and asserts the year count so that a walk cannot be quietly narrowed to a
// sample (or widened) without the assertion catching it.

namespace {

/** Which years a walk covers: every year the algorithm declares, or a random sample of them. */
enum class YearSupply : uint8_t { ALL, SAMPLED };

/** How many distinct years `SAMPLED` draws. algo2 is too slow to walk end to end. */
constexpr size_t SAMPLE_SIZE = 8;

template <common::Algo A>
auto years_to_walk(const YearSupply supply) -> std::vector<int32_t> {
  const auto& bounds = common::AlgoMetadata<A>::bounds();

  if (supply == YearSupply::SAMPLED) {
    std::set<int32_t> sampled;
    while (sampled.size() < SAMPLE_SIZE) {
      sampled.insert(util::random(bounds.start_lunar_year, bounds.end_lunar_year));
    }
    return { sampled.cbegin(), sampled.cend() };
  }

  std::vector<int32_t> all;
  for (auto year = bounds.start_lunar_year; year <= bounds.end_lunar_year; ++year) {
    all.push_back(year);
  }
  return all;
}

/** The year count `YearSupply::ALL` must yield — what an `ALL` walk asserts it really got. */
template <common::Algo A>
auto full_span() -> size_t {
  const auto& bounds = common::AlgoMetadata<A>::bounds();
  const int32_t span = bounds.end_lunar_year - bounds.start_lunar_year + 1;
  return static_cast<size_t>(span);
}

/** Every day of every given year, Gregorian to Lunar. */
template <common::Algo A>
void check_gregorian_to_lunar(const std::vector<int32_t>& years) {
  using namespace util::ymd_operator;
  using Converter = converter::Converter<A>;

  for (const auto year : years) {
    const auto& info = common::AlgoMetadata<A>::get_info_for_year(year);
    ASSERT_EQ(util::to_ymd(year, 1, 1), Converter::gregorian_to_lunar(info.date_of_first_day));

    uint32_t days_count = 0;
    const auto& ml = info.month_lengths;
    for (uint32_t month_idx = 0; month_idx < ml.size(); ++month_idx) { // Iterate over all months.
      for (uint32_t day = 1; day <= ml[month_idx]; ++day) { // Iterate over all days in the month.
        const auto lunar_date = util::to_ymd(year, month_idx + 1, day);
        ASSERT_EQ(lunar_date, Converter::gregorian_to_lunar(info.date_of_first_day + days_count));
        days_count++;
      }
    }
  }
}

/** Every day of every given year, Lunar to Gregorian. */
template <common::Algo A>
void check_lunar_to_gregorian(const std::vector<int32_t>& years) {
  using namespace util::ymd_operator;
  using Converter = converter::Converter<A>;

  for (const auto year : years) {
    const auto& info = common::AlgoMetadata<A>::get_info_for_year(year);
    ASSERT_EQ(info.date_of_first_day, Converter::lunar_to_gregorian(util::to_ymd(year, 1, 1)));

    uint32_t days_count = 0;
    const auto& ml = info.month_lengths;
    for (uint32_t month_idx = 0; month_idx < ml.size(); ++month_idx) { // Iterate over all months.
      for (uint32_t day = 1; day <= ml[month_idx]; ++day) { // Iterate over all days in the month.
        const auto lunar_date = util::to_ymd(year, month_idx + 1, day);
        ASSERT_EQ(info.date_of_first_day + days_count, Converter::lunar_to_gregorian(lunar_date));
        days_count++;
      }
    }
  }
}

/** Random Gregorian dates through the lunar calendar and back, `draws` times. */
template <common::Algo A>
void check_round_trip(const int draws) {
  using namespace util::ymd_operator;
  using std::chrono::sys_days;
  using std::chrono::year_month_day;

  using AlgoMetadata = common::AlgoMetadata<A>;
  using Converter = converter::Converter<A>;

  const uint32_t difference = (sys_days(AlgoMetadata::bounds().last_gregorian_date) - sys_days(AlgoMetadata::bounds().first_gregorian_date)).count();
  for (auto _ = 0; _ < draws; ++_) {
    const year_month_day solar_date = AlgoMetadata::bounds().first_gregorian_date + util::random<uint32_t>(0, difference);
    ASSERT_TRUE(Converter::is_valid_gregorian(solar_date));

    const std::optional<year_month_day> optional_lunar_date = Converter::gregorian_to_lunar(solar_date);
    ASSERT_TRUE(optional_lunar_date.has_value());

    const year_month_day lunar_date = optional_lunar_date.value(); // NOLINT(bugprone-unchecked-optional-access)
    ASSERT_TRUE(Converter::is_valid_lunar(lunar_date));

    ASSERT_EQ(solar_date, Converter::lunar_to_gregorian(lunar_date));
    ASSERT_EQ(lunar_date, Converter::gregorian_to_lunar(solar_date));
  }
}

} // namespace


TEST(Converter, GregorianToLunarAlgo1) {
  const auto years = years_to_walk<common::Algo::ALGO_1>(YearSupply::ALL);
  ASSERT_EQ(years.size(), full_span<common::Algo::ALGO_1>());
  check_gregorian_to_lunar<common::Algo::ALGO_1>(years);
}

TEST(Converter, GregorianToLunarAlgo2) {
  const auto years = years_to_walk<common::Algo::ALGO_2>(YearSupply::SAMPLED);
  ASSERT_EQ(years.size(), SAMPLE_SIZE);
  check_gregorian_to_lunar<common::Algo::ALGO_2>(years);
}

TEST(Converter, GregorianToLunarAlgo3) {
  const auto years = years_to_walk<common::Algo::ALGO_3>(YearSupply::ALL);
  ASSERT_EQ(years.size(), full_span<common::Algo::ALGO_3>());
  check_gregorian_to_lunar<common::Algo::ALGO_3>(years);
}

TEST(Converter, LunarToGregorianAlgo1) {
  const auto years = years_to_walk<common::Algo::ALGO_1>(YearSupply::ALL);
  ASSERT_EQ(years.size(), full_span<common::Algo::ALGO_1>());
  check_lunar_to_gregorian<common::Algo::ALGO_1>(years);
}

TEST(Converter, LunarToGregorianAlgo2) {
  const auto years = years_to_walk<common::Algo::ALGO_2>(YearSupply::SAMPLED);
  ASSERT_EQ(years.size(), SAMPLE_SIZE);
  check_lunar_to_gregorian<common::Algo::ALGO_2>(years);
}

TEST(Converter, LunarToGregorianAlgo3) {
  const auto years = years_to_walk<common::Algo::ALGO_3>(YearSupply::ALL);
  ASSERT_EQ(years.size(), full_span<common::Algo::ALGO_3>());
  check_lunar_to_gregorian<common::Algo::ALGO_3>(years);
}

TEST(Converter, IntegrationAlgo1) {
  check_round_trip<common::Algo::ALGO_1>(5000);
}

TEST(Converter, IntegrationAlgo2) {
  check_round_trip<common::Algo::ALGO_2>(16); // algo2 is too slow for a full round.
}

TEST(Converter, IntegrationAlgo3) {
  check_round_trip<common::Algo::ALGO_3>(5000);
}

}  // namespace calendar::lunar::converter::test
