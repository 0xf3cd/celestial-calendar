/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>

#include "chart.hpp"
#include "ephemeris_event_facts.hpp"
#include "house_test_helper.hpp"

namespace astro::test {
namespace {

using delta_t::Model;
using ephemeris::Target;
using julian_day::Scale;
using toolbox::AngleDeg;
using toolbox::DistanceAu;

constexpr GeoLocation SITE { .latitude = AngleDeg { 40.0 }, .longitude = AngleDeg { -75.0 } };

auto expect_axes(const house::Axes& axes, const house::Result& houses) -> void {
  EXPECT_EQ(std::bit_cast<uint64_t>(axes.ascendant.deg()), std::bit_cast<uint64_t>(houses.ascendant.deg()));
  EXPECT_EQ(std::bit_cast<uint64_t>(axes.midheaven.deg()), std::bit_cast<uint64_t>(houses.midheaven.deg()));
  EXPECT_EQ(std::bit_cast<uint64_t>(axes.descendant.deg()), std::bit_cast<uint64_t>(houses.descendant.deg()));
  EXPECT_EQ(std::bit_cast<uint64_t>(axes.imum_coeli.deg()), std::bit_cast<uint64_t>(houses.imum_coeli.deg()));
}

} // namespace

TEST(CoreOwnership, TargetFactsOwnTheRosterAndSeparateNodeTrajectories) {
  EXPECT_EQ(ephemeris::TARGETS.size(), ephemeris::TARGET_FACTS.size());
  EXPECT_EQ(chart::BODY_COUNT, ephemeris::TARGETS.size());
  for (const auto& facts : ephemeris::TARGET_FACTS) {
    EXPECT_TRUE(ephemeris::is_target(facts.target));
    EXPECT_FALSE(facts.name.empty());
    EXPECT_EQ(ephemeris::source_of(facts.target), facts.source);
    EXPECT_EQ(ephemeris::position_domain(facts.target), facts.position_domain);
  }
  EXPECT_EQ(ephemeris::target_facts(Target::MEAN_DESCENDING).node, lunar_node::Node::MEAN_DESCENDING);
  EXPECT_EQ(ephemeris::target_facts(Target::MEAN_DESCENDING).rate_node, lunar_node::Node::MEAN_ASCENDING);
  EXPECT_EQ(ephemeris::target_facts(Target::TRUE_DESCENDING).node, lunar_node::Node::TRUE_DESCENDING);
  EXPECT_EQ(ephemeris::target_facts(Target::TRUE_DESCENDING).rate_node, lunar_node::Node::TRUE_ASCENDING);
  EXPECT_FALSE(ephemeris::position_domain(Target::SUN).has_value());
  EXPECT_EQ(ephemeris::position_domain(Target::PLUTO),
            (std::array { planet::geocentric_coord::PLUTO_START_JDE_TT, planet::geocentric_coord::PLUTO_END_JDE_TT }));
  EXPECT_FALSE(ephemeris::is_target(static_cast<Target>(255)));
  EXPECT_THROW(std::ignore = ephemeris::position_domain(static_cast<Target>(255)), std::invalid_argument);
}

TEST(CoreOwnership, ComputedCoordinateValidityChecksEachClauseIndependently) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  const auto check = [](const double longitude, const double latitude, const std::optional<double> distance) {
    ephemeris::detail::validate_position({
      .longitude = AngleDeg { longitude },
      .latitude = AngleDeg { latitude },
      .distance = distance.has_value() ? std::optional { DistanceAu { *distance } } : std::nullopt,
    });
  };

  EXPECT_NO_THROW(check(30.0, 10.0, 1.0));
  EXPECT_NO_THROW(check(30.0, -90.0, std::nullopt));
  EXPECT_NO_THROW(check(30.0, 90.0, std::nullopt));
  EXPECT_THROW(check(nan, 10.0, 1.0), std::runtime_error);
  EXPECT_THROW(check(infinity, 10.0, 1.0), std::runtime_error);
  EXPECT_THROW(check(30.0, nan, 1.0), std::runtime_error);
  EXPECT_THROW(check(30.0, infinity, 1.0), std::runtime_error);
  EXPECT_THROW(check(30.0, -90.01, 1.0), std::runtime_error);
  EXPECT_THROW(check(30.0, 90.01, 1.0), std::runtime_error);
  EXPECT_THROW(check(30.0, 10.0, nan), std::runtime_error);
  EXPECT_THROW(check(30.0, 10.0, infinity), std::runtime_error);
  EXPECT_THROW(check(30.0, 10.0, 0.0), std::runtime_error);
  EXPECT_THROW(check(30.0, 10.0, -1.0), std::runtime_error);
}

TEST(CoreOwnership, InputErrorsAndProviderWindowsRemainInputErrors) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (const auto target : ephemeris::TARGETS) {
    EXPECT_THROW(std::ignore = ephemeris::position(target, nan), std::invalid_argument);
    EXPECT_NO_THROW(std::ignore = ephemeris::position(target, julian_day::J2000));
  }
  EXPECT_THROW(std::ignore = sun::geocentric_coord::apparent(nan), std::invalid_argument);
  EXPECT_THROW(std::ignore = moon::geocentric_coord::apparent(nan), std::invalid_argument);
  for (const auto target : { Target::PLUTO, Target::MEAN_ASCENDING, Target::TRUE_DESCENDING }) {
    const auto domain = ephemeris::position_domain(target);
    ASSERT_TRUE(domain.has_value());
    if (domain.has_value()) {
      EXPECT_NO_THROW(std::ignore = ephemeris::position(target, domain->front()));
      EXPECT_THROW(std::ignore = ephemeris::longitude_rate(target, domain->front()), std::invalid_argument);
      EXPECT_THROW(std::ignore = ephemeris::position(target, domain->back()), std::invalid_argument);
    }
  }
}

TEST(CoreOwnership, GeneralResolverHasNoCivilPolicyAndUtcForwardIgnoresDeltaT) {
  const calendar::Datetime historical { util::to_ymd(1800, 1, 1), 0.5 };
  EXPECT_NO_THROW(std::ignore = julian_day::resolve_times(historical, Scale::UT1));
  EXPECT_THROW(std::ignore = chart::resolve_civil_times(historical, Scale::UT1), std::invalid_argument);
  EXPECT_THROW(std::ignore = julian_day::civil_to_tt(historical, Scale::UTC), std::invalid_argument);

  const calendar::Datetime modern { util::to_ymd(2040, 1, 1), 0.5 };
  EXPECT_EQ(julian_day::civil_to_tt(modern, Scale::UTC, Model::ALGO4), leap_second::utc_to_tt(modern));
  EXPECT_THROW(std::ignore = julian_day::resolve_times(modern, Scale::UTC, Model::ALGO4), std::invalid_argument);
  EXPECT_THROW(std::ignore = julian_day::civil_to_tt(modern, static_cast<Scale>(255)), std::invalid_argument);
}

TEST(CoreOwnership, Algo4DerivedCutoffPrecedesReverseAndJointAdmission) {
  const calendar::Datetime tail { util::to_ymd(2034, 12, 31), 0.9999 };
  const auto general = julian_day::resolve_times(tail, Scale::UT1, Model::ALGO4);
  EXPECT_EQ(general.jd_ut1, julian_day::ut1_to_jd(tail));
  EXPECT_GE(general.jde_tt, delta_t::ALGO4_END_JDE_TT);
  EXPECT_THROW(std::ignore = delta_t::tt_to_ut1(Model::ALGO4, julian_day::jde_to_tt(general.jde_tt)), std::invalid_argument);
  for (const auto scale : { Scale::UT1, Scale::UTC }) {
    EXPECT_THROW(std::ignore = chart::resolve_civil_times(tail, scale, Model::ALGO4), std::invalid_argument);
    const calendar::Datetime before { util::to_ymd(2034, 12, 31), 0.99 };
    EXPECT_NO_THROW(std::ignore = chart::resolve_civil_times(before, scale, Model::ALGO4));
  }
  const calendar::Datetime multiply_invalid { util::to_ymd(2099, 12, 31), 0.99 };
  try {
    std::ignore = chart::calculate(multiply_invalid, Scale::UTC, SITE, house::System::EQUAL, Model::ALGO4);
    FAIL() << "Expected civil Algo4 cutoff";
  } catch (const std::invalid_argument& error) {
    EXPECT_EQ(std::string_view { error.what() }, "Civil Algo4 requires JDE(TT) before 2035-01-01");
  }
  EXPECT_EQ(chart::civil_derived_tt_cutoff(Model::ALGO4), delta_t::ALGO4_END_JDE_TT);
  for (const auto model : { Model::ALGO1, Model::ALGO2, Model::ALGO3, Model::ALGO5 }) {
    EXPECT_FALSE(chart::civil_derived_tt_cutoff(model).has_value());
  }
}

TEST(CoreOwnership, CivilComposerHasNoJointWindowAndKeepsPolicyEdges) {
  for (const auto scale : { Scale::UTC, Scale::UT1 }) {
    for (const int32_t year : { chart::CIVIL_START_YEAR - 1, chart::CIVIL_END_YEAR_EXCLUSIVE }) {
      EXPECT_THROW(std::ignore = chart::resolve_civil_times(
        calendar::Datetime { util::to_ymd(year, 1, 1), 0.0 }, scale), std::invalid_argument);
    }
  }
  const calendar::Datetime first { util::to_ymd(chart::CIVIL_START_YEAR, 1, 1), 0.25 };
  EXPECT_LT(chart::resolve_civil_times(first, Scale::UT1).jde_tt, chart::detail::START_JDE_TT);
  const calendar::Datetime utc_start { leap_second::MODERN_UTC_START, 0.0 };
  EXPECT_NO_THROW(std::ignore = chart::resolve_civil_times(utc_start, Scale::UTC));
  EXPECT_THROW(std::ignore = chart::resolve_civil_times(calendar::add_seconds(utc_start, -1.0), Scale::UTC), std::invalid_argument);
  auto invalid = utc_start;
  invalid.ymd = util::to_ymd(1972, 2, 30);
  EXPECT_THROW(std::ignore = julian_day::civil_to_tt(invalid, Scale::UT1), std::invalid_argument);
}

TEST(CoreOwnership, FourAxesMatchEveryHouseSystemAndSelectedModel) {
  // Both compositions must evaluate at runtime; constant-folded sidereal polynomials can round differently.
  const volatile double jd_ut1 = 2461042.0;
  const volatile double jde_tt = 2461042.0008;
  for (const auto model : { earth::nutation::Model::MEEUS, earth::nutation::Model::IAU_1980 }) {
    const auto axes = house::axes(jd_ut1, jde_tt, SITE, model);
    for (const auto system : { house::System::EQUAL, house::System::WHOLE_SIGN, house::System::PLACIDUS }) {
      expect_axes(axes, house::calculate(jd_ut1, jde_tt, SITE, system, model));
    }
  }
  const GeoLocation polar { .latitude = AngleDeg { 89.0 }, .longitude = AngleDeg { 0.0 } };
  EXPECT_NO_THROW(std::ignore = house::axes(2461042.0, 2461042.0008, polar));
  EXPECT_THROW(std::ignore = house::calculate(2461042.0, 2461042.0008, polar, house::System::PLACIDUS), std::invalid_argument);
  const GeoLocation pole { .latitude = AngleDeg { 90.0 }, .longitude = AngleDeg { 0.0 } };
  EXPECT_THROW(std::ignore = house::axes(2461042.0, 2461042.0008, pole), std::invalid_argument);
  EXPECT_THROW(std::ignore = house::axes(std::numeric_limits<double>::quiet_NaN(), 2461042.0, SITE), std::invalid_argument);
  EXPECT_THROW(std::ignore = house::axes(2461042.0, std::numeric_limits<double>::quiet_NaN(), SITE), std::invalid_argument);
}

TEST(CoreOwnership, FourAxesRetainTheUnresolvedDirectionGuard) {
  const calendar::Datetime civil { util::to_ymd(2026, 1, 1), std::chrono::seconds { 43205 } };
  const auto times = chart::resolve_civil_times(civil, Scale::UTC);
  for (const auto& [latitude, longitude] : std::array {
    std::pair { house::test::WITNESS_LATITUDE_DEG, house::test::WITNESS_NORTH_LONGITUDE_DEG },
    std::pair { -house::test::WITNESS_LATITUDE_DEG, house::test::WITNESS_SOUTH_LONGITUDE_DEG },
  }) {
    const GeoLocation witness { .latitude = AngleDeg { latitude }, .longitude = AngleDeg { longitude } };
    EXPECT_THROW(std::ignore = house::axes(times.jd_ut1, times.jde_tt, witness), std::invalid_argument);
  }
}

} // namespace astro::test
