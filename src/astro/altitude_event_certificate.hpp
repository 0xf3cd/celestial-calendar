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
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "altitude_event_bounds.hpp"
#include "altitude_event_geometry.hpp"
#include "event_search.hpp"
#include "julian_day.hpp"
#include "observer.hpp"
#include "toolbox.hpp"

namespace astro::detail::altitude_bounds {

/** @brief Decode a normal binary64 power of two; -128 is the exact-zero sentinel. */
[[nodiscard]] inline auto decode(const int8_t exponent) -> double {
  return exponent == -128 ? 0.0 : std::bit_cast<double>(static_cast<uint64_t>(static_cast<int32_t>(exponent) + 1023) << 52U);
}

/** @brief Caller certifies source and clock membership before supplying rotation and radius bounds. */
[[nodiscard]] inline auto source(
  const Packet& packet,
  const double rotation_first,
  const double rotation_second,
  const double earth_radius_au
) -> observer_c2::Source {
  return {
    .direction = { 1.0, decode(packet.at(0)), decode(packet.at(1)) },
    .distance_au = { decode(packet.at(2)), decode(packet.at(3)), decode(packet.at(4)) },
    .distance_lower_au = decode(packet.at(5)),
    .rotation_first_rad = rotation_first,
    .rotation_second_rad = rotation_second,
    .earth_radius_au = earth_radius_au,
  };
}

/** @brief Evaluate a serialized nonnegative affine field, rounding outward. */
[[nodiscard]] inline auto affine(const int8_t constant, const int8_t slope, const double absolute_height_km) -> double {
  return observer_c2::add(decode(constant), observer_c2::multiply(decode(slope), absolute_height_km));
}

/** @brief Complete source/site bounds and dimensionless evaluation allowances. */
struct ObserverInputs {
  observer_c2::Source source;
  observer_c2::Site site;
  double value_error;
  double q_upper;
};

/** @brief Positive site bounds relative to stored WGS84 constants; height is in km. */
[[nodiscard]] inline auto site_bounds(
  const observer_c2::Source& source,
  const double latitude_degrees,
  const double height_km
) -> observer_c2::Site {
  namespace bounds = observer_c2;
  if (not std::isfinite(latitude_degrees) or std::fabs(latitude_degrees) > 90.0 or not std::isfinite(height_km)) {
    throw std::runtime_error { "The packet observer input is outside its finite latitude/height certificate" };
  }

  const double phi = astro::toolbox::AngleDeg { latitude_degrees }.rad();
  const double wave_error = LATITUDE_TRIG_ERROR;
  const double c = std::fabs(latitude_degrees) == 90.0 ? 0.0 : std::min(1.0, bounds::add(std::fabs(std::cos(phi)), wave_error));
  const double s = latitude_degrees == 0.0 ? 0.0 : std::min(1.0, bounds::add(std::fabs(std::sin(phi)), wave_error));
  const double height = bounds::divide(std::fabs(height_km), astro::observer::WGS84_EQUATORIAL_RADIUS_KM);
  const double equatorial = bounds::multiply(bounds::add(NORMAL_UPPER, height), c);
  const double polar = bounds::multiply(bounds::add(bounds::multiply(astro::observer::WGS84_POLAR_RATIO_SQUARED, NORMAL_UPPER), height), s);
  const double p0 = observer_c2::parallax(source).at(0);
  const double q_lower = astro::detail::event_search::lower(1.0 - bounds::multiply(p0, bounds::add(equatorial, polar)));
  if (not std::isfinite(q_lower) or q_lower <= 0.0) {
    throw std::runtime_error { "The packet observer has no positive local separation certificate" };
  }
  return { .equatorial_upper = equatorial, .polar_upper = polar, .cos_latitude_upper = c,
           .sin_latitude_upper = s, .q_lower = q_lower };
}

/** @brief A closed proof piece; a collar carries every model jump it contains. */
struct ClockPiece {
  double start_jde_tt;
  double end_jde_tt;
  bool collar;
  double jump;
};

/** @brief Exact half-integral Gregorian January 1 on the TT Julian-day scale. */
[[nodiscard]] inline auto year_start(const int32_t year) -> double {
  return astro::julian_day::tt_to_jde(calendar::Datetime { std::chrono::year { year } / std::chrono::January / 1, 0.0 });
}

/** @brief Merge ordered Gregorian/model joins into closed proof pieces within a piece budget. */
[[nodiscard]] inline auto near_clock_pieces(
  const double start_jde_tt,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- TT endpoint and work limit have distinct names.
  const double end_jde_tt,
  const std::size_t maximum_pieces
) -> std::vector<ClockPiece> {
  namespace search = astro::detail::event_search;
  const bool within_clock_tier = CLOCK_START <= start_jde_tt and start_jde_tt < end_jde_tt and end_jde_tt <= CLOCK_END;
  if (not within_clock_tier) {
    throw std::runtime_error { "The dynamic clock leaves its proved source tier" };
  }
  const int32_t first = static_cast<int32_t>(astro::julian_day::jde_to_tt(start_jde_tt).ymd.year());
  const int32_t last = static_cast<int32_t>(astro::julian_day::jde_to_tt(end_jde_tt).ymd.year());
  if (maximum_pieces == 0 or std::cmp_greater(last - first, maximum_pieces)) {
    throw std::runtime_error { "The clock proof-piece budget is exhausted" };
  }

  std::vector<ClockPiece> result;
  double cursor = start_jde_tt;
  bool pending = false;
  ClockJoin current {};
  const auto emit = [&] {
    if (not pending) {
      return;
    }
    const double left = std::max(start_jde_tt, current.before);
    const double right = std::min(end_jde_tt, current.after);
    if (cursor < left) {
      result.push_back({ .start_jde_tt = cursor, .end_jde_tt = left, .collar = false, .jump = 0.0 });
    }
    result.push_back({ .start_jde_tt = left, .end_jde_tt = right, .collar = true, .jump = current.jump });
    cursor = right;
  };
  int32_t year = first;
  std::size_t model = 0;
  while (year <= last + 1 or model < MODEL_JOINS.size()) {
    const bool have_year = year <= last + 1;
    const double at = have_year ? year_start(year) : 0.0;
    const ClockJoin yearly {
      .before = search::lower(at - YEAR_PAD_DAYS), .after = search::upper(at + YEAR_PAD_DAYS), .jump = 0.0
    };
    const bool take_model = model < MODEL_JOINS.size() and (not have_year or MODEL_JOINS.at(model).before < yearly.before);
    const auto join = take_model ? MODEL_JOINS.at(model++) : yearly;
    if (not take_model) {
      ++year;
    }
    const bool overlaps = join.before < end_jde_tt and start_jde_tt < join.after;
    if (not overlaps) {
      continue;
    }
    if (pending and join.before <= current.after) {
      current.after = std::max(current.after, join.after);
      current.jump = observer_c2::add(current.jump, join.jump);
    } else {
      emit();
      current = join;
      pending = true;
    }
  }
  emit();
  if (cursor < end_jde_tt) {
    result.push_back({ .start_jde_tt = cursor, .end_jde_tt = end_jde_tt, .collar = false, .jump = 0.0 });
  }
  if (result.size() > maximum_pieces) {
    throw std::runtime_error { "The clock proof-piece budget is exhausted" };
  }
  return result;
}

/** @brief Find a smooth clock envelope covering the closed interval, or return the table end. */
[[nodiscard]] inline auto local_clock_index(const double start_jde_tt, const double end_jde_tt) -> std::size_t {
  for (std::size_t index = 0; index < LOCAL_CLOCKS.size(); ++index) {
    const auto& clock = LOCAL_CLOCKS.at(index);
    if (clock.start <= start_jde_tt and start_jde_tt < end_jde_tt and end_jde_tt <= clock.end) {
      return index;
    }
  }
  return LOCAL_CLOCKS.size();
}

/** @brief Partition inside a complete near-tier or local clock certificate; missing coverage fails. */
[[nodiscard]] inline auto clock_pieces(
  const double start_jde_tt,
  const double end_jde_tt,
  const std::size_t maximum_pieces
) -> std::vector<ClockPiece> {
  if (CLOCK_START <= start_jde_tt and end_jde_tt <= CLOCK_END) {
    return near_clock_pieces(start_jde_tt, end_jde_tt, maximum_pieces);
  }
  if (maximum_pieces == 0 or local_clock_index(start_jde_tt, end_jde_tt) == LOCAL_CLOCKS.size()) {
    throw std::runtime_error { "No complete local clock certificate covers the canonical cell" };
  }
  std::vector<ClockPiece> result;
  result.push_back({ .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt, .collar = false, .jump = 0.0 });
  return result;
}

/** @brief Select source and clock separately, then compose observer bounds for a retained proof piece. */
[[nodiscard]] inline auto observer_for_piece(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- Wire target and site latitude have distinct names.
  const uint32_t target,
  const double latitude_degrees,
  const double height_km,
  const ClockPiece& piece
) -> ObserverInputs {
  if (target > 9) {
    throw std::invalid_argument { "The physical target is outside0 through9" };
  }
  Packet packet {};
  double first = ROTATION_FIRST;
  double second = ROTATION_SECOND;
  if (CLOCK_START <= piece.start_jde_tt and piece.start_jde_tt < piece.end_jde_tt and piece.end_jde_tt <= CLOCK_END) {
    const bool within_pluto_tier = PLUTO_START <= piece.start_jde_tt and piece.end_jde_tt <= PLUTO_END;
    if (target == 9 and not within_pluto_tier) {
      throw std::runtime_error { "The clock piece leaves Pluto's proved source tier" };
    }
    packet = RECORDS.at(target);
    if (piece.collar) {
      packet.at(6) = COLLAR_ERRORS.at(target).at(0);
      packet.at(7) = COLLAR_ERRORS.at(target).at(1);
    }
  } else {
    const std::size_t index = local_clock_index(piece.start_jde_tt, piece.end_jde_tt);
    const LocalSource* selected = nullptr;
    for (const auto& record : LOCAL_SOURCES) {
      if (record.target == target and record.clock == index and not piece.collar) {
        selected = &record;
        break;
      }
    }
    if (selected == nullptr or index == LOCAL_CLOCKS.size()) {
      throw std::runtime_error { "No complete local source certificate covers the target and proof piece" };
    }
    packet = selected->packet;
    first = LOCAL_CLOCKS.at(index).first;
    second = LOCAL_CLOCKS.at(index).second;
  }
  const auto bound_source = source(packet, first, second, EARTH_RADIUS_AU);
  const auto site = site_bounds(bound_source, latitude_degrees, height_km);
  const double height = std::fabs(height_km);
  const double error = affine(packet.at(6), packet.at(7), height);
  const double q = affine(packet.at(8), packet.at(9), height);
  if (not std::isfinite(error) or not std::isfinite(q)) {
    throw std::runtime_error { "The packet observer value allowance overflowed" };
  }
  return { .source = bound_source, .site = site, .value_error = error, .q_upper = q };
}

} // namespace astro::detail::altitude_bounds
