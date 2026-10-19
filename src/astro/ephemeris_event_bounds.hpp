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
#include <optional>

#include "ephemeris.hpp"
#include "event_search.hpp"

namespace astro::ephemeris::detail {

/** @brief Source-model derivative and evaluation allowances under the event-search floating-point contract. */
struct EventBounds {
  astro::detail::event_search::TrajectoryBounds trajectory;
  double position_error_deg;
  double rate_error_deg_per_day;
  std::optional<double> minimum_rate;
  std::optional<double> maximum_rate;
};

// Indexed by Target; all floating literals enclose the exact offline bound in its required direction.
inline constexpr std::array<EventBounds, 14> EVENT_BOUNDS_WIDE {{
  // SUN
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.0e1cf4dddd6ebp+0, .second_derivative = 0x1.750732ff5de9dp-10 },
      .rate = { .first_derivative = 0x1.1dfcfc6e72aacp-9, .second_derivative = 0x1.0cfc68bd4f115p-13 },
      .third_derivative = 0x1.5ed9ecca6721bp-14,
      .rate_remainder = 0x1.3ffdd437db219p-43,
    },
    .position_error_deg = 0x1.45df012b8fa02p-23,
    .rate_error_deg_per_day = 0x1.2ab718fe33113p-19,
    .minimum_rate = std::optional<double> { 0x1.d513563aa146bp-1 },
    .maximum_rate = std::nullopt,
  },
  // MOON
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.eee162f8de9bep+3, .second_derivative = 0x1.49c777129ebc8p-1 },
      .rate = { .first_derivative = 0x1.f9a9502d9e0ffp-1, .second_derivative = 0x1.4da59159f5f86p-2 },
      .third_derivative = 0x1.b331005f144f1p-3,
      .rate_remainder = 0x1.c3b8c1619ee47p-32,
    },
    .position_error_deg = 0x1.8ad594e14a29ep-25,
    .rate_error_deg_per_day = 0x1.69ee7df8d9170p-21,
    .minimum_rate = std::optional<double> { 0x1.5c68b00d589b0p+3 },
    .maximum_rate = std::nullopt,
  },
  // MERCURY
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.b45d5392321a0p+3, .second_derivative = 0x1.764f3d6996448p+2 },
      .rate = { .first_derivative = 0x1.1ef87be20445ap+3, .second_derivative = 0x1.98f4e7d31c79bp+2 },
      .third_derivative = 0x1.0ab5fb5d28d4fp+2,
      .rate_remainder = 0x1.68f2e4dd0ef22p-21,
    },
    .position_error_deg = 0x1.2013ec0500225p-19,
    .rate_error_deg_per_day = 0x1.0812432faa8c7p-15,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // VENUS
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.5d765e3189ac9p+3, .second_derivative = 0x1.35a45f4e83b98p+1 },
      .rate = { .first_derivative = 0x1.dac8d6675282cp+1, .second_derivative = 0x1.8a9a9078c2997p+0 },
      .third_derivative = 0x1.0159ac223c215p+0,
      .rate_remainder = 0x1.d0dec7a49eba7p-25,
    },
    .position_error_deg = 0x1.46063f860f833p-20,
    .rate_error_deg_per_day = 0x1.2adb0fe5a83b3p-16,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // MARS
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.555ab57b59f9ap+4, .second_derivative = 0x1.0ec8983df2451p+3 },
      .rate = { .first_derivative = 0x1.9f33941ab7bf4p+3, .second_derivative = 0x1.4dd3ec76d503bp+3 },
      .third_derivative = 0x1.b36d7736d30ffp+2,
      .rate_remainder = 0x1.fd15f4927276dp-14,
    },
    .position_error_deg = 0x1.b583ef949904bp-19,
    .rate_error_deg_per_day = 0x1.910e467454d8ap-15,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // JUPITER
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.0e8a480938b02p-1, .second_derivative = 0x1.90438838b92d1p-7 },
      .rate = { .first_derivative = 0x1.32de70f849b3ap-6, .second_derivative = 0x1.92404dbec4e88p-11 },
      .third_derivative = 0x1.0656757c6a285p-11,
      .rate_remainder = 0x1.36fce4962abeep-43,
    },
    .position_error_deg = 0x1.28286fac9716cp-24,
    .rate_error_deg_per_day = 0x1.0f7a6b887ad3ap-20,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // SATURN
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.9cc1d1752d6eep-2, .second_derivative = 0x1.fc6bbde71769bp-8 },
      .rate = { .first_derivative = 0x1.85ca119791f33p-7, .second_derivative = 0x1.cf80a5dae4685p-12 },
      .third_derivative = 0x1.2e48c535b65a5p-12,
      .rate_remainder = 0x1.16e7548ee0039p-43,
    },
    .position_error_deg = 0x1.d0bd50062e416p-25,
    .rate_error_deg_per_day = 0x1.aa02e903cc7f0p-21,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // URANUS
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.5a563207bdb43p-4, .second_derivative = 0x1.9798f091349fep-10 },
      .rate = { .first_derivative = 0x1.387dc98064143p-9, .second_derivative = 0x1.8580870813b4dp-14 },
      .third_derivative = 0x1.fc0bd184f84ffp-15,
      .rate_remainder = 0x1.d8619faa5e74ep-44,
    },
    .position_error_deg = 0x1.9fe7599702a6fp-27,
    .rate_error_deg_per_day = 0x1.7d3ee60d55b8fp-23,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // NEPTUNE
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.7ec2fba29bf92p-5, .second_derivative = 0x1.d4b5bfedf2d56p-11 },
      .rate = { .first_derivative = 0x1.675824366d5f5p-10, .second_derivative = 0x1.01418620efcdbp-14 },
      .third_derivative = 0x1.4f8d1e4138c98p-15,
      .rate_remainder = 0x1.caaf899f30703p-44,
    },
    .position_error_deg = 0x1.a614c6d539919p-28,
    .rate_error_deg_per_day = 0x1.82e8b37156605p-24,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // PLUTO
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.0a04fcb2ac2ccp-4, .second_derivative = 0x1.0d4d064853cfep-10 },
      .rate = { .first_derivative = 0x1.9ced922aa2a52p-10, .second_derivative = 0x1.1933ab74d3e9ep-14 },
      .third_derivative = 0x1.6ec8f5e64c0fbp-15,
      .rate_remainder = 0x1.cc07beda7194fp-44,
    },
    .position_error_deg = 0x1.4967bbccea30ap-33,
    .rate_error_deg_per_day = 0x1.2dfebcb57d9f3p-29,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // MEAN_ASCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.b267268503595p-5, .second_derivative = 0x1.c6313ac72a979p-16 },
      .rate = { .first_derivative = 0x1.5c36cf324b521p-15, .second_derivative = 0x1.7151bd1378ab1p-16 },
      .third_derivative = 0x1.e1b887510cb29p-17,
      .rate_remainder = 0x1.b64ecdedef973p-44,
    },
    .position_error_deg = 0x1.041fc24d8bea8p-39,
    .rate_error_deg_per_day = 0x1.e08fc9fe04095p-36,
    .minimum_rate = std::optional<double> { -0x1.b2672688c7e58p-5 },
    .maximum_rate = std::optional<double> { -0x1.b1310a585d610p-5 },
  },
  // MEAN_DESCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.b267268503595p-5, .second_derivative = 0x1.c6313ac72a979p-16 },
      .rate = { .first_derivative = 0x1.5c36cf324b521p-15, .second_derivative = 0x1.7151bd1378ab1p-16 },
      .third_derivative = 0x1.e1b887510cb29p-17,
      .rate_remainder = 0x1.b64ecdedef973p-44,
    },
    .position_error_deg = 0x1.101fc2f6dd380p-39,
    .rate_error_deg_per_day = 0x1.e08fc9fe04095p-36,
    .minimum_rate = std::optional<double> { -0x1.b2672688c7e58p-5 },
    .maximum_rate = std::optional<double> { -0x1.b1310a585d610p-5 },
  },
  // TRUE_ASCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.bc1bdb5e2991dp-3, .second_derivative = 0x1.940b7f709c8b0p-5 },
      .rate = { .first_derivative = 0x1.35c48c5ede6a9p-4, .second_derivative = 0x1.09391d5c043a7p-5 },
      .third_derivative = 0x1.59f174353d2adp-6,
      .rate_remainder = 0x1.92a8885a22dc5p-36,
    },
    .position_error_deg = 0x1.86e01ccca74bdp-35,
    .rate_error_deg_per_day = 0x1.666afa9c0c56ap-31,
    .minimum_rate = std::optional<double> { -0x1.bc1bdb755995cp-3 },
    .maximum_rate = std::optional<double> { 0x1.c66b9e7a20883p-4 },
  },
  // TRUE_DESCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.bc1bdb5e2991dp-3, .second_derivative = 0x1.940b7f709c8b0p-5 },
      .rate = { .first_derivative = 0x1.35c48c5ede6a9p-4, .second_derivative = 0x1.09391d5c043a7p-5 },
      .third_derivative = 0x1.59f174353d2adp-6,
      .rate_remainder = 0x1.92a8885a22dc5p-36,
    },
    .position_error_deg = 0x1.87a01cb9b8975p-35,
    .rate_error_deg_per_day = 0x1.666afa9c0c56ap-31,
    .minimum_rate = std::optional<double> { -0x1.bc1bdb755995cp-3 },
    .maximum_rate = std::optional<double> { 0x1.c66b9e7a20883p-4 },
  },
}};

inline constexpr std::array<EventBounds, 14> EVENT_BOUNDS_CENTURY {{
  // SUN
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.0e1cf4dddd6ebp+0, .second_derivative = 0x1.750732ff5de9dp-10 },
      .rate = { .first_derivative = 0x1.1dfcfc6e72aacp-9, .second_derivative = 0x1.0cfc68bd4f115p-13 },
      .third_derivative = 0x1.5ed9ecca6721bp-14,
      .rate_remainder = 0x1.3ffdd437db219p-43,
    },
    .position_error_deg = 0x1.a3d36a69c4565p-30,
    .rate_error_deg_per_day = 0x1.80d86cadfdc58p-26,
    .minimum_rate = std::optional<double> { 0x1.d513a027fb4ffp-1 },
    .maximum_rate = std::nullopt,
  },
  // MOON
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.eee162f8de9bep+3, .second_derivative = 0x1.49c777129ebc8p-1 },
      .rate = { .first_derivative = 0x1.f9a9502d9e0ffp-1, .second_derivative = 0x1.4da59159f5f86p-2 },
      .third_derivative = 0x1.b331005f144f1p-3,
      .rate_remainder = 0x1.c3b8c1619ee47p-32,
    },
    .position_error_deg = 0x1.fc8613bad2a7cp-32,
    .rate_error_deg_per_day = 0x1.d22afc95ceaafp-28,
    .minimum_rate = std::optional<double> { 0x1.5c68b173a2c30p+3 },
    .maximum_rate = std::nullopt,
  },
  // MERCURY
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.39f03cf8d6fa3p+3, .second_derivative = 0x1.929c19f4df2fcp+1 },
      .rate = { .first_derivative = 0x1.34aae0b333a4ap+2, .second_derivative = 0x1.481237879c12bp+1 },
      .third_derivative = 0x1.abeb3d4cb5501p+0,
      .rate_remainder = 0x1.3befd31c65cd3p-24,
    },
    .position_error_deg = 0x1.0649c8d418b90p-25,
    .rate_error_deg_per_day = 0x1.e0dcb022eb40fp-22,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // VENUS
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.23794311c657ap+3, .second_derivative = 0x1.b0a91574347aap+0 },
      .rate = { .first_derivative = 0x1.4bb4d4b6f508bp+1, .second_derivative = 0x1.cb6546c831eb3p-1 },
      .third_derivative = 0x1.2b9b17e6bc61cp-1,
      .rate_remainder = 0x1.2b6a61a888919p-27,
    },
    .position_error_deg = 0x1.5c7c28744de63p-26,
    .rate_error_deg_per_day = 0x1.3f71e4fc9cda6p-22,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // MARS
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.8d8d199a7e6adp+2, .second_derivative = 0x1.8ef802119ec51p-1 },
      .rate = { .first_derivative = 0x1.31e045da4f0e9p+0, .second_derivative = 0x1.1f99f03d3605ap-2 },
      .third_derivative = 0x1.7721d52e72fc4p-3,
      .rate_remainder = 0x1.44c88e8c3b73ep-31,
    },
    .position_error_deg = 0x1.42b123723b901p-25,
    .rate_error_deg_per_day = 0x1.27cd15ae225c3p-21,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // JUPITER
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.a0bb50a9036d2p-2, .second_derivative = 0x1.117e96468c92fp-7 },
      .rate = { .first_derivative = 0x1.a35bb338f9ae1p-7, .second_derivative = 0x1.d7a3f1e922040p-12 },
      .third_derivative = 0x1.3397713effec6p-12,
      .rate_remainder = 0x1.f98cd55e0580cp-44,
    },
    .position_error_deg = 0x1.18bff804dfd0ap-30,
    .rate_error_deg_per_day = 0x1.015bedc9cc4e2p-26,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // SATURN
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.7e11cb8af1988p-3, .second_derivative = 0x1.a784943fb3716p-9 },
      .rate = { .first_derivative = 0x1.44b271a84dd6fp-8, .second_derivative = 0x1.4dcd04e03b6d9p-13 },
      .third_derivative = 0x1.b36475aa0abb7p-14,
      .rate_remainder = 0x1.d5881da5be9b6p-44,
    },
    .position_error_deg = 0x1.946d25dfcc42cp-31,
    .rate_error_deg_per_day = 0x1.72bbf71e7ef49p-27,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // URANUS
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.3e86df49d1687p-4, .second_derivative = 0x1.5857f5cbcafe9p-10 },
      .rate = { .first_derivative = 0x1.07ff2b6082077p-9, .second_derivative = 0x1.2f32396123c6cp-14 },
      .third_derivative = 0x1.8b79297392d6cp-15,
      .rate_remainder = 0x1.c44d6ae99fba1p-44,
    },
    .position_error_deg = 0x1.f1c7fc45cd3cbp-33,
    .rate_error_deg_per_day = 0x1.c856f7eb316dcp-29,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // NEPTUNE
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.6699c24703ef7p-5, .second_derivative = 0x1.922d87088604cp-11 },
      .rate = { .first_derivative = 0x1.34561ab9bc14cp-10, .second_derivative = 0x1.a3d87d0a16cb0p-15 },
      .third_derivative = 0x1.11cff88103bc1p-15,
      .rate_remainder = 0x1.bea592cb2f68bp-44,
    },
    .position_error_deg = 0x1.f73666176460fp-34,
    .rate_error_deg_per_day = 0x1.cd5bd3ab86207p-30,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // PLUTO
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.0a04fcb2ac2ccp-4, .second_derivative = 0x1.0d4d064853cfep-10 },
      .rate = { .first_derivative = 0x1.9ced922aa2a52p-10, .second_derivative = 0x1.1933ab74d3e9ep-14 },
      .third_derivative = 0x1.6ec8f5e64c0fbp-15,
      .rate_remainder = 0x1.cc07beda7194fp-44,
    },
    .position_error_deg = 0x1.4967bbccea30ap-33,
    .rate_error_deg_per_day = 0x1.2dfebcb57d9f3p-29,
    .minimum_rate = std::nullopt,
    .maximum_rate = std::nullopt,
  },
  // MEAN_ASCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.b267268503595p-5, .second_derivative = 0x1.c6313ac72a979p-16 },
      .rate = { .first_derivative = 0x1.5c36cf324b521p-15, .second_derivative = 0x1.7151bd1378ab1p-16 },
      .third_derivative = 0x1.e1b887510cb29p-17,
      .rate_remainder = 0x1.b64ecdedef973p-44,
    },
    .position_error_deg = 0x1.041fc24d8bea8p-39,
    .rate_error_deg_per_day = 0x1.e08fc9fe04095p-36,
    .minimum_rate = std::optional<double> { -0x1.b2672688c7e58p-5 },
    .maximum_rate = std::optional<double> { -0x1.b1310a585d610p-5 },
  },
  // MEAN_DESCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.b267268503595p-5, .second_derivative = 0x1.c6313ac72a979p-16 },
      .rate = { .first_derivative = 0x1.5c36cf324b521p-15, .second_derivative = 0x1.7151bd1378ab1p-16 },
      .third_derivative = 0x1.e1b887510cb29p-17,
      .rate_remainder = 0x1.b64ecdedef973p-44,
    },
    .position_error_deg = 0x1.101fc2f6dd380p-39,
    .rate_error_deg_per_day = 0x1.e08fc9fe04095p-36,
    .minimum_rate = std::optional<double> { -0x1.b2672688c7e58p-5 },
    .maximum_rate = std::optional<double> { -0x1.b1310a585d610p-5 },
  },
  // TRUE_ASCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.bc1bdb5e2991dp-3, .second_derivative = 0x1.940b7f709c8b0p-5 },
      .rate = { .first_derivative = 0x1.35c48c5ede6a9p-4, .second_derivative = 0x1.09391d5c043a7p-5 },
      .third_derivative = 0x1.59f174353d2adp-6,
      .rate_remainder = 0x1.92a8885a22dc5p-36,
    },
    .position_error_deg = 0x1.86e01ccca74bdp-35,
    .rate_error_deg_per_day = 0x1.666afa9c0c56ap-31,
    .minimum_rate = std::optional<double> { -0x1.bc1bdb755995cp-3 },
    .maximum_rate = std::optional<double> { 0x1.c66b9e7a20883p-4 },
  },
  // TRUE_DESCENDING
  {
    .trajectory = {
      .longitude = { .first_derivative = 0x1.bc1bdb5e2991dp-3, .second_derivative = 0x1.940b7f709c8b0p-5 },
      .rate = { .first_derivative = 0x1.35c48c5ede6a9p-4, .second_derivative = 0x1.09391d5c043a7p-5 },
      .third_derivative = 0x1.59f174353d2adp-6,
      .rate_remainder = 0x1.92a8885a22dc5p-36,
    },
    .position_error_deg = 0x1.87a01cb9b8975p-35,
    .rate_error_deg_per_day = 0x1.666afa9c0c56ap-31,
    .minimum_rate = std::optional<double> { -0x1.bc1bdb755995cp-3 },
    .maximum_rate = std::optional<double> { 0x1.c66b9e7a20883p-4 },
  },
}};

} // namespace astro::ephemeris::detail
