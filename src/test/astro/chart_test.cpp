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
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include <gtest/gtest.h>

#include "chart.hpp"

namespace astro::chart::test {
namespace {

using astro::delta_t::Model;
using astro::ephemeris::Target;
using astro::house::System;
using astro::toolbox::AngleDeg;
using calendar::Datetime;

constexpr std::array MODELS { Model::ALGO1, Model::ALGO2, Model::ALGO3, Model::ALGO4, Model::ALGO5 };
constexpr std::array TARGETS {
  Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS, Target::JUPITER,
  Target::SATURN, Target::URANUS, Target::NEPTUNE, Target::PLUTO, Target::MEAN_ASCENDING,
  Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING,
};

static_assert(std::is_same_v<std::underlying_type_t<Scale>, uint8_t>);
static_assert(not std::is_convertible_v<Scale, int>);
static_assert(std::tuple_size_v<decltype(Snapshot::bodies)> == 14);
static_assert(std::tuple_size_v<decltype(astro::house::Result::cusps)> == 12);
static_assert(noexcept(astro::delta_t::compute(2026.0)));
static_assert(not std::is_default_constructible_v<Snapshot>);
static_assert(not std::is_default_constructible_v<detail::Position>);
static_assert(detail::START_JDE_TT == 2409543.875);
static_assert(detail::END_JDE_TT == 2488069.125);

auto location(const double latitude, const double longitude) -> astro::GeoLocation {
  return { .latitude = AngleDeg { latitude }, .longitude = AngleDeg { longitude } };
}

auto civil(const int32_t year, const uint32_t month, const uint32_t day,
           const int64_t elapsed_ns) -> Datetime {
  return Datetime { util::to_ymd(year, month, day), std::chrono::hh_mm_ss { std::chrono::nanoseconds { elapsed_ns } } };
}

auto independent_delta_t(const Model model, const double year) -> double {
  switch (model) {
    case Model::ALGO1: return astro::delta_t::algo1::compute(year);
    case Model::ALGO2: return astro::delta_t::algo2::compute(year);
    case Model::ALGO3: return astro::delta_t::algo3::compute(year);
    case Model::ALGO4: return astro::delta_t::algo4::compute(year);
    case Model::ALGO5: return astro::delta_t::algo5::compute(year);
    default: throw std::logic_error { "Unknown test model" };
  }
}

auto distance_au(const std::optional<astro::toolbox::DistanceAu>& distance) -> double {
  if (not distance) {
    throw std::logic_error { "Missing test body distance" };
  }
  return distance->au();
}

auto expected_coordinates(const Target target, const double jde_tt) -> astro::toolbox::SphericalCoordinate {
  using astro::planet::Planet;
  switch (target) {
    case Target::SUN: return astro::sun::geocentric_coord::apparent(jde_tt);
    case Target::MOON: return astro::moon::geocentric_coord::apparent(jde_tt);
    case Target::MERCURY: return astro::planet::geocentric_coord::apparent(Planet::MERCURY, jde_tt);
    case Target::VENUS: return astro::planet::geocentric_coord::apparent(Planet::VENUS, jde_tt);
    case Target::MARS: return astro::planet::geocentric_coord::apparent(Planet::MARS, jde_tt);
    case Target::JUPITER: return astro::planet::geocentric_coord::apparent(Planet::JUPITER, jde_tt);
    case Target::SATURN: return astro::planet::geocentric_coord::apparent(Planet::SATURN, jde_tt);
    case Target::URANUS: return astro::planet::geocentric_coord::apparent(Planet::URANUS, jde_tt);
    case Target::NEPTUNE: return astro::planet::geocentric_coord::apparent(Planet::NEPTUNE, jde_tt);
    case Target::PLUTO: return astro::planet::geocentric_coord::apparent(Planet::PLUTO, jde_tt);
    default: throw std::logic_error { "Nodes have no spherical distance" };
  }
}

auto expect_same(const BodyState& before, const BodyState& after) -> void {
  EXPECT_EQ(before.target, after.target);
  EXPECT_EQ(before.longitude.deg(), after.longitude.deg());
  EXPECT_EQ(before.latitude.deg(), after.latitude.deg());
  ASSERT_EQ(before.distance.has_value(), after.distance.has_value());
  if (before.distance) {
    EXPECT_EQ(distance_au(before.distance), distance_au(after.distance));
  }
  EXPECT_EQ(std::bit_cast<uint64_t>(before.longitude_rate.deg_per_tt_day),
            std::bit_cast<uint64_t>(after.longitude_rate.deg_per_tt_day));
}

auto check_composition(const Snapshot& result, const astro::GeoLocation& observer, const System system) -> void {
  for (std::size_t index = 0; index < result.bodies.size(); ++index) {
    const auto& body = result.bodies.at(index);
    const auto target = TARGETS.at(index);
    EXPECT_EQ(body.target, target);
    EXPECT_EQ(std::bit_cast<uint64_t>(body.longitude_rate.deg_per_tt_day),
              std::bit_cast<uint64_t>(astro::ephemeris::longitude_rate(target, result.times.jde_tt).deg_per_tt_day));
    if (index < 10) {
      const auto expected = expected_coordinates(target, result.times.jde_tt);
      EXPECT_EQ(body.longitude.deg(), expected.λ.deg());
      EXPECT_EQ(body.latitude.deg(), expected.β.deg());
      ASSERT_TRUE(body.distance.has_value());
      EXPECT_EQ(distance_au(body.distance), expected.r.au());
    } else {
      using astro::lunar_node::Node;
      constexpr std::array nodes { Node::MEAN_ASCENDING, Node::MEAN_DESCENDING, Node::TRUE_ASCENDING, Node::TRUE_DESCENDING };
      EXPECT_EQ(body.longitude.deg(), astro::lunar_node::position(nodes.at(index - 10), result.times.jde_tt).deg());
      EXPECT_EQ(body.latitude.deg(), 0.0);
      EXPECT_FALSE(body.distance.has_value());
    }
    EXPECT_GE(body.longitude.deg(), 0.0);
    EXPECT_LT(body.longitude.deg(), 360.0);
  }
  const auto expected = astro::house::calculate(result.times.jd_ut1, result.times.jde_tt, observer, system);
  EXPECT_EQ(result.houses.ascendant.deg(), expected.ascendant.deg());
  EXPECT_EQ(result.houses.midheaven.deg(), expected.midheaven.deg());
  EXPECT_EQ(result.houses.descendant.deg(), expected.descendant.deg());
  EXPECT_EQ(result.houses.imum_coeli.deg(), expected.imum_coeli.deg());
  for (std::size_t index = 0; index < expected.cusps.size(); ++index) {
    EXPECT_EQ(result.houses.cusps.at(index).deg(), expected.cusps.at(index).deg());
  }
}

} // namespace

TEST(Chart, ModelSelectorsPreserveRetainedAlgorithms) {
  for (const auto model : MODELS) {
    for (const double year : std::array { 1885.0, 1972.0, 2004.9999999999, 2005.0, 2026.125, 2034.5 }) {
      EXPECT_EQ(astro::delta_t::compute(model, year), independent_delta_t(model, year));
    }
    for (const double fraction : std::array { 0.0, 0.123456789, 0.5, 0.999999 }) {
      const Datetime dt { util::to_ymd(2026, 1, 1), fraction };
      const double year = 2026.0 + (dt.fraction() / 365.0);
      EXPECT_EQ(astro::delta_t::compute(model, dt), independent_delta_t(model, year));
      if (model == Model::ALGO5) {
        EXPECT_EQ(astro::delta_t::compute(model, dt), astro::delta_t::compute(dt));
      }
    }
  }
  const double nan = std::numeric_limits<double>::quiet_NaN();
  EXPECT_TRUE(std::isnan(astro::delta_t::compute(Model::ALGO2, nan)));
  EXPECT_TRUE(std::isnan(astro::delta_t::compute(Model::ALGO5, nan)));
  EXPECT_THROW(std::ignore = astro::delta_t::compute(Model::ALGO4, 2035.0), std::out_of_range);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Exercises the invalid-enumerator contract.
  const auto unknown_model = static_cast<Model>(255);
  EXPECT_THROW(std::ignore = astro::delta_t::compute(unknown_model, 2026.0), std::invalid_argument);
}

TEST(Chart, DateModelSelectionHandlesChronoMaximumYear) {
  const Datetime dt { util::to_ymd(32767, 12, 31), 0.5 };
  const double year = 32767.0 + ((364.0 + dt.fraction()) / 365.0);
  EXPECT_EQ(astro::delta_t::compute(Model::ALGO5, dt), astro::delta_t::algo5::compute(year));
  auto invalid = Datetime { util::to_ymd(2026, 1, 1), 0.0 };
  invalid.ymd = util::to_ymd(2026, 2, 30);
  EXPECT_THROW(std::ignore = astro::delta_t::compute(Model::ALGO5, invalid), std::invalid_argument);
}

TEST(Chart, SameInstantCompositionForEveryModelAndHouseSystem) {
  for (const auto model : MODELS) {
    for (const auto system : std::array { System::EQUAL, System::WHOLE_SIGN, System::PLACIDUS }) {
      for (const auto observer : std::array { location(51.5, 0.0), location(-33.87, 151.21) }) {
        const Datetime dt { util::to_ymd(2026, 1, 1), 0.5123456789 };
        const auto result = calculate(dt, Scale::UTC, observer, system, model);
        const auto tt = astro::leap_second::utc_to_tt(dt);
        const auto ut1 = calendar::add_seconds(tt, -astro::delta_t::compute(model, tt));
        EXPECT_EQ(result.times.jde_tt, astro::julian_day::tt_to_jde(tt));
        EXPECT_EQ(result.times.jd_ut1, astro::julian_day::ut1_to_jd(ut1));
        check_composition(result, observer, system);
      }
    }
  }
}

TEST(Chart, HistoricalUt1AndDefaultModel) {
  const auto observer = location(40.0, -75.0);
  const Datetime dt { util::to_ymd(1900, 6, 1), 0.5 };
  for (const auto model : MODELS) {
    const auto result = calculate(dt, Scale::UT1, observer, System::EQUAL, model);
    EXPECT_EQ(result.times.jd_ut1, astro::julian_day::ut1_to_jd(dt));
    EXPECT_EQ(result.times.jde_tt, astro::julian_day::tt_to_jde(
      calendar::add_seconds(dt, astro::delta_t::compute(model, dt))));
    check_composition(result, observer, System::EQUAL);
  }
  EXPECT_THROW(std::ignore = calculate(dt, Scale::UTC, observer, System::EQUAL), std::invalid_argument);
  const Datetime modern { util::to_ymd(2026, 1, 1), 0.5 };
  const auto implicit = calculate(modern, Scale::UTC, observer, System::EQUAL);
  const auto explicit_model = calculate(modern, Scale::UTC, observer, System::EQUAL, Model::ALGO5);
  EXPECT_EQ(implicit.times.jde_tt, explicit_model.times.jde_tt);
  EXPECT_EQ(implicit.times.jd_ut1, explicit_model.times.jd_ut1);
}

TEST(Chart, EveryLeapTableStepAndFrozenFuture) {
  const auto observer = location(0.0, 0.0);
  for (const auto& entry : astro::leap_second::LEAP_SECOND_TABLE) {
    const Datetime after { entry.start_utc, 0.0 };
    const auto before = calendar::add_seconds(after, -1.0);
    const auto after_result = calculate(after, Scale::UTC, observer, System::EQUAL);
    EXPECT_EQ(after_result.times.jde_tt, astro::julian_day::tt_to_jde(
      calendar::add_seconds(after, entry.tai_minus_utc + astro::leap_second::TT_MINUS_TAI_SEC)));
    if (entry.start_utc == astro::leap_second::MODERN_UTC_START) {
      EXPECT_THROW(std::ignore = calculate(before, Scale::UTC, observer, System::EQUAL), std::invalid_argument);
      EXPECT_TRUE(std::isfinite(calculate(before, Scale::UT1, observer, System::EQUAL).times.jde_tt));
    } else {
      const auto before_result = calculate(before, Scale::UTC, observer, System::EQUAL);
      EXPECT_EQ(before_result.times.jde_tt, astro::julian_day::utc_to_jde(before));
      // One civil second plus the one-second table step; tolerance is one JDE subtraction ulp in seconds.
      EXPECT_NEAR((after_result.times.jde_tt - before_result.times.jde_tt) * 86400.0, 2.0, 5e-5);
    }
  }
  const Datetime future { util::to_ymd(2099, 7, 1), 0.5 };
  const auto result = calculate(future, Scale::UTC, observer, System::WHOLE_SIGN);
  EXPECT_EQ(result.times.jde_tt, astro::julian_day::tt_to_jde(calendar::add_seconds(future, 69.184)));
}

TEST(Chart, LocationAffectsOnlyHouseGeometry) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5 };
  const auto north = calculate(dt, Scale::UTC, location(51.5, 0.0), System::PLACIDUS);
  const auto south = calculate(dt, Scale::UTC, location(-33.87, 151.21), System::PLACIDUS);
  EXPECT_EQ(north.times.jd_ut1, south.times.jd_ut1);
  EXPECT_EQ(north.times.jde_tt, south.times.jde_tt);
  for (std::size_t index = 0; index < north.bodies.size(); ++index) {
    expect_same(north.bodies.at(index), south.bodies.at(index));
  }
  EXPECT_NE(north.houses.ascendant.deg(), south.houses.ascendant.deg());
}

TEST(Chart, CivilAndEnumValidationPrecedesLegacyAssertions) {
  const auto observer = location(0.0, 0.0);
  auto dt = Datetime { util::to_ymd(2026, 1, 1), 0.0 };
  dt.ymd = util::to_ymd(2026, 2, 30);
  EXPECT_THROW(std::ignore = calculate(dt, Scale::UTC, observer, System::EQUAL), std::invalid_argument);
  dt.ymd = util::to_ymd(2026, 1, 1);
  dt.time_of_day = std::chrono::hh_mm_ss { std::chrono::nanoseconds { -1 } };
  EXPECT_THROW(std::ignore = calculate(dt, Scale::UTC, observer, System::EQUAL), std::invalid_argument);
  dt.time_of_day = std::chrono::hh_mm_ss { std::chrono::nanoseconds { 86400'000'000'000 } };
  EXPECT_THROW(std::ignore = calculate(dt, Scale::UTC, observer, System::EQUAL), std::invalid_argument);
  const auto valid = Datetime { util::to_ymd(2026, 1, 1), 0.5 };
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Exercises the invalid-enumerator contract.
  const auto unknown_scale = static_cast<Scale>(255);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Exercises the invalid-enumerator contract.
  const auto unknown_system = static_cast<System>(255);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Exercises the invalid-enumerator contract.
  const auto unknown_model = static_cast<Model>(255);
  EXPECT_THROW(std::ignore = calculate(valid, unknown_scale, observer, System::EQUAL), std::invalid_argument);
  EXPECT_THROW(std::ignore = calculate(valid, Scale::UTC, observer, unknown_system), std::invalid_argument);
  EXPECT_THROW(std::ignore = calculate(valid, Scale::UTC, observer, System::EQUAL, unknown_model), std::invalid_argument);
  for (const int32_t year : std::array { -32767, 1, 1884, 2100, 32767 }) {
    EXPECT_THROW(std::ignore = calculate(Datetime { util::to_ymd(year, 1, 1), 0.5 }, Scale::UT1, observer,
                                       System::EQUAL), std::invalid_argument);
  }
}

TEST(Chart, SiteAndPlacidusFailuresNeverFallback) {
  const auto dt = Datetime { util::to_ymd(2026, 1, 1), 0.5 };
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (const auto bad : std::array { location(nan, 0.0), location(0.0, nan), location(0.0, 181.0),
                                    location(-90.0, 0.0), location(90.0, 0.0) }) {
    for (const auto system : std::array { System::EQUAL, System::WHOLE_SIGN, System::PLACIDUS }) {
      EXPECT_THROW(std::ignore = calculate(dt, Scale::UTC, bad, system), std::invalid_argument);
    }
  }
  for (const double latitude : std::array { -70.0, 70.0 }) {
    EXPECT_THROW(std::ignore = calculate(dt, Scale::UTC, location(latitude, 0.0), System::PLACIDUS), std::invalid_argument);
    EXPECT_TRUE(std::isfinite(calculate(dt, Scale::UTC, location(latitude, 0.0), System::EQUAL).houses.ascendant.deg()));
    EXPECT_TRUE(std::isfinite(calculate(dt, Scale::UTC, location(latitude, 0.0), System::WHOLE_SIGN).houses.ascendant.deg()));
  }
}

TEST(Chart, JointTimeDomainAndAlgo4Cap) {
  const auto observer = location(0.0, 0.0);
  for (const double edge : std::array { detail::START_JDE_TT, detail::END_JDE_TT }) {
    const auto tt = astro::julian_day::jde_to_tt(edge);
    const auto ut1 = astro::delta_t::tt_to_ut1(tt);
    if (edge == detail::START_JDE_TT) {
      const auto result = calculate(ut1, Scale::UT1, observer, System::EQUAL);
      EXPECT_NEAR(result.times.jde_tt, edge, astro::toolbox::ulp(edge));
    } else {
      EXPECT_THROW(std::ignore = calculate(ut1, Scale::UT1, observer, System::EQUAL), std::invalid_argument);
    }
  }
  const auto before = civil(2034, 12, 31, (23LL*3600'000'000'000) + (58LL*60'000'000'000) + (47LL*1'000'000'000));
  const auto after = civil(2034, 12, 31, (23LL*3600'000'000'000) + (58LL*60'000'000'000) + (49LL*1'000'000'000));
  EXPECT_LT(calculate(before, Scale::UT1, observer, System::EQUAL, Model::ALGO4).times.jde_tt, detail::ALGO4_END_JDE_TT);
  EXPECT_THROW(std::ignore = calculate(after, Scale::UT1, observer, System::EQUAL, Model::ALGO4), std::invalid_argument);
  const Datetime exact_utc_cap { util::to_ymd(2034, 12, 31), 0.99919925925925923 };
  EXPECT_THROW(std::ignore = calculate(exact_utc_cap, Scale::UTC, observer, System::EQUAL, Model::ALGO4), std::invalid_argument);
  const Datetime pinned_utc { util::to_ymd(2034, 12, 31), 0.99919925902642359 };
  EXPECT_EQ(calculate(pinned_utc, Scale::UTC, observer, System::EQUAL, Model::ALGO4).times.jde_tt,
            std::nextafter(detail::ALGO4_END_JDE_TT, 0.0));
  const auto too_late = civil(2034, 12, 31, 86330815999999);
  EXPECT_THROW(std::ignore = calculate(too_late, Scale::UTC, observer, System::EQUAL, Model::ALGO4), std::invalid_argument);
  EXPECT_TRUE(std::isfinite(calculate(after, Scale::UT1, observer, System::EQUAL, Model::ALGO5).times.jde_tt));
  const Datetime outside_model { util::to_ymd(2035, 6, 1), 0.5 };
  EXPECT_THROW(std::ignore = calculate(outside_model, Scale::UT1, observer, System::EQUAL, Model::ALGO4),
               std::invalid_argument);
  EXPECT_TRUE(std::isfinite(calculate(outside_model, Scale::UT1, observer, System::EQUAL, Model::ALGO5).times.jde_tt));
  for (const int64_t elapsed : std::array<int64_t, 3> { 86399999996413, 86399999999990, 86399999999999 }) {
    EXPECT_THROW(std::ignore = calculate(civil(2034, 12, 31, elapsed), Scale::UT1, observer, System::EQUAL,
                                       Model::ALGO4), std::invalid_argument);
  }
}

TEST(Chart, WrapsStationsAndNodeAntipodesReplayUnderlyingApis) {
  const auto observer = location(-33.87, 151.21);
  // Sun/Moon wraps and station-center epochs from ephemeris_test.cpp; facade wiring, not a new oracle.
  for (const double jde_tt : std::array { 2460754.876816418, 2460681.293070456, 2460749.7828680002,
                                        2460772.964472, 2460681.5899983719 }) {
    const auto tt = astro::julian_day::jde_to_tt(jde_tt);
    const auto ut1 = astro::delta_t::tt_to_ut1(tt);
    const auto result = calculate(ut1, Scale::UT1, observer, System::PLACIDUS);
    check_composition(result, observer, System::PLACIDUS);
    EXPECT_EQ(std::bit_cast<uint64_t>(result.bodies.at(10).longitude_rate.deg_per_tt_day),
              std::bit_cast<uint64_t>(result.bodies.at(11).longitude_rate.deg_per_tt_day));
    EXPECT_EQ(std::bit_cast<uint64_t>(result.bodies.at(12).longitude_rate.deg_per_tt_day),
              std::bit_cast<uint64_t>(result.bodies.at(13).longitude_rate.deg_per_tt_day));
  }
}

TEST(Chart, JointDomainAdjacentDoubleLadders) {
  const auto observer = location(0.0, 0.0);
  const double infinity = std::numeric_limits<double>::infinity();
  for (const double edge : std::array { detail::START_JDE_TT, detail::END_JDE_TT }) {
    for (const double jde_tt : std::array {
      std::nextafter(edge, -infinity), edge, std::nextafter(edge, infinity)
    }) {
      const auto tt = astro::julian_day::jde_to_tt(jde_tt);
      const auto ut1 = astro::delta_t::tt_to_ut1(tt);
      const double recomposed = astro::julian_day::ut1_to_jde(ut1);
      const bool admitted = recomposed >= detail::START_JDE_TT and recomposed < detail::END_JDE_TT;
      if (admitted) {
        EXPECT_EQ(calculate(ut1, Scale::UT1, observer, System::EQUAL).times.jde_tt, recomposed);
      } else {
        EXPECT_THROW(std::ignore = calculate(ut1, Scale::UT1, observer, System::EQUAL), std::invalid_argument);
      }
      if (edge == detail::END_JDE_TT) {
        const auto utc = astro::leap_second::tt_to_utc(tt);
        const double utc_recomposed = astro::julian_day::utc_to_jde(utc);
        if (utc_recomposed < detail::END_JDE_TT) {
          EXPECT_EQ(calculate(utc, Scale::UTC, observer, System::EQUAL).times.jde_tt, utc_recomposed);
        } else {
          EXPECT_THROW(std::ignore = calculate(utc, Scale::UTC, observer, System::EQUAL), std::invalid_argument);
        }
      }
    }
  }
}

TEST(Chart, RetainedModelSeamsKeepTheirSelectedEvaluationPolicy) {
  const auto observer = location(51.5, 0.0);
  for (const auto model : MODELS) {
    for (const auto dt : std::array {
      civil(2004, 12, 31, 86399'999'999'999), civil(2005, 1, 1, 0),
      civil(2004, 12, 31, 86340'000'000'000),
      civil(2026, 5, 31, 43199'999'999'999), civil(2026, 5, 31, 43200'000'000'000)
    }) {
      const auto ut1_result = calculate(dt, Scale::UT1, observer, System::EQUAL, model);
      EXPECT_EQ(ut1_result.times.jd_ut1, astro::julian_day::ut1_to_jd(dt));
      const auto past_days = (std::chrono::sys_days { dt.ymd }
                              - std::chrono::sys_days { util::to_ymd(dt.year(), 1, 1) }).count();
      const double year_days = dt.ymd.year().is_leap() ? 366.0 : 365.0;
      const double year = dt.year() + ((dt.fraction() + static_cast<double>(past_days)) / year_days);
      const auto expected_tt = calendar::add_seconds(dt, independent_delta_t(model, year));
      EXPECT_EQ(ut1_result.times.jde_tt, astro::julian_day::tt_to_jde(expected_tt));
      const auto utc_result = calculate(dt, Scale::UTC, observer, System::EQUAL, model);
      const auto tt = astro::leap_second::utc_to_tt(dt);
      EXPECT_EQ(utc_result.times.jd_ut1, astro::julian_day::ut1_to_jd(
        calendar::add_seconds(tt, -astro::delta_t::compute(model, tt))));
    }
  }
}

} // namespace astro::chart::test
