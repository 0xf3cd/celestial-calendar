#!/usr/bin/env python3
#
# Single source of truth for the two ABI manifest twins (bindings/python/test/abi/manifest.json and
# bindings/javascript/test/abi/manifest.json). Run `python3 toolbox/make_abi_manifest.py` after the C ABI
# surface changes; `python3 toolbox/make_abi_manifest.py --check` fails on drift instead of writing.
#
#########################################################################################
#
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""One ABI manifest table, two rendered views.

The Python (ctypes) and JavaScript (WASM) ABI manifests describe the same `celestial.h` surface and
differ only in view-specific terms: pointer width (64 vs 32), the `struct`/`sret` kind labels, and
protocol prose where the two ABIs genuinely differ (a WASM pointer cannot be declared non-null, and
struct returns carry no padding-consumption promise). Those differences are rules or explicit
view-pairs below -- everything else is one value (#347).
"""

import argparse
import json
import sys

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]

VIEWS = {
  "python": {
    "schema": "celestial-calendar/python-native-abi@1",
    "width_key": "native_width_bits",
    "pointer_width_bits": 64,
  },
  "javascript": {
    "schema": "celestial-calendar/javascript-wasm-abi@1",
    "width_key": "wasm_width_bits",
    "pointer_width_bits": 32,
  },
}

MANIFEST_PATHS = {
  "python": Path("bindings/python/test/abi/manifest.json"),
  "javascript": Path("bindings/javascript/test/abi/manifest.json"),
}

# Fields the native view documents and the WASM view does not: ownership/presence prose, and
# array extent/stride on struct fields.
PYTHON_ONLY_KEYS = frozenset({"ownership", "presence", "extent", "stride"})

LAYOUTS = {
  "JulianDay": {
    "size": 16,
    "alignment": 8,
    "fields": [{"name": "valid", "type": "bool", "offset": 0}, {"name": "value", "type": "double", "offset": 8}],
  },
  "UT1Time": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "year", "type": "int32_t", "offset": 4},
      {"name": "month", "type": "uint32_t", "offset": 8},
      {"name": "day", "type": "uint32_t", "offset": 12},
      {"name": "fraction", "type": "double", "offset": 16},
    ],
  },
  "SunCoordinate": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "lon", "type": "double", "offset": 8},
      {"name": "lat", "type": "double", "offset": 16},
      {"name": "r", "type": "double", "offset": 24},
    ],
  },
  "MoonCoordinate": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "lon", "type": "double", "offset": 8},
      {"name": "lat", "type": "double", "offset": 16},
      {"name": "r", "type": "double", "offset": 24},
    ],
  },
  "MoonIllumination": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "illumination", "type": "double", "offset": 8},
      {"name": "elongation_deg", "type": "double", "offset": 16},
    ],
  },
  "MoonPositionAngle": {
    "size": 16,
    "alignment": 8,
    "fields": [{"name": "valid", "type": "bool", "offset": 0}, {"name": "angle_deg", "type": "double", "offset": 8}],
  },
  "Discriminant": {
    "size": 8,
    "alignment": 4,
    "fields": [{"name": "valid", "type": "bool", "offset": 0}, {"name": "count", "type": "uint32_t", "offset": 4}],
  },
  "EquationOfTime": {
    "size": 16,
    "alignment": 8,
    "fields": [{"name": "valid", "type": "bool", "offset": 0}, {"name": "value", "type": "double", "offset": 8}],
  },
  "ApparentSolarTime": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "year", "type": "int32_t", "offset": 4},
      {"name": "month", "type": "uint32_t", "offset": 8},
      {"name": "day", "type": "uint32_t", "offset": 12},
      {"name": "fraction", "type": "double", "offset": 16},
    ],
  },
  "SiderealTime": {
    "size": 16,
    "alignment": 8,
    "fields": [{"name": "valid", "type": "bool", "offset": 0}, {"name": "value", "type": "double", "offset": 8}],
  },
  "JieqiMomentQuery": {
    "size": 24,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "jq_idx", "type": "uint8_t", "offset": 1},
      {"name": "y", "type": "int32_t", "offset": 4},
      {"name": "m", "type": "uint32_t", "offset": 8},
      {"name": "d", "type": "uint32_t", "offset": 12},
      {"name": "frac", "type": "double", "offset": 16},
    ],
  },
  "SupportedLunarYearRange": {
    "size": 12,
    "alignment": 4,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "start", "type": "int32_t", "offset": 4},
      {"name": "end", "type": "int32_t", "offset": 8},
    ],
  },
  "LunarYearInfo": {
    "size": 16,
    "alignment": 4,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "year", "type": "int32_t", "offset": 4},
      {"name": "month", "type": "uint8_t", "offset": 8},
      {"name": "day", "type": "uint8_t", "offset": 9},
      {"name": "leap_month", "type": "uint8_t", "offset": 10},
      {"name": "month_len", "type": "uint16_t", "offset": 12},
    ],
  },
  "LunarDate": {
    "size": 12,
    "alignment": 4,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "year", "type": "int32_t", "offset": 4},
      {"name": "month", "type": "uint8_t", "offset": 8},
      {"name": "is_leap", "type": "bool", "offset": 9},
      {"name": "day", "type": "uint8_t", "offset": 10},
    ],
  },
  "GregorianDate": {
    "size": 12,
    "alignment": 4,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "year", "type": "int32_t", "offset": 4},
      {"name": "month", "type": "uint8_t", "offset": 8},
      {"name": "day", "type": "uint8_t", "offset": 9},
    ],
  },
  "DeltaT": {
    "size": 16,
    "alignment": 8,
    "fields": [{"name": "valid", "type": "bool", "offset": 0}, {"name": "value", "type": "double", "offset": 8}],
  },
  "ChartBodyV1": {
    "size": 40,
    "alignment": 8,
    "fields": [
      {"name": "target_code", "type": "uint32_t", "offset": 0},
      {"name": "present_fields", "type": "uint32_t", "offset": 4},
      {"name": "longitude_deg", "type": "double", "offset": 8},
      {"name": "latitude_deg", "type": "double", "offset": 16},
      {"name": "distance_au", "type": "double", "offset": 24},
      {"name": "longitude_rate_deg_per_tt_day", "type": "double", "offset": 32},
    ],
  },
  "ChartHousesV1": {
    "size": 128,
    "alignment": 8,
    "fields": [
      {"name": "ascendant_deg", "type": "double", "offset": 0},
      {"name": "midheaven_deg", "type": "double", "offset": 8},
      {"name": "descendant_deg", "type": "double", "offset": 16},
      {"name": "imum_coeli_deg", "type": "double", "offset": 24},
      {"name": "cusps_deg", "type": "double[12]", "offset": 32, "extent": 12, "stride": 8},
    ],
  },
  "ChartSnapshotV1": {
    "size": 712,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "jd_ut1", "type": "double", "offset": 8},
      {"name": "jde_tt", "type": "double", "offset": 16},
      {"name": "bodies", "type": "ChartBodyV1[14]", "offset": 24, "extent": 14, "stride": 40},
      {"name": "houses", "type": "ChartHousesV1", "offset": 584},
    ],
  },
  "LongitudeEventRecord": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {"name": "jde_tt", "type": "double", "offset": 0},
      {"name": "longitude_deg", "type": "double", "offset": 8},
      {"name": "longitude_rate_deg_per_tt_day", "type": "double", "offset": 16},
      {"name": "direction_code", "type": "uint32_t", "offset": 24},
    ],
  },
  "StationEventRecord": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {"name": "jde_tt", "type": "double", "offset": 0},
      {"name": "longitude_deg", "type": "double", "offset": 8},
      {"name": "longitude_rate_deg_per_tt_day", "type": "double", "offset": 16},
      {"name": "motion_before_code", "type": "uint32_t", "offset": 24},
      {"name": "motion_after_code", "type": "uint32_t", "offset": 28},
    ],
  },
  "LongitudeDifferenceEventRecord": {
    "size": 56,
    "alignment": 8,
    "fields": [
      {"name": "jde_tt", "type": "double", "offset": 0},
      {"name": "longitude_a_deg", "type": "double", "offset": 8},
      {"name": "longitude_b_deg", "type": "double", "offset": 16},
      {"name": "longitude_rate_a_deg_per_tt_day", "type": "double", "offset": 24},
      {"name": "longitude_rate_b_deg_per_tt_day", "type": "double", "offset": 32},
      {"name": "signed_difference_deg", "type": "double", "offset": 40},
      {"name": "direction_code", "type": "uint32_t", "offset": 48},
    ],
  },
  "CapabilityRecordV1": {
    "size": 40,
    "alignment": 8,
    "fields": [
      {"name": "operation_code", "type": "uint32_t", "offset": 0},
      {"name": "subject_code", "type": "uint32_t", "offset": 4},
      {"name": "status_code", "type": "uint32_t", "offset": 8},
      {"name": "source_code", "type": "uint32_t", "offset": 12},
      {"name": "domain_start_jde_tt", "type": "double", "offset": 16},
      {"name": "domain_end_jde_tt", "type": "double", "offset": 24},
      {"name": "maximum_span_tt_days", "type": "uint32_t", "offset": 32},
      {"name": "flags", "type": "uint32_t", "offset": 36},
    ],
  },
  "ModelIdentityRecordV1": {
    "size": 32,
    "alignment": 8,
    "fields": [
      {"name": "kind_code", "type": "uint32_t", "offset": 0},
      {"name": "identity_code", "type": "uint32_t", "offset": 4},
      {"name": "status_code", "type": "uint32_t", "offset": 8},
      {"name": "default_alias_code", "type": "uint32_t", "offset": 12},
      {"name": "domain_start_jde_tt", "type": "double", "offset": 16},
      {"name": "domain_end_jde_tt", "type": "double", "offset": 24},
    ],
  },
  "PositionRecordV1": {
    "size": 40,
    "alignment": 8,
    "fields": [
      {"name": "target_code", "type": "uint32_t", "offset": 0},
      {"name": "present_fields", "type": "uint32_t", "offset": 4},
      {"name": "longitude_deg", "type": "double", "offset": 8},
      {"name": "latitude_deg", "type": "double", "offset": 16},
      {"name": "distance_au", "type": "double", "offset": 24},
      {"name": "source_code", "type": "uint32_t", "offset": 32},
      {"name": "flags", "type": "uint32_t", "offset": 36},
    ],
  },
  "HouseQueryResultV1": {
    "size": 168,
    "alignment": 8,
    "fields": [
      {"name": "valid", "type": "bool", "offset": 0},
      {"name": "house_system_code", "type": "uint32_t", "offset": 4},
      {"name": "delta_t_identity_code", "type": "uint32_t", "offset": 8},
      {"name": "nutation_identity_code", "type": "uint32_t", "offset": 12},
      {"name": "flags", "type": "uint32_t", "offset": 16},
      {"name": "jd_ut1", "type": "double", "offset": 24},
      {"name": "jde_tt", "type": "double", "offset": 32},
      {"name": "houses", "type": "ChartHousesV1", "offset": 40},
    ],
  },
}

CHART_V1_CODES = {
  "CHART_CIVIL_SCALE_UTC": 0,
  "CHART_CIVIL_SCALE_UT1": 1,
  "CHART_HOUSE_SYSTEM_EQUAL": 0,
  "CHART_HOUSE_SYSTEM_WHOLE_SIGN": 1,
  "CHART_HOUSE_SYSTEM_PLACIDUS": 2,
  "CHART_DELTA_T_MODEL_DEFAULT": 0,
  "CHART_DELTA_T_MODEL_ALGO1": 1,
  "CHART_DELTA_T_MODEL_ALGO2": 2,
  "CHART_DELTA_T_MODEL_ALGO3": 3,
  "CHART_DELTA_T_MODEL_ALGO4": 4,
  "CHART_DELTA_T_MODEL_ALGO5": 5,
  "CHART_TARGET_SUN": 0,
  "CHART_TARGET_MOON": 1,
  "CHART_TARGET_MERCURY": 2,
  "CHART_TARGET_VENUS": 3,
  "CHART_TARGET_MARS": 4,
  "CHART_TARGET_JUPITER": 5,
  "CHART_TARGET_SATURN": 6,
  "CHART_TARGET_URANUS": 7,
  "CHART_TARGET_NEPTUNE": 8,
  "CHART_TARGET_PLUTO": 9,
  "CHART_TARGET_MEAN_ASCENDING": 10,
  "CHART_TARGET_MEAN_DESCENDING": 11,
  "CHART_TARGET_TRUE_ASCENDING": 12,
  "CHART_TARGET_TRUE_DESCENDING": 13,
  "CHART_PRESENT_LATITUDE": 1,
  "CHART_PRESENT_DISTANCE": 2,
}

EXPORTS = [
  {
    "name": "set_log_verbosity",
    "signature": "bool set_log_verbosity(uint8_t new_value)",
    "return": {"kind": "primitive", "type": "bool"},
    "protocol": {"kind": "primitive", "zero": "false is failure; true is success"},
    "recording": True,
  },
  {
    "name": "last_error",
    "signature": "const char *last_error(void)",
    "return": {"kind": "borrowed-string", "pointer_width_bits": 64},
    "protocol": {
      "kind": "borrowed-string",
      "zero": "an empty string is the legal no-error state",
      "lifetime": "until the next recording call",
    },
    "recording": False,
  },
  {
    "name": "ut1_to_jd",
    "signature": "JulianDay ut1_to_jd(int32_t y, uint32_t m, uint32_t d, double fraction)",
    "return": {"kind": "struct", "layout": "JulianDay"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "ut1_to_jde",
    "signature": "JulianDay ut1_to_jde(int32_t y, uint32_t m, uint32_t d, double fraction)",
    "return": {"kind": "struct", "layout": "JulianDay"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "jde_to_ut1",
    "signature": "UT1Time jde_to_ut1(double jde)",
    "return": {"kind": "struct", "layout": "UT1Time"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "sun_apparent_geocentric_coord",
    "signature": "SunCoordinate sun_apparent_geocentric_coord(double jde)",
    "return": {"kind": "struct", "layout": "SunCoordinate"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "moon_apparent_geocentric_coord",
    "signature": "MoonCoordinate moon_apparent_geocentric_coord(double jde)",
    "return": {"kind": "struct", "layout": "MoonCoordinate"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "moon_illumination",
    "signature": "MoonIllumination moon_illumination(double jde)",
    "return": {"kind": "struct", "layout": "MoonIllumination"},
    "protocol": {"kind": "struct", "zero": "illumination=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "moon_position_angle",
    "signature": "MoonPositionAngle moon_position_angle(double jde)",
    "return": {"kind": "struct", "layout": "MoonPositionAngle"},
    "protocol": {"kind": "struct", "zero": "angle_deg=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "moon_phase_moments",
    "signature": "uint32_t moon_phase_moments(int32_t year, uint8_t phase_kind, uint32_t *root_count, double *slots, "
    "uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": {
        "python": "root_count is non-null and always reports the total",
        "javascript": "root_count always reports the total",
      },
      "fill": {
        "python": "slots may be null only when slot_count=0; writes min(root_count, slot_count)",
        "javascript": "writes min(root_count, slot_count)",
      },
      "zero": "zero written is legal for a count query",
    },
    "recording": True,
  },
  {
    "name": "solar_lon_root_discriminant",
    "signature": "Discriminant solar_lon_root_discriminant(int32_t year, double longitude)",
    "return": {"kind": "struct", "layout": "Discriminant"},
    "protocol": {"kind": "companion-count", "zero": "count=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "solar_lon_roots",
    "signature": "uint32_t solar_lon_roots(int32_t year, double longitude, double *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "companion-fill",
      "count": "solar_lon_root_discriminant",
      "fill": {
        "python": "slots may be null only when slot_count=0; writes min(discriminant count, slot_count)",
        "javascript": "writes min(discriminant count, slot_count)",
      },
      "zero": "zero written is legal when slot_count=0 or count=0",
    },
    "recording": True,
  },
  {
    "name": "new_moons_after_jde",
    "signature": "uint32_t new_moons_after_jde(double jde, double *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "requested-fill",
      "count": "caller requests slot_count results",
      "fill": {
        "python": "slots may be null only when slot_count=0; writes exactly slot_count on success",
        "javascript": "writes exactly slot_count on success",
      },
      "zero": "zero is legal when slot_count=0",
    },
    "recording": True,
  },
  {
    "name": "new_moons_in_year",
    "signature": "uint32_t new_moons_in_year(int32_t year, uint32_t *root_count, double *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": {
        "python": "root_count is non-null and always reports the total",
        "javascript": "root_count always reports the total",
      },
      "fill": {
        "python": "slots may be null only when slot_count=0; writes min(root_count, slot_count)",
        "javascript": "writes min(root_count, slot_count)",
      },
      "zero": "zero written is legal for a count query",
    },
    "recording": True,
  },
  {
    "name": "equation_of_time",
    "signature": "EquationOfTime equation_of_time(double jde)",
    "return": {"kind": "struct", "layout": "EquationOfTime"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "apparent_solar_time",
    "signature": "ApparentSolarTime apparent_solar_time(int32_t y, uint32_t m, uint32_t d, double fraction, double "
    "longitude)",
    "return": {"kind": "struct", "layout": "ApparentSolarTime"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "local_apparent_sidereal_time",
    "signature": "SiderealTime local_apparent_sidereal_time(double jd_ut1, double longitude)",
    "return": {"kind": "struct", "layout": "SiderealTime"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "query_jieqi_moment",
    "signature": "JieqiMomentQuery query_jieqi_moment(int32_t year, uint8_t jq_idx)",
    "return": {"kind": "struct", "layout": "JieqiMomentQuery"},
    "protocol": {"kind": "struct", "zero": "jq_idx=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "get_jieqi_name",
    "signature": "bool get_jieqi_name(uint8_t jq_idx, char *buf, uint32_t buf_size)",
    "return": {"kind": "primitive", "type": "bool"},
    "protocol": {
      "kind": "caller-string",
      "fill": {
        "python": "buf is non-null; writes a NUL-terminated UTF-8 string without truncation",
        "javascript": "writes a NUL-terminated UTF-8 string without truncation",
      },
      "zero": "false is failure",
    },
    "recording": True,
  },
  {
    "name": "get_supported_lunar_year_range",
    "signature": "SupportedLunarYearRange get_supported_lunar_year_range(uint8_t algo)",
    "return": {"kind": "struct", "layout": "SupportedLunarYearRange"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "get_lunar_year_info",
    "signature": "LunarYearInfo get_lunar_year_info(uint8_t algo, int32_t year)",
    "return": {"kind": "struct", "layout": "LunarYearInfo"},
    "protocol": {"kind": "struct", "zero": "leap_month=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "gregorian_to_lunar",
    "signature": "LunarDate gregorian_to_lunar(uint8_t algo, int32_t year, uint8_t month, uint8_t day)",
    "return": {"kind": "struct", "layout": "LunarDate"},
    "protocol": {"kind": "struct", "zero": "is_leap=false is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "lunar_to_gregorian",
    "signature": "GregorianDate lunar_to_gregorian(uint8_t algo, int32_t year, uint8_t month, bool is_leap, uint8_t day)",  # noqa: E501
    "return": {"kind": "struct", "layout": "GregorianDate"},
    "protocol": {"kind": "struct", "zero": "valid=false is failure"},
    "recording": True,
  },
  {
    "name": "delta_t_algo1",
    "signature": "DeltaT delta_t_algo1(double year)",
    "return": {"kind": "struct", "layout": "DeltaT"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "delta_t_algo2",
    "signature": "DeltaT delta_t_algo2(double year)",
    "return": {"kind": "struct", "layout": "DeltaT"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "delta_t_algo3",
    "signature": "DeltaT delta_t_algo3(double year)",
    "return": {"kind": "struct", "layout": "DeltaT"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "delta_t_algo4",
    "signature": "DeltaT delta_t_algo4(double year)",
    "return": {"kind": "struct", "layout": "DeltaT"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "delta_t_algo5",
    "signature": "DeltaT delta_t_algo5(double year)",
    "return": {"kind": "struct", "layout": "DeltaT"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "delta_t",
    "signature": "DeltaT delta_t(double year)",
    "return": {"kind": "struct", "layout": "DeltaT"},
    "protocol": {"kind": "struct", "zero": "value=0 is legal when valid=true"},
    "recording": True,
  },
  {
    "name": "chart_snapshot_v1",
    "signature": "ChartSnapshotV1 chart_snapshot_v1(int32_t year, uint32_t month, uint32_t day, double fraction, "
    "uint32_t civil_scale_code, double latitude_deg, double longitude_deg, uint32_t house_system_code, "
    "uint32_t delta_t_model_code)",
    "return": {"kind": "struct", "layout": "ChartSnapshotV1"},
    "protocol": {
      "kind": "struct",
      "zero": {
        "python": "valid=false is failure; invalid payload and padding are not consumed",
        "javascript": "valid=false is failure; invalid payload is not consumed; absence uses present_fields",
      },
      "presence": "present_fields marks latitude and distance; absent distance is unused storage",
      "ownership": "by-value fixed snapshot of 14 bodies and 12 cusps; no allocation or partial result",
    },
    "recording": True,
  },
  {
    "name": "longitude_events",
    "signature": "uint32_t longitude_events(uint32_t target_code, double target_longitude_deg, double start_jde_tt, "
    "double end_jde_tt, uint32_t *event_count, LongitudeEventRecord *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": "event_count is non-null and always reports the total",
      "fill": "slots may be null only when slot_count=0; writes min(event_count, slot_count)",
      "zero": "zero written is legal for a count query or an empty result; failure is told apart by last_error",
    },
    "recording": True,
  },
  {
    "name": "longitude_stations",
    "signature": "uint32_t longitude_stations(uint32_t target_code, double start_jde_tt, double end_jde_tt, uint32_t "
    "*event_count, StationEventRecord *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": "event_count is non-null and always reports the total",
      "fill": "slots may be null only when slot_count=0; writes min(event_count, slot_count)",
      "zero": "zero written is legal for a count query or an empty result; failure is told apart by last_error",
    },
    "recording": True,
  },
  {
    "name": "longitude_difference_events",
    "signature": "uint32_t longitude_difference_events(uint32_t first_target_code, uint32_t second_target_code, double "
    "target_difference_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, "
    "LongitudeDifferenceEventRecord *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": "event_count is non-null and always reports the total",
      "fill": "slots may be null only when slot_count=0; writes min(event_count, slot_count)",
      "zero": "zero written is legal for a count query or an empty result; failure is told apart by last_error",
    },
    "recording": True,
  },
  {
    "name": "celestial_capabilities",
    "signature": "uint32_t celestial_capabilities(uint32_t operation_code, uint32_t subject_code, uint32_t "
    "*record_count, CapabilityRecordV1 *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": "record_count is non-null and always reports the total",
      "fill": "slots may be null only when slot_count=0; writes min(record_count, slot_count)",
      "zero": "zero written is legal for a count query; failure is told apart by last_error",
      "provenance": "every bound derives from the operation's own defining constants (#357)",
    },
    "recording": True,
  },
  {
    "name": "celestial_model_identities",
    "signature": "uint32_t celestial_model_identities(uint32_t kind_code, uint32_t *record_count, "
    "ModelIdentityRecordV1 *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": "record_count is non-null and always reports the total",
      "fill": "slots may be null only when slot_count=0; writes min(record_count, slot_count)",
      "zero": "zero written is legal for a count query; failure is told apart by last_error",
    },
    "recording": True,
  },
  {
    "name": "celestial_capability_text",
    "signature": "uint32_t celestial_capability_text(uint32_t operation_code, uint32_t subject_code, char *buffer, "
    "uint32_t capacity)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "caller-string",
      "size": "returns the required byte count excluding the null terminator; buffer may be null to size first",
      "fill": "writes the text truncated and null-terminated when buffer is non-null and capacity is positive",
      "lifetime": "the buffer is caller-owned; the text is produced on every call",
    },
    "recording": True,
  },
  {
    "name": "position_query_v1",
    "signature": "uint32_t position_query_v1(const uint32_t *target_codes, uint32_t target_count, double jde_tt, "
    "uint32_t *record_count, PositionRecordV1 *slots, uint32_t slot_count)",
    "return": {"kind": "primitive", "type": "uint32_t"},
    "protocol": {
      "kind": "count-fill",
      "count": "record_count is non-null and always reports the total",
      "fill": "slots may be null only when slot_count=0; writes min(record_count, slot_count)",
      "zero": "zero written is legal for a count query; failure is told apart by last_error",
      "selection": "output records follow the caller's target order; duplicates, empty selections and unknown "
      "codes are rejected, and one failed target fails the whole query (#355)",
    },
    "recording": True,
  },
  {
    "name": "house_query_v1",
    "signature": "HouseQueryResultV1 house_query_v1(int32_t year, uint32_t month, uint32_t day, double fraction, "
    "uint32_t civil_scale_code, uint32_t delta_t_model_code, uint32_t nutation_model_code, double latitude_deg, "
    "double longitude_deg, uint32_t house_system_code)",
    "return": {"kind": "struct", "layout": "HouseQueryResultV1"},
    "protocol": {
      "kind": "struct",
      "zero": {
        "python": "valid=false is failure; invalid payload and padding are not consumed",
        "javascript": "valid=false is failure; invalid payload is not consumed",
      },
      "ownership": "by-value house geometry at one civil instant; the snapshot joint window does not apply",
      "identity": "delta_t_identity_code and nutation_identity_code carry the resolved model ordinals, never "
      "an unresolved DEFAULT code (#357)",
    },
    "recording": True,
  },
]


def _resolve_views(node: object, view: str) -> object:
  """Pick one side of every {python, javascript} prose pair."""
  if isinstance(node, dict):
    if set(node) == {"python", "javascript"}:
      return node[view]
    return {key: _resolve_views(value, view) for key, value in node.items()}
  if isinstance(node, list):
    return [_resolve_views(item, view) for item in node]
  return node


def _apply_view(node: object, view: str, *, pointer_width_bits: int) -> object:
  if isinstance(node, dict):
    out = {}
    for key, value in node.items():
      if view != "python" and key in PYTHON_ONLY_KEYS:
        continue
      if key == "kind" and value == "struct" and view != "python":
        out[key] = "sret"
      elif key == "pointer_width_bits":
        out[key] = pointer_width_bits
      else:
        out[key] = _apply_view(value, view, pointer_width_bits=pointer_width_bits)
    return out
  if isinstance(node, list):
    return [_apply_view(item, view, pointer_width_bits=pointer_width_bits) for item in node]
  return node


def build_manifest(view: str) -> dict:
  spec = VIEWS[view]
  widths = {
    "bool": 8,
    "uint8_t": 8,
    "uint16_t": 16,
    "int32_t": 32,
    "uint32_t": 32,
    "pointer": spec["pointer_width_bits"],
    "double": 64,
  }
  body = _apply_view(
    _resolve_views({"layouts": LAYOUTS, "chart_v1_codes": CHART_V1_CODES, "exports": EXPORTS}, view),
    view,
    pointer_width_bits=spec["pointer_width_bits"],
  )
  return {
    "schema": spec["schema"],
    "source_header": "src/shared_lib/celestial.h",
    **{spec["width_key"]: widths},
    **body,
  }


def _inline(node: dict) -> str:
  return (
    "{" + ", ".join(f"{json.dumps(key)}: {json.dumps(value, ensure_ascii=False)}" for key, value in node.items()) + "}"
  )


def _flat(node: dict) -> bool:
  return all(not isinstance(value, (dict, list)) for value in node.values())


def _dump(node: object, level: int, parent_key: str | None = None) -> str:
  indent = "  " * level
  child = "  " * (level + 1)
  if isinstance(node, dict):
    if not node:
      return "{}"
    # The committed shape inlines flat dicts under return/protocol and struct-field entries;
    # everything else (exports, layouts, code tables, width tables) stays multi-line.
    if _flat(node) and (parent_key in {"return", "protocol"} or "offset" in node):
      return _inline(node)
    items = [f"{child}{json.dumps(key)}: {_dump(value, level + 1, key)}" for key, value in node.items()]
    return "{\n" + ",\n".join(items) + f"\n{indent}}}"
  if isinstance(node, list):
    if not node:
      return "[]"
    if all(isinstance(item, dict) and "name" in item and "offset" in item for item in node):
      return "[\n" + ",\n".join(f"{child}{_inline(item)}" for item in node) + f"\n{indent}]"
    return "[\n" + ",\n".join(f"{child}{_dump(item, level + 1, parent_key)}" for item in node) + f"\n{indent}]"
  return json.dumps(node, ensure_ascii=False)


def render(view: str) -> str:
  return _dump(build_manifest(view), 0) + "\n"


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--check", action="store_true", help="fail on drift instead of writing")
  args = parser.parse_args(argv)

  ok = True
  for view, relative in MANIFEST_PATHS.items():
    path = REPO_ROOT / relative
    rendered = render(view)
    if args.check:
      if path.read_text(encoding="utf-8") != rendered:
        print(f"{relative} drifts from toolbox/make_abi_manifest.py; rerun the generator", file=sys.stderr)
        ok = False
    else:
      path.write_text(rendered, encoding="utf-8")
      print(f"wrote {relative}")
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
