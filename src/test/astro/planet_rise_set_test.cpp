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

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "planet_rise_set.hpp"
#include "planet_rise_set_reference_data.hpp"
#include "../test_helper.hpp"

namespace astro::rise_set::planet::test {
namespace {

using ephemeris::Target;
using observer::Observer;
using toolbox::AngleDeg;
using toolbox::DistanceKm;
using util::test::req;

/** @brief Construct explicit UT1 test dates without a civil time-scale conversion. */
auto date(const int32_t year = 2000, const uint32_t month = 1, const uint32_t day = 1) -> std::chrono::year_month_day {
  return { std::chrono::year { year }, std::chrono::month { month }, std::chrono::day { day } };
}

/** @brief Fixed WGS84 input in latitude/east-longitude degrees and ellipsoidal kilometres. */
auto site(const double latitude = 40.0, const double longitude = 0.0, const double height = 0.0) -> Observer {
  return { .location = { .latitude = AngleDeg { latitude }, .longitude = AngleDeg { longitude } }, .elevation = DistanceKm { height } };
}

/** @brief Preserve the fixture's rise/transit/set order independently of Result member order. */
auto events(const Result& result) -> std::array<std::optional<double>, 3> {
  return { result.rise_jde_tt, result.transit_jde_tt, result.set_jde_tt };
}

/** @brief Check the specific failure reason rather than accepting an unrelated throw. */
template <typename Exception, typename F>
auto error_message(const F& f) -> std::string {
  try {
    f();
  } catch (const Exception& error) {
    return error.what();
  }
  throw std::logic_error { "Expected exception was not thrown" };
}

/** @brief Admitted equatorial site whose projected displacement cancels Mercury's equatorial vector. */
auto unresolved_ra_site(const double jde_tt) -> Observer {
  const auto position = ephemeris::equatorial_position(Target::MERCURY, jde_tt);
  const double jd_ut1 = rise_set::detail::jde_tt_to_jd_ut1(jde_tt);
  const auto θ = sidereal::local_apparent(jd_ut1, jde_tt, AngleDeg { 0.0 });
  return site(
    0.0,
    toolbox::normalize_pm180(position.right_ascension.deg() - θ.deg()),
    (req(position.distance).km() * std::cos(position.declination.rad())) - observer::WGS84_EQUATORIAL_RADIUS_KM
  );
}

} // namespace

TEST(PlanetRiseSet, StandardCentreConventionAndProviderComposition) {
  EXPECT_DOUBLE_EQ(STANDARD_CENTER_ALTITUDE.deg(), -34.0 / 60.0);
  const auto observer = site(-40.0, 30.0, 1.0);
  constexpr double JDE_TT = 2451545.0;
  for (const auto target : { Target::MERCURY, Target::VENUS, Target::MARS, Target::JUPITER,
                            Target::SATURN, Target::URANUS, Target::NEPTUNE, Target::PLUTO }) {
    const auto provider = detail::provider_for(observer, target);
    static_assert(BodyProvider<decltype(provider)>);
    const auto expected = astro::observer::observe(
      { .jd_ut1 = rise_set::detail::jde_tt_to_jd_ut1(JDE_TT), .jde_tt = JDE_TT }, observer, target
    );
    const auto actual = provider(JDE_TT);
    EXPECT_DOUBLE_EQ(actual.α.deg(), req(expected.right_ascension).deg());
    EXPECT_DOUBLE_EQ(actual.δ.deg(), expected.declination.deg());
    EXPECT_FALSE(expected.apparent_altitude.has_value());
    EXPECT_NEAR(rise_set::detail::altitude(JDE_TT, observer.location, provider).deg(), expected.altitude.deg(), 1e-10);
  }
}

TEST(PlanetRiseSet, ProviderOwnsItsInputs) {
  auto observer = site();
  auto target = Target::MERCURY;
  const auto provider = detail::provider_for(observer, target);
  const auto expected = provider(2451545.0);
  observer = site(-70.0, 120.0, 5.0);
  target = Target::PLUTO;
  EXPECT_EQ(target, Target::PLUTO);
  EXPECT_DOUBLE_EQ(observer.location.latitude.deg(), -70.0);
  const auto actual = provider(2451545.0);
  EXPECT_DOUBLE_EQ(actual.α.deg(), expected.α.deg());
  EXPECT_DOUBLE_EQ(actual.δ.deg(), expected.δ.deg());
  const auto temporary = detail::provider_for(site(), Target::MERCURY);
  EXPECT_DOUBLE_EQ(temporary(2451545.0).α.deg(), expected.α.deg());
}

TEST(PlanetRiseSet, HorizonsBracketsAndIndependentBisection) {
  // Source brackets and per-target allowances were frozen before this adapter existed.
  // See statistics/planet_rise_set_reference.md for sampling, TT/TDB and physical-model qualifications.
  std::size_t checked = 0;
  for (std::size_t index = 0; index < reference::DAYS.size(); ++index) {
    const auto& input = reference::DAYS.at(index);
    const auto ymd = date(
      static_cast<int32_t>(input.at(1)),
      static_cast<uint32_t>(input.at(2)),
      static_cast<uint32_t>(input.at(3))
    );
    const auto observer = site(input.at(4), input.at(5), input.at(6));
    const auto target = static_cast<Target>(input.at(0));
    const auto result = calculate(ymd, observer, target);
    SCOPED_TRACE(index);
    EXPECT_EQ(static_cast<uint32_t>(result.polar), static_cast<uint32_t>(input.at(7)));
    const auto actual = events(result);
    for (std::size_t event = 0; event < actual.size(); ++event) {
      EXPECT_EQ(actual.at(event).has_value(), (static_cast<uint32_t>(input.at(8)) & (uint32_t { 1 } << event)) != 0);
    }
    const auto window = rise_set::detail::day_window(ymd);
    for (const auto& row : reference::EVENTS) {
      if (static_cast<std::size_t>(row.at(0)) != index) { continue; }
      const auto event = static_cast<std::size_t>(row.at(1));
      const double t = req(actual.at(event));
      EXPECT_GE(t, window.start_jde_tt);
      EXPECT_LT(t, window.end_jde_tt);
      EXPECT_GE(t, row.at(2) - (row.at(6) / 86400.0));
      EXPECT_LE(t, row.at(3) + (row.at(6) / 86400.0));
      // The independent bisection used the same physical provider but no rise_set solver.
      EXPECT_GE(t, row.at(4) - (0.0002 / 86400.0));
      EXPECT_LE(t, row.at(5) + (0.0002 / 86400.0));
      const double jd_ut1 = rise_set::detail::jde_tt_to_jd_ut1(t);
      const auto position = astro::observer::observe({ .jd_ut1 = jd_ut1, .jde_tt = t }, observer, target);
      if (event == 1) {
        const auto θ = sidereal::local_apparent(jd_ut1, t, -observer.location.longitude);
        EXPECT_NEAR(std::remainder(θ.deg() - req(position.right_ascension).deg(), 360.0), 0.0, 1e-6);
      } else {
        EXPECT_NEAR(position.altitude.deg(), STANDARD_CENTER_ALTITUDE.deg(), 1e-6);
      }
      ++checked;
    }
  }
  EXPECT_EQ(checked, 32U);
}

TEST(PlanetRiseSet, CustomHeightAndDateLineUseTheSameDayEngine) {
  const auto observer = site(40.0, 180.0, 1.0);
  const auto provider = detail::provider_for(observer, Target::MARS);
  const auto h0 = AngleDeg { -6.0 };
  const auto actual = calculate(date(), observer, Target::MARS, h0);
  const auto expected = calculate_day(date(), observer.location, h0, provider);
  EXPECT_EQ(events(actual), events(expected));
  EXPECT_EQ(actual.polar, expected.polar);
  EXPECT_EQ(events(actual), events(calculate(date(), site(40.0, -180.0, 1.0), Target::MARS, h0)));
}

TEST(PlanetRiseSet, PolarSitesRetainMeaningfulTransitAndNoCrossings) {
  for (const double latitude : { -90.0, 90.0 }) {
    const auto result = calculate(date(), site(latitude), Target::VENUS);
    EXPECT_FALSE(result.rise_jde_tt.has_value());
    EXPECT_FALSE(result.set_jde_tt.has_value());
    EXPECT_TRUE(result.transit_jde_tt.has_value());
    EXPECT_NE(result.polar, Polar::NONE);
  }
}

TEST(PlanetRiseSet, NonPlanetAndUnknownTargetsAreRejected) {
  for (const auto target : { Target::SUN, Target::MOON, Target::MEAN_ASCENDING, Target::MEAN_DESCENDING,
                            Target::TRUE_ASCENDING, Target::TRUE_DESCENDING }) {
    const auto message = error_message<std::invalid_argument>([&] { static_cast<void>(calculate(date(), site(), target)); });
    EXPECT_NE(message.find("requires Mercury through Pluto"), std::string::npos);
  }
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Exercise an unnamed fixed-underlying target.
  const auto unknown = static_cast<Target>(255);
  EXPECT_THROW(static_cast<void>(calculate(date(), site(), unknown)), std::invalid_argument);
}

TEST(PlanetRiseSet, MalformedDatesSitesAndThresholdsAreNotAbsence) {
  EXPECT_THROW(static_cast<void>(calculate(date(2000, 2, 30), site(), Target::MARS)), std::invalid_argument);
  for (const double value : { -91.0, 91.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
    EXPECT_THROW(static_cast<void>(calculate(date(), site(value), Target::MARS)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(calculate(date(), site(), Target::MARS, AngleDeg { value })), std::invalid_argument);
  }
  EXPECT_THROW(static_cast<void>(calculate(date(), site(0.0, 181.0), Target::MARS)), std::invalid_argument);
  for (const double height : { observer::MINIMUM_ELEVATION_KM, -7000.0,
                              std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
    const auto message = error_message<std::invalid_argument>([&] {
      static_cast<void>(calculate(date(), site(40.0, 0.0, height), Target::MARS));
    });
    EXPECT_NE(message.find("UT1 date 2000-01-01"), std::string::npos);
    EXPECT_NE(message.find("Observer elevation"), std::string::npos);
  }
}

TEST(PlanetRiseSet, MissingRAIsReportedByTheActualProviderAndDayQuery) {
  // Observer admission has no upper height limit. Cancel the equatorial projection at the first day sample.
  const double t = rise_set::detail::day_window(date()).start_jde_tt;
  const auto observer = unresolved_ra_site(t);
  const auto position = astro::observer::observe(
    { .jd_ut1 = rise_set::detail::jde_tt_to_jd_ut1(t), .jde_tt = t }, observer, Target::MERCURY
  );
  EXPECT_FALSE(position.right_ascension.has_value());
  EXPECT_TRUE(std::isfinite(position.altitude.deg()));
  EXPECT_GT(position.distance.au(), 0.0);
  const auto provider = detail::provider_for(observer, Target::MERCURY);
  const auto direct = error_message<std::runtime_error>([&] { static_cast<void>(provider(t)); });
  const auto daily = error_message<std::runtime_error>([&] { static_cast<void>(calculate(date(), observer, Target::MERCURY)); });
  EXPECT_NE(direct.find("topocentric right ascension is unavailable"), std::string::npos);
  EXPECT_EQ(daily, direct);
}

TEST(PlanetRiseSet, PlutoUsesThePositionDomainAtEverySample) {
  const auto domain = req(ephemeris::position_domain(Target::PLUTO));
  const auto provider = detail::provider_for(site(), Target::PLUTO);
  EXPECT_NO_THROW(static_cast<void>(provider(domain.front())));
  EXPECT_NO_THROW(static_cast<void>(provider(std::nextafter(domain.back(), domain.front()))));
  EXPECT_THROW(static_cast<void>(provider(std::nextafter(domain.front(), -std::numeric_limits<double>::infinity()))), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(provider(domain.back())), std::invalid_argument);
  EXPECT_NO_THROW(static_cast<void>(calculate(date(1885, 1, 3), site(), Target::PLUTO)));
  EXPECT_NO_THROW(static_cast<void>(calculate(date(2099, 12, 30), site(), Target::PLUTO)));
  for (const auto ymd : { date(1885, 1, 2), date(2099, 12, 31) }) {
    const auto message = error_message<std::invalid_argument>([&] { static_cast<void>(calculate(ymd, site(), Target::PLUTO)); });
    EXPECT_NE(message.find("UT1 date"), std::string::npos);
    EXPECT_NE(message.find("Pluto apparent-position JDE(TT)"), std::string::npos);
    EXPECT_NE(message.find("out of range ["), std::string::npos);
  }
  // A rate window must not become an artificial position/day window for an unbounded position provider.
  EXPECT_FALSE(ephemeris::position_domain(Target::MERCURY).has_value());
  const auto outside_rate_day = date(12001);
  EXPECT_GT(
    rise_set::detail::day_window(outside_rate_day).start_jde_tt,
    ephemeris::target_facts(Target::MERCURY).rate_provider_domain.back()
  );
  // Admission only: this is not an astronomical accuracy check at that distant epoch.
  EXPECT_NO_THROW(static_cast<void>(calculate(outside_rate_day, site(), Target::MERCURY)));
}

TEST(PlanetRiseSet, DaySamplingIncludesDerivativeExcursions) {
  const auto ymd = date(2024, 1, 15);
  const auto observer = site(65.0, 180.0);
  const auto window = rise_set::detail::day_window(ymd);
  const auto provider = detail::provider_for(observer, Target::VENUS);
  std::vector<double> samples;
  const auto recorded = [&](const double t) {
    samples.push_back(t);
    return provider(t);
  };
  const auto result = calculate_day(ymd, observer.location, STANDARD_CENTER_ALTITUDE, recorded);
  EXPECT_EQ(events(result), events(calculate(ymd, observer, Target::VENUS)));
  EXPECT_NE(std::ranges::find(samples, window.start_jde_tt), samples.end());
  EXPECT_NE(std::ranges::find(samples, window.end_jde_tt), samples.end());
  EXPECT_NE(std::ranges::find(samples, window.start_jde_tt + 1.0), samples.end());
  // Newton clamps its iterates, not its derivative probes. Every such evaluation still reaches the provider.
  EXPECT_GT(*std::ranges::max_element(samples), window.end_jde_tt + (20.0 / 86400.0));
  EXPECT_LT(*std::ranges::max_element(samples), window.end_jde_tt + (43.3 / 86400.0));
}

} // namespace astro::rise_set::planet::test
