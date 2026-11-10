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

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

#include "earth/refraction.hpp"
#include "ephemeris.hpp"
#include "geolocation.hpp"
#include "julian_day.hpp"
#include "sidereal_time.hpp"
#include "toolbox.hpp"

namespace astro::observer {

/** @brief WGS84 semi-major axis, in kilometres. @see NGA WGS84, Defining Parameters. */
inline constexpr double WGS84_EQUATORIAL_RADIUS_KM = 6378.137;

/** @brief WGS84 inverse flattening. @see https://epsg.org/ellipsoid_7030/WGS-84.html */
inline constexpr double WGS84_INVERSE_FLATTENING = 298.257223563;

/** @brief Squared polar-to-equatorial radius ratio, 1 - e². */
inline constexpr double WGS84_POLAR_RATIO_SQUARED =
  (1.0 - (1.0 / WGS84_INVERSE_FLATTENING)) * (1.0 - (1.0 / WGS84_INVERSE_FLATTENING));

/** @brief Conservative geodetic-map elevation floor, the negative minimum meridian curvature radius. */
inline constexpr double MINIMUM_ELEVATION_KM = -WGS84_EQUATORIAL_RADIUS_KM * WGS84_POLAR_RATIO_SQUARED;

/** @brief Projection resolution relative to the pre-subtraction vectors, not an angular accuracy bound. */
inline constexpr double DIRECTION_RESOLUTION = 32.0 * std::numeric_limits<double>::epsilon();

/** @brief Fixed WGS84 geodetic site; elevation is signed ellipsoidal height, not height above the geoid. */
struct Observer {
  GeoLocation location;
  toolbox::DistanceKm elevation;
};

/** @brief Topocentric centre coordinates; azimuth is north-zero/east-positive, all angles in degrees. */
struct Position {
  std::optional<toolbox::AngleDeg> right_ascension;
  toolbox::AngleDeg declination;
  toolbox::DistanceAu distance;
  toolbox::AngleDeg altitude;
  std::optional<toolbox::AngleDeg> azimuth;
  std::optional<toolbox::AngleDeg> apparent_altitude;
};

/** @brief A civil observation with the resolved time pair and source/model identities. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Strong coordinate members require values; all aggregate fields are supplied.
struct Observation {
  julian_day::Times times;
  ephemeris::Target target;
  ephemeris::PositionSource source;
  delta_t::Model delta_t_model;
  Position position;
};

namespace detail {

/** @brief Observer's cylindrical radius and axial position, in units of the WGS84 semi-major axis. */
struct GeocentricSite {
  double equatorial;
  double polar;
};

/** @brief Dimensionless translated and horizontal components, with observer distance in AU. */
struct LocalGeometry {
  double A;
  double B;
  double C;
  double q;
  double scale;
  double distance_au;
  double east;
  double north;
  double up;
  double horizontal;
  double equatorial;
};

/** @brief Validate the inclusive polar geometry and conservative ellipsoidal-height admission. */
inline auto validate(const Observer& observer) -> void {
  geolocation::validate_latitude(observer.location.latitude, "latitude");
  geolocation::validate_longitude(observer.location.longitude, "longitude");
  if (not std::isfinite(observer.elevation.km()) or observer.elevation.km() <= MINIMUM_ELEVATION_KM) {
    throw std::invalid_argument { "Observer elevation must be finite and above the WGS84 geodetic-map floor" };
  }
}

/** @brief Convert a validated geodetic site to its meridian-plane vector. @see Meeus, Chapter 11, p. 82. */
[[nodiscard]] inline auto geocentric_site(const Observer& observer) -> GeocentricSite {
  const double φ = observer.location.latitude.rad();
  const double sin_φ = std::sin(φ);
  const double normal = 1.0 / std::sqrt(1.0 - ((1.0 - WGS84_POLAR_RATIO_SQUARED) * sin_φ * sin_φ));
  const double height = observer.elevation.km() / WGS84_EQUATORIAL_RADIUS_KM;
  return GeocentricSite {
    .equatorial = (normal + height) * std::cos(φ),
    .polar = ((WGS84_POLAR_RATIO_SQUARED * normal) + height) * sin_φ,
  };
}

/**
 * @brief Translate a finite geocentric position for a validated observer and apparent sidereal angle.
 * @note The observer displacement and geocentric distance use the same WGS84 radius/AU relation.
 * @see Meeus, formulas (40.6), (40.7), and horizontal projection (13.5), (13.6).
 */
[[nodiscard]] inline auto local_geometry(
  const ephemeris::EquatorialPosition& geocentric,
  const toolbox::AngleDeg θ,
  const Observer& observer
) -> LocalGeometry {
  if (not geocentric.distance.has_value() or not std::isfinite(geocentric.distance->au())
      or geocentric.distance->au() <= 0.0) {
    throw std::runtime_error { "Topocentric reduction requires a finite positive physical distance" };
  }
  const double distance_au = geocentric.distance->au();
  const auto site = geocentric_site(observer);
  const double sin_π = toolbox::km_to_au(WGS84_EQUATORIAL_RADIUS_KM) / distance_au;
  const double ρ_cos_φp_sin_π = site.equatorial * sin_π;
  const double ρ_sin_φp_sin_π = site.polar * sin_π;
  const double H = (θ - geocentric.right_ascension).rad();
  const double δ = geocentric.declination.rad();
  const double A = std::cos(δ) * std::sin(H);
  const double B = (std::cos(δ) * std::cos(H)) - ρ_cos_φp_sin_π;
  const double C = std::sin(δ) - ρ_sin_φp_sin_π;
  const double q = std::hypot(A, B, C);
  const double scale = std::max(1.0, std::hypot(ρ_cos_φp_sin_π, ρ_sin_φp_sin_π));
  const double distance = q * distance_au;
  const bool finite_translation = std::isfinite(A) and std::isfinite(B) and std::isfinite(C)
    and std::isfinite(q) and std::isfinite(scale) and std::isfinite(distance);
  if (not finite_translation or q <= DIRECTION_RESOLUTION * scale or distance <= 0.0) {
    throw std::runtime_error { "Observer-to-body direction or distance is unresolved" };
  }

  const double φ = observer.location.latitude.rad();
  const double east = -A;
  const double north = (C * std::cos(φ)) - (B * std::sin(φ));
  const double up = (C * std::sin(φ)) + (B * std::cos(φ));
  const double horizontal = std::hypot(east, north);
  const double equatorial = std::hypot(A, B);
  const bool finite_projection = std::isfinite(east) and std::isfinite(north) and std::isfinite(up)
    and std::isfinite(horizontal) and std::isfinite(equatorial);
  if (not finite_projection) {
    throw std::runtime_error { "Topocentric coordinate projection is not finite" };
  }

  return {
    .A = A,
    .B = B,
    .C = C,
    .q = q,
    .scale = scale,
    .distance_au = distance,
    .east = east,
    .north = north,
    .up = up,
    .horizontal = horizontal,
    .equatorial = equatorial,
  };
}

/** @brief Assemble topocentric coordinates from the observer's translated and projected geometry. */
[[nodiscard]] inline auto reduce(
  const ephemeris::EquatorialPosition& geocentric,
  const toolbox::AngleDeg θ,
  const Observer& observer
) -> Position {
  using toolbox::AngleDeg;

  const auto geometry = local_geometry(geocentric, θ, observer);
  const bool geographic_pole = std::abs(observer.location.latitude.deg()) == 90.0;
  return Position {
    .right_ascension = geometry.equatorial <= DIRECTION_RESOLUTION * geometry.scale ? std::nullopt
      : std::optional { (θ - AngleDeg { toolbox::rad_to_deg(std::atan2(geometry.A, geometry.B)) }).normalize() },
    .declination = AngleDeg { toolbox::rad_to_deg(std::atan2(geometry.C, geometry.equatorial)) },
    .distance = toolbox::DistanceAu { geometry.distance_au },
    .altitude = AngleDeg { toolbox::rad_to_deg(std::atan2(geometry.up, geometry.horizontal)) },
    .azimuth = geographic_pole or geometry.horizontal <= DIRECTION_RESOLUTION * geometry.scale ? std::nullopt
      : std::optional { AngleDeg { toolbox::rad_to_deg(std::atan2(geometry.east, geometry.north)) }.normalize() },
    .apparent_altitude = std::nullopt,
  };
}

} // namespace detail

/**
 * @brief Observe one physical target using caller-resolved UT1 and TT at the same instant.
 * @param times Finite JD(UT1) and JDE(TT); their same-instant correspondence is the caller's contract.
 * @param observer WGS84 site, latitude [-90, 90]°, east longitude [-180, 180]°, elevation above the floor.
 * @param target Sun, Moon or a planet; mathematical lunar nodes are rejected.
 * @param refraction Absent by default; an unsupported altitude leaves apparent_altitude absent.
 * @return Centre RA/Dec, observer distance, geometric altitude and north-zero/east-positive azimuth.
 * @note RA is absent at an unresolved equatorial projection; azimuth is absent at exact geographic
 *       poles and unresolved horizontal projections. Other meaningful fields remain available.
 *       Fixed IAU1980 axes; no added diurnal aberration, polar motion or observer light-time solve.
 * @throw std::invalid_argument For malformed input, nonphysical target, provider-window or weather rejection.
 * @throw std::runtime_error For failed evaluation or unresolved observer-to-body distance/direction.
 */
[[nodiscard]] inline auto observe(
  const julian_day::Times& times,
  const Observer& observer,
  const ephemeris::Target target,
  const std::optional<earth::refraction::Params>& refraction = std::nullopt
) -> Position {
  detail::validate(observer);
  if (not std::isfinite(times.jd_ut1) or not std::isfinite(times.jde_tt)) {
    throw std::invalid_argument { "Observer times must be finite JD(UT1) and JDE(TT)" };
  }
  if (ephemeris::target_facts(target).node.has_value()) {
    throw std::invalid_argument { "Topocentric queries require a physical target, not a lunar node" };
  }

  const auto θ = sidereal::local_apparent(
    times.jd_ut1,
    times.jde_tt,
    -observer.location.longitude,
    earth::nutation::Model::IAU_1980
  );
  auto result = detail::reduce(ephemeris::equatorial_position(target, times.jde_tt), θ, observer);
  if (refraction.has_value()) {
    result.apparent_altitude = earth::refraction::apparent_altitude(result.altitude, *refraction);
  }
  return result;
}

/**
 * @brief Resolve a civil UTC/UT1 instant and observe one physical target without a chart/rate window.
 * @note UTC begins at 1972-01-01; derived UT1 is modelled through the chosen Delta T, not observed DUT1.
 *       The target's position-provider window and the selected Delta T model's own guards apply.
 * @see observe for coordinate, observer, refraction and failure contracts.
 */
[[nodiscard]] inline auto query(
  const calendar::Datetime& civil_dt,
  const julian_day::Scale scale,
  const Observer& observer,
  const ephemeris::Target target,
  const std::optional<earth::refraction::Params>& refraction = std::nullopt,
  const delta_t::Model delta_t_model = delta_t::DEFAULT_MODEL
) -> Observation {
  const auto times = julian_day::resolve_times(civil_dt, scale, delta_t_model);
  return Observation {
    .times = times,
    .target = target,
    .source = ephemeris::source_of(target),
    .delta_t_model = delta_t_model,
    .position = observe(times, observer, target, refraction),
  };
}

} // namespace astro::observer
