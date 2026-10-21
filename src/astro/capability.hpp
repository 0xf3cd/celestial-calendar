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

#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "delta_t.hpp"
#include "earth.hpp"
#include "ephemeris.hpp"
#include "ephemeris_events.hpp"
#include "lunar_node.hpp"
#include "planet.hpp"

/**
 * @file capability.hpp
 * @brief Machine-readable operation domains and model identities (#357).
 * @note Every bound here is computed from the defining constants in the operation's own header
 *       (@ref ephemeris::longitude_rate_domain, the Pluto and node windows, the event-search span);
 *       this header adds no second table of numbers. Measured validation coverage is prose in
 *       @ref describe, clearly labelled as measured — a capability record never implies accuracy.
 */

namespace astro::capability {

/** @brief A describable operation. */
enum class Operation : uint8_t {
  POSITION,
  LONGITUDE_RATE,
  EVENTS,
  HOUSES,
};

/**
 * @brief What kind of bound a domain carries.
 * @note The status names the bound, not the accuracy: `UNBOUNDED_FINITE` still degrades in
 *       accuracy far from J2000 (the mean-obliquity polynomial warns at ±2000 years); the
 *       capability layer does not turn domains into precision promises.
 */
enum class Status : uint8_t {
  SOURCE_WINDOW,         ///< The source data/method publishes this window.
  IMPLEMENTATION_WINDOW, ///< The project selected this window (rate working range, node domain).
  UNBOUNDED_FINITE,      ///< No explicit JDE bound; only finiteness is checked.
};

/** @brief Stable identity of a position computation, per target (#357). */
enum class PositionSource : uint8_t {
  SUN_VSOP87D,
  MOON_ELP2000_82B_TRUNC,
  PLANET_VSOP87D,
  PLUTO_PLUTO95_APPARENT,
  NODE_MEAN,
  NODE_TRUE_MEEUS_FIVE_TERM, ///< The Meeus five-term approximation; not an osculating node.
};

/** @brief Model family for the identity registry. */
enum class ModelKind : uint8_t {
  DELTA_T,
  NUTATION,
  POSITION_SOURCE,
};

/** @brief Lifecycle status of a registered model. */
enum class ModelStatus : uint8_t {
  CURRENT,
  FROZEN,
};

/** @brief One operation's accepted-input description for one subject. */
struct Record {
  Operation operation;
  uint8_t subject_code;        ///< ephemeris::Target code for target operations; unused (0) for HOUSES.
  Status status;
  PositionSource source;       ///< Unused (value 0) for HOUSES.
  double domain_start_jde_tt;  ///< Inclusive; NaN = no lower bound.
  double domain_end_jde_tt;    ///< Exclusive; NaN = no upper bound.
  uint32_t maximum_span_tt_days; ///< EVENTS: EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS; 0 = no span limit.
};

/** @brief One registered model identity with its applicability. */
struct ModelIdentity {
  ModelKind kind;
  uint8_t identity_code;       ///< Ordinal within the kind; pinned by the C ABI MODEL_IDENTITY_* macros and
                               ///< stable across releases. Not a CHART_* boundary selection code.
  ModelStatus status;
  uint32_t default_alias_code; ///< Boundary code this identity answers to as the default; UINT32_MAX when none.
  double domain_start_jde_tt;  ///< Published applicability in JDE(TT); NaN = unbounded.
  double domain_end_jde_tt;
};

/** @brief The C-boundary Delta T default: code 0 resolves to Algo5 (celestial.h CHART_DELTA_T_MODEL_*). */
inline constexpr uint32_t DEFAULT_DELTA_T_CODE = 0;

/** @brief Resolve a boundary Delta T code to a model; DEFAULT (0) selects Algo5. */
[[nodiscard]] constexpr auto resolve_delta_t_code(const uint32_t code) -> std::optional<astro::delta_t::Model> {
  using enum astro::delta_t::Model;
  switch (code) {
    case DEFAULT_DELTA_T_CODE: return ALGO5;
    case 1: return ALGO1;
    case 2: return ALGO2;
    case 3: return ALGO3;
    case 4: return ALGO4;
    case 5: return ALGO5;
    default: return std::nullopt;
  }
}

/** @brief The position-source identity a target's computation carries. */
[[nodiscard]] constexpr auto position_source_of(const ephemeris::Target target) -> PositionSource {
  using enum ephemeris::Target;
  switch (target) {
    case SUN: return PositionSource::SUN_VSOP87D;
    case MOON: return PositionSource::MOON_ELP2000_82B_TRUNC;
    case MERCURY:
    case VENUS:
    case MARS:
    case JUPITER:
    case SATURN:
    case URANUS:
    case NEPTUNE: return PositionSource::PLANET_VSOP87D;
    case PLUTO: return PositionSource::PLUTO_PLUTO95_APPARENT;
    case MEAN_ASCENDING:
    case MEAN_DESCENDING: return PositionSource::NODE_MEAN;
    case TRUE_ASCENDING:
    case TRUE_DESCENDING: return PositionSource::NODE_TRUE_MEEUS_FIVE_TERM;
    default:
      throw std::invalid_argument { std::format("Unknown ephemeris target {}", static_cast<uint32_t>(target)) };
  }
}

/** @brief A target's position-provider window; NaN ends mark the absence of an explicit bound. */
[[nodiscard]] constexpr auto position_domain(const ephemeris::Target target) -> std::array<double, 2> {
  using enum ephemeris::Target;
  using astro::planet::geocentric_coord::PLUTO_END_JDE_TT;
  using astro::planet::geocentric_coord::PLUTO_START_JDE_TT;
  switch (target) {
    case PLUTO: return { PLUTO_START_JDE_TT, PLUTO_END_JDE_TT };
    case MEAN_ASCENDING:
    case MEAN_DESCENDING:
    case TRUE_ASCENDING:
    case TRUE_DESCENDING: return { lunar_node::START_JDE_TT, lunar_node::END_JDE_TT };
    default: {
      const double unbounded = std::numeric_limits<double>::quiet_NaN();
      return { unbounded, unbounded };
    }
  }
}

namespace detail {

inline constexpr double UNBOUNDED = std::numeric_limits<double>::quiet_NaN();

[[nodiscard]] constexpr auto target_count() -> uint8_t { return 14; }

[[nodiscard]] constexpr auto all_targets() -> std::array<ephemeris::Target, 14> {
  using enum ephemeris::Target;
  return { SUN, MOON, MERCURY, VENUS, MARS, JUPITER, SATURN, URANUS, NEPTUNE, PLUTO,
           MEAN_ASCENDING, MEAN_DESCENDING, TRUE_ASCENDING, TRUE_DESCENDING };
}

// Every rate/events center domain is the root window shrunk by the stencil reach
// (ephemeris.hpp), an implementation derivation no matter what the root window's own nature is;
// the root window's status is stated on the POSITION record.
[[nodiscard]] constexpr auto rate_status() -> Status {
  return Status::IMPLEMENTATION_WINDOW;
}

[[nodiscard]] constexpr auto position_status(const ephemeris::Target target, const double domain_start) -> Status {
  if (std::isnan(domain_start)) {
    return Status::UNBOUNDED_FINITE;
  }
  return target == ephemeris::Target::PLUTO ? Status::SOURCE_WINDOW : Status::IMPLEMENTATION_WINDOW;
}

} // namespace detail

/** @brief The domain record for one target operation; HOUSES ignores the target. */
[[nodiscard]] inline auto domain_record(const Operation operation, const ephemeris::Target target) -> Record {
  using enum Operation;
  switch (operation) {
    case POSITION: {
      const auto [start, end] = position_domain(target);
      return { .operation = operation,
               .subject_code = static_cast<uint8_t>(target),
               .status = detail::position_status(target, start),
               .source = position_source_of(target),
               .domain_start_jde_tt = start,
               .domain_end_jde_tt = end,
               .maximum_span_tt_days = 0 };
    }
    case LONGITUDE_RATE: {
      const auto [start, end] = ephemeris::longitude_rate_domain(target);
      return { .operation = operation,
               .subject_code = static_cast<uint8_t>(target),
               .status = detail::rate_status(),
               .source = position_source_of(target),
               .domain_start_jde_tt = start,
               .domain_end_jde_tt = end,
               .maximum_span_tt_days = 0 };
    }
    case EVENTS: {
      const auto [start, end] = ephemeris::longitude_rate_domain(target);
      return { .operation = operation,
               .subject_code = static_cast<uint8_t>(target),
               .status = detail::rate_status(),
               .source = position_source_of(target),
               .domain_start_jde_tt = start,
               .domain_end_jde_tt = end,
               .maximum_span_tt_days = static_cast<uint32_t>(ephemeris::detail::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS) };
    }
    case HOUSES:
      // house::calculate checks finiteness and geographic range only; there is no JDE window,
      // and the snapshot's joint window must not be attributed to the house operation.
      return { .operation = operation,
               .subject_code = 0,
               .status = Status::UNBOUNDED_FINITE,
               .source = PositionSource::SUN_VSOP87D, // unused; the record names no position source
               .domain_start_jde_tt = detail::UNBOUNDED,
               .domain_end_jde_tt = detail::UNBOUNDED,
               .maximum_span_tt_days = 0 };
  }
  throw std::invalid_argument { std::format("Unknown capability operation {}", static_cast<uint32_t>(operation)) };
}

/** @brief All domain records of an operation: one per target, or the single HOUSES record. */
[[nodiscard]] inline auto domain_records(const Operation operation) -> std::vector<Record> {
  if (operation == Operation::HOUSES) {
    return { domain_record(operation, ephemeris::Target::SUN) };
  }
  const auto targets = detail::all_targets();
  std::vector<Record> records;
  records.reserve(targets.size());
  for (const auto target : targets) {
    records.push_back(domain_record(operation, target));
  }
  return records;
}

/** @brief The full model identity registry: 5 Delta T, 2 nutation, 6 position sources. */
[[nodiscard]] inline auto model_identities() -> std::vector<ModelIdentity> {
  using enum ModelKind;
  using enum ModelStatus;
  constexpr uint32_t NO_ALIAS = std::numeric_limits<uint32_t>::max();
  constexpr double unbounded = std::numeric_limits<double>::quiet_NaN();

  std::vector<ModelIdentity> identities;
  identities.reserve(13);

  using enum astro::delta_t::Model;
  // algo1–algo4 are frozen exhibits; algo5 is the trained default refreshed by the grant ritual.
  // Each model's applicability is owned by delta_t.hpp; algo2 and algo5 publish no finite bound.
  const std::array models = { ALGO1, ALGO2, ALGO3, ALGO4, ALGO5 };
  for (const auto model : models) {
    const auto code = static_cast<uint8_t>(model);
    const double start = model == ALGO1 ? astro::delta_t::ALGO1_START_JDE_TT : unbounded;
    const double end = [&] {
      if (model == ALGO3) {
        return astro::delta_t::ALGO3_END_JDE_TT;
      }
      return model == ALGO4 ? astro::delta_t::ALGO4_END_JDE_TT : unbounded;
    }();
    identities.push_back({ .kind = DELTA_T,
                           .identity_code = code,
                           .status = model == ALGO5 ? CURRENT : FROZEN,
                           .default_alias_code = model == ALGO5 ? DEFAULT_DELTA_T_CODE : NO_ALIAS,
                           .domain_start_jde_tt = start,
                           .domain_end_jde_tt = end });
  }

  using enum astro::earth::nutation::Model;
  const std::array nutations = { MEEUS, IAU_1980 };
  for (const auto model : nutations) {
    identities.push_back({ .kind = NUTATION,
                           .identity_code = static_cast<uint8_t>(model),
                           .status = CURRENT,
                           .default_alias_code = NO_ALIAS,
                           .domain_start_jde_tt = unbounded,
                           .domain_end_jde_tt = unbounded });
  }

  using enum PositionSource;
  const std::array sources = { SUN_VSOP87D, MOON_ELP2000_82B_TRUNC, PLANET_VSOP87D,
                               PLUTO_PLUTO95_APPARENT, NODE_MEAN, NODE_TRUE_MEEUS_FIVE_TERM };
  for (const auto source : sources) {
    // A position source's applicability is its targets' provider window where one is published.
    auto [start, end] = std::array { unbounded, unbounded };
    if (source == PLUTO_PLUTO95_APPARENT) {
      start = astro::planet::geocentric_coord::PLUTO_START_JDE_TT;
      end = astro::planet::geocentric_coord::PLUTO_END_JDE_TT;
    } else if (source == NODE_MEAN or source == NODE_TRUE_MEEUS_FIVE_TERM) {
      start = lunar_node::START_JDE_TT;
      end = lunar_node::END_JDE_TT;
    }
    identities.push_back({ .kind = POSITION_SOURCE,
                           .identity_code = static_cast<uint8_t>(source),
                           .status = CURRENT,
                           .default_alias_code = NO_ALIAS,
                           .domain_start_jde_tt = start,
                           .domain_end_jde_tt = end });
  }

  return identities;
}

namespace detail {

inline constexpr std::array<const char*, 6> SOURCE_PROSE = {
  "VSOP87D heliocentric series transported to geocentric apparent ecliptic of date; distance in AU; "
  "aberration and light time included",
  "truncated ELP2000-82B lunar series; geocentric apparent ecliptic of date; distance in AU",
  "VSOP87D heliocentric series transported to geocentric apparent ecliptic of date; distance in AU; "
  "aberration and light time included",
  "PLUTO95-derived state with apparent-place corrections (inverse FK5); geocentric apparent "
  "ecliptic of date; distance in AU",
  "mean lunar node on the fixed J2000 ecliptic projected to ecliptic-of-date longitude; "
  "a mathematical point with no physical distance",
  "Meeus five-term true-node approximation plus IAU 1980 delta-psi; a mathematical point with "
  "no physical distance; not an osculating node",
};

inline constexpr std::array<const char*, 14> TARGET_NAMES = {
  "the Sun", "the Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto",
  "the mean ascending node", "the mean descending node", "the true ascending node", "the true descending node",
};

// Measured validation coverage; every line names its reference so the text stays auditable.
inline constexpr std::array<const char*, 14> COVERAGE_PROSE = {
  "measured against JPL Horizons ecliptic references (three bands) and apparent RA/Dec "
  "1946-2053 within 0.12 arcseconds",
  "measured against JPL Horizons ecliptic references, 41 epochs in three bands to "
  "JDE(TT) 4194304.5",
  "measured against JPL Horizons ecliptic references, six epochs 1901-2094",
  "measured against JPL Horizons ecliptic references, six epochs 1901-2094",
  "measured against JPL Horizons ecliptic references, six epochs 1901-2094",
  "measured against JPL Horizons ecliptic references, six epochs 1901-2094",
  "measured against JPL Horizons ecliptic references, six epochs 1901-2094",
  "measured against JPL Horizons ecliptic references, six epochs 1901-2094",
  "measured against JPL Horizons ecliptic references, six epochs 1901-2094",
  "measured against PLUTO95 vectors and a 256-epoch JPL Horizons sample",
  "cross-checked against AA+/SwiftAA and SOFA nut80 implementations",
  "cross-checked against AA+/SwiftAA and SOFA nut80 implementations",
  "cross-checked against AA+/SwiftAA and SOFA nut80 implementations",
  "cross-checked against AA+/SwiftAA and SOFA nut80 implementations",
};

[[nodiscard]] inline auto domain_phrase(const Record& record) -> std::string {
  const bool start_bounded = not std::isnan(record.domain_start_jde_tt);
  const bool end_bounded = not std::isnan(record.domain_end_jde_tt);
  if (not start_bounded and not end_bounded) {
    return "any finite JDE(TT)";
  }
  if (not start_bounded) {
    return std::format("JDE(TT) below {}", record.domain_end_jde_tt);
  }
  if (not end_bounded) {
    return std::format("JDE(TT) at or above {}", record.domain_start_jde_tt);
  }
  return std::format("JDE(TT) in the half-open interval [{}, {})", record.domain_start_jde_tt,
                     record.domain_end_jde_tt);
}

[[nodiscard]] constexpr auto status_phrase(const Status status) -> const char* {
  switch (status) {
    case Status::SOURCE_WINDOW: return "a source-published window";
    case Status::IMPLEMENTATION_WINDOW: return "an implementation-selected window";
    case Status::UNBOUNDED_FINITE: return "no explicit JDE bound (finite input only)";
  }
  throw std::invalid_argument { "Unknown capability status" };
}

[[nodiscard]] constexpr auto delta_t_provenance(const uint8_t identity_code) -> const char* {
  switch (identity_code) {
    case 4: return "trained in AstroTime-Analysis (algo5.py) and refreshed by the grant/record ritual";
    case 3: return "USNO-prediction linear blend (#64)";
    default: return "frozen exhibit coefficients";
  }
}

} // namespace detail

/**
 * @brief Human-readable description of one capability record (#357).
 * @param operation The operation the record describes.
 * @param subject_code An ephemeris::Target code; ignored for HOUSES.
 * @return Origin, frame, units, corrections, domain, status, role and measured coverage.
 */
[[nodiscard]] inline auto describe(const Operation operation, const uint8_t subject_code) -> std::string {
  if (operation != Operation::HOUSES and subject_code >= detail::target_count()) {
    throw std::invalid_argument { std::format("Unknown ephemeris target {}", subject_code) };
  }
  // HOUSES names no target; pass a placeholder so the cast below only sees validated codes.
  const auto target = operation == Operation::HOUSES
    ? ephemeris::Target::SUN
    : static_cast<ephemeris::Target>(subject_code);
  const auto record = domain_record(operation, target);
  using enum Operation;
  switch (operation) {
    case POSITION:
      return std::format(
        "Position of {}: {}. Accepted input {}; the bound is {}. Role: direct position query "
        "input boundary{}. Position coverage: {}.",
        detail::TARGET_NAMES.at(subject_code), detail::SOURCE_PROSE.at(static_cast<uint8_t>(record.source)),
        detail::domain_phrase(record), detail::status_phrase(record.status),
        record.status == Status::UNBOUNDED_FINITE
          ? "; accuracy degrades with distance from J2000 (about 1 arcsecond over +/- 2000 years, "
            "worse farther out, per the mean-obliquity polynomial)"
          : "",
        detail::COVERAGE_PROSE.at(subject_code));
    case LONGITUDE_RATE:
      return std::format(
        "Longitude rate of {}: seven-point centered difference of the apparent geocentric "
        "longitude, certified derivative and floating-point bounds (ephemeris_event_bounds). "
        "Center domain {}; the bound is {}. Role: rate-stencil acceptance boundary. Position "
        "coverage: {}.",
        detail::TARGET_NAMES.at(subject_code), detail::domain_phrase(record),
        detail::status_phrase(record.status), detail::COVERAGE_PROSE.at(subject_code));
    case EVENTS:
      return std::format(
        "Longitude events of {}: crossing, station and difference queries accept a nonempty "
        "half-open [start, end) with span at most {} TT days inside {}; the bound is {}. The "
        "J2000 +/- 100-year analysis band selects only the certified error-bound table, never the "
        "accepted domain. Role: event-query acceptance boundary. Validation: event goldens and "
        "bound records; position coverage: {}.",
        detail::TARGET_NAMES.at(subject_code), record.maximum_span_tt_days, detail::domain_phrase(record),
        detail::status_phrase(record.status), detail::COVERAGE_PROSE.at(subject_code));
    case HOUSES:
      return "Houses: ascendant, midheaven, descendant, imum coeli and twelve cusps in the "
             "ecliptic of date. Accepted input: any finite UT1/TT pair (both scales are inputs) "
             "with latitude in (-90, 90) and longitude in [-180, 180]; Placidus additionally "
             "rejects polar geometry, and every system rejects an Ascendant direction "
             "unresolved at round-off. No JDE window; the snapshot joint window is not a house "
             "bound. Role: house-query acceptance boundary. Validation: agreement with Swiss "
             "Ephemeris formula-level and date-level references within a 1e-4-degree comparison "
             "tolerance (a comparison tolerance, not a validated error bound) plus Meeus 14.a.";
  }
  throw std::invalid_argument { std::format("Unknown capability operation {}", static_cast<uint32_t>(operation)) };
}

/**
 * @brief Human-readable description of one registered model identity (#357).
 * @param ordinal The identity's ordinal in @ref model_identities (0-12).
 */
[[nodiscard]] inline auto describe_identity(const uint8_t ordinal) -> std::string {
  const auto identities = model_identities();
  if (ordinal >= identities.size()) {
    throw std::invalid_argument { std::format("Unknown model identity {}", ordinal) };
  }
  const auto& identity = identities[ordinal];
  const auto *const status = identity.status == ModelStatus::CURRENT ? "current" : "frozen exhibit";

  const auto domain = [&] {
    const bool start_bounded = not std::isnan(identity.domain_start_jde_tt);
    const bool end_bounded = not std::isnan(identity.domain_end_jde_tt);
    if (start_bounded and end_bounded) {
      return std::format("applicability [{}, {}) in JDE(TT)", identity.domain_start_jde_tt,
                         identity.domain_end_jde_tt);
    }
    if (start_bounded) {
      return std::format("applicability starts at JDE(TT) {}", identity.domain_start_jde_tt);
    }
    if (end_bounded) {
      return std::format("applicability ends at JDE(TT) {}", identity.domain_end_jde_tt);
    }
    return std::string { "no published JDE(TT) bound" };
  }();

  using enum ModelKind;
  switch (identity.kind) {
    case DELTA_T: {
      const auto name = std::format("Delta T algo{}", static_cast<uint32_t>(identity.identity_code) + 1);
      const auto *const alias = identity.default_alias_code == DEFAULT_DELTA_T_CODE
        ? "; answers to boundary code 0 (DEFAULT)"
        : "";
      return std::format("{}: {}; {}; {}{}.", name, status, domain,
                         detail::delta_t_provenance(identity.identity_code), alias);
    }
    case NUTATION:
      return std::format("Nutation model {} (code {}): {}; {}.", identity.identity_code == 0 ? "Meeus low-precision" : "IAU 1980",
                         static_cast<uint32_t>(identity.identity_code), status, domain);
    case POSITION_SOURCE:
      return std::format("Position source code {}: {}; {}; {}.", static_cast<uint32_t>(identity.identity_code),
                         status, domain, detail::SOURCE_PROSE.at(identity.identity_code));
  }
  throw std::invalid_argument { "Unknown model kind" };
}

} // namespace astro::capability
