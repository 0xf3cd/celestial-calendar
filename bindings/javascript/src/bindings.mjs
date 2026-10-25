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

// Regenerate with python3 toolbox/make_bindings.py.
const deepFreeze = (value) => {
  for (const child of Object.values(value)) {
    if (child !== null && typeof child === "object") deepFreeze(child);
  }
  return Object.freeze(value);
};

export const CHART_CODES = deepFreeze({
  "scales": {
    "utc": 0,
    "ut1": 1
  },
  "models": {
    "default": 0,
    "algo1": 1,
    "algo2": 2,
    "algo3": 3,
    "algo4": 4,
    "algo5": 5
  },
  "systems": {
    "equal": 0,
    "wholeSign": 1,
    "placidus": 2
  },
  "present": {
    "latitude": 1,
    "distance": 2
  },
  "targets": {
    "sun": 0,
    "moon": 1,
    "mercury": 2,
    "venus": 3,
    "mars": 4,
    "jupiter": 5,
    "saturn": 6,
    "uranus": 7,
    "neptune": 8,
    "pluto": 9,
    "meanAscending": 10,
    "meanDescending": 11,
    "trueAscending": 12,
    "trueDescending": 13
  }
});

export const LAYOUTS = deepFreeze({
  "CelestialJulianDay": {
    "size": 16,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "value",
        "type": "double",
        "offset": 8
      }
    ]
  },
  "CelestialUT1Time": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "year",
        "type": "int32_t",
        "offset": 4
      },
      {
        "name": "month",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "day",
        "type": "uint32_t",
        "offset": 12
      },
      {
        "name": "fraction",
        "type": "double",
        "offset": 16
      }
    ]
  },
  "CelestialSunCoordinate": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "lon",
        "type": "double",
        "offset": 8
      },
      {
        "name": "lat",
        "type": "double",
        "offset": 16
      },
      {
        "name": "r",
        "type": "double",
        "offset": 24
      }
    ]
  },
  "CelestialMoonCoordinate": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "lon",
        "type": "double",
        "offset": 8
      },
      {
        "name": "lat",
        "type": "double",
        "offset": 16
      },
      {
        "name": "r",
        "type": "double",
        "offset": 24
      }
    ]
  },
  "CelestialMoonIllumination": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "illumination",
        "type": "double",
        "offset": 8
      },
      {
        "name": "elongation_deg",
        "type": "double",
        "offset": 16
      }
    ]
  },
  "CelestialMoonPositionAngle": {
    "size": 16,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "angle_deg",
        "type": "double",
        "offset": 8
      }
    ]
  },
  "CelestialDiscriminant": {
    "size": 8,
    "alignment": 4,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "count",
        "type": "uint32_t",
        "offset": 4
      }
    ]
  },
  "CelestialEquationOfTime": {
    "size": 16,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "value",
        "type": "double",
        "offset": 8
      }
    ]
  },
  "CelestialApparentSolarTime": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "year",
        "type": "int32_t",
        "offset": 4
      },
      {
        "name": "month",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "day",
        "type": "uint32_t",
        "offset": 12
      },
      {
        "name": "fraction",
        "type": "double",
        "offset": 16
      }
    ]
  },
  "CelestialSiderealTime": {
    "size": 16,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "value",
        "type": "double",
        "offset": 8
      }
    ]
  },
  "CelestialJieqiMomentQuery": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "jq_idx",
        "type": "uint8_t",
        "offset": 1
      },
      {
        "name": "y",
        "type": "int32_t",
        "offset": 4
      },
      {
        "name": "m",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "d",
        "type": "uint32_t",
        "offset": 12
      },
      {
        "name": "frac",
        "type": "double",
        "offset": 16
      }
    ]
  },
  "CelestialSupportedLunarYearRange": {
    "size": 12,
    "alignment": 4,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "start",
        "type": "int32_t",
        "offset": 4
      },
      {
        "name": "end",
        "type": "int32_t",
        "offset": 8
      }
    ]
  },
  "CelestialLunarYearInfo": {
    "size": 16,
    "alignment": 4,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "year",
        "type": "int32_t",
        "offset": 4
      },
      {
        "name": "month",
        "type": "uint8_t",
        "offset": 8
      },
      {
        "name": "day",
        "type": "uint8_t",
        "offset": 9
      },
      {
        "name": "leap_month",
        "type": "uint8_t",
        "offset": 10
      },
      {
        "name": "month_len",
        "type": "uint16_t",
        "offset": 12
      }
    ]
  },
  "CelestialLunarDate": {
    "size": 12,
    "alignment": 4,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "year",
        "type": "int32_t",
        "offset": 4
      },
      {
        "name": "month",
        "type": "uint8_t",
        "offset": 8
      },
      {
        "name": "is_leap",
        "type": "bool",
        "offset": 9
      },
      {
        "name": "day",
        "type": "uint8_t",
        "offset": 10
      }
    ]
  },
  "CelestialGregorianDate": {
    "size": 12,
    "alignment": 4,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "year",
        "type": "int32_t",
        "offset": 4
      },
      {
        "name": "month",
        "type": "uint8_t",
        "offset": 8
      },
      {
        "name": "day",
        "type": "uint8_t",
        "offset": 9
      }
    ]
  },
  "CelestialDeltaT": {
    "size": 16,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "model_identity_code",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "value",
        "type": "double",
        "offset": 8
      }
    ]
  },
  "CelestialChartBody": {
    "size": 48,
    "alignment": 8,
    "fields": [
      {
        "name": "target_code",
        "type": "uint32_t",
        "offset": 0
      },
      {
        "name": "source_code",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "present_fields",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "longitude_deg",
        "type": "double",
        "offset": 16
      },
      {
        "name": "latitude_deg",
        "type": "double",
        "offset": 24
      },
      {
        "name": "distance_au",
        "type": "double",
        "offset": 32
      },
      {
        "name": "longitude_rate_deg_per_tt_day",
        "type": "double",
        "offset": 40
      }
    ]
  },
  "CelestialChartHouses": {
    "size": 128,
    "alignment": 8,
    "fields": [
      {
        "name": "ascendant_deg",
        "type": "double",
        "offset": 0
      },
      {
        "name": "midheaven_deg",
        "type": "double",
        "offset": 8
      },
      {
        "name": "descendant_deg",
        "type": "double",
        "offset": 16
      },
      {
        "name": "imum_coeli_deg",
        "type": "double",
        "offset": 24
      },
      {
        "name": "cusps_deg",
        "type": "double[12]",
        "offset": 32
      }
    ]
  },
  "CelestialChartSnapshot": {
    "size": 832,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "delta_t_identity_code",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "nutation_identity_code",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "jd_ut1",
        "type": "double",
        "offset": 16
      },
      {
        "name": "jde_tt",
        "type": "double",
        "offset": 24
      },
      {
        "name": "bodies",
        "type": "CelestialChartBody[14]",
        "offset": 32
      },
      {
        "name": "houses",
        "type": "CelestialChartHouses",
        "offset": 704
      }
    ]
  },
  "CelestialLongitudeEventRecord": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {
        "name": "jde_tt",
        "type": "double",
        "offset": 0
      },
      {
        "name": "longitude_deg",
        "type": "double",
        "offset": 8
      },
      {
        "name": "longitude_rate_deg_per_tt_day",
        "type": "double",
        "offset": 16
      },
      {
        "name": "direction_code",
        "type": "uint32_t",
        "offset": 24
      }
    ]
  },
  "CelestialStationEventRecord": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {
        "name": "jde_tt",
        "type": "double",
        "offset": 0
      },
      {
        "name": "longitude_deg",
        "type": "double",
        "offset": 8
      },
      {
        "name": "longitude_rate_deg_per_tt_day",
        "type": "double",
        "offset": 16
      },
      {
        "name": "motion_before_code",
        "type": "uint32_t",
        "offset": 24
      },
      {
        "name": "motion_after_code",
        "type": "uint32_t",
        "offset": 28
      }
    ]
  },
  "CelestialLongitudeDifferenceEventRecord": {
    "size": 56,
    "alignment": 8,
    "fields": [
      {
        "name": "jde_tt",
        "type": "double",
        "offset": 0
      },
      {
        "name": "longitude_a_deg",
        "type": "double",
        "offset": 8
      },
      {
        "name": "longitude_b_deg",
        "type": "double",
        "offset": 16
      },
      {
        "name": "longitude_rate_a_deg_per_tt_day",
        "type": "double",
        "offset": 24
      },
      {
        "name": "longitude_rate_b_deg_per_tt_day",
        "type": "double",
        "offset": 32
      },
      {
        "name": "signed_difference_deg",
        "type": "double",
        "offset": 40
      },
      {
        "name": "direction_code",
        "type": "uint32_t",
        "offset": 48
      }
    ]
  },
  "CelestialCapabilityRecord": {
    "size": 40,
    "alignment": 8,
    "fields": [
      {
        "name": "operation_code",
        "type": "uint32_t",
        "offset": 0
      },
      {
        "name": "subject_code",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "status_code",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "source_code",
        "type": "uint32_t",
        "offset": 12
      },
      {
        "name": "domain_start_jde_tt",
        "type": "double",
        "offset": 16
      },
      {
        "name": "domain_end_jde_tt",
        "type": "double",
        "offset": 24
      },
      {
        "name": "maximum_span_tt_days",
        "type": "uint32_t",
        "offset": 32
      }
    ]
  },
  "CelestialModelIdentityRecord": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {
        "name": "kind_code",
        "type": "uint32_t",
        "offset": 0
      },
      {
        "name": "identity_code",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "status_code",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "domain_start_jde_tt",
        "type": "double",
        "offset": 16
      },
      {
        "name": "domain_end_jde_tt",
        "type": "double",
        "offset": 24
      }
    ]
  },
  "CelestialPositionRecord": {
    "size": 40,
    "alignment": 8,
    "fields": [
      {
        "name": "target_code",
        "type": "uint32_t",
        "offset": 0
      },
      {
        "name": "present_fields",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "longitude_deg",
        "type": "double",
        "offset": 8
      },
      {
        "name": "latitude_deg",
        "type": "double",
        "offset": 16
      },
      {
        "name": "distance_au",
        "type": "double",
        "offset": 24
      },
      {
        "name": "source_code",
        "type": "uint32_t",
        "offset": 32
      }
    ]
  },
  "CelestialEquatorialPositionRecord": {
    "size": 40,
    "alignment": 8,
    "fields": [
      {
        "name": "target_code",
        "type": "uint32_t",
        "offset": 0
      },
      {
        "name": "present_fields",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "right_ascension_deg",
        "type": "double",
        "offset": 8
      },
      {
        "name": "declination_deg",
        "type": "double",
        "offset": 16
      },
      {
        "name": "distance_au",
        "type": "double",
        "offset": 24
      },
      {
        "name": "source_code",
        "type": "uint32_t",
        "offset": 32
      }
    ]
  },
  "CelestialHouseQueryResult": {
    "size": 160,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "house_system_code",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "delta_t_identity_code",
        "type": "uint32_t",
        "offset": 8
      },
      {
        "name": "nutation_identity_code",
        "type": "uint32_t",
        "offset": 12
      },
      {
        "name": "jd_ut1",
        "type": "double",
        "offset": 16
      },
      {
        "name": "jde_tt",
        "type": "double",
        "offset": 24
      },
      {
        "name": "houses",
        "type": "CelestialChartHouses",
        "offset": 32
      }
    ]
  },
  "CelestialCivilHouseCapability": {
    "size": 48,
    "alignment": 8,
    "fields": [
      {
        "name": "valid",
        "type": "bool",
        "offset": 0
      },
      {
        "name": "status_code",
        "type": "uint32_t",
        "offset": 4
      },
      {
        "name": "civil_start_year",
        "type": "int32_t",
        "offset": 8
      },
      {
        "name": "civil_end_year_exclusive",
        "type": "int32_t",
        "offset": 12
      },
      {
        "name": "civil_scale_mask",
        "type": "uint32_t",
        "offset": 16
      },
      {
        "name": "utc_start_year",
        "type": "int32_t",
        "offset": 20
      },
      {
        "name": "utc_start_month",
        "type": "uint32_t",
        "offset": 24
      },
      {
        "name": "utc_start_day",
        "type": "uint32_t",
        "offset": 28
      },
      {
        "name": "delta_t_identity_code",
        "type": "uint32_t",
        "offset": 32
      },
      {
        "name": "has_derived_tt_cutoff",
        "type": "bool",
        "offset": 36
      },
      {
        "name": "derived_tt_end_jde_tt",
        "type": "double",
        "offset": 40
      }
    ]
  }
});

export const BINDINGS = deepFreeze([
  {
    "cName": "celestial_last_error",
    "wasmName": "_celestial_last_error",
    "result": "borrowed-string",
    "readsLastError": false
  },
  {
    "cName": "celestial_last_error_kind",
    "wasmName": "_celestial_last_error_kind",
    "result": "uint32_t",
    "readsLastError": false
  },
  {
    "cName": "celestial_ut1_to_jd",
    "wasmName": "_celestial_ut1_to_jd",
    "result": "sret:CelestialJulianDay",
    "readsLastError": true
  },
  {
    "cName": "celestial_ut1_to_jde",
    "wasmName": "_celestial_ut1_to_jde",
    "result": "sret:CelestialJulianDay",
    "readsLastError": true
  },
  {
    "cName": "celestial_jde_to_ut1",
    "wasmName": "_celestial_jde_to_ut1",
    "result": "sret:CelestialUT1Time",
    "readsLastError": true
  },
  {
    "cName": "celestial_sun_apparent_geocentric_coord",
    "wasmName": "_celestial_sun_apparent_geocentric_coord",
    "result": "sret:CelestialSunCoordinate",
    "readsLastError": true
  },
  {
    "cName": "celestial_moon_apparent_geocentric_coord",
    "wasmName": "_celestial_moon_apparent_geocentric_coord",
    "result": "sret:CelestialMoonCoordinate",
    "readsLastError": true
  },
  {
    "cName": "celestial_moon_illumination",
    "wasmName": "_celestial_moon_illumination",
    "result": "sret:CelestialMoonIllumination",
    "readsLastError": true
  },
  {
    "cName": "celestial_moon_position_angle",
    "wasmName": "_celestial_moon_position_angle",
    "result": "sret:CelestialMoonPositionAngle",
    "readsLastError": true
  },
  {
    "cName": "celestial_moon_phase_moments",
    "wasmName": "_celestial_moon_phase_moments",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_solar_lon_root_discriminant",
    "wasmName": "_celestial_solar_lon_root_discriminant",
    "result": "sret:CelestialDiscriminant",
    "readsLastError": true
  },
  {
    "cName": "celestial_solar_lon_roots",
    "wasmName": "_celestial_solar_lon_roots",
    "result": "companion-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_new_moons_after_jde",
    "wasmName": "_celestial_new_moons_after_jde",
    "result": "requested-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_new_moons_in_year",
    "wasmName": "_celestial_new_moons_in_year",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_equation_of_time",
    "wasmName": "_celestial_equation_of_time",
    "result": "sret:CelestialEquationOfTime",
    "readsLastError": true
  },
  {
    "cName": "celestial_apparent_solar_time",
    "wasmName": "_celestial_apparent_solar_time",
    "result": "sret:CelestialApparentSolarTime",
    "readsLastError": true
  },
  {
    "cName": "celestial_local_apparent_sidereal_time",
    "wasmName": "_celestial_local_apparent_sidereal_time",
    "result": "sret:CelestialSiderealTime",
    "readsLastError": true
  },
  {
    "cName": "celestial_query_jieqi_moment",
    "wasmName": "_celestial_query_jieqi_moment",
    "result": "sret:CelestialJieqiMomentQuery",
    "readsLastError": true
  },
  {
    "cName": "celestial_get_jieqi_name",
    "wasmName": "_celestial_get_jieqi_name",
    "result": "sized-string",
    "readsLastError": true
  },
  {
    "cName": "celestial_get_supported_lunar_year_range",
    "wasmName": "_celestial_get_supported_lunar_year_range",
    "result": "sret:CelestialSupportedLunarYearRange",
    "readsLastError": true
  },
  {
    "cName": "celestial_get_lunar_year_info",
    "wasmName": "_celestial_get_lunar_year_info",
    "result": "sret:CelestialLunarYearInfo",
    "readsLastError": true
  },
  {
    "cName": "celestial_gregorian_to_lunar",
    "wasmName": "_celestial_gregorian_to_lunar",
    "result": "sret:CelestialLunarDate",
    "readsLastError": true
  },
  {
    "cName": "celestial_lunar_to_gregorian",
    "wasmName": "_celestial_lunar_to_gregorian",
    "result": "sret:CelestialGregorianDate",
    "readsLastError": true
  },
  {
    "cName": "celestial_delta_t",
    "wasmName": "_celestial_delta_t",
    "result": "sret:CelestialDeltaT",
    "readsLastError": true
  },
  {
    "cName": "celestial_chart_snapshot",
    "wasmName": "_celestial_chart_snapshot",
    "result": "sret:CelestialChartSnapshot",
    "readsLastError": true
  },
  {
    "cName": "celestial_longitude_events",
    "wasmName": "_celestial_longitude_events",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_longitude_stations",
    "wasmName": "_celestial_longitude_stations",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_longitude_difference_events",
    "wasmName": "_celestial_longitude_difference_events",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_capabilities",
    "wasmName": "_celestial_capabilities",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_model_identities",
    "wasmName": "_celestial_model_identities",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_model_identity_text",
    "wasmName": "_celestial_model_identity_text",
    "result": "sized-string",
    "readsLastError": true
  },
  {
    "cName": "celestial_capability_text",
    "wasmName": "_celestial_capability_text",
    "result": "sized-string",
    "readsLastError": true
  },
  {
    "cName": "celestial_positions",
    "wasmName": "_celestial_positions",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_equatorial_positions",
    "wasmName": "_celestial_equatorial_positions",
    "result": "count-fill",
    "readsLastError": true
  },
  {
    "cName": "celestial_house_query",
    "wasmName": "_celestial_house_query",
    "result": "sret:CelestialHouseQueryResult",
    "readsLastError": true
  },
  {
    "cName": "celestial_house_capability",
    "wasmName": "_celestial_house_capability",
    "result": "sret:CelestialCivilHouseCapability",
    "readsLastError": true
  }
]);

export const C_DECLARATIONS = deepFreeze({
  "source_sha256": "a0341a44c9a188ebff19350abd5a906512a47ff0f59670f1576f675acc0a09d4",
  "functions": {
    "celestial_last_error": {
      "signature": "const char *celestial_last_error(void)",
      "result": "const char *",
      "parameters": []
    },
    "celestial_last_error_kind": {
      "signature": "uint32_t celestial_last_error_kind(void)",
      "result": "uint32_t",
      "parameters": []
    },
    "celestial_ut1_to_jd": {
      "signature": "CelestialJulianDay celestial_ut1_to_jd(int32_t y, uint32_t m, uint32_t d, double fraction)",
      "result": "CelestialJulianDay",
      "parameters": [
        {
          "name": "y",
          "type": "int32_t"
        },
        {
          "name": "m",
          "type": "uint32_t"
        },
        {
          "name": "d",
          "type": "uint32_t"
        },
        {
          "name": "fraction",
          "type": "double"
        }
      ]
    },
    "celestial_ut1_to_jde": {
      "signature": "CelestialJulianDay celestial_ut1_to_jde(int32_t y, uint32_t m, uint32_t d, double fraction)",
      "result": "CelestialJulianDay",
      "parameters": [
        {
          "name": "y",
          "type": "int32_t"
        },
        {
          "name": "m",
          "type": "uint32_t"
        },
        {
          "name": "d",
          "type": "uint32_t"
        },
        {
          "name": "fraction",
          "type": "double"
        }
      ]
    },
    "celestial_jde_to_ut1": {
      "signature": "CelestialUT1Time celestial_jde_to_ut1(double jde)",
      "result": "CelestialUT1Time",
      "parameters": [
        {
          "name": "jde",
          "type": "double"
        }
      ]
    },
    "celestial_sun_apparent_geocentric_coord": {
      "signature": "CelestialSunCoordinate celestial_sun_apparent_geocentric_coord(double jde)",
      "result": "CelestialSunCoordinate",
      "parameters": [
        {
          "name": "jde",
          "type": "double"
        }
      ]
    },
    "celestial_moon_apparent_geocentric_coord": {
      "signature": "CelestialMoonCoordinate celestial_moon_apparent_geocentric_coord(double jde)",
      "result": "CelestialMoonCoordinate",
      "parameters": [
        {
          "name": "jde",
          "type": "double"
        }
      ]
    },
    "celestial_moon_illumination": {
      "signature": "CelestialMoonIllumination celestial_moon_illumination(double jde)",
      "result": "CelestialMoonIllumination",
      "parameters": [
        {
          "name": "jde",
          "type": "double"
        }
      ]
    },
    "celestial_moon_position_angle": {
      "signature": "CelestialMoonPositionAngle celestial_moon_position_angle(double jde)",
      "result": "CelestialMoonPositionAngle",
      "parameters": [
        {
          "name": "jde",
          "type": "double"
        }
      ]
    },
    "celestial_moon_phase_moments": {
      "signature": "uint32_t celestial_moon_phase_moments(int32_t year, uint8_t phase_kind, uint32_t *root_count, double *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "phase_kind",
          "type": "uint8_t"
        },
        {
          "name": "root_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "double *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_solar_lon_root_discriminant": {
      "signature": "CelestialDiscriminant celestial_solar_lon_root_discriminant(int32_t year, double longitude)",
      "result": "CelestialDiscriminant",
      "parameters": [
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "longitude",
          "type": "double"
        }
      ]
    },
    "celestial_solar_lon_roots": {
      "signature": "uint32_t celestial_solar_lon_roots(int32_t year, double longitude, double *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "longitude",
          "type": "double"
        },
        {
          "name": "slots",
          "type": "double *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_new_moons_after_jde": {
      "signature": "uint32_t celestial_new_moons_after_jde(double jde, double *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "jde",
          "type": "double"
        },
        {
          "name": "slots",
          "type": "double *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_new_moons_in_year": {
      "signature": "uint32_t celestial_new_moons_in_year(int32_t year, uint32_t *root_count, double *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "root_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "double *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_equation_of_time": {
      "signature": "CelestialEquationOfTime celestial_equation_of_time(double jde)",
      "result": "CelestialEquationOfTime",
      "parameters": [
        {
          "name": "jde",
          "type": "double"
        }
      ]
    },
    "celestial_apparent_solar_time": {
      "signature": "CelestialApparentSolarTime celestial_apparent_solar_time(int32_t y, uint32_t m, uint32_t d, double fraction, double longitude)",
      "result": "CelestialApparentSolarTime",
      "parameters": [
        {
          "name": "y",
          "type": "int32_t"
        },
        {
          "name": "m",
          "type": "uint32_t"
        },
        {
          "name": "d",
          "type": "uint32_t"
        },
        {
          "name": "fraction",
          "type": "double"
        },
        {
          "name": "longitude",
          "type": "double"
        }
      ]
    },
    "celestial_local_apparent_sidereal_time": {
      "signature": "CelestialSiderealTime celestial_local_apparent_sidereal_time(double jd_ut1, double longitude)",
      "result": "CelestialSiderealTime",
      "parameters": [
        {
          "name": "jd_ut1",
          "type": "double"
        },
        {
          "name": "longitude",
          "type": "double"
        }
      ]
    },
    "celestial_query_jieqi_moment": {
      "signature": "CelestialJieqiMomentQuery celestial_query_jieqi_moment(int32_t year, uint8_t jq_idx)",
      "result": "CelestialJieqiMomentQuery",
      "parameters": [
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "jq_idx",
          "type": "uint8_t"
        }
      ]
    },
    "celestial_get_jieqi_name": {
      "signature": "uint32_t celestial_get_jieqi_name(uint8_t jq_idx, char *buf, uint32_t buf_size)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "jq_idx",
          "type": "uint8_t"
        },
        {
          "name": "buf",
          "type": "char *"
        },
        {
          "name": "buf_size",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_get_supported_lunar_year_range": {
      "signature": "CelestialSupportedLunarYearRange celestial_get_supported_lunar_year_range(uint8_t algo)",
      "result": "CelestialSupportedLunarYearRange",
      "parameters": [
        {
          "name": "algo",
          "type": "uint8_t"
        }
      ]
    },
    "celestial_get_lunar_year_info": {
      "signature": "CelestialLunarYearInfo celestial_get_lunar_year_info(uint8_t algo, int32_t year)",
      "result": "CelestialLunarYearInfo",
      "parameters": [
        {
          "name": "algo",
          "type": "uint8_t"
        },
        {
          "name": "year",
          "type": "int32_t"
        }
      ]
    },
    "celestial_gregorian_to_lunar": {
      "signature": "CelestialLunarDate celestial_gregorian_to_lunar(uint8_t algo, int32_t year, uint8_t month, uint8_t day)",
      "result": "CelestialLunarDate",
      "parameters": [
        {
          "name": "algo",
          "type": "uint8_t"
        },
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "month",
          "type": "uint8_t"
        },
        {
          "name": "day",
          "type": "uint8_t"
        }
      ]
    },
    "celestial_lunar_to_gregorian": {
      "signature": "CelestialGregorianDate celestial_lunar_to_gregorian(uint8_t algo, int32_t year, uint8_t month, bool is_leap, uint8_t day)",
      "result": "CelestialGregorianDate",
      "parameters": [
        {
          "name": "algo",
          "type": "uint8_t"
        },
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "month",
          "type": "uint8_t"
        },
        {
          "name": "is_leap",
          "type": "bool"
        },
        {
          "name": "day",
          "type": "uint8_t"
        }
      ]
    },
    "celestial_delta_t": {
      "signature": "CelestialDeltaT celestial_delta_t(double year, uint32_t model_code)",
      "result": "CelestialDeltaT",
      "parameters": [
        {
          "name": "year",
          "type": "double"
        },
        {
          "name": "model_code",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_chart_snapshot": {
      "signature": "CelestialChartSnapshot celestial_chart_snapshot(int32_t year, uint32_t month, uint32_t day, double fraction, uint32_t civil_scale_code, double latitude_deg, double longitude_deg, uint32_t house_system_code, uint32_t delta_t_model_code)",
      "result": "CelestialChartSnapshot",
      "parameters": [
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "month",
          "type": "uint32_t"
        },
        {
          "name": "day",
          "type": "uint32_t"
        },
        {
          "name": "fraction",
          "type": "double"
        },
        {
          "name": "civil_scale_code",
          "type": "uint32_t"
        },
        {
          "name": "latitude_deg",
          "type": "double"
        },
        {
          "name": "longitude_deg",
          "type": "double"
        },
        {
          "name": "house_system_code",
          "type": "uint32_t"
        },
        {
          "name": "delta_t_model_code",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_longitude_events": {
      "signature": "uint32_t celestial_longitude_events(uint32_t target_code, double target_longitude_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, CelestialLongitudeEventRecord *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "target_code",
          "type": "uint32_t"
        },
        {
          "name": "target_longitude_deg",
          "type": "double"
        },
        {
          "name": "start_jde_tt",
          "type": "double"
        },
        {
          "name": "end_jde_tt",
          "type": "double"
        },
        {
          "name": "event_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "CelestialLongitudeEventRecord *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_longitude_stations": {
      "signature": "uint32_t celestial_longitude_stations(uint32_t target_code, double start_jde_tt, double end_jde_tt, uint32_t *event_count, CelestialStationEventRecord *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "target_code",
          "type": "uint32_t"
        },
        {
          "name": "start_jde_tt",
          "type": "double"
        },
        {
          "name": "end_jde_tt",
          "type": "double"
        },
        {
          "name": "event_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "CelestialStationEventRecord *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_longitude_difference_events": {
      "signature": "uint32_t celestial_longitude_difference_events(uint32_t first_target_code, uint32_t second_target_code, double target_difference_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, CelestialLongitudeDifferenceEventRecord *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "first_target_code",
          "type": "uint32_t"
        },
        {
          "name": "second_target_code",
          "type": "uint32_t"
        },
        {
          "name": "target_difference_deg",
          "type": "double"
        },
        {
          "name": "start_jde_tt",
          "type": "double"
        },
        {
          "name": "end_jde_tt",
          "type": "double"
        },
        {
          "name": "event_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "CelestialLongitudeDifferenceEventRecord *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_capabilities": {
      "signature": "uint32_t celestial_capabilities(uint32_t operation_code, uint32_t subject_code, uint32_t *record_count, CelestialCapabilityRecord *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "operation_code",
          "type": "uint32_t"
        },
        {
          "name": "subject_code",
          "type": "uint32_t"
        },
        {
          "name": "record_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "CelestialCapabilityRecord *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_model_identities": {
      "signature": "uint32_t celestial_model_identities(uint32_t kind_code, uint32_t *record_count, CelestialModelIdentityRecord *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "kind_code",
          "type": "uint32_t"
        },
        {
          "name": "record_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "CelestialModelIdentityRecord *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_model_identity_text": {
      "signature": "uint32_t celestial_model_identity_text(uint32_t kind_code, uint32_t identity_code, char *buffer, uint32_t capacity)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "kind_code",
          "type": "uint32_t"
        },
        {
          "name": "identity_code",
          "type": "uint32_t"
        },
        {
          "name": "buffer",
          "type": "char *"
        },
        {
          "name": "capacity",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_capability_text": {
      "signature": "uint32_t celestial_capability_text(uint32_t operation_code, uint32_t subject_code, char *buffer, uint32_t capacity)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "operation_code",
          "type": "uint32_t"
        },
        {
          "name": "subject_code",
          "type": "uint32_t"
        },
        {
          "name": "buffer",
          "type": "char *"
        },
        {
          "name": "capacity",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_positions": {
      "signature": "uint32_t celestial_positions(const uint32_t *target_codes, uint32_t target_count, double jde_tt, uint32_t *record_count, CelestialPositionRecord *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "target_codes",
          "type": "const uint32_t *"
        },
        {
          "name": "target_count",
          "type": "uint32_t"
        },
        {
          "name": "jde_tt",
          "type": "double"
        },
        {
          "name": "record_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "CelestialPositionRecord *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_equatorial_positions": {
      "signature": "uint32_t celestial_equatorial_positions(const uint32_t *target_codes, uint32_t target_count, double jde_tt, uint32_t *record_count, CelestialEquatorialPositionRecord *slots, uint32_t slot_count)",
      "result": "uint32_t",
      "parameters": [
        {
          "name": "target_codes",
          "type": "const uint32_t *"
        },
        {
          "name": "target_count",
          "type": "uint32_t"
        },
        {
          "name": "jde_tt",
          "type": "double"
        },
        {
          "name": "record_count",
          "type": "uint32_t *"
        },
        {
          "name": "slots",
          "type": "CelestialEquatorialPositionRecord *"
        },
        {
          "name": "slot_count",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_house_query": {
      "signature": "CelestialHouseQueryResult celestial_house_query(int32_t year, uint32_t month, uint32_t day, double fraction, uint32_t civil_scale_code, uint32_t delta_t_model_code, uint32_t nutation_model_code, double latitude_deg, double longitude_deg, uint32_t house_system_code)",
      "result": "CelestialHouseQueryResult",
      "parameters": [
        {
          "name": "year",
          "type": "int32_t"
        },
        {
          "name": "month",
          "type": "uint32_t"
        },
        {
          "name": "day",
          "type": "uint32_t"
        },
        {
          "name": "fraction",
          "type": "double"
        },
        {
          "name": "civil_scale_code",
          "type": "uint32_t"
        },
        {
          "name": "delta_t_model_code",
          "type": "uint32_t"
        },
        {
          "name": "nutation_model_code",
          "type": "uint32_t"
        },
        {
          "name": "latitude_deg",
          "type": "double"
        },
        {
          "name": "longitude_deg",
          "type": "double"
        },
        {
          "name": "house_system_code",
          "type": "uint32_t"
        }
      ]
    },
    "celestial_house_capability": {
      "signature": "CelestialCivilHouseCapability celestial_house_capability(uint32_t delta_t_model_code)",
      "result": "CelestialCivilHouseCapability",
      "parameters": [
        {
          "name": "delta_t_model_code",
          "type": "uint32_t"
        }
      ]
    }
  },
  "constants": {
    "CELESTIAL_ERROR_NONE": 0,
    "CELESTIAL_ERROR_INVALID_INPUT": 1,
    "CELESTIAL_ERROR_COMPUTATION": 2,
    "CELESTIAL_ERROR_RESOURCE": 3,
    "CELESTIAL_ERROR_INTERNAL": 4,
    "CELESTIAL_CIVIL_SCALE_UTC": 0,
    "CELESTIAL_CIVIL_SCALE_UT1": 1,
    "CELESTIAL_HOUSE_SYSTEM_EQUAL": 0,
    "CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN": 1,
    "CELESTIAL_HOUSE_SYSTEM_PLACIDUS": 2,
    "CELESTIAL_DELTA_T_MODEL_DEFAULT": 0,
    "CELESTIAL_DELTA_T_MODEL_ALGO1": 1,
    "CELESTIAL_DELTA_T_MODEL_ALGO2": 2,
    "CELESTIAL_DELTA_T_MODEL_ALGO3": 3,
    "CELESTIAL_DELTA_T_MODEL_ALGO4": 4,
    "CELESTIAL_DELTA_T_MODEL_ALGO5": 5,
    "CELESTIAL_TARGET_SUN": 0,
    "CELESTIAL_TARGET_MOON": 1,
    "CELESTIAL_TARGET_MERCURY": 2,
    "CELESTIAL_TARGET_VENUS": 3,
    "CELESTIAL_TARGET_MARS": 4,
    "CELESTIAL_TARGET_JUPITER": 5,
    "CELESTIAL_TARGET_SATURN": 6,
    "CELESTIAL_TARGET_URANUS": 7,
    "CELESTIAL_TARGET_NEPTUNE": 8,
    "CELESTIAL_TARGET_PLUTO": 9,
    "CELESTIAL_TARGET_MEAN_ASCENDING": 10,
    "CELESTIAL_TARGET_MEAN_DESCENDING": 11,
    "CELESTIAL_TARGET_TRUE_ASCENDING": 12,
    "CELESTIAL_TARGET_TRUE_DESCENDING": 13,
    "CELESTIAL_TARGET_ALL": 4294967295,
    "CELESTIAL_POSITION_SOURCE_SUN_VSOP87D": 1,
    "CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC": 2,
    "CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D": 3,
    "CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT": 4,
    "CELESTIAL_POSITION_SOURCE_NODE_MEAN": 5,
    "CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM": 6,
    "CELESTIAL_NUTATION_MODEL_DEFAULT": 0,
    "CELESTIAL_NUTATION_MODEL_MEEUS": 1,
    "CELESTIAL_NUTATION_MODEL_IAU_1980": 2,
    "CELESTIAL_PRESENT_LATITUDE": 1,
    "CELESTIAL_PRESENT_DISTANCE": 2,
    "CELESTIAL_EVENT_DIRECTION_INCREASING": 0,
    "CELESTIAL_EVENT_DIRECTION_DECREASING": 1,
    "CELESTIAL_EVENT_DIRECTION_TANGENT": 2,
    "CELESTIAL_EVENT_MOTION_DIRECT": 0,
    "CELESTIAL_EVENT_MOTION_RETROGRADE": 1,
    "CELESTIAL_OPERATION_POSITION": 0,
    "CELESTIAL_OPERATION_LONGITUDE_RATE": 1,
    "CELESTIAL_OPERATION_EVENTS": 2,
    "CELESTIAL_OPERATION_HOUSES": 3,
    "CELESTIAL_OPERATION_EQUATORIAL": 4,
    "CELESTIAL_OPERATION_CHART": 5,
    "CELESTIAL_OPERATION_DELTA_T": 6,
    "CELESTIAL_STATUS_SOURCE_WINDOW": 0,
    "CELESTIAL_STATUS_IMPLEMENTATION_WINDOW": 1,
    "CELESTIAL_STATUS_FINITE_INPUT_EVALUATION": 2,
    "CELESTIAL_MODEL_KIND_DELTA_T": 0,
    "CELESTIAL_MODEL_KIND_NUTATION": 1,
    "CELESTIAL_MODEL_KIND_POSITION_SOURCE": 2,
    "CELESTIAL_MODEL_KIND_ALL": 4294967295,
    "CELESTIAL_MODEL_STATUS_CURRENT": 0,
    "CELESTIAL_MODEL_STATUS_FROZEN": 1
  },
  "aliases": {}
});

export const WASM_SIGNATURES = deepFreeze({
  "celestial_last_error": {
    "result": "i32",
    "parameters": []
  },
  "celestial_last_error_kind": {
    "result": "i32",
    "parameters": []
  },
  "celestial_ut1_to_jd": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "f64"
    ]
  },
  "celestial_ut1_to_jde": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "f64"
    ]
  },
  "celestial_jde_to_ut1": {
    "result": "void",
    "parameters": [
      "i32",
      "f64"
    ]
  },
  "celestial_sun_apparent_geocentric_coord": {
    "result": "void",
    "parameters": [
      "i32",
      "f64"
    ]
  },
  "celestial_moon_apparent_geocentric_coord": {
    "result": "void",
    "parameters": [
      "i32",
      "f64"
    ]
  },
  "celestial_moon_illumination": {
    "result": "void",
    "parameters": [
      "i32",
      "f64"
    ]
  },
  "celestial_moon_position_angle": {
    "result": "void",
    "parameters": [
      "i32",
      "f64"
    ]
  },
  "celestial_moon_phase_moments": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_solar_lon_root_discriminant": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "f64"
    ]
  },
  "celestial_solar_lon_roots": {
    "result": "i32",
    "parameters": [
      "i32",
      "f64",
      "i32",
      "i32"
    ]
  },
  "celestial_new_moons_after_jde": {
    "result": "i32",
    "parameters": [
      "f64",
      "i32",
      "i32"
    ]
  },
  "celestial_new_moons_in_year": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_equation_of_time": {
    "result": "void",
    "parameters": [
      "i32",
      "f64"
    ]
  },
  "celestial_apparent_solar_time": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "f64",
      "f64"
    ]
  },
  "celestial_local_apparent_sidereal_time": {
    "result": "void",
    "parameters": [
      "i32",
      "f64",
      "f64"
    ]
  },
  "celestial_query_jieqi_moment": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_get_jieqi_name": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_get_supported_lunar_year_range": {
    "result": "void",
    "parameters": [
      "i32",
      "i32"
    ]
  },
  "celestial_get_lunar_year_info": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_gregorian_to_lunar": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_lunar_to_gregorian": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_delta_t": {
    "result": "void",
    "parameters": [
      "i32",
      "f64",
      "i32"
    ]
  },
  "celestial_chart_snapshot": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "f64",
      "i32",
      "f64",
      "f64",
      "i32",
      "i32"
    ]
  },
  "celestial_longitude_events": {
    "result": "i32",
    "parameters": [
      "i32",
      "f64",
      "f64",
      "f64",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_longitude_stations": {
    "result": "i32",
    "parameters": [
      "i32",
      "f64",
      "f64",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_longitude_difference_events": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "f64",
      "f64",
      "f64",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_capabilities": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_model_identities": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_model_identity_text": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_capability_text": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_positions": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "f64",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_equatorial_positions": {
    "result": "i32",
    "parameters": [
      "i32",
      "i32",
      "f64",
      "i32",
      "i32",
      "i32"
    ]
  },
  "celestial_house_query": {
    "result": "void",
    "parameters": [
      "i32",
      "i32",
      "i32",
      "i32",
      "f64",
      "i32",
      "i32",
      "i32",
      "f64",
      "f64",
      "i32"
    ]
  },
  "celestial_house_capability": {
    "result": "void",
    "parameters": [
      "i32",
      "i32"
    ]
  }
});

export const PROTOCOLS = deepFreeze({
  "celestial_last_error": {
    "kind": "borrowed-string",
    "error_reader": "message",
    "lifetime": "next-recording-call",
    "error_channel": "preserve"
  },
  "celestial_last_error_kind": {
    "kind": "primitive",
    "error_reader": "kind",
    "error_channel": "preserve"
  },
  "celestial_ut1_to_jd": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_ut1_to_jde": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_jde_to_ut1": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_sun_apparent_geocentric_coord": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_moon_apparent_geocentric_coord": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_moon_illumination": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_moon_position_angle": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_moon_phase_moments": {
    "kind": "count-fill",
    "count_parameter": "root_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": false,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "error_channel": "kind"
  },
  "celestial_solar_lon_root_discriminant": {
    "kind": "companion-count",
    "error_channel": "kind"
  },
  "celestial_solar_lon_roots": {
    "kind": "companion-fill",
    "count_export": "celestial_solar_lon_root_discriminant",
    "legal_empty": true,
    "written_policy": "min-total-capacity",
    "error_channel": "kind"
  },
  "celestial_new_moons_after_jde": {
    "kind": "requested-fill",
    "capacity_parameter": "slot_count",
    "legal_empty": true,
    "written_policy": "exact-requested-count",
    "null_slots_requires_zero_capacity": true,
    "error_channel": "kind"
  },
  "celestial_new_moons_in_year": {
    "kind": "count-fill",
    "count_parameter": "root_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": false,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "error_channel": "kind"
  },
  "celestial_equation_of_time": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_apparent_solar_time": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_local_apparent_sidereal_time": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_query_jieqi_moment": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_get_jieqi_name": {
    "kind": "sized-string",
    "buffer_parameter": "buf",
    "capacity_parameter": "buf_size",
    "size_includes_nul": true,
    "null_requires_zero_capacity": true,
    "truncates": true,
    "terminates": true,
    "error_channel": "kind"
  },
  "celestial_get_supported_lunar_year_range": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_get_lunar_year_info": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_gregorian_to_lunar": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_lunar_to_gregorian": {
    "kind": "sret",
    "error_channel": "kind"
  },
  "celestial_delta_t": {
    "kind": "sret",
    "identity": "resolved-explicit-model-key",
    "error_channel": "kind"
  },
  "celestial_chart_snapshot": {
    "kind": "sret",
    "identity": "resolved-explicit-model-keys-and-per-body-source",
    "error_channel": "kind"
  },
  "celestial_longitude_events": {
    "kind": "count-fill",
    "count_parameter": "event_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": true,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "error_channel": "kind"
  },
  "celestial_longitude_stations": {
    "kind": "count-fill",
    "count_parameter": "event_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": true,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "error_channel": "kind"
  },
  "celestial_longitude_difference_events": {
    "kind": "count-fill",
    "count_parameter": "event_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": true,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "error_channel": "kind"
  },
  "celestial_capabilities": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": false,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "operations": [
      "CELESTIAL_OPERATION_POSITION",
      "CELESTIAL_OPERATION_LONGITUDE_RATE",
      "CELESTIAL_OPERATION_EVENTS",
      "CELESTIAL_OPERATION_EQUATORIAL"
    ],
    "subject_policy": "concrete-target-or-all",
    "error_channel": "kind"
  },
  "celestial_model_identities": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": false,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "identity": "kind-and-explicit-code",
    "error_channel": "kind"
  },
  "celestial_model_identity_text": {
    "kind": "sized-string",
    "buffer_parameter": "buffer",
    "capacity_parameter": "capacity",
    "size_includes_nul": true,
    "null_requires_zero_capacity": true,
    "truncates": true,
    "terminates": true,
    "identity": "kind-and-explicit-code",
    "error_channel": "kind"
  },
  "celestial_capability_text": {
    "kind": "sized-string",
    "buffer_parameter": "buffer",
    "capacity_parameter": "capacity",
    "size_includes_nul": true,
    "null_requires_zero_capacity": true,
    "truncates": true,
    "terminates": true,
    "subject_policy": "concrete-target-for-target-operations-all-for-nontarget-operations",
    "nontarget_operations": [
      "CELESTIAL_OPERATION_HOUSES",
      "CELESTIAL_OPERATION_CHART",
      "CELESTIAL_OPERATION_DELTA_T"
    ],
    "error_channel": "kind"
  },
  "celestial_positions": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": false,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "selection_policy": "nonempty-distinct-targets-in-caller-order",
    "error_channel": "kind"
  },
  "celestial_equatorial_positions": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": false,
    "total_must_match": true,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": true,
    "selection_policy": "nonempty-distinct-targets-in-caller-order",
    "error_channel": "kind"
  },
  "celestial_house_query": {
    "kind": "sret",
    "identity": "resolved-explicit-model-keys",
    "error_channel": "kind"
  },
  "celestial_house_capability": {
    "kind": "sret",
    "error_channel": "kind"
  }
});
