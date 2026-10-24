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

/* Independent native64/wasm32 C11 expectations. Binding generation must never update these pins. */
#include "celestial.h"
#include <stddef.h>

#define ABI_RECORD(T, S, A) \
  _Static_assert(sizeof(T) == S && _Alignof(T) == A, #T " layout drifted")
#define ABI_FIELD(T, F, K, O) \
  _Static_assert(offsetof(T, F) == O && _Generic(&((T*)0)->F, K*: 1, default: 0), #T "." #F " drifted")
#define ABI_ARRAY(T, F, K, N, O, S) \
  _Static_assert(offsetof(T, F) == O && _Generic(&((T*)0)->F, K (*)[N]: 1, default: 0) && \
                 sizeof(((T*)0)->F[0]) == S && \
                 offsetof(T, F[1]) - offsetof(T, F[0]) == S, #T "." #F " array drifted")
#define ABI_CODE(N, V) _Static_assert(N == UINT32_C(V), #N " code drifted")

ABI_RECORD(CelestialJulianDay, 16, 8);
ABI_FIELD(CelestialJulianDay, valid, bool, 0);
ABI_FIELD(CelestialJulianDay, value, double, 8);

ABI_RECORD(CelestialUT1Time, 24, 8);
ABI_FIELD(CelestialUT1Time, valid, bool, 0);
ABI_FIELD(CelestialUT1Time, year, int32_t, 4);
ABI_FIELD(CelestialUT1Time, month, uint32_t, 8);
ABI_FIELD(CelestialUT1Time, day, uint32_t, 12);
ABI_FIELD(CelestialUT1Time, fraction, double, 16);

ABI_RECORD(CelestialSunCoordinate, 32, 8);
ABI_FIELD(CelestialSunCoordinate, valid, bool, 0);
ABI_FIELD(CelestialSunCoordinate, lon, double, 8);
ABI_FIELD(CelestialSunCoordinate, lat, double, 16);
ABI_FIELD(CelestialSunCoordinate, r, double, 24);

ABI_RECORD(CelestialMoonCoordinate, 32, 8);
ABI_FIELD(CelestialMoonCoordinate, valid, bool, 0);
ABI_FIELD(CelestialMoonCoordinate, lon, double, 8);
ABI_FIELD(CelestialMoonCoordinate, lat, double, 16);
ABI_FIELD(CelestialMoonCoordinate, r, double, 24);

ABI_RECORD(CelestialMoonIllumination, 24, 8);
ABI_FIELD(CelestialMoonIllumination, valid, bool, 0);
ABI_FIELD(CelestialMoonIllumination, illumination, double, 8);
ABI_FIELD(CelestialMoonIllumination, elongation_deg, double, 16);

ABI_RECORD(CelestialMoonPositionAngle, 16, 8);
ABI_FIELD(CelestialMoonPositionAngle, valid, bool, 0);
ABI_FIELD(CelestialMoonPositionAngle, angle_deg, double, 8);

ABI_RECORD(CelestialDiscriminant, 8, 4);
ABI_FIELD(CelestialDiscriminant, valid, bool, 0);
ABI_FIELD(CelestialDiscriminant, count, uint32_t, 4);

ABI_RECORD(CelestialEquationOfTime, 16, 8);
ABI_FIELD(CelestialEquationOfTime, valid, bool, 0);
ABI_FIELD(CelestialEquationOfTime, value, double, 8);

ABI_RECORD(CelestialApparentSolarTime, 24, 8);
ABI_FIELD(CelestialApparentSolarTime, valid, bool, 0);
ABI_FIELD(CelestialApparentSolarTime, year, int32_t, 4);
ABI_FIELD(CelestialApparentSolarTime, month, uint32_t, 8);
ABI_FIELD(CelestialApparentSolarTime, day, uint32_t, 12);
ABI_FIELD(CelestialApparentSolarTime, fraction, double, 16);

ABI_RECORD(CelestialSiderealTime, 16, 8);
ABI_FIELD(CelestialSiderealTime, valid, bool, 0);
ABI_FIELD(CelestialSiderealTime, value, double, 8);

ABI_RECORD(CelestialJieqiMomentQuery, 24, 8);
ABI_FIELD(CelestialJieqiMomentQuery, valid, bool, 0);
ABI_FIELD(CelestialJieqiMomentQuery, jq_idx, uint8_t, 1);
ABI_FIELD(CelestialJieqiMomentQuery, y, int32_t, 4);
ABI_FIELD(CelestialJieqiMomentQuery, m, uint32_t, 8);
ABI_FIELD(CelestialJieqiMomentQuery, d, uint32_t, 12);
ABI_FIELD(CelestialJieqiMomentQuery, frac, double, 16);

ABI_RECORD(CelestialSupportedLunarYearRange, 12, 4);
ABI_FIELD(CelestialSupportedLunarYearRange, valid, bool, 0);
ABI_FIELD(CelestialSupportedLunarYearRange, start, int32_t, 4);
ABI_FIELD(CelestialSupportedLunarYearRange, end, int32_t, 8);

ABI_RECORD(CelestialLunarYearInfo, 16, 4);
ABI_FIELD(CelestialLunarYearInfo, valid, bool, 0);
ABI_FIELD(CelestialLunarYearInfo, year, int32_t, 4);
ABI_FIELD(CelestialLunarYearInfo, month, uint8_t, 8);
ABI_FIELD(CelestialLunarYearInfo, day, uint8_t, 9);
ABI_FIELD(CelestialLunarYearInfo, leap_month, uint8_t, 10);
ABI_FIELD(CelestialLunarYearInfo, month_len, uint16_t, 12);

ABI_RECORD(CelestialLunarDate, 12, 4);
ABI_FIELD(CelestialLunarDate, valid, bool, 0);
ABI_FIELD(CelestialLunarDate, year, int32_t, 4);
ABI_FIELD(CelestialLunarDate, month, uint8_t, 8);
ABI_FIELD(CelestialLunarDate, is_leap, bool, 9);
ABI_FIELD(CelestialLunarDate, day, uint8_t, 10);

ABI_RECORD(CelestialGregorianDate, 12, 4);
ABI_FIELD(CelestialGregorianDate, valid, bool, 0);
ABI_FIELD(CelestialGregorianDate, year, int32_t, 4);
ABI_FIELD(CelestialGregorianDate, month, uint8_t, 8);
ABI_FIELD(CelestialGregorianDate, day, uint8_t, 9);

ABI_RECORD(CelestialDeltaT, 16, 8);
ABI_FIELD(CelestialDeltaT, valid, bool, 0);
ABI_FIELD(CelestialDeltaT, model_identity_code, uint32_t, 4);
ABI_FIELD(CelestialDeltaT, value, double, 8);

ABI_RECORD(CelestialChartBody, 48, 8);
ABI_FIELD(CelestialChartBody, target_code, uint32_t, 0);
ABI_FIELD(CelestialChartBody, source_code, uint32_t, 4);
ABI_FIELD(CelestialChartBody, present_fields, uint32_t, 8);
ABI_FIELD(CelestialChartBody, longitude_deg, double, 16);
ABI_FIELD(CelestialChartBody, latitude_deg, double, 24);
ABI_FIELD(CelestialChartBody, distance_au, double, 32);
ABI_FIELD(CelestialChartBody, longitude_rate_deg_per_tt_day, double, 40);

ABI_RECORD(CelestialChartHouses, 128, 8);
ABI_FIELD(CelestialChartHouses, ascendant_deg, double, 0);
ABI_FIELD(CelestialChartHouses, midheaven_deg, double, 8);
ABI_FIELD(CelestialChartHouses, descendant_deg, double, 16);
ABI_FIELD(CelestialChartHouses, imum_coeli_deg, double, 24);
ABI_ARRAY(CelestialChartHouses, cusps_deg, double, 12, 32, 8);

ABI_RECORD(CelestialChartSnapshot, 832, 8);
ABI_FIELD(CelestialChartSnapshot, valid, bool, 0);
ABI_FIELD(CelestialChartSnapshot, delta_t_identity_code, uint32_t, 4);
ABI_FIELD(CelestialChartSnapshot, nutation_identity_code, uint32_t, 8);
ABI_FIELD(CelestialChartSnapshot, jd_ut1, double, 16);
ABI_FIELD(CelestialChartSnapshot, jde_tt, double, 24);
ABI_ARRAY(CelestialChartSnapshot, bodies, CelestialChartBody, 14, 32, 48);
ABI_FIELD(CelestialChartSnapshot, houses, CelestialChartHouses, 704);

ABI_RECORD(CelestialLongitudeEventRecord, 32, 8);
ABI_FIELD(CelestialLongitudeEventRecord, jde_tt, double, 0);
ABI_FIELD(CelestialLongitudeEventRecord, longitude_deg, double, 8);
ABI_FIELD(CelestialLongitudeEventRecord, longitude_rate_deg_per_tt_day, double, 16);
ABI_FIELD(CelestialLongitudeEventRecord, direction_code, uint32_t, 24);

ABI_RECORD(CelestialStationEventRecord, 32, 8);
ABI_FIELD(CelestialStationEventRecord, jde_tt, double, 0);
ABI_FIELD(CelestialStationEventRecord, longitude_deg, double, 8);
ABI_FIELD(CelestialStationEventRecord, longitude_rate_deg_per_tt_day, double, 16);
ABI_FIELD(CelestialStationEventRecord, motion_before_code, uint32_t, 24);
ABI_FIELD(CelestialStationEventRecord, motion_after_code, uint32_t, 28);

ABI_RECORD(CelestialLongitudeDifferenceEventRecord, 56, 8);
ABI_FIELD(CelestialLongitudeDifferenceEventRecord, jde_tt, double, 0);
ABI_FIELD(CelestialLongitudeDifferenceEventRecord, longitude_a_deg, double, 8);
ABI_FIELD(CelestialLongitudeDifferenceEventRecord, longitude_b_deg, double, 16);
ABI_FIELD(CelestialLongitudeDifferenceEventRecord, longitude_rate_a_deg_per_tt_day, double, 24);
ABI_FIELD(CelestialLongitudeDifferenceEventRecord, longitude_rate_b_deg_per_tt_day, double, 32);
ABI_FIELD(CelestialLongitudeDifferenceEventRecord, signed_difference_deg, double, 40);
ABI_FIELD(CelestialLongitudeDifferenceEventRecord, direction_code, uint32_t, 48);

ABI_RECORD(CelestialCapabilityRecord, 40, 8);
ABI_FIELD(CelestialCapabilityRecord, operation_code, uint32_t, 0);
ABI_FIELD(CelestialCapabilityRecord, subject_code, uint32_t, 4);
ABI_FIELD(CelestialCapabilityRecord, status_code, uint32_t, 8);
ABI_FIELD(CelestialCapabilityRecord, source_code, uint32_t, 12);
ABI_FIELD(CelestialCapabilityRecord, domain_start_jde_tt, double, 16);
ABI_FIELD(CelestialCapabilityRecord, domain_end_jde_tt, double, 24);
ABI_FIELD(CelestialCapabilityRecord, maximum_span_tt_days, uint32_t, 32);

ABI_RECORD(CelestialModelIdentityRecord, 32, 8);
ABI_FIELD(CelestialModelIdentityRecord, kind_code, uint32_t, 0);
ABI_FIELD(CelestialModelIdentityRecord, identity_code, uint32_t, 4);
ABI_FIELD(CelestialModelIdentityRecord, status_code, uint32_t, 8);
ABI_FIELD(CelestialModelIdentityRecord, domain_start_jde_tt, double, 16);
ABI_FIELD(CelestialModelIdentityRecord, domain_end_jde_tt, double, 24);

ABI_RECORD(CelestialPositionRecord, 40, 8);
ABI_FIELD(CelestialPositionRecord, target_code, uint32_t, 0);
ABI_FIELD(CelestialPositionRecord, present_fields, uint32_t, 4);
ABI_FIELD(CelestialPositionRecord, longitude_deg, double, 8);
ABI_FIELD(CelestialPositionRecord, latitude_deg, double, 16);
ABI_FIELD(CelestialPositionRecord, distance_au, double, 24);
ABI_FIELD(CelestialPositionRecord, source_code, uint32_t, 32);

ABI_RECORD(CelestialEquatorialPositionRecord, 40, 8);
ABI_FIELD(CelestialEquatorialPositionRecord, target_code, uint32_t, 0);
ABI_FIELD(CelestialEquatorialPositionRecord, present_fields, uint32_t, 4);
ABI_FIELD(CelestialEquatorialPositionRecord, right_ascension_deg, double, 8);
ABI_FIELD(CelestialEquatorialPositionRecord, declination_deg, double, 16);
ABI_FIELD(CelestialEquatorialPositionRecord, distance_au, double, 24);
ABI_FIELD(CelestialEquatorialPositionRecord, source_code, uint32_t, 32);

ABI_RECORD(CelestialHouseQueryResult, 160, 8);
ABI_FIELD(CelestialHouseQueryResult, valid, bool, 0);
ABI_FIELD(CelestialHouseQueryResult, house_system_code, uint32_t, 4);
ABI_FIELD(CelestialHouseQueryResult, delta_t_identity_code, uint32_t, 8);
ABI_FIELD(CelestialHouseQueryResult, nutation_identity_code, uint32_t, 12);
ABI_FIELD(CelestialHouseQueryResult, jd_ut1, double, 16);
ABI_FIELD(CelestialHouseQueryResult, jde_tt, double, 24);
ABI_FIELD(CelestialHouseQueryResult, houses, CelestialChartHouses, 32);

ABI_RECORD(CelestialCivilHouseCapability, 48, 8);
ABI_FIELD(CelestialCivilHouseCapability, valid, bool, 0);
ABI_FIELD(CelestialCivilHouseCapability, status_code, uint32_t, 4);
ABI_FIELD(CelestialCivilHouseCapability, civil_start_year, int32_t, 8);
ABI_FIELD(CelestialCivilHouseCapability, civil_end_year_exclusive, int32_t, 12);
ABI_FIELD(CelestialCivilHouseCapability, civil_scale_mask, uint32_t, 16);
ABI_FIELD(CelestialCivilHouseCapability, utc_start_year, int32_t, 20);
ABI_FIELD(CelestialCivilHouseCapability, utc_start_month, uint32_t, 24);
ABI_FIELD(CelestialCivilHouseCapability, utc_start_day, uint32_t, 28);
ABI_FIELD(CelestialCivilHouseCapability, delta_t_identity_code, uint32_t, 32);
ABI_FIELD(CelestialCivilHouseCapability, has_derived_tt_cutoff, bool, 36);
ABI_FIELD(CelestialCivilHouseCapability, derived_tt_end_jde_tt, double, 40);

ABI_CODE(CELESTIAL_ERROR_NONE, 0);
ABI_CODE(CELESTIAL_ERROR_INVALID_INPUT, 1);
ABI_CODE(CELESTIAL_ERROR_COMPUTATION, 2);
ABI_CODE(CELESTIAL_ERROR_RESOURCE, 3);
ABI_CODE(CELESTIAL_ERROR_INTERNAL, 4);
ABI_CODE(CELESTIAL_CIVIL_SCALE_UTC, 0);
ABI_CODE(CELESTIAL_CIVIL_SCALE_UT1, 1);
ABI_CODE(CELESTIAL_HOUSE_SYSTEM_EQUAL, 0);
ABI_CODE(CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN, 1);
ABI_CODE(CELESTIAL_HOUSE_SYSTEM_PLACIDUS, 2);
ABI_CODE(CELESTIAL_DELTA_T_MODEL_DEFAULT, 0);
ABI_CODE(CELESTIAL_DELTA_T_MODEL_ALGO1, 1);
ABI_CODE(CELESTIAL_DELTA_T_MODEL_ALGO2, 2);
ABI_CODE(CELESTIAL_DELTA_T_MODEL_ALGO3, 3);
ABI_CODE(CELESTIAL_DELTA_T_MODEL_ALGO4, 4);
ABI_CODE(CELESTIAL_DELTA_T_MODEL_ALGO5, 5);
ABI_CODE(CELESTIAL_TARGET_SUN, 0);
ABI_CODE(CELESTIAL_TARGET_MOON, 1);
ABI_CODE(CELESTIAL_TARGET_MERCURY, 2);
ABI_CODE(CELESTIAL_TARGET_VENUS, 3);
ABI_CODE(CELESTIAL_TARGET_MARS, 4);
ABI_CODE(CELESTIAL_TARGET_JUPITER, 5);
ABI_CODE(CELESTIAL_TARGET_SATURN, 6);
ABI_CODE(CELESTIAL_TARGET_URANUS, 7);
ABI_CODE(CELESTIAL_TARGET_NEPTUNE, 8);
ABI_CODE(CELESTIAL_TARGET_PLUTO, 9);
ABI_CODE(CELESTIAL_TARGET_MEAN_ASCENDING, 10);
ABI_CODE(CELESTIAL_TARGET_MEAN_DESCENDING, 11);
ABI_CODE(CELESTIAL_TARGET_TRUE_ASCENDING, 12);
ABI_CODE(CELESTIAL_TARGET_TRUE_DESCENDING, 13);
ABI_CODE(CELESTIAL_PRESENT_LATITUDE, 1);
ABI_CODE(CELESTIAL_PRESENT_DISTANCE, 2);
ABI_CODE(CELESTIAL_EVENT_DIRECTION_INCREASING, 0);
ABI_CODE(CELESTIAL_EVENT_DIRECTION_DECREASING, 1);
ABI_CODE(CELESTIAL_EVENT_DIRECTION_TANGENT, 2);
ABI_CODE(CELESTIAL_EVENT_MOTION_DIRECT, 0);
ABI_CODE(CELESTIAL_EVENT_MOTION_RETROGRADE, 1);
ABI_CODE(CELESTIAL_OPERATION_POSITION, 0);
ABI_CODE(CELESTIAL_OPERATION_LONGITUDE_RATE, 1);
ABI_CODE(CELESTIAL_OPERATION_EVENTS, 2);
ABI_CODE(CELESTIAL_OPERATION_HOUSES, 3);
ABI_CODE(CELESTIAL_OPERATION_EQUATORIAL, 4);
ABI_CODE(CELESTIAL_OPERATION_CHART, 5);
ABI_CODE(CELESTIAL_OPERATION_DELTA_T, 6);
ABI_CODE(CELESTIAL_STATUS_SOURCE_WINDOW, 0);
ABI_CODE(CELESTIAL_STATUS_IMPLEMENTATION_WINDOW, 1);
ABI_CODE(CELESTIAL_STATUS_FINITE_INPUT_EVALUATION, 2);
ABI_CODE(CELESTIAL_POSITION_SOURCE_SUN_VSOP87D, 1);
ABI_CODE(CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC, 2);
ABI_CODE(CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, 3);
ABI_CODE(CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT, 4);
ABI_CODE(CELESTIAL_POSITION_SOURCE_NODE_MEAN, 5);
ABI_CODE(CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM, 6);
ABI_CODE(CELESTIAL_TARGET_ALL, 0xFFFFFFFF);
ABI_CODE(CELESTIAL_MODEL_KIND_DELTA_T, 0);
ABI_CODE(CELESTIAL_MODEL_KIND_NUTATION, 1);
ABI_CODE(CELESTIAL_MODEL_KIND_POSITION_SOURCE, 2);
ABI_CODE(CELESTIAL_MODEL_KIND_ALL, 0xFFFFFFFF);
ABI_CODE(CELESTIAL_MODEL_STATUS_CURRENT, 0);
ABI_CODE(CELESTIAL_MODEL_STATUS_FROZEN, 1);
ABI_CODE(CELESTIAL_NUTATION_MODEL_DEFAULT, 0);
ABI_CODE(CELESTIAL_NUTATION_MODEL_MEEUS, 1);
ABI_CODE(CELESTIAL_NUTATION_MODEL_IAU_1980, 2);

/* Function types include pointee constness, which ctypes cannot express. */
_Static_assert(_Generic(&celestial_last_error,
               const char * (*)(void): 1,
               default: 0) != 0, "celestial_last_error signature drifted");
_Static_assert(_Generic(&celestial_last_error_kind,
               uint32_t (*)(void): 1,
               default: 0) != 0, "celestial_last_error_kind signature drifted");
_Static_assert(_Generic(&celestial_ut1_to_jd,
               CelestialJulianDay (*)(int32_t, uint32_t, uint32_t, double): 1,
               default: 0) != 0, "celestial_ut1_to_jd signature drifted");
_Static_assert(_Generic(&celestial_ut1_to_jde,
               CelestialJulianDay (*)(int32_t, uint32_t, uint32_t, double): 1,
               default: 0) != 0, "celestial_ut1_to_jde signature drifted");
_Static_assert(_Generic(&celestial_jde_to_ut1,
               CelestialUT1Time (*)(double): 1,
               default: 0) != 0, "celestial_jde_to_ut1 signature drifted");
_Static_assert(_Generic(&celestial_sun_apparent_geocentric_coord,
               CelestialSunCoordinate (*)(double): 1,
               default: 0) != 0, "celestial_sun_apparent_geocentric_coord signature drifted");
_Static_assert(_Generic(&celestial_moon_apparent_geocentric_coord,
               CelestialMoonCoordinate (*)(double): 1,
               default: 0) != 0, "celestial_moon_apparent_geocentric_coord signature drifted");
_Static_assert(_Generic(&celestial_moon_illumination,
               CelestialMoonIllumination (*)(double): 1,
               default: 0) != 0, "celestial_moon_illumination signature drifted");
_Static_assert(_Generic(&celestial_moon_position_angle,
               CelestialMoonPositionAngle (*)(double): 1,
               default: 0) != 0, "celestial_moon_position_angle signature drifted");
_Static_assert(_Generic(&celestial_moon_phase_moments,
               uint32_t (*)(int32_t, uint8_t, uint32_t*, double*, uint32_t): 1,
               default: 0) != 0, "celestial_moon_phase_moments signature drifted");
_Static_assert(_Generic(&celestial_solar_lon_root_discriminant,
               CelestialDiscriminant (*)(int32_t, double): 1,
               default: 0) != 0, "celestial_solar_lon_root_discriminant signature drifted");
_Static_assert(_Generic(&celestial_solar_lon_roots,
               uint32_t (*)(int32_t, double, double*, uint32_t): 1,
               default: 0) != 0, "celestial_solar_lon_roots signature drifted");
_Static_assert(_Generic(&celestial_new_moons_after_jde,
               uint32_t (*)(double, double*, uint32_t): 1,
               default: 0) != 0, "celestial_new_moons_after_jde signature drifted");
_Static_assert(_Generic(&celestial_new_moons_in_year,
               uint32_t (*)(int32_t, uint32_t*, double*, uint32_t): 1,
               default: 0) != 0, "celestial_new_moons_in_year signature drifted");
_Static_assert(_Generic(&celestial_equation_of_time,
               CelestialEquationOfTime (*)(double): 1,
               default: 0) != 0, "celestial_equation_of_time signature drifted");
_Static_assert(_Generic(&celestial_apparent_solar_time,
               CelestialApparentSolarTime (*)(int32_t, uint32_t, uint32_t, double, double): 1,
               default: 0) != 0, "celestial_apparent_solar_time signature drifted");
_Static_assert(_Generic(&celestial_local_apparent_sidereal_time,
               CelestialSiderealTime (*)(double, double): 1,
               default: 0) != 0, "celestial_local_apparent_sidereal_time signature drifted");
_Static_assert(_Generic(&celestial_query_jieqi_moment,
               CelestialJieqiMomentQuery (*)(int32_t, uint8_t): 1,
               default: 0) != 0, "celestial_query_jieqi_moment signature drifted");
_Static_assert(_Generic(&celestial_get_jieqi_name,
               uint32_t (*)(uint8_t, char*, uint32_t): 1,
               default: 0) != 0, "celestial_get_jieqi_name signature drifted");
_Static_assert(_Generic(&celestial_get_supported_lunar_year_range,
               CelestialSupportedLunarYearRange (*)(uint8_t): 1,
               default: 0) != 0, "celestial_get_supported_lunar_year_range signature drifted");
_Static_assert(_Generic(&celestial_get_lunar_year_info,
               CelestialLunarYearInfo (*)(uint8_t, int32_t): 1,
               default: 0) != 0, "celestial_get_lunar_year_info signature drifted");
_Static_assert(_Generic(&celestial_gregorian_to_lunar,
               CelestialLunarDate (*)(uint8_t, int32_t, uint8_t, uint8_t): 1,
               default: 0) != 0, "celestial_gregorian_to_lunar signature drifted");
_Static_assert(_Generic(&celestial_lunar_to_gregorian,
               CelestialGregorianDate (*)(uint8_t, int32_t, uint8_t, bool, uint8_t): 1,
               default: 0) != 0, "celestial_lunar_to_gregorian signature drifted");
_Static_assert(_Generic(&celestial_delta_t,
               CelestialDeltaT (*)(double, uint32_t): 1,
               default: 0) != 0, "celestial_delta_t signature drifted");
_Static_assert(_Generic(&celestial_chart_snapshot,
               CelestialChartSnapshot (*)(int32_t, uint32_t, uint32_t, double, uint32_t, double, double, uint32_t, uint32_t): 1,
               default: 0) != 0, "celestial_chart_snapshot signature drifted");
_Static_assert(_Generic(&celestial_longitude_events,
               uint32_t (*)(uint32_t, double, double, double, uint32_t*, CelestialLongitudeEventRecord*, uint32_t): 1,
               default: 0) != 0, "celestial_longitude_events signature drifted");
_Static_assert(_Generic(&celestial_longitude_stations,
               uint32_t (*)(uint32_t, double, double, uint32_t*, CelestialStationEventRecord*, uint32_t): 1,
               default: 0) != 0, "celestial_longitude_stations signature drifted");
_Static_assert(_Generic(&celestial_longitude_difference_events,
               uint32_t (*)(uint32_t, uint32_t, double, double, double, uint32_t*, CelestialLongitudeDifferenceEventRecord*, uint32_t): 1,
               default: 0) != 0, "celestial_longitude_difference_events signature drifted");
_Static_assert(_Generic(&celestial_capabilities,
               uint32_t (*)(uint32_t, uint32_t, uint32_t*, CelestialCapabilityRecord*, uint32_t): 1,
               default: 0) != 0, "celestial_capabilities signature drifted");
_Static_assert(_Generic(&celestial_model_identities,
               uint32_t (*)(uint32_t, uint32_t*, CelestialModelIdentityRecord*, uint32_t): 1,
               default: 0) != 0, "celestial_model_identities signature drifted");
_Static_assert(_Generic(&celestial_model_identity_text,
               uint32_t (*)(uint32_t, uint32_t, char*, uint32_t): 1,
               default: 0) != 0, "celestial_model_identity_text signature drifted");
_Static_assert(_Generic(&celestial_capability_text,
               uint32_t (*)(uint32_t, uint32_t, char*, uint32_t): 1,
               default: 0) != 0, "celestial_capability_text signature drifted");
_Static_assert(_Generic(&celestial_positions,
               uint32_t (*)(const uint32_t*, uint32_t, double, uint32_t*, CelestialPositionRecord*, uint32_t): 1,
               default: 0) != 0, "celestial_positions signature drifted");
_Static_assert(_Generic(&celestial_equatorial_positions,
               uint32_t (*)(const uint32_t*, uint32_t, double, uint32_t*, CelestialEquatorialPositionRecord*, uint32_t): 1,
               default: 0) != 0, "celestial_equatorial_positions signature drifted");
_Static_assert(_Generic(&celestial_house_query,
               CelestialHouseQueryResult (*)(int32_t, uint32_t, uint32_t, double, uint32_t, uint32_t, uint32_t, double, double, uint32_t): 1,
               default: 0) != 0, "celestial_house_query signature drifted");
_Static_assert(_Generic(&celestial_house_capability,
               CelestialCivilHouseCapability (*)(uint32_t): 1,
               default: 0) != 0, "celestial_house_capability signature drifted");
