# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Private declarations. Regenerate with python3 toolbox/make_bindings.py."""

from __future__ import annotations

import ctypes


class CelestialJulianDay(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("value", ctypes.c_double),
  ]


class CelestialUT1Time(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint32),
    ("day", ctypes.c_uint32),
    ("fraction", ctypes.c_double),
  ]


class CelestialSunCoordinate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("lon", ctypes.c_double),
    ("lat", ctypes.c_double),
    ("r", ctypes.c_double),
  ]


class CelestialMoonCoordinate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("lon", ctypes.c_double),
    ("lat", ctypes.c_double),
    ("r", ctypes.c_double),
  ]


class CelestialMoonIllumination(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("illumination", ctypes.c_double),
    ("elongation_deg", ctypes.c_double),
  ]


class CelestialMoonPositionAngle(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("angle_deg", ctypes.c_double),
  ]


class CelestialDiscriminant(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("count", ctypes.c_uint32),
  ]


class CelestialEquationOfTime(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("value", ctypes.c_double),
  ]


class CelestialApparentSolarTime(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint32),
    ("day", ctypes.c_uint32),
    ("fraction", ctypes.c_double),
  ]


class CelestialSiderealTime(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("value", ctypes.c_double),
  ]


class CelestialJieqiMomentQuery(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("jq_idx", ctypes.c_uint8),
    ("y", ctypes.c_int32),
    ("m", ctypes.c_uint32),
    ("d", ctypes.c_uint32),
    ("frac", ctypes.c_double),
  ]


class CelestialSupportedLunarYearRange(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("start", ctypes.c_int32),
    ("end", ctypes.c_int32),
  ]


class CelestialLunarYearInfo(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint8),
    ("day", ctypes.c_uint8),
    ("leap_month", ctypes.c_uint8),
    ("month_len", ctypes.c_uint16),
  ]


class CelestialLunarDate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint8),
    ("is_leap", ctypes.c_bool),
    ("day", ctypes.c_uint8),
  ]


class CelestialGregorianDate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint8),
    ("day", ctypes.c_uint8),
  ]


class CelestialDeltaT(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("model_identity_code", ctypes.c_uint32),
    ("value", ctypes.c_double),
  ]


class CelestialChartBody(ctypes.Structure):
  _fields_ = [
    ("target_code", ctypes.c_uint32),
    ("source_code", ctypes.c_uint32),
    ("present_fields", ctypes.c_uint32),
    ("longitude_deg", ctypes.c_double),
    ("latitude_deg", ctypes.c_double),
    ("distance_au", ctypes.c_double),
    ("longitude_rate_deg_per_tt_day", ctypes.c_double),
  ]


class CelestialChartHouses(ctypes.Structure):
  _fields_ = [
    ("ascendant_deg", ctypes.c_double),
    ("midheaven_deg", ctypes.c_double),
    ("descendant_deg", ctypes.c_double),
    ("imum_coeli_deg", ctypes.c_double),
    ("cusps_deg", ctypes.c_double * 12),
  ]


class CelestialChartSnapshot(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("delta_t_identity_code", ctypes.c_uint32),
    ("nutation_identity_code", ctypes.c_uint32),
    ("jd_ut1", ctypes.c_double),
    ("jde_tt", ctypes.c_double),
    ("bodies", CelestialChartBody * 14),
    ("houses", CelestialChartHouses),
  ]


class CelestialLongitudeEventRecord(ctypes.Structure):
  _fields_ = [
    ("jde_tt", ctypes.c_double),
    ("longitude_deg", ctypes.c_double),
    ("longitude_rate_deg_per_tt_day", ctypes.c_double),
    ("direction_code", ctypes.c_uint32),
  ]


class CelestialStationEventRecord(ctypes.Structure):
  _fields_ = [
    ("jde_tt", ctypes.c_double),
    ("longitude_deg", ctypes.c_double),
    ("longitude_rate_deg_per_tt_day", ctypes.c_double),
    ("motion_before_code", ctypes.c_uint32),
    ("motion_after_code", ctypes.c_uint32),
  ]


class CelestialLongitudeDifferenceEventRecord(ctypes.Structure):
  _fields_ = [
    ("jde_tt", ctypes.c_double),
    ("longitude_a_deg", ctypes.c_double),
    ("longitude_b_deg", ctypes.c_double),
    ("longitude_rate_a_deg_per_tt_day", ctypes.c_double),
    ("longitude_rate_b_deg_per_tt_day", ctypes.c_double),
    ("signed_difference_deg", ctypes.c_double),
    ("direction_code", ctypes.c_uint32),
  ]


class CelestialCapabilityRecord(ctypes.Structure):
  _fields_ = [
    ("operation_code", ctypes.c_uint32),
    ("subject_code", ctypes.c_uint32),
    ("status_code", ctypes.c_uint32),
    ("source_code", ctypes.c_uint32),
    ("domain_start_jde_tt", ctypes.c_double),
    ("domain_end_jde_tt", ctypes.c_double),
    ("maximum_span_tt_days", ctypes.c_uint32),
  ]


class CelestialModelIdentityRecord(ctypes.Structure):
  _fields_ = [
    ("kind_code", ctypes.c_uint32),
    ("identity_code", ctypes.c_uint32),
    ("status_code", ctypes.c_uint32),
    ("domain_start_jde_tt", ctypes.c_double),
    ("domain_end_jde_tt", ctypes.c_double),
  ]


class CelestialPositionRecord(ctypes.Structure):
  _fields_ = [
    ("target_code", ctypes.c_uint32),
    ("present_fields", ctypes.c_uint32),
    ("longitude_deg", ctypes.c_double),
    ("latitude_deg", ctypes.c_double),
    ("distance_au", ctypes.c_double),
    ("source_code", ctypes.c_uint32),
  ]


class CelestialEquatorialPositionRecord(ctypes.Structure):
  _fields_ = [
    ("target_code", ctypes.c_uint32),
    ("present_fields", ctypes.c_uint32),
    ("right_ascension_deg", ctypes.c_double),
    ("declination_deg", ctypes.c_double),
    ("distance_au", ctypes.c_double),
    ("source_code", ctypes.c_uint32),
  ]


class CelestialHouseQueryResult(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("house_system_code", ctypes.c_uint32),
    ("delta_t_identity_code", ctypes.c_uint32),
    ("nutation_identity_code", ctypes.c_uint32),
    ("jd_ut1", ctypes.c_double),
    ("jde_tt", ctypes.c_double),
    ("houses", CelestialChartHouses),
  ]


class CelestialCivilHouseCapability(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("status_code", ctypes.c_uint32),
    ("civil_start_year", ctypes.c_int32),
    ("civil_end_year_exclusive", ctypes.c_int32),
    ("civil_scale_mask", ctypes.c_uint32),
    ("utc_start_year", ctypes.c_int32),
    ("utc_start_month", ctypes.c_uint32),
    ("utc_start_day", ctypes.c_uint32),
    ("delta_t_identity_code", ctypes.c_uint32),
    ("has_derived_tt_cutoff", ctypes.c_bool),
    ("derived_tt_end_jde_tt", ctypes.c_double),
  ]


P_U32 = ctypes.POINTER(ctypes.c_uint32)
P_DOUBLE = ctypes.POINTER(ctypes.c_double)
P_CHAR = ctypes.POINTER(ctypes.c_char)
P_LONGITUDE_EVENT = ctypes.POINTER(CelestialLongitudeEventRecord)
P_STATION_EVENT = ctypes.POINTER(CelestialStationEventRecord)
P_LONGITUDE_DIFFERENCE_EVENT = ctypes.POINTER(CelestialLongitudeDifferenceEventRecord)
P_CAPABILITY = ctypes.POINTER(CelestialCapabilityRecord)
P_MODEL_IDENTITY = ctypes.POINTER(CelestialModelIdentityRecord)
P_POSITION = ctypes.POINTER(CelestialPositionRecord)
P_EQUATORIAL_POSITION = ctypes.POINTER(CelestialEquatorialPositionRecord)

BINDING_SPECS = {
  "celestial_last_error": ((), ctypes.c_char_p),
  "celestial_last_error_kind": ((), ctypes.c_uint32),
  "celestial_ut1_to_jd": ((ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_double), CelestialJulianDay),
  "celestial_ut1_to_jde": ((ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_double), CelestialJulianDay),
  "celestial_jde_to_ut1": ((ctypes.c_double,), CelestialUT1Time),
  "celestial_sun_apparent_geocentric_coord": ((ctypes.c_double,), CelestialSunCoordinate),
  "celestial_moon_apparent_geocentric_coord": ((ctypes.c_double,), CelestialMoonCoordinate),
  "celestial_moon_illumination": ((ctypes.c_double,), CelestialMoonIllumination),
  "celestial_moon_position_angle": ((ctypes.c_double,), CelestialMoonPositionAngle),
  "celestial_moon_phase_moments": ((ctypes.c_int32, ctypes.c_uint8, P_U32, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_solar_lon_root_discriminant": ((ctypes.c_int32, ctypes.c_double), CelestialDiscriminant),
  "celestial_solar_lon_roots": ((ctypes.c_int32, ctypes.c_double, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_new_moons_after_jde": ((ctypes.c_double, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_new_moons_in_year": ((ctypes.c_int32, P_U32, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_equation_of_time": ((ctypes.c_double,), CelestialEquationOfTime),
  "celestial_apparent_solar_time": (
    (
      ctypes.c_int32,
      ctypes.c_uint32,
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_double,
    ),
    CelestialApparentSolarTime,
  ),
  "celestial_local_apparent_sidereal_time": ((ctypes.c_double, ctypes.c_double), CelestialSiderealTime),
  "celestial_query_jieqi_moment": ((ctypes.c_int32, ctypes.c_uint8), CelestialJieqiMomentQuery),
  "celestial_get_jieqi_name": ((ctypes.c_uint8, P_CHAR, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_get_supported_lunar_year_range": ((ctypes.c_uint8,), CelestialSupportedLunarYearRange),
  "celestial_get_lunar_year_info": ((ctypes.c_uint8, ctypes.c_int32), CelestialLunarYearInfo),
  "celestial_gregorian_to_lunar": (
    (
      ctypes.c_uint8,
      ctypes.c_int32,
      ctypes.c_uint8,
      ctypes.c_uint8,
    ),
    CelestialLunarDate,
  ),
  "celestial_lunar_to_gregorian": (
    (
      ctypes.c_uint8,
      ctypes.c_int32,
      ctypes.c_uint8,
      ctypes.c_bool,
      ctypes.c_uint8,
    ),
    CelestialGregorianDate,
  ),
  "celestial_delta_t": ((ctypes.c_double, ctypes.c_uint32), CelestialDeltaT),
  "celestial_chart_snapshot": (
    (
      ctypes.c_int32,
      ctypes.c_uint32,
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_double,
      ctypes.c_uint32,
      ctypes.c_uint32,
    ),
    CelestialChartSnapshot,
  ),
  "celestial_longitude_events": (
    (
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_double,
      ctypes.c_double,
      P_U32,
      P_LONGITUDE_EVENT,
      ctypes.c_uint32,
    ),
    ctypes.c_uint32,
  ),
  "celestial_longitude_stations": (
    (
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_double,
      P_U32,
      P_STATION_EVENT,
      ctypes.c_uint32,
    ),
    ctypes.c_uint32,
  ),
  "celestial_longitude_difference_events": (
    (
      ctypes.c_uint32,
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_double,
      ctypes.c_double,
      P_U32,
      P_LONGITUDE_DIFFERENCE_EVENT,
      ctypes.c_uint32,
    ),
    ctypes.c_uint32,
  ),
  "celestial_capabilities": ((ctypes.c_uint32, ctypes.c_uint32, P_U32, P_CAPABILITY, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_model_identities": ((ctypes.c_uint32, P_U32, P_MODEL_IDENTITY, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_model_identity_text": ((ctypes.c_uint32, ctypes.c_uint32, P_CHAR, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_capability_text": ((ctypes.c_uint32, ctypes.c_uint32, P_CHAR, ctypes.c_uint32), ctypes.c_uint32),
  "celestial_positions": (
    (
      P_U32,
      ctypes.c_uint32,
      ctypes.c_double,
      P_U32,
      P_POSITION,
      ctypes.c_uint32,
    ),
    ctypes.c_uint32,
  ),
  "celestial_equatorial_positions": (
    (
      P_U32,
      ctypes.c_uint32,
      ctypes.c_double,
      P_U32,
      P_EQUATORIAL_POSITION,
      ctypes.c_uint32,
    ),
    ctypes.c_uint32,
  ),
  "celestial_house_query": (
    (
      ctypes.c_int32,
      ctypes.c_uint32,
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_uint32,
      ctypes.c_uint32,
      ctypes.c_uint32,
      ctypes.c_double,
      ctypes.c_double,
      ctypes.c_uint32,
    ),
    CelestialHouseQueryResult,
  ),
  "celestial_house_capability": ((ctypes.c_uint32,), CelestialCivilHouseCapability),
}

STRUCT_TYPES = {
  "CelestialJulianDay": CelestialJulianDay,
  "CelestialUT1Time": CelestialUT1Time,
  "CelestialSunCoordinate": CelestialSunCoordinate,
  "CelestialMoonCoordinate": CelestialMoonCoordinate,
  "CelestialMoonIllumination": CelestialMoonIllumination,
  "CelestialMoonPositionAngle": CelestialMoonPositionAngle,
  "CelestialDiscriminant": CelestialDiscriminant,
  "CelestialEquationOfTime": CelestialEquationOfTime,
  "CelestialApparentSolarTime": CelestialApparentSolarTime,
  "CelestialSiderealTime": CelestialSiderealTime,
  "CelestialJieqiMomentQuery": CelestialJieqiMomentQuery,
  "CelestialSupportedLunarYearRange": CelestialSupportedLunarYearRange,
  "CelestialLunarYearInfo": CelestialLunarYearInfo,
  "CelestialLunarDate": CelestialLunarDate,
  "CelestialGregorianDate": CelestialGregorianDate,
  "CelestialDeltaT": CelestialDeltaT,
  "CelestialChartBody": CelestialChartBody,
  "CelestialChartHouses": CelestialChartHouses,
  "CelestialChartSnapshot": CelestialChartSnapshot,
  "CelestialLongitudeEventRecord": CelestialLongitudeEventRecord,
  "CelestialStationEventRecord": CelestialStationEventRecord,
  "CelestialLongitudeDifferenceEventRecord": CelestialLongitudeDifferenceEventRecord,
  "CelestialCapabilityRecord": CelestialCapabilityRecord,
  "CelestialModelIdentityRecord": CelestialModelIdentityRecord,
  "CelestialPositionRecord": CelestialPositionRecord,
  "CelestialEquatorialPositionRecord": CelestialEquatorialPositionRecord,
  "CelestialHouseQueryResult": CelestialHouseQueryResult,
  "CelestialCivilHouseCapability": CelestialCivilHouseCapability,
}

RECORDING_EXPORTS = frozenset(
  (
    "celestial_ut1_to_jd",
    "celestial_ut1_to_jde",
    "celestial_jde_to_ut1",
    "celestial_sun_apparent_geocentric_coord",
    "celestial_moon_apparent_geocentric_coord",
    "celestial_moon_illumination",
    "celestial_moon_position_angle",
    "celestial_moon_phase_moments",
    "celestial_solar_lon_root_discriminant",
    "celestial_solar_lon_roots",
    "celestial_new_moons_after_jde",
    "celestial_new_moons_in_year",
    "celestial_equation_of_time",
    "celestial_apparent_solar_time",
    "celestial_local_apparent_sidereal_time",
    "celestial_query_jieqi_moment",
    "celestial_get_jieqi_name",
    "celestial_get_supported_lunar_year_range",
    "celestial_get_lunar_year_info",
    "celestial_gregorian_to_lunar",
    "celestial_lunar_to_gregorian",
    "celestial_delta_t",
    "celestial_chart_snapshot",
    "celestial_longitude_events",
    "celestial_longitude_stations",
    "celestial_longitude_difference_events",
    "celestial_capabilities",
    "celestial_model_identities",
    "celestial_model_identity_text",
    "celestial_capability_text",
    "celestial_positions",
    "celestial_equatorial_positions",
    "celestial_house_query",
    "celestial_house_capability",
  )
)

# ctypes erases pointer qualifiers; retain the complete C spelling for verification.
C_SIGNATURES = {
  "celestial_last_error": (
    "const char *",
    (),
  ),
  "celestial_last_error_kind": (
    "uint32_t",
    (),
  ),
  "celestial_ut1_to_jd": (
    "CelestialJulianDay",
    (
      "int32_t",
      "uint32_t",
      "uint32_t",
      "double",
    ),
  ),
  "celestial_ut1_to_jde": (
    "CelestialJulianDay",
    (
      "int32_t",
      "uint32_t",
      "uint32_t",
      "double",
    ),
  ),
  "celestial_jde_to_ut1": (
    "CelestialUT1Time",
    ("double",),
  ),
  "celestial_sun_apparent_geocentric_coord": (
    "CelestialSunCoordinate",
    ("double",),
  ),
  "celestial_moon_apparent_geocentric_coord": (
    "CelestialMoonCoordinate",
    ("double",),
  ),
  "celestial_moon_illumination": (
    "CelestialMoonIllumination",
    ("double",),
  ),
  "celestial_moon_position_angle": (
    "CelestialMoonPositionAngle",
    ("double",),
  ),
  "celestial_moon_phase_moments": (
    "uint32_t",
    (
      "int32_t",
      "uint8_t",
      "uint32_t *",
      "double *",
      "uint32_t",
    ),
  ),
  "celestial_solar_lon_root_discriminant": (
    "CelestialDiscriminant",
    (
      "int32_t",
      "double",
    ),
  ),
  "celestial_solar_lon_roots": (
    "uint32_t",
    (
      "int32_t",
      "double",
      "double *",
      "uint32_t",
    ),
  ),
  "celestial_new_moons_after_jde": (
    "uint32_t",
    (
      "double",
      "double *",
      "uint32_t",
    ),
  ),
  "celestial_new_moons_in_year": (
    "uint32_t",
    (
      "int32_t",
      "uint32_t *",
      "double *",
      "uint32_t",
    ),
  ),
  "celestial_equation_of_time": (
    "CelestialEquationOfTime",
    ("double",),
  ),
  "celestial_apparent_solar_time": (
    "CelestialApparentSolarTime",
    (
      "int32_t",
      "uint32_t",
      "uint32_t",
      "double",
      "double",
    ),
  ),
  "celestial_local_apparent_sidereal_time": (
    "CelestialSiderealTime",
    (
      "double",
      "double",
    ),
  ),
  "celestial_query_jieqi_moment": (
    "CelestialJieqiMomentQuery",
    (
      "int32_t",
      "uint8_t",
    ),
  ),
  "celestial_get_jieqi_name": (
    "uint32_t",
    (
      "uint8_t",
      "char *",
      "uint32_t",
    ),
  ),
  "celestial_get_supported_lunar_year_range": (
    "CelestialSupportedLunarYearRange",
    ("uint8_t",),
  ),
  "celestial_get_lunar_year_info": (
    "CelestialLunarYearInfo",
    (
      "uint8_t",
      "int32_t",
    ),
  ),
  "celestial_gregorian_to_lunar": (
    "CelestialLunarDate",
    (
      "uint8_t",
      "int32_t",
      "uint8_t",
      "uint8_t",
    ),
  ),
  "celestial_lunar_to_gregorian": (
    "CelestialGregorianDate",
    (
      "uint8_t",
      "int32_t",
      "uint8_t",
      "bool",
      "uint8_t",
    ),
  ),
  "celestial_delta_t": (
    "CelestialDeltaT",
    (
      "double",
      "uint32_t",
    ),
  ),
  "celestial_chart_snapshot": (
    "CelestialChartSnapshot",
    (
      "int32_t",
      "uint32_t",
      "uint32_t",
      "double",
      "uint32_t",
      "double",
      "double",
      "uint32_t",
      "uint32_t",
    ),
  ),
  "celestial_longitude_events": (
    "uint32_t",
    (
      "uint32_t",
      "double",
      "double",
      "double",
      "uint32_t *",
      "CelestialLongitudeEventRecord *",
      "uint32_t",
    ),
  ),
  "celestial_longitude_stations": (
    "uint32_t",
    (
      "uint32_t",
      "double",
      "double",
      "uint32_t *",
      "CelestialStationEventRecord *",
      "uint32_t",
    ),
  ),
  "celestial_longitude_difference_events": (
    "uint32_t",
    (
      "uint32_t",
      "uint32_t",
      "double",
      "double",
      "double",
      "uint32_t *",
      "CelestialLongitudeDifferenceEventRecord *",
      "uint32_t",
    ),
  ),
  "celestial_capabilities": (
    "uint32_t",
    (
      "uint32_t",
      "uint32_t",
      "uint32_t *",
      "CelestialCapabilityRecord *",
      "uint32_t",
    ),
  ),
  "celestial_model_identities": (
    "uint32_t",
    (
      "uint32_t",
      "uint32_t *",
      "CelestialModelIdentityRecord *",
      "uint32_t",
    ),
  ),
  "celestial_model_identity_text": (
    "uint32_t",
    (
      "uint32_t",
      "uint32_t",
      "char *",
      "uint32_t",
    ),
  ),
  "celestial_capability_text": (
    "uint32_t",
    (
      "uint32_t",
      "uint32_t",
      "char *",
      "uint32_t",
    ),
  ),
  "celestial_positions": (
    "uint32_t",
    (
      "const uint32_t *",
      "uint32_t",
      "double",
      "uint32_t *",
      "CelestialPositionRecord *",
      "uint32_t",
    ),
  ),
  "celestial_equatorial_positions": (
    "uint32_t",
    (
      "const uint32_t *",
      "uint32_t",
      "double",
      "uint32_t *",
      "CelestialEquatorialPositionRecord *",
      "uint32_t",
    ),
  ),
  "celestial_house_query": (
    "CelestialHouseQueryResult",
    (
      "int32_t",
      "uint32_t",
      "uint32_t",
      "double",
      "uint32_t",
      "uint32_t",
      "uint32_t",
      "double",
      "double",
      "uint32_t",
    ),
  ),
  "celestial_house_capability": (
    "CelestialCivilHouseCapability",
    ("uint32_t",),
  ),
}

C_CODES = {
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
  "CELESTIAL_MODEL_STATUS_FROZEN": 1,
}

C_PARAMETERS = {
  "celestial_last_error": [],
  "celestial_last_error_kind": [],
  "celestial_ut1_to_jd": [
    "y",
    "m",
    "d",
    "fraction",
  ],
  "celestial_ut1_to_jde": [
    "y",
    "m",
    "d",
    "fraction",
  ],
  "celestial_jde_to_ut1": [
    "jde",
  ],
  "celestial_sun_apparent_geocentric_coord": [
    "jde",
  ],
  "celestial_moon_apparent_geocentric_coord": [
    "jde",
  ],
  "celestial_moon_illumination": [
    "jde",
  ],
  "celestial_moon_position_angle": [
    "jde",
  ],
  "celestial_moon_phase_moments": [
    "year",
    "phase_kind",
    "root_count",
    "slots",
    "slot_count",
  ],
  "celestial_solar_lon_root_discriminant": [
    "year",
    "longitude",
  ],
  "celestial_solar_lon_roots": [
    "year",
    "longitude",
    "slots",
    "slot_count",
  ],
  "celestial_new_moons_after_jde": [
    "jde",
    "slots",
    "slot_count",
  ],
  "celestial_new_moons_in_year": [
    "year",
    "root_count",
    "slots",
    "slot_count",
  ],
  "celestial_equation_of_time": [
    "jde",
  ],
  "celestial_apparent_solar_time": [
    "y",
    "m",
    "d",
    "fraction",
    "longitude",
  ],
  "celestial_local_apparent_sidereal_time": [
    "jd_ut1",
    "longitude",
  ],
  "celestial_query_jieqi_moment": [
    "year",
    "jq_idx",
  ],
  "celestial_get_jieqi_name": [
    "jq_idx",
    "buf",
    "buf_size",
  ],
  "celestial_get_supported_lunar_year_range": [
    "algo",
  ],
  "celestial_get_lunar_year_info": [
    "algo",
    "year",
  ],
  "celestial_gregorian_to_lunar": [
    "algo",
    "year",
    "month",
    "day",
  ],
  "celestial_lunar_to_gregorian": [
    "algo",
    "year",
    "month",
    "is_leap",
    "day",
  ],
  "celestial_delta_t": [
    "year",
    "model_code",
  ],
  "celestial_chart_snapshot": [
    "year",
    "month",
    "day",
    "fraction",
    "civil_scale_code",
    "latitude_deg",
    "longitude_deg",
    "house_system_code",
    "delta_t_model_code",
  ],
  "celestial_longitude_events": [
    "target_code",
    "target_longitude_deg",
    "start_jde_tt",
    "end_jde_tt",
    "event_count",
    "slots",
    "slot_count",
  ],
  "celestial_longitude_stations": [
    "target_code",
    "start_jde_tt",
    "end_jde_tt",
    "event_count",
    "slots",
    "slot_count",
  ],
  "celestial_longitude_difference_events": [
    "first_target_code",
    "second_target_code",
    "target_difference_deg",
    "start_jde_tt",
    "end_jde_tt",
    "event_count",
    "slots",
    "slot_count",
  ],
  "celestial_capabilities": [
    "operation_code",
    "subject_code",
    "record_count",
    "slots",
    "slot_count",
  ],
  "celestial_model_identities": [
    "kind_code",
    "record_count",
    "slots",
    "slot_count",
  ],
  "celestial_model_identity_text": [
    "kind_code",
    "identity_code",
    "buffer",
    "capacity",
  ],
  "celestial_capability_text": [
    "operation_code",
    "subject_code",
    "buffer",
    "capacity",
  ],
  "celestial_positions": [
    "target_codes",
    "target_count",
    "jde_tt",
    "record_count",
    "slots",
    "slot_count",
  ],
  "celestial_equatorial_positions": [
    "target_codes",
    "target_count",
    "jde_tt",
    "record_count",
    "slots",
    "slot_count",
  ],
  "celestial_house_query": [
    "year",
    "month",
    "day",
    "fraction",
    "civil_scale_code",
    "delta_t_model_code",
    "nutation_model_code",
    "latitude_deg",
    "longitude_deg",
    "house_system_code",
  ],
  "celestial_house_capability": [
    "delta_t_model_code",
  ],
}

PROTOCOLS = {
  "celestial_last_error": {
    "kind": "borrowed-string",
    "error_reader": "message",
    "lifetime": "next-recording-call",
    "error_channel": "preserve",
  },
  "celestial_last_error_kind": {
    "kind": "primitive",
    "error_reader": "kind",
    "error_channel": "preserve",
  },
  "celestial_ut1_to_jd": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_ut1_to_jde": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_jde_to_ut1": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_sun_apparent_geocentric_coord": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_moon_apparent_geocentric_coord": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_moon_illumination": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_moon_position_angle": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_moon_phase_moments": {
    "kind": "count-fill",
    "count_parameter": "root_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": False,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "error_channel": "kind",
  },
  "celestial_solar_lon_root_discriminant": {
    "kind": "companion-count",
    "error_channel": "kind",
  },
  "celestial_solar_lon_roots": {
    "kind": "companion-fill",
    "count_export": "celestial_solar_lon_root_discriminant",
    "legal_empty": True,
    "written_policy": "min-total-capacity",
    "error_channel": "kind",
  },
  "celestial_new_moons_after_jde": {
    "kind": "requested-fill",
    "capacity_parameter": "slot_count",
    "legal_empty": True,
    "written_policy": "exact-requested-count",
    "null_slots_requires_zero_capacity": True,
    "error_channel": "kind",
  },
  "celestial_new_moons_in_year": {
    "kind": "count-fill",
    "count_parameter": "root_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": False,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "error_channel": "kind",
  },
  "celestial_equation_of_time": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_apparent_solar_time": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_local_apparent_sidereal_time": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_query_jieqi_moment": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_get_jieqi_name": {
    "kind": "sized-string",
    "buffer_parameter": "buf",
    "capacity_parameter": "buf_size",
    "size_includes_nul": True,
    "null_requires_zero_capacity": True,
    "truncates": True,
    "terminates": True,
    "error_channel": "kind",
  },
  "celestial_get_supported_lunar_year_range": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_get_lunar_year_info": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_gregorian_to_lunar": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_lunar_to_gregorian": {
    "kind": "struct",
    "error_channel": "kind",
  },
  "celestial_delta_t": {
    "kind": "struct",
    "identity": "resolved-explicit-model-key",
    "error_channel": "kind",
  },
  "celestial_chart_snapshot": {
    "kind": "struct",
    "ownership": "by-value snapshot of 14 bodies and 12 cusps",
    "identity": "resolved-explicit-model-keys-and-per-body-source",
    "error_channel": "kind",
  },
  "celestial_longitude_events": {
    "kind": "count-fill",
    "count_parameter": "event_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": True,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "error_channel": "kind",
  },
  "celestial_longitude_stations": {
    "kind": "count-fill",
    "count_parameter": "event_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": True,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "error_channel": "kind",
  },
  "celestial_longitude_difference_events": {
    "kind": "count-fill",
    "count_parameter": "event_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": True,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "error_channel": "kind",
  },
  "celestial_capabilities": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": False,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "operations": [
      "CELESTIAL_OPERATION_POSITION",
      "CELESTIAL_OPERATION_LONGITUDE_RATE",
      "CELESTIAL_OPERATION_EVENTS",
      "CELESTIAL_OPERATION_EQUATORIAL",
    ],
    "subject_policy": "concrete-target-or-all",
    "error_channel": "kind",
  },
  "celestial_model_identities": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": False,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "identity": "kind-and-explicit-code",
    "error_channel": "kind",
  },
  "celestial_model_identity_text": {
    "kind": "sized-string",
    "buffer_parameter": "buffer",
    "capacity_parameter": "capacity",
    "size_includes_nul": True,
    "null_requires_zero_capacity": True,
    "truncates": True,
    "terminates": True,
    "identity": "kind-and-explicit-code",
    "error_channel": "kind",
  },
  "celestial_capability_text": {
    "kind": "sized-string",
    "buffer_parameter": "buffer",
    "capacity_parameter": "capacity",
    "size_includes_nul": True,
    "null_requires_zero_capacity": True,
    "truncates": True,
    "terminates": True,
    "subject_policy": "concrete-target-for-target-operations-all-for-nontarget-operations",
    "nontarget_operations": [
      "CELESTIAL_OPERATION_HOUSES",
      "CELESTIAL_OPERATION_CHART",
      "CELESTIAL_OPERATION_DELTA_T",
    ],
    "error_channel": "kind",
  },
  "celestial_positions": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": False,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "selection_policy": "nonempty-distinct-targets-in-caller-order",
    "error_channel": "kind",
  },
  "celestial_equatorial_positions": {
    "kind": "count-fill",
    "count_parameter": "record_count",
    "slots_parameter": "slots",
    "capacity_parameter": "slot_count",
    "legal_empty": False,
    "total_must_match": True,
    "written_policy": "min-total-capacity",
    "null_slots_requires_zero_capacity": True,
    "selection_policy": "nonempty-distinct-targets-in-caller-order",
    "error_channel": "kind",
  },
  "celestial_house_query": {
    "kind": "struct",
    "identity": "resolved-explicit-model-keys",
    "error_channel": "kind",
  },
  "celestial_house_capability": {
    "kind": "struct",
    "error_channel": "kind",
  },
}
