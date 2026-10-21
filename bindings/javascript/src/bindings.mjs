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

const binding = (cName, result, readsLastError = false) => Object.freeze({
  cName,
  wasmName: `_${cName}`,
  result,
  readsLastError,
});

const deepFreeze = (value) => {
  for (const child of Object.values(value)) {
    if (child !== null && typeof child === "object") deepFreeze(child);
  }
  return Object.freeze(value);
};

export const CHART_CODES = deepFreeze({
  scales: { utc: 0, ut1: 1 },
  models: { default: 0, algo1: 1, algo2: 2, algo3: 3, algo4: 4, algo5: 5 },
  systems: { equal: 0, wholeSign: 1, placidus: 2 },
  present: { latitude: 1, distance: 2 },
  targets: {
    sun: 0, moon: 1, mercury: 2, venus: 3, mars: 4,
    jupiter: 5, saturn: 6, uranus: 7, neptune: 8, pluto: 9,
    meanAscending: 10, meanDescending: 11, trueAscending: 12, trueDescending: 13,
  },
});

// The runtime decoder reads these offsets after every native call. Keeping the table here,
// rather than in the public wrapper, lets the ABI verifier compare it directly with the C header.
export const LAYOUTS = deepFreeze({
  JulianDay: {
    size: 16,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "value", type: "double", offset: 8 },
    ],
  },
  UT1Time: {
    size: 24,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "year", type: "int32_t", offset: 4 },
      { name: "month", type: "uint32_t", offset: 8 },
      { name: "day", type: "uint32_t", offset: 12 },
      { name: "fraction", type: "double", offset: 16 },
    ],
  },
  SunCoordinate: {
    size: 32,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "lon", type: "double", offset: 8 },
      { name: "lat", type: "double", offset: 16 },
      { name: "r", type: "double", offset: 24 },
    ],
  },
  MoonCoordinate: {
    size: 32,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "lon", type: "double", offset: 8 },
      { name: "lat", type: "double", offset: 16 },
      { name: "r", type: "double", offset: 24 },
    ],
  },
  MoonIllumination: {
    size: 24,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "illumination", type: "double", offset: 8 },
      { name: "elongation_deg", type: "double", offset: 16 },
    ],
  },
  MoonPositionAngle: {
    size: 16,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "angle_deg", type: "double", offset: 8 },
    ],
  },
  Discriminant: {
    size: 8,
    alignment: 4,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "count", type: "uint32_t", offset: 4 },
    ],
  },
  EquationOfTime: {
    size: 16,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "value", type: "double", offset: 8 },
    ],
  },
  ApparentSolarTime: {
    size: 24,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "year", type: "int32_t", offset: 4 },
      { name: "month", type: "uint32_t", offset: 8 },
      { name: "day", type: "uint32_t", offset: 12 },
      { name: "fraction", type: "double", offset: 16 },
    ],
  },
  SiderealTime: {
    size: 16,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "value", type: "double", offset: 8 },
    ],
  },
  JieqiMomentQuery: {
    size: 24,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "jq_idx", type: "uint8_t", offset: 1 },
      { name: "y", type: "int32_t", offset: 4 },
      { name: "m", type: "uint32_t", offset: 8 },
      { name: "d", type: "uint32_t", offset: 12 },
      { name: "frac", type: "double", offset: 16 },
    ],
  },
  SupportedLunarYearRange: {
    size: 12,
    alignment: 4,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "start", type: "int32_t", offset: 4 },
      { name: "end", type: "int32_t", offset: 8 },
    ],
  },
  LunarYearInfo: {
    size: 16,
    alignment: 4,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "year", type: "int32_t", offset: 4 },
      { name: "month", type: "uint8_t", offset: 8 },
      { name: "day", type: "uint8_t", offset: 9 },
      { name: "leap_month", type: "uint8_t", offset: 10 },
      { name: "month_len", type: "uint16_t", offset: 12 },
    ],
  },
  LunarDate: {
    size: 12,
    alignment: 4,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "year", type: "int32_t", offset: 4 },
      { name: "month", type: "uint8_t", offset: 8 },
      { name: "is_leap", type: "bool", offset: 9 },
      { name: "day", type: "uint8_t", offset: 10 },
    ],
  },
  GregorianDate: {
    size: 12,
    alignment: 4,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "year", type: "int32_t", offset: 4 },
      { name: "month", type: "uint8_t", offset: 8 },
      { name: "day", type: "uint8_t", offset: 9 },
    ],
  },
  DeltaT: {
    size: 16,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "value", type: "double", offset: 8 },
    ],
  },
  ChartBodyV1: {
    size: 40,
    alignment: 8,
    fields: [
      { name: "target_code", type: "uint32_t", offset: 0 },
      { name: "present_fields", type: "uint32_t", offset: 4 },
      { name: "longitude_deg", type: "double", offset: 8 },
      { name: "latitude_deg", type: "double", offset: 16 },
      { name: "distance_au", type: "double", offset: 24 },
      { name: "longitude_rate_deg_per_tt_day", type: "double", offset: 32 },
    ],
  },
  ChartHousesV1: {
    size: 128,
    alignment: 8,
    fields: [
      { name: "ascendant_deg", type: "double", offset: 0 },
      { name: "midheaven_deg", type: "double", offset: 8 },
      { name: "descendant_deg", type: "double", offset: 16 },
      { name: "imum_coeli_deg", type: "double", offset: 24 },
      { name: "cusps_deg", type: "double[12]", offset: 32 },
    ],
  },
  ChartSnapshotV1: {
    size: 712,
    alignment: 8,
    fields: [
      { name: "valid", type: "bool", offset: 0 },
      { name: "jd_ut1", type: "double", offset: 8 },
      { name: "jde_tt", type: "double", offset: 16 },
      { name: "bodies", type: "ChartBodyV1[14]", offset: 24 },
      { name: "houses", type: "ChartHousesV1", offset: 584 },
    ],
  },
  LongitudeEventRecord: {
    size: 32,
    alignment: 8,
    fields: [
      { name: "jde_tt", type: "double", offset: 0 },
      { name: "longitude_deg", type: "double", offset: 8 },
      { name: "longitude_rate_deg_per_tt_day", type: "double", offset: 16 },
      { name: "direction_code", type: "uint32_t", offset: 24 },
    ],
  },
  StationEventRecord: {
    size: 32,
    alignment: 8,
    fields: [
      { name: "jde_tt", type: "double", offset: 0 },
      { name: "longitude_deg", type: "double", offset: 8 },
      { name: "longitude_rate_deg_per_tt_day", type: "double", offset: 16 },
      { name: "motion_before_code", type: "uint32_t", offset: 24 },
      { name: "motion_after_code", type: "uint32_t", offset: 28 },
    ],
  },
  LongitudeDifferenceEventRecord: {
    size: 56,
    alignment: 8,
    fields: [
      { name: "jde_tt", type: "double", offset: 0 },
      { name: "longitude_a_deg", type: "double", offset: 8 },
      { name: "longitude_b_deg", type: "double", offset: 16 },
      { name: "longitude_rate_a_deg_per_tt_day", type: "double", offset: 24 },
      { name: "longitude_rate_b_deg_per_tt_day", type: "double", offset: 32 },
      { name: "signed_difference_deg", type: "double", offset: 40 },
      { name: "direction_code", type: "uint32_t", offset: 48 },
    ],
  },
});

// Internal metadata for the complete celestial.h surface. Package entry points consume
// this table; it is not itself part of the JavaScript package's public exports.
export const BINDINGS = Object.freeze([
  binding("set_log_verbosity", "bool", true),
  binding("last_error", "borrowed-string"),
  binding("ut1_to_jd", "sret:JulianDay", true),
  binding("ut1_to_jde", "sret:JulianDay", true),
  binding("jde_to_ut1", "sret:UT1Time", true),
  binding("sun_apparent_geocentric_coord", "sret:SunCoordinate", true),
  binding("moon_apparent_geocentric_coord", "sret:MoonCoordinate", true),
  binding("moon_illumination", "sret:MoonIllumination", true),
  binding("moon_position_angle", "sret:MoonPositionAngle", true),
  binding("moon_phase_moments", "count-fill", true),
  binding("solar_lon_root_discriminant", "sret:Discriminant", true),
  binding("solar_lon_roots", "companion-fill", true),
  binding("new_moons_after_jde", "requested-fill", true),
  binding("new_moons_in_year", "count-fill", true),
  binding("equation_of_time", "sret:EquationOfTime", true),
  binding("apparent_solar_time", "sret:ApparentSolarTime", true),
  binding("local_apparent_sidereal_time", "sret:SiderealTime", true),
  binding("query_jieqi_moment", "sret:JieqiMomentQuery", true),
  binding("get_jieqi_name", "caller-string", true),
  binding("get_supported_lunar_year_range", "sret:SupportedLunarYearRange", true),
  binding("get_lunar_year_info", "sret:LunarYearInfo", true),
  binding("gregorian_to_lunar", "sret:LunarDate", true),
  binding("lunar_to_gregorian", "sret:GregorianDate", true),
  binding("delta_t_algo1", "sret:DeltaT", true),
  binding("delta_t_algo2", "sret:DeltaT", true),
  binding("delta_t_algo3", "sret:DeltaT", true),
  binding("delta_t_algo4", "sret:DeltaT", true),
  binding("delta_t_algo5", "sret:DeltaT", true),
  binding("delta_t", "sret:DeltaT", true),
  binding("chart_snapshot_v1", "sret:ChartSnapshotV1", true),
  binding("longitude_events", "count-fill", true),
  binding("longitude_stations", "count-fill", true),
  binding("longitude_difference_events", "count-fill", true),
  binding("celestial_capabilities", "count-fill", true),
  binding("celestial_model_identities", "count-fill", true),
  binding("celestial_capability_text", "caller-string", true),
]);
