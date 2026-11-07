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
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>

#include <gtest/gtest.h>

#include "observer.hpp"
#include "observer_reference_data.hpp"
#include "coord_transform.hpp"
#include "../test_helper.hpp"

namespace astro::observer::test {
namespace {

using toolbox::AngleDeg;
using toolbox::DistanceAu;
using toolbox::DistanceKm;
using ephemeris::Target;
using util::test::req;

/** @brief Construct a site from test inputs expressed in degrees and kilometres. */
auto site(const double latitude, const double longitude = 0.0, const double height_km = 0.0) -> Observer {
  return { .location = { .latitude = AngleDeg { latitude }, .longitude = AngleDeg { longitude } },
           .elevation = DistanceKm { height_km } };
}

/** @brief Physical geocentric direction for same-input reduction tests. */
auto body(const double ra, const double dec, const double distance) -> ephemeris::EquatorialPosition {
  return { .right_ascension = AngleDeg { ra }, .declination = AngleDeg { dec }, .distance = DistanceAu { distance } };
}

/** @brief Shortest signed difference across the RA/azimuth wrap. */
auto difference(const double actual, const double expected) -> double {
  return std::remainder(actual - expected, 360.0);
}

} // namespace

TEST(Observer, Wgs84SofaVectors) {
  // V20 SOFA 2023-10-11 iauGd2gce, 2026-11-07, seed 42; 5 nm exceeds the measured 1-2 nm form gap.
  for (const auto& row : reference::SOFA) {
    const auto observer = site(row[1], row[0], row[2] / 1000.0);
    detail::validate(observer);
    const auto vector = detail::geocentric_site(observer);
    const double radius_m = WGS84_EQUATORIAL_RADIUS_KM * 1000.0;
    EXPECT_NEAR(vector.equatorial * std::cos(observer.location.longitude.rad()) * radius_m, row[3], 5e-9);
    EXPECT_NEAR(vector.equatorial * std::sin(observer.location.longitude.rad()) * radius_m, row[4], 5e-9);
    EXPECT_NEAR(vector.polar * radius_m, row[5], 5e-9);
  }
}

TEST(Observer, ControlledPymeeusReduction) {
  // V15 PyMeeus ordinary branch, 2026-11-07, seed 42. Actual rescaled oracle AU is column 6.
  // A 1e-12 degree per-column tolerance covers the measured 2.1e-10 arcsec separation/form gap.
  for (const auto& row : reference::PYMEEUS) {
    const auto result = detail::reduce(body(row[0], row[1], row[2]), AngleDeg { row[0] + row[5] },
                                       site(row[3], 0.0, row[4] / 1000.0));
    EXPECT_NEAR(difference(req(result.right_ascension).deg(), row[7]), 0.0, 1e-12);
    EXPECT_NEAR(result.declination.deg(), row[8], 1e-12);
    const auto horizontal = coords::equatorial_to_horizontal(
      AngleDeg { row[0] + row[5] } - req(result.right_ascension), result.declination, AngleDeg { row[3] }
    );
    EXPECT_NEAR(result.altitude.deg(), horizontal.h.deg(), 1e-12);
    EXPECT_NEAR(difference(req(result.azimuth).deg(), horizontal.A.deg() + 180.0), 0.0, 1e-12);
    EXPECT_NEAR(row[6] / row[2], std::sin(AngleDeg::from_arcsec(8.794).rad())
                / toolbox::km_to_au(WGS84_EQUATORIAL_RADIUS_KM), 3e-16);
  }
}

TEST(Observer, HorizonsReductionAndFullProviderLayers) {
  // V06 Horizons AIRLESS Q2/4/20/49, 2026-11-07; explicit sites/epochs, no random selection.
  // Bounds are 3x the independent pre-implementation calibration gaps, rounded outward per target/column.
  for (const auto& row : reference::HORIZONS) {
    const auto target = static_cast<Target>(static_cast<uint8_t>(row[0]));
    const auto observer = site(row[2], row[1], row[3]);
    const julian_day::Times times { .jd_ut1 = row[4], .jde_tt = row[5] };
    const auto reduced = detail::reduce(body(row[7], row[8], row[9]), AngleDeg { row[6] + row[1] }, observer);
    const auto provided = observe(times, observer, target);
    const auto index = static_cast<std::size_t>(row[0]);
    for (const bool provider : { false, true }) {
      const auto& result = provider ? provided : reduced;
      const auto& tolerance = provider ? std::span { reference::PROVIDER_TOL }[index]
                                       : std::span { reference::REDUCTION_TOL }[index];
      EXPECT_NEAR(difference(req(result.right_ascension).deg(), row[10]), 0.0, tolerance[0]);
      EXPECT_NEAR(result.declination.deg(), row[11], tolerance[1]);
      EXPECT_NEAR(result.distance.au(), row[12], tolerance[2]);
      EXPECT_NEAR(difference(req(result.azimuth).deg(), row[13]), 0.0, tolerance[3]);
      EXPECT_NEAR(result.altitude.deg(), row[14], tolerance[4]);
    }
  }
}

TEST(Observer, MeeusPrintedConditionsAndWgs84Distinction) {
  // Retained material boundary: V32 Meeus Examples 11.a/40.a numeric inputs/results remain under
  // the book's source terms, outside the project MIT grant. No book prose is reproduced.
  // Example 11.a uses IAU1976 (6378.14 km, 1/298.257), not WGS84; its rho values are rounded.
  const auto palomar = site(33.0 + (21.0 / 60.0) + (22.0 / 3600.0), 0.0, 1.706);
  const auto vector = detail::geocentric_site(palomar);
  EXPECT_NEAR(vector.equatorial, 0.836339, 5e-7);
  EXPECT_NEAR(vector.polar, 0.546861, 5e-7);
  const double α = 339.530208;
  const double δ = toolbox::deg_to_rad(-15.771083);
  const double H = toolbox::deg_to_rad(288.7958);
  const double sin_π = std::sin(AngleDeg::from_arcsec(23.592).rad());
  const double A = std::cos(δ) * std::sin(H);
  const double B = (std::cos(δ) * std::cos(H)) - (0.836339 * sin_π);
  const double C = std::sin(δ) - (0.546861 * sin_π);
  const double ra = toolbox::normalize_deg(α + 288.7958 - toolbox::rad_to_deg(std::atan2(A, B)));
  // Printed RA is 0.01 second of time, Dec 0.1 arcsec; intermediate H/rho are rounded too.
  EXPECT_NEAR(ra, (22.0 + (38.0 / 60.0) + (8.54 / 3600.0)) * 15.0, 0.08 / 3600.0);
  EXPECT_NEAR(toolbox::rad_to_deg(std::atan2(C, std::hypot(A, B))), -15.0 - (46.0 / 60.0) - (30.0 / 3600.0),
              0.1 / 3600.0);
  const auto production = detail::reduce(body(α, -15.771083, 0.37276), AngleDeg { α + 288.7958 }, palomar);
  EXPECT_NEAR(difference(req(production.right_ascension).deg(), ra), 0.0, 0.001 / 3600.0);
}

TEST(Observer, ExactGeographicPolesAndNextafterNeighbours) {
  for (const double pole : { -90.0, 90.0 }) {
    const auto exact = detail::reduce(body(120.0, 20.0, 0.00257), AngleDeg { 32.0 }, site(pole, 45.0));
    EXPECT_FALSE(exact.azimuth.has_value());
    EXPECT_TRUE(exact.right_ascension.has_value());
    EXPECT_TRUE(std::isfinite(exact.altitude.deg()));
    const double inside = std::nextafter(pole, 0.0);
    EXPECT_TRUE(detail::reduce(body(120.0, 20.0, 0.00257), AngleDeg { 32.0 }, site(inside, 45.0)).azimuth.has_value());
    EXPECT_THROW(detail::validate(site(std::nextafter(pole, pole * 2.0))), std::invalid_argument);
  }
}

TEST(Observer, ZenithNadirCelestialPoleAndPolarBranch) {
  for (const double distance : { 0.00257, 40.0 }) {
    const auto zenith = detail::reduce(body(0.0, 0.0, distance), AngleDeg { 0.0 }, site(0.0));
    EXPECT_FALSE(zenith.azimuth.has_value());
    EXPECT_DOUBLE_EQ(zenith.altitude.deg(), 90.0);
    const auto nadir = detail::reduce(body(180.0, 0.0, distance), AngleDeg { 0.0 }, site(0.0));
    EXPECT_FALSE(nadir.azimuth.has_value());
    EXPECT_DOUBLE_EQ(nadir.altitude.deg(), -90.0);
  }
  const double radius_au = toolbox::km_to_au(WGS84_EQUATORIAL_RADIUS_KM);
  const double distance = 0.00257;
  const double dec = toolbox::rad_to_deg(std::acos(radius_au / distance));
  const auto pole = detail::reduce(body(0.0, dec, distance), AngleDeg { 0.0 }, site(0.0));
  EXPECT_FALSE(pole.right_ascension.has_value());
  EXPECT_NEAR(pole.declination.deg(), 90.0, 1e-12);
  EXPECT_TRUE(pole.azimuth.has_value());
  // PyMeeus's ordinary branch returns Dec < -90 here; vector geometry must retain the northern solution.
  const auto control = detail::reduce(body(0.0, 89.9, 0.0023), AngleDeg { 0.0 }, site(0.0));
  EXPECT_DOUBLE_EQ(req(control.right_ascension).deg(), 180.0);
  EXPECT_GT(control.declination.deg(), 89.0);
  EXPECT_LT(control.declination.deg(), 90.0);
  const auto horizon = detail::reduce(body(90.0, 0.0, distance), AngleDeg { 0.0 }, site(0.0));
  EXPECT_NEAR(horizon.altitude.deg(), -toolbox::rad_to_deg(std::atan2(radius_au, distance)), 1e-12);
  EXPECT_NEAR(req(horizon.azimuth).deg(), 90.0, 1e-12);
  const double large_radius = toolbox::km_to_au(1e200);
  const auto large_pole = detail::reduce(body(0.0, 45.0, std::hypot(large_radius, large_radius)),
                                       AngleDeg { 0.0 }, site(0.0, 0.0, 1e200));
  EXPECT_FALSE(large_pole.right_ascension.has_value());
  EXPECT_NEAR(large_pole.declination.deg(), 90.0, 1e-12);
}

TEST(Observer, CancellationScaleAndTotalDirectionRejection) {
  // Long-double Cartesian construction on the geodetic vertical, independent of normalized A/B/C.
  for (const double latitude : { -43.0, 33.0, 71.0 }) {
    const auto observer = site(latitude, 0.0, 1e7);
    const long double φ = observer.location.latitude.rad();
    const long double f = 1.0L / 298.257223563L;
    const long double normal = 6378.137L / std::sqrt(1.0L - (f * (2.0L - f) * std::sin(φ) * std::sin(φ)));
    const long double x = (normal + 1e7L + 1.0L) * std::cos(φ);
    const long double z = ((normal * (1.0L - f) * (1.0L - f)) + 1e7L + 1.0L) * std::sin(φ);
    const auto result = detail::reduce(body(0.0, static_cast<double>(std::atan2(z, x) * 180.0L / std::numbers::pi_v<long double>),
      static_cast<double>(std::hypot(x, z) / 149597870.7L)), AngleDeg { 0.0 }, observer);
    EXPECT_FALSE(result.azimuth.has_value());
    EXPECT_NEAR(result.distance.km(), 1.0, 1e-8);
  }
  const double radius = toolbox::km_to_au(WGS84_EQUATORIAL_RADIUS_KM);
  const double eps = std::numeric_limits<double>::epsilon();
  EXPECT_THROW((void)detail::reduce(body(0.0, 0.0, radius / (1.0 - (31.0 * eps))), AngleDeg { 0.0 }, site(0.0)),
               std::runtime_error);
  EXPECT_NO_THROW((void)detail::reduce(body(0.0, 0.0, radius / (1.0 - (33.0 * eps))), AngleDeg { 0.0 }, site(0.0)));
}

TEST(Observer, InputAdmissionAndFiniteArithmetic) {
  const julian_day::Times times { .jd_ut1 = 2451545.0, .jde_tt = 2451545.001 };
  for (const double invalid : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
    EXPECT_THROW((void)observe({ .jd_ut1 = invalid, .jde_tt = times.jde_tt }, site(0.0), Target::SUN), std::invalid_argument);
    EXPECT_THROW((void)observe({ .jd_ut1 = times.jd_ut1, .jde_tt = invalid }, site(0.0), Target::SUN), std::invalid_argument);
    EXPECT_THROW((void)observe(times, site(invalid), Target::SUN), std::invalid_argument);
    EXPECT_THROW((void)observe(times, site(0.0, invalid), Target::SUN), std::invalid_argument);
    EXPECT_THROW((void)observe(times, site(0.0, 0.0, invalid), Target::SUN), std::invalid_argument);
  }
  EXPECT_THROW((void)observe(times, site(0.0), Target::MEAN_ASCENDING), std::invalid_argument);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Unknown enum values must be rejected at the public boundary.
  EXPECT_THROW((void)observe(times, site(0.0), static_cast<Target>(255)), std::invalid_argument);
  EXPECT_THROW((void)observe(times, site(0.0, 181.0), Target::SUN), std::invalid_argument);
  EXPECT_THROW((void)observe(times, site(0.0, 0.0, MINIMUM_ELEVATION_KM), Target::SUN), std::invalid_argument);
  EXPECT_NO_THROW((void)observe(times, site(0.0, 0.0, std::nextafter(MINIMUM_ELEVATION_KM, 0.0)), Target::SUN));
  EXPECT_NO_THROW((void)observe(times, site(43.0, 0.0, 1e200), Target::MOON));
  EXPECT_THROW((void)detail::reduce(body(0.0, 0.0, std::numeric_limits<double>::denorm_min()), AngleDeg { 0.0 }, site(0.0)),
               std::runtime_error);
}

TEST(Observer, CivilTimeModelAndPositionProviderDomains) {
  using julian_day::Scale;
  const auto civil = calendar::Datetime { util::to_ymd(1972, 1, 1), 0.0 };
  const auto result = query(civil, Scale::UTC, site(45.0), Target::SUN);
  const auto resolved = julian_day::resolve_times(civil, Scale::UTC);
  EXPECT_DOUBLE_EQ(result.times.jd_ut1, resolved.jd_ut1);
  EXPECT_DOUBLE_EQ(result.times.jde_tt, resolved.jde_tt);
  EXPECT_EQ(result.delta_t_model, delta_t::DEFAULT_MODEL);
  EXPECT_NO_THROW((void)query(calendar::Datetime { util::to_ymd(1000, 1, 1), 0.0 }, Scale::UT1, site(0.0), Target::SUN));
  EXPECT_THROW((void)query(calendar::Datetime { util::to_ymd(1971, 12, 31), 0.0 }, Scale::UTC, site(0.0), Target::SUN), std::invalid_argument);
  EXPECT_THROW((void)query(calendar::Datetime { util::to_ymd(2036, 1, 1), 0.0 }, Scale::UT1, site(0.0), Target::SUN,
                     std::nullopt, delta_t::Model::ALGO4), std::invalid_argument);
  const auto domain = req(ephemeris::position_domain(Target::PLUTO));
  EXPECT_DOUBLE_EQ(domain[0], 2409543.5);
  EXPECT_DOUBLE_EQ(domain[1], 2488069.5);
  for (const double jde : { domain[0], std::nextafter(domain[1], 0.0) }) {
    EXPECT_NO_THROW((void)observe({ .jd_ut1 = jde, .jde_tt = jde }, site(0.0), Target::PLUTO));
  }
  for (const double jde : { std::nextafter(domain[0], 0.0), domain[1] }) {
    EXPECT_THROW((void)observe({ .jd_ut1 = jde, .jde_tt = jde }, site(0.0), Target::PLUTO), std::invalid_argument);
  }
  for (const double jde : { std::nextafter(domain[0], 0.0), domain[0],
                           std::nextafter(domain[1], 0.0), domain[1] }) {
    const auto civil_ut1 = julian_day::jde_to_ut1(jde);
    const auto mapped = julian_day::resolve_times(civil_ut1, Scale::UT1);
    // Civil round-trip quantization is checked at the actual resolved TT, never a rate-centre bound.
    if (mapped.jde_tt >= domain[0] and mapped.jde_tt < domain[1]) {
      EXPECT_DOUBLE_EQ(query(civil_ut1, Scale::UT1, site(0.0), Target::PLUTO).times.jde_tt, mapped.jde_tt);
    } else {
      EXPECT_THROW((void)query(civil_ut1, Scale::UT1, site(0.0), Target::PLUTO), std::invalid_argument);
    }
  }
  EXPECT_NO_THROW((void)query(calendar::Datetime { util::to_ymd(2099, 12, 31), 0.0 }, Scale::UTC, site(0.0), Target::PLUTO));
  EXPECT_THROW((void)query(calendar::Datetime { util::to_ymd(2100, 1, 1), 0.0 }, Scale::UTC, site(0.0), Target::PLUTO), std::invalid_argument);
}

} // namespace astro::observer::test
