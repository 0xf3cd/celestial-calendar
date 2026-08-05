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

#include <gtest/gtest.h>

#include <tuple>
#include <print>
#include <limits>
#include <ranges>
#include "util.hpp"
#include "datetime.hpp"
#include "ymd.hpp"

namespace calendar::test {

TEST(Datetime, FromTimepoint) {
  const auto now = system_clock::now();

  { // Test now.
    const Datetime dt { now };
    const auto ymd = dt.ymd;
    const auto time_of_day = dt.time_of_day;
    const auto fraction = dt.fraction();

    std::println("Now is {}.\nymd {}, hh_mm_ss {}, fraction of day is {}", 
                  now, ymd, time_of_day, fraction);

    ASSERT_LT(time_of_day.hours().count(), 24);
    ASSERT_GE(fraction, 0.0);
    ASSERT_LT(fraction, 1.0);
  }

  { // Test template.
    {
      [[maybe_unused]] const Datetime dt { now };
    }
    {
      const sys_time<days> dur { floor<days>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<nanoseconds> dur { floor<nanoseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<microseconds> dur { floor<microseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
  }

  { // Test with random nanoseconds.
    // The offset is bounded, and not by taste. `now` sits some 1.8e18 ns past the epoch, so an
    // offset drawn from the whole `int64_t` range overflows this addition for roughly its top 40%
    // -- signed overflow, on hundreds of the thousand iterations below. None of the assertions can
    // see it: `Datetime` normalizes whatever wrapped value it is handed, so they held and the test
    // passed while the addition was undefined. UBSan reported it on its first run (#72).
    // `system_clock::duration` is not nanoseconds everywhere (libc++ counts microseconds, MSVC
    // 100ns ticks), so the headroom is computed after converting, not from `count()` directly.
    const auto now_ns = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    const auto headroom = std::numeric_limits<int64_t>::max() - now_ns;

    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + nanoseconds { util::random<int64_t>(-headroom, headroom) };
      const Datetime dt { tp };

      const auto time_of_day = dt.time_of_day;
      const auto fraction = dt.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_ns_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return nanoseconds { 
        util::random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
      }; 
    });

    for (nanoseconds ns : random_ns_views) {
      const auto tp = floor<days>(now) + ns;
      const Datetime dt { tp };

      ASSERT_EQ(dt.fraction(), to_fraction(ns));
    }
  }

  { // Test with random microseconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + std::chrono::microseconds { util::random<int32_t>() };
      const Datetime dt { tp };

      const auto time_of_day = dt.time_of_day;
      const auto fraction = dt.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_us_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return microseconds { 
        util::random<uint64_t>(0, in_a_day<microseconds>() - 1)
      }; 
    });

    for (microseconds us : random_us_views) {
      const auto tp = floor<days>(now) + us;
      const Datetime dt { tp };
      ASSERT_EQ(dt.fraction(), to_fraction(us));
    }
  }

  { // Test with random seconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + seconds { util::random<int32_t>() };
      const Datetime dt { tp };

      const auto time_of_day = dt.time_of_day;
      const auto fraction = dt.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_s_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return seconds { 
        util::random<uint32_t>(0, in_a_day<seconds>() - 1)
      }; 
    });

    for (seconds s : random_s_views) {
      const auto tp = floor<days>(now) + s;
      const Datetime dt { tp };

      ASSERT_EQ(dt.fraction(), to_fraction(s));
    }
  }
}

TEST(Datetime, FromYmdHms) {
  const auto now = system_clock::now();

  { // Test now.
    const year_month_day ymd { floor<days>(now) };
    const hh_mm_ss<nanoseconds> hms { now - floor<days>(now) };

    const Datetime dt { ymd, hms };
    ASSERT_EQ(dt.ymd, ymd);
    ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
  }

  { // Test template.
    {
      [[maybe_unused]] const Datetime dt { now };
    }
    {
      const sys_time<days> dur { floor<days>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<nanoseconds> dur { floor<nanoseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<microseconds> dur { floor<microseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
  }

  { // Test with random nanoseconds.
    for (auto i = 0; i < 100; i++) {
      const sys_days random_day = floor<days>(system_clock::now()) + days { 
        util::random<int32_t>(-365 * 30, 365 * 30) 
      };
      const year_month_day ymd { random_day };

      const auto random_hms_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
        return hh_mm_ss<nanoseconds> { 
          nanoseconds { 
            util::random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
          } 
        };
      });

      for (hh_mm_ss<nanoseconds> hms : random_hms_views) {
        const Datetime dt { ymd, hms };
        ASSERT_EQ(dt.ymd, ymd);
        ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
      }
    }
  }

  { // Test with random microseconds.
    for (auto i = 0; i < 100; i++) {
      const sys_days random_day = floor<days>(system_clock::now()) + days { 
        util::random<int32_t>(-365 * 30, 365 * 30) 
      };
      const year_month_day ymd { random_day };

      const auto random_hms_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
        return hh_mm_ss<microseconds> { 
          microseconds { 
            util::random<uint64_t>(0, in_a_day<microseconds>() - 1)
          } 
        };
      });

      for (hh_mm_ss<microseconds> hms : random_hms_views) {
        const Datetime dt { ymd, hms };
        ASSERT_EQ(dt.ymd, ymd);
        ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
      }
    }
  }
}

TEST(Datetime, FromFraction) {
  for (auto i = 0; i < 100; i++) {
    const sys_days random_day = floor<days>(system_clock::now()) + days { 
      util::random<int32_t>(-365 * 30, 365 * 30) 
    };
    const year_month_day ymd { random_day };

    const auto random_fraction_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
      return util::random<double>(0.0, 1.0 - 1e-8);
    });

    for (double fraction : random_fraction_views) {
      const Datetime dt { ymd, fraction };
      ASSERT_EQ(dt.ymd, ymd);
      ASSERT_NEAR(dt.fraction(), fraction, 1e-10);
    }
  }
}

TEST(Datetime, Consistency) {
  const auto now = system_clock::now();
  constexpr auto ns_per_year = 365 * in_a_day<nanoseconds>();

  const auto random_tp_views = std::views::iota(0, 10000) | std::views::transform([&](auto) {
    const auto signed_ns = static_cast<int64_t>(ns_per_year);
    // `nanoseconds`, matching what `ns_per_year` counts. It used to be `microseconds`, which asked
    // for twenty *thousand* years rather than twenty, and converting that to the nanoseconds this
    // addition promotes to overflowed `int64_t` -- undefined, and invisible to every assertion
    // below, which only ever saw the wrapped result. UBSan reported it (#72).
    return now + nanoseconds { util::random<int64_t>(-20 * signed_ns, 20 * signed_ns) };
  });

  for (auto tp : random_tp_views) {
    const Datetime dt { tp };

    const year_month_day ymd { floor<days>(tp) };
    const hh_mm_ss<nanoseconds> hms { tp - floor<days>(tp) };
    const double fraction = to_fraction(hms.to_duration());

    ASSERT_TRUE(dt.ok());
    ASSERT_EQ(dt.ymd, ymd);
    ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
    ASSERT_NEAR(dt.fraction(), fraction, 1e-10);

    { // Test from ymd and hms.
      const Datetime dt2 { ymd, hms };

      ASSERT_TRUE(dt2.ok());
      ASSERT_EQ(dt2.ymd, ymd);
      ASSERT_EQ(dt2.time_of_day.to_duration(), hms.to_duration());
      ASSERT_NEAR(dt2.fraction(), fraction, 1e-10);
    }

    { // Test from ymd and fraction of day.
      const Datetime dt2 { ymd, fraction };

      ASSERT_TRUE(dt2.ok());
      ASSERT_EQ(dt2.ymd, ymd);

      const nanoseconds dt2_elapsed_ns = dt2.time_of_day.to_duration();
      ASSERT_NEAR(dt2_elapsed_ns.count(), hms.to_duration().count(), 10);

      ASSERT_NEAR(dt2.fraction(), fraction, 1e-10);
    }
  }
}

TEST(Datetime, EdgeCases) {
  { // Test time_point constructor.
    using namespace util::ymd_operator;

    const auto today_tp = floor<days>(system_clock::now());
    const year_month_day ymd { today_tp };

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { 0 } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { -1 } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd - days { 1 });
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() - 1 } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd + days { 1 });
    }
  }

  { // Test ymd and fraction constructor.
    const auto today_tp = floor<days>(system_clock::now());
    
    {
      const Datetime dt { today_tp, 0.0 };
      ASSERT_EQ(dt.ymd, today_tp);
      ASSERT_EQ(dt.fraction(), 0.0); // Enforce strict equality here.
    }

    {
      const Datetime dt { today_tp, 1.0 - 1e-11 };
      ASSERT_EQ(dt.ymd, today_tp);
      ASSERT_NEAR(dt.fraction(), 1.0, 1e-10); // The input is 1e-11 day (864 ns) short of 1.0, so it round-trips just below.
    }

    {
      // Ensure the exceptions are thrown.
      ASSERT_THROW((Datetime { today_tp, 1.0 + 1e-11 }), 
                   std::invalid_argument);

      ASSERT_THROW((Datetime { today_tp, 1.0 }), 
                   std::invalid_argument);

      ASSERT_THROW((Datetime { today_tp, -1e-11 }), 
                   std::invalid_argument);

      // #67: NaN and huge fractions slip past `<`-style checks — and used to reach the
      // undefined double→int64 cast in `from_fraction` before the (too-late) body check.
      ASSERT_THROW((Datetime { today_tp, std::numeric_limits<double>::quiet_NaN() }),
                   std::invalid_argument);

      ASSERT_THROW((Datetime { today_tp, 1e300 }),
                   std::invalid_argument);
    }
  }

  { // Test ymd and hms constructor.
    const auto today_tp = floor<days>(system_clock::now());
    const year_month_day ymd { today_tp };

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { 0 } };
      const Datetime dt { today_tp, hms };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { -1 } };
      ASSERT_THROW((Datetime { today_tp, hms }),
                   std::runtime_error);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() - 1 } };
      const Datetime dt { today_tp, hms };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() } };
      ASSERT_THROW((Datetime { today_tp, hms }),
                   std::runtime_error);
    }
  }
}

TEST(Datetime, OperatorsEqualNonequal) {
  const Datetime dt1 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt2 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt3 { util::to_ymd(2024, 1, 1), 0.5 };

  ASSERT_EQ(dt1, dt2);
  ASSERT_NE(dt1, dt3);
}

TEST(Datetime, OperatorsSpaceship) {
  const Datetime dt1 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt2 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt3 { util::to_ymd(2024, 1, 1), 0.5 };
  const Datetime dt4 { util::to_ymd(2024, 1, 2), 0.0 };

  // Equal
  ASSERT_EQ(dt1 <=> dt2, std::strong_ordering::equal);

  // Less than
  ASSERT_EQ(dt1 <=> dt3, std::strong_ordering::less);
  ASSERT_EQ(dt3 <=> dt4, std::strong_ordering::less);
  ASSERT_TRUE(dt1 < dt3);
  ASSERT_TRUE(dt3 < dt4);

  // Less than or equal
  ASSERT_EQ(dt1 <=> dt2, std::strong_ordering::equal);
  ASSERT_EQ(dt1 <=> dt3, std::strong_ordering::less);
  ASSERT_EQ(dt3 <=> dt4, std::strong_ordering::less);
  ASSERT_TRUE(dt1 <= dt2);
  ASSERT_TRUE(dt1 <= dt3);
  ASSERT_TRUE(dt3 <= dt4);

  // Greater than
  ASSERT_EQ(dt4 <=> dt3, std::strong_ordering::greater);
  ASSERT_EQ(dt3 <=> dt1, std::strong_ordering::greater);
  ASSERT_TRUE(dt4 > dt3);
  ASSERT_TRUE(dt3 > dt1);

  // Greater than or equal
  ASSERT_EQ(dt4 <=> dt3, std::strong_ordering::greater);
  ASSERT_EQ(dt3 <=> dt1, std::strong_ordering::greater);
  ASSERT_EQ(dt2 <=> dt1, std::strong_ordering::equal);
  ASSERT_TRUE(dt4 >= dt3);
  ASSERT_TRUE(dt3 >= dt1);
  ASSERT_TRUE(dt2 >= dt1);
}

TEST(Datetime, AddSeconds) {
  using namespace std::chrono_literals;

  const Datetime noon { util::to_ymd(2024, 6, 15), 0.5 };
  ASSERT_EQ(add_seconds(noon, 3600.0),   (Datetime { util::to_ymd(2024, 6, 15), std::chrono::hh_mm_ss { 13h } }));
  ASSERT_EQ(add_seconds(noon, 43200.0),  (Datetime { util::to_ymd(2024, 6, 16), 0.0 }));
  ASSERT_EQ(add_seconds(noon, -43200.0), (Datetime { util::to_ymd(2024, 6, 15), 0.0 }));
  ASSERT_EQ(add_seconds(noon, -86400.0 * 2.5), (Datetime { util::to_ymd(2024, 6, 13), 0.0 }));

  // #84 review: a shift landing exactly on midnight can leave the fractional sum a half-ulp
  // below zero, which floor/carry used to round into fraction == 1.0 — a constructor throw on
  // a perfectly valid shift. Shift one nanosecond back to midnight, the smallest such case.
  const Datetime just_past_midnight { util::to_ymd(2024, 6, 15), std::chrono::hh_mm_ss { 1ns } };
  const auto back = add_seconds(just_past_midnight, -1e-9);
  ASSERT_EQ(back, (Datetime { util::to_ymd(2024, 6, 15), 0.0 }));

  // Non-finite or day-carry-overflowing offsets must throw, not reach the float→int cast.
  ASSERT_THROW({ std::ignore = add_seconds(noon, std::numeric_limits<double>::quiet_NaN()); }, std::invalid_argument);
  ASSERT_THROW({ std::ignore = add_seconds(noon, 4e14); }, std::invalid_argument);
  ASSERT_THROW({ std::ignore = add_seconds(noon, -4e14); }, std::invalid_argument);

  // Offsets under the day-carry bound can still leave `chrono::year`'s ±32767 — beyond it the
  // stored year is unspecified and `ok()` cannot be trusted, so this must throw, not wrap.
  ASSERT_THROW({ std::ignore = add_seconds(noon, 86400.0 * 2.0e7); }, std::invalid_argument);
  ASSERT_THROW({ std::ignore = add_seconds(noon, -86400.0 * 2.0e7); }, std::invalid_argument);
}

} // namespace calendar::test
