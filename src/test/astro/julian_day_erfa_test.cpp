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

#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "datetime.hpp"
#include "julian_day.hpp"
#include "random.hpp"
#include "ymd.hpp"

extern "C" {
#include "erfa.h"
}

namespace astro::julian_day::test {

namespace {

using calendar::Datetime;

constexpr double LOWER_JD = 1867522.5;
constexpr double UPPER_JD = 13689325.5;
constexpr std::int64_t NS_PER_DAY = 86'400'000'000'000;

[[nodiscard]] auto same_double(const double left, const double right) -> bool {
  return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
}

[[nodiscard]] auto erfa_day_start(const int year, const unsigned month, const unsigned day) -> double {
  double djm0 = 0.0;
  double djm = 0.0;
  if (eraCal2jd(year, static_cast<int>(month), static_cast<int>(day), &djm0, &djm) != 0) {
    throw std::logic_error { "ERFA rejected a valid Gregorian date" };
  }
  return djm0 + djm;
}

[[nodiscard]] auto erfa_datetime(const double jd) -> Datetime {
  int year = 0;
  int month = 0;
  int day = 0;
  double fraction = 0.0;
  if (eraJd2cal(jd, 0.0, &year, &month, &day, &fraction) != 0) {
    throw std::logic_error { "ERFA rejected an accepted Julian date" };
  }
  return Datetime {
    util::to_ymd(year, static_cast<unsigned>(month), static_cast<unsigned>(day)),
    fraction,
  };
}

[[nodiscard]] auto forward_matches(const Datetime& input) -> bool {
  const auto [year, month, day] = util::from_ymd(input.ymd);
  const double expected = erfa_day_start(year, month, day) + input.fraction();
  return same_double(ut1_to_jd(input), expected);
}

[[nodiscard]] auto inverse_matches(const double jd) -> bool {
  return jd_to_ut1(jd) == erfa_datetime(jd);
}

template <typename Callable>
[[nodiscard]] auto runtime_error_message(Callable call) -> std::string {
  try {
    call();
  } catch (const std::runtime_error& error) {
    return error.what();
  }
  throw std::logic_error { "expected std::runtime_error" };
}

} // namespace

TEST(JulianDayErfaOracle, ExhaustiveDomainAndFractions) {
  std::uint64_t forward_dates = 0;
  for (int year = 1; year <= 32767; ++year) {
    for (unsigned month = 1; month <= 12; ++month) {
      const auto last = std::chrono::year_month_day_last {
        std::chrono::year { year },
        std::chrono::month_day_last { std::chrono::month { month } },
      };
      for (unsigned day = 1; day <= static_cast<unsigned>(last.day()); ++day) {
        const Datetime input { util::to_ymd(year, month, day), 0.0 };
        if (!forward_matches(input)) {
          FAIL() << "forward mismatch at " << year << '-' << month << '-' << day;
        }
        ++forward_dates;
      }
    }
  }
  EXPECT_EQ(forward_dates, 11'967'900U);

  const auto inverse_days = static_cast<std::int64_t>(UPPER_JD - LOWER_JD);
  for (std::int64_t offset = 0; offset < inverse_days; ++offset) {
    const double jd = LOWER_JD + static_cast<double>(offset);
    if (!inverse_matches(jd)) {
      FAIL() << "inverse mismatch at JD " << jd;
    }
  }
  EXPECT_EQ(inverse_days, 11'821'803);

  constexpr std::array<int, 9> FRACTION_YEARS { 1, 4, 100, 400, 401, 1582, 2000, 2024, 32767 };
  constexpr std::array<std::int64_t, 5> FRACTION_NS {
    0,
    1,
    1'000'000'000,
    NS_PER_DAY / 2,
    NS_PER_DAY - 1,
  };
  std::uint64_t directed_fractions = 0;
  for (const int year : FRACTION_YEARS) {
    for (unsigned month = 1; month <= 12; ++month) {
      const auto last = std::chrono::year_month_day_last {
        std::chrono::year { year },
        std::chrono::month_day_last { std::chrono::month { month } },
      };
      for (const unsigned day : { 1U, static_cast<unsigned>(last.day()) }) {
        const auto ymd = util::to_ymd(year, month, day);
        for (const std::int64_t nanoseconds : FRACTION_NS) {
          const Datetime input {
            ymd,
            std::chrono::hh_mm_ss { std::chrono::nanoseconds { nanoseconds } },
          };
          if (!forward_matches(input)) {
            FAIL() << "fractional forward mismatch at " << year << '-' << month << '-' << day;
          }

          const double jd = erfa_day_start(year, month, day) + input.fraction();
          if (year >= 401 && jd < UPPER_JD && !inverse_matches(jd)) {
            FAIL() << "fractional inverse mismatch at JD " << jd;
          }
          ++directed_fractions;
        }
      }
    }
  }
  EXPECT_EQ(directed_fractions, 1'080U);

  constexpr std::uint64_t RANDOM_ROWS = 200'000;
  for (std::uint64_t index = 0; index < RANDOM_ROWS; ++index) {
    const double jd = LOWER_JD + static_cast<double>(util::random<std::int64_t>(0, inverse_days - 1))
                    + (static_cast<double>(util::random<std::int64_t>(0, NS_PER_DAY - 1))
                       / static_cast<double>(NS_PER_DAY));
    if (!inverse_matches(jd)) {
      FAIL() << "random inverse mismatch at row " << index;
    }

    const Datetime current = jd_to_ut1(jd);
    const Datetime independent_forward {
      current.ymd,
      std::chrono::hh_mm_ss {
        std::chrono::nanoseconds { util::random<std::int64_t>(0, NS_PER_DAY - 1) },
      },
    };
    if (!forward_matches(independent_forward)) {
      FAIL() << "random forward mismatch at row " << index;
    }
    if (!forward_matches(current) || current != jd_to_ut1(ut1_to_jd(current))) {
      FAIL() << "random round-trip mismatch at row " << index;
    }
  }
}

TEST(JulianDayErfaOracle, BoundaryAndMessageContracts) {
  EXPECT_TRUE(inverse_matches(LOWER_JD));
  EXPECT_TRUE(inverse_matches(std::nextafter(UPPER_JD, -std::numeric_limits<double>::infinity())));

  EXPECT_EQ(
    runtime_error_message([] {
      static_cast<void>(jd_to_ut1(std::nextafter(LOWER_JD, -std::numeric_limits<double>::infinity())));
    }),
    "The julian day number 1867522.4999999998 is below JD 1867522.5 (401-01-01), "
    "where the estimated gregorian year drops under 401."
  );
  EXPECT_EQ(
    runtime_error_message([] { static_cast<void>(jd_to_ut1(UPPER_JD)); }),
    "The julian day number 13689325.5 is beyond the representable years."
  );
  EXPECT_EQ(
    runtime_error_message([] { static_cast<void>(jd_to_ut1(std::numeric_limits<double>::quiet_NaN())); }),
    "The julian day number nan is not finite."
  );
  EXPECT_EQ(
    runtime_error_message([] { static_cast<void>(jd_to_ut1(std::numeric_limits<double>::infinity())); }),
    "The julian day number inf is not finite."
  );
  EXPECT_EQ(
    runtime_error_message([] { static_cast<void>(jd_to_ut1(-std::numeric_limits<double>::infinity())); }),
    "The julian day number -inf is not finite."
  );
  EXPECT_EQ(
    runtime_error_message([] {
      static_cast<void>(ut1_to_jd(Datetime { util::to_ymd(0, 1, 1), 0.0 }));
    }),
    "The year 0 is < 1, not supported by this algorithm."
  );
}

} // namespace astro::julian_day::test
