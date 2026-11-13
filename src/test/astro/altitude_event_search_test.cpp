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
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "altitude_event_certificate.hpp"
#include "altitude_event_search.hpp"

namespace astro::observer::detail::altitude_events::test {
namespace {

inline constexpr double MAP_ERROR = 0x1.ff3bd80ce8623p-43;
inline constexpr double RESIDUAL_ERROR = 0x1.1644b85b12e5ap-42;
inline constexpr double SLOPE_LOWER = 0x1.a9ad8af7d556cp-4;
inline constexpr double LEVEL_RESOLUTION = 0x1p-30;

/** @brief Fixed north-pole Sun trajectory with no daily horizon crossing at J2000. */
auto polar_sun() -> Altitude {
  return {
    .target = ephemeris::Target::SUN,
    .observer = {
      .location = { .latitude = toolbox::AngleDeg { 90.0 }, .longitude = toolbox::AngleDeg { 0.0 } },
      .elevation = toolbox::DistanceKm { 0.0 },
    },
    .level_sine = std::sin(toolbox::AngleDeg { -45.0 }.rad()), .residual_error = 0x1p-24,
    .envelope = { .start_jde_tt = 2451545.0, .end_jde_tt = 2451545.25 }, .meridian = false,
  };
}

/** @brief Evaluate Saemundsson's actual source map in apparent degrees. */
auto apparent_map(const double geometric, const refraction::Params& params) -> double {
  return geometric + refraction::detail::apply_tp_correction(
    refraction::saemundsson(toolbox::AngleDeg { geometric }), params
  ).deg();
}

} // namespace

TEST(AltitudeEventSearch, OneObservationRetainsTheTwoRoundedResidualOperands) {
  const auto provider = polar_sun();
  const auto observed = provider.observe(2451545.0);
  const volatile double product = provider.level_sine * observed.q;
  const double residual = observed.up - product;
  EXPECT_EQ(std::bit_cast<uint64_t>(observed.product), std::bit_cast<uint64_t>(static_cast<double>(product)));
  EXPECT_EQ(std::bit_cast<uint64_t>(observed.residual), std::bit_cast<uint64_t>(residual));
  EXPECT_DOUBLE_EQ(observed.jde_tt, 2451545.0);
  EXPECT_DOUBLE_EQ(provider(2451545.0).value, observed.residual);
  auto meridian = provider;
  meridian.meridian = true;
  EXPECT_DOUBLE_EQ(meridian(2451545.0).value, observed.meridian);
}

TEST(AltitudeEventSearch, EnvelopeFailurePrecedesAnyOutOfEnvelopeProviderCall) {
  auto provider = polar_sun();
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) -- Forged identity distinguishes the envelope guard from provider admission.
  provider.target = static_cast<ephemeris::Target>(255);
  EXPECT_THROW((void)provider.observe(2451544.0), std::runtime_error);
  EXPECT_THROW((void)provider.observe(2451545.5), std::runtime_error);
  EXPECT_THROW((void)provider.observe(2451545.0), std::invalid_argument);
}

TEST(AltitudeEventSearch, CompleteRealProviderCensusCanCertifyNoRootsButCannotHideUncertainty) {
  namespace bounds = astro::detail::altitude_bounds;
  auto provider = polar_sun();
  const bounds::ClockPiece piece {
    .start_jde_tt = provider.envelope.start_jde_tt, .end_jde_tt = provider.envelope.end_jde_tt,
    .collar = false, .jump = 0.0,
  };
  const auto input = bounds::observer_for_piece(0, 90.0, 0.0, piece);
  provider.residual_error = input.value_error;
  const auto composed = bounds::observer_c2::compose(input.source, input.site, 1.0);
  const search::Bounds certificate {
    .first_derivative = std::max(composed.first, composed.local_vector.at(1)),
    .second_derivative = std::max(composed.second, composed.local_vector.at(2)),
  };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_TRUE(events(provider, provider.envelope, certificate, budget, 0x1p-20).empty());
  EXPECT_LT(budget.evaluations_remaining, 8192U);
  provider.residual_error = 2.0;
  budget = { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)events(provider, provider.envelope, certificate, budget, 0x1p-20), std::runtime_error);
}

TEST(AltitudeEventSearch, ZeroEvaluationBudgetDoesNotEvaluateTheProvider) {
  auto provider = polar_sun();
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) -- The exhausted budget must precede malformed-provider admission.
  provider.target = static_cast<ephemeris::Target>(255);
  search::Budget budget { .evaluations_remaining = 0, .candidates_remaining = 1 };
  EXPECT_THROW((void)search::sample(provider, 2451545.0, budget), std::runtime_error);
  EXPECT_EQ(budget.evaluations_remaining, 0U);
}

TEST(AltitudeEventSearch, ClockCollarRequiresExclusionAcrossItsCompleteJump) {
  const search::Sample left { .jde_tt = 2451545.0, .evaluated = { .value = 1.0, .error = 0.0 } };
  const search::Sample right { .jde_tt = 2451545.25, .evaluated = { .value = 1.0, .error = 0.0 } };
  EXPECT_EQ(certify_clock_collar(left, right, 1.0, 0.0), 1);
  EXPECT_THROW((void)certify_clock_collar(left, right, 1.0, 2.0), std::runtime_error);
  auto opposite = right;
  opposite.evaluated.value = -1.0;
  EXPECT_THROW((void)certify_clock_collar(left, opposite, 0.0, 2.0), std::runtime_error);
}

TEST(AltitudeEventSearch, ClockCollarRejectsContradictoryOrMalformedEvidence) {
  const search::Sample left { .jde_tt = 2451545.0, .evaluated = { .value = 1.0, .error = 0.0 } };
  const search::Sample right { .jde_tt = 2451545.25, .evaluated = { .value = 2.0, .error = 0.0 } };
  EXPECT_THROW((void)certify_clock_collar(left, right, 0.0, 0.0), std::runtime_error);
  // NOLINTNEXTLINE(readability-suspicious-call-argument) -- Reversed time order is the malformed input under test.
  EXPECT_THROW((void)certify_clock_collar(right, left, 1.0, 0.0), std::runtime_error);
  EXPECT_THROW((void)certify_clock_collar(left, right, -1.0, 0.0), std::runtime_error);
}

TEST(AltitudeEventSearch, BennettInverseChargesOneActualMapEvaluation) {
  const refraction::Params params { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = refraction::Model::BENNETT };
  const auto level = inverse(10.0, params, MAP_ERROR, RESIDUAL_ERROR, 1.0, LEVEL_RESOLUTION);
  const double expected = 10.0 - refraction::detail::apply_tp_correction(
    refraction::bennett(toolbox::AngleDeg { 10.0 }), params
  ).deg();
  EXPECT_DOUBLE_EQ(level.geometric_degrees, expected);
  EXPECT_DOUBLE_EQ(level.error_degrees, MAP_ERROR);
  EXPECT_EQ(level.evaluation_charge, 1U);
}

TEST(AltitudeEventSearch, SaemundssonInverseRetainsTheResolvedBracketAndWorkCharge) {
  const refraction::Params params { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = refraction::Model::SAEMUNDSSON };
  const auto level = inverse(10.0, params, MAP_ERROR, RESIDUAL_ERROR, SLOPE_LOWER, LEVEL_RESOLUTION);
  EXPECT_LE(level.error_degrees, LEVEL_RESOLUTION);
  EXPECT_GT(level.evaluation_charge, 1U);
  EXPECT_LE(level.evaluation_charge, 87U);
  EXPECT_LT(apparent_map(level.geometric_degrees - level.error_degrees, params), 10.0 + MAP_ERROR);
  EXPECT_GT(apparent_map(level.geometric_degrees + level.error_degrees, params), 10.0 - MAP_ERROR);
}

TEST(AltitudeEventSearch, LegalWeatherWithoutACertificateIsNumericalWhileMalformedWeatherIsInvalid) {
  refraction::Params params { .temperature_c = 100.0, .pressure_hpa = 1013.25, .model = refraction::Model::BENNETT };
  EXPECT_THROW((void)inverse(10.0, params, MAP_ERROR, RESIDUAL_ERROR, 1.0, LEVEL_RESOLUTION), std::runtime_error);
  params.temperature_c = 15.0;
  params.pressure_hpa = 0.0;
  EXPECT_THROW((void)inverse(10.0, params, MAP_ERROR, RESIDUAL_ERROR, 1.0, LEVEL_RESOLUTION), std::invalid_argument);
}

TEST(AltitudeEventSearch, ApparentImageEndpointsAndUnavailableResolutionDoNotBecomeSuccess) {
  refraction::Params params { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = refraction::Model::SAEMUNDSSON };
  const double left = -refraction::at_horizon(params).deg();
  const double endpoint = apparent_map(left, params);
  EXPECT_THROW((void)inverse(endpoint, params, MAP_ERROR, RESIDUAL_ERROR, SLOPE_LOWER, LEVEL_RESOLUTION), std::runtime_error);
  EXPECT_THROW((void)inverse(-10.0, params, MAP_ERROR, RESIDUAL_ERROR, SLOPE_LOWER, LEVEL_RESOLUTION), std::invalid_argument);
  params.model = refraction::Model::BENNETT;
  EXPECT_THROW((void)inverse(100.0, params, MAP_ERROR, RESIDUAL_ERROR, 1.0, LEVEL_RESOLUTION), std::invalid_argument);
  EXPECT_THROW((void)inverse(10.0, params, 1.0, RESIDUAL_ERROR, 1.0, LEVEL_RESOLUTION), std::runtime_error);
}

} // namespace astro::observer::detail::altitude_events::test
