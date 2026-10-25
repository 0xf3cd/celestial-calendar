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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include <gtest/gtest.h>

#include "capability.hpp"

namespace astro::capability::test {

using ephemeris::Target;
using ephemeris::PositionSource;

namespace {

/** @brief Unwrap a declared interval with a guard visible to static analysis. */
auto interval(const Record& record) -> std::array<double, 2> {
  if (not record.domain.has_value()) {
    throw std::logic_error { "Expected a bounded capability record" };
  }
  return *record.domain;
}

} // namespace

TEST(Capability, PositionDomainPerTarget) {
  const auto pluto = domain_record(Operation::POSITION, Target::PLUTO);
  EXPECT_EQ(pluto.status, Status::SOURCE_WINDOW);
  EXPECT_EQ(pluto.source, PositionSource::PLUTO_PLUTO95_APPARENT);
  EXPECT_DOUBLE_EQ(interval(pluto).front(), planet::geocentric_coord::PLUTO_START_JDE_TT);
  EXPECT_DOUBLE_EQ(interval(pluto).back(), planet::geocentric_coord::PLUTO_END_JDE_TT);

  for (const auto target : { Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING }) {
    const auto record = domain_record(Operation::POSITION, target);
    EXPECT_EQ(record.status, Status::IMPLEMENTATION_WINDOW);
    EXPECT_DOUBLE_EQ(interval(record).front(), lunar_node::START_JDE_TT);
    EXPECT_DOUBLE_EQ(interval(record).back(), lunar_node::END_JDE_TT);
  }
  EXPECT_EQ(domain_record(Operation::POSITION, Target::TRUE_ASCENDING).source, PositionSource::NODE_TRUE_MEEUS_FIVE_TERM);
  EXPECT_EQ(domain_record(Operation::POSITION, Target::MEAN_DESCENDING).source, PositionSource::NODE_MEAN);

  const auto sun = domain_record(Operation::POSITION, Target::SUN);
  EXPECT_EQ(sun.status, Status::FINITE_INPUT_EVALUATION);
  EXPECT_EQ(sun.domain, std::nullopt);
  EXPECT_EQ(sun.maximum_span_tt_days, std::nullopt);
}

TEST(Capability, RateAndEventsDomainsTrackTheRateFunction) {
  for (const auto target : ephemeris::TARGETS) {
    const auto expected = ephemeris::longitude_rate_domain(target);
    const auto rate = domain_record(Operation::LONGITUDE_RATE, target);
    EXPECT_EQ(rate.status, Status::IMPLEMENTATION_WINDOW);
    EXPECT_EQ(interval(rate), expected);
    EXPECT_EQ(rate.maximum_span_tt_days, std::nullopt);

    const auto events = domain_record(Operation::EVENTS, target);
    EXPECT_EQ(events.status, Status::IMPLEMENTATION_WINDOW);
    EXPECT_EQ(interval(events), expected);
    EXPECT_EQ(events.maximum_span_tt_days, ephemeris::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS);
  }

  // The century band selects certified error bounds, not the accepted +/-10 Jm rate domain.
  const double century_low = 2451545.0 - 36525.0;
  const double century_high = 2451545.0 + 36525.0;
  for (const auto target : { Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS,
                             Target::JUPITER, Target::SATURN, Target::URANUS, Target::NEPTUNE }) {
    const auto domain = interval(domain_record(Operation::EVENTS, target));
    EXPECT_LT(domain.front(), century_low);
    EXPECT_GT(domain.back(), century_high);
  }
}

TEST(Capability, EquatorialDomainIsTheProviderDomainWithoutARateStencil) {
  for (const auto target : ephemeris::TARGETS) {
    const auto ecliptic = domain_record(Operation::POSITION, target);
    const auto equatorial = domain_record(Operation::EQUATORIAL, target);
    EXPECT_EQ(equatorial.target, target);
    EXPECT_EQ(equatorial.domain, ecliptic.domain);
    EXPECT_EQ(equatorial.status, ecliptic.status);
    EXPECT_EQ(equatorial.source, ecliptic.source);
    EXPECT_EQ(equatorial.maximum_span_tt_days, std::nullopt);
  }
  const auto position = interval(domain_record(Operation::EQUATORIAL, Target::PLUTO));
  const auto rate = interval(domain_record(Operation::LONGITUDE_RATE, Target::PLUTO));
  EXPECT_LT(position.front(), rate.front());
  EXPECT_GT(position.back(), rate.back());
}

TEST(Capability, DomainRecordsEnumerateTheCoreRoster) {
  for (const auto operation : { Operation::POSITION, Operation::LONGITUDE_RATE, Operation::EVENTS, Operation::EQUATORIAL }) {
    const auto records = domain_records(operation);
    ASSERT_EQ(records.size(), ephemeris::TARGETS.size());
    for (std::size_t index = 0; index < records.size(); ++index) {
      EXPECT_EQ(records.at(index).target, ephemeris::TARGETS.at(index));
      EXPECT_EQ(records.at(index).source, ephemeris::source_of(ephemeris::TARGETS.at(index)));
    }
  }
}

TEST(Capability, UnknownTypedInputsAreRejected) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Deliberately forge an unknown operation to test rejection.
  EXPECT_THROW((void)domain_record(static_cast<Operation>(255), Target::SUN), std::invalid_argument);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Deliberately forge an unknown target to test rejection.
  EXPECT_THROW((void)domain_record(Operation::POSITION, static_cast<Target>(255)), std::invalid_argument);
}

} // namespace astro::capability::test
