/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
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

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "lunar/common.hpp"


namespace calendar::lunar::common::test {

TEST(LunarCommon, CalcBoundsBindsTheCallable) {
  // #78: algo2 hands `calc_bounds` a cache-carrying callable. Taking it by value forks the
  // cache for the duration of the call and recomputes every year the shared path already held.
  // Nothing else in the suite can see that happen — the bounds come out identical either way,
  // just slower — so the binding needs its own guard.
  struct CountingAlgo {
    std::shared_ptr<int> copies = std::make_shared<int>(0);

    CountingAlgo() = default;
    CountingAlgo(const CountingAlgo& other) : copies { other.copies } { ++*copies; }
    CountingAlgo(CountingAlgo&&) = default;
    auto operator=(const CountingAlgo&) -> CountingAlgo& = delete;
    auto operator=(CountingAlgo&&) -> CountingAlgo& = delete;
    ~CountingAlgo() = default;

    auto operator()(const int32_t year) const -> LunarYear {
      return {
        .date_of_first_day = util::to_ymd(year, 2, 1),
        .leap_month        = 0,
        .month_lengths     = std::vector<uint32_t>(12, 30),
      };
    }
  };

  const CountingAlgo algo;
  const auto bounds = calc_bounds(2000, 2010, algo);

  ASSERT_EQ(*algo.copies, 0);
  ASSERT_EQ(bounds.start_lunar_year, 2000);
  ASSERT_EQ(bounds.end_lunar_year, 2010);
}

} // namespace calendar::lunar::common::test
