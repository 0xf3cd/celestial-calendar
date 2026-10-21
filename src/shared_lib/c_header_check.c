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

/*
 * #67: compile `celestial.h` as pure C and pin the ABI — field offsets and struct sizes
 * are part of the published contract, so any layout drift fails the build here.
 * The absolute offsets below assume 64-bit natural alignment (the CI matrix is all 64-bit).
 */

#include "celestial.h"

#include <stddef.h>

_Static_assert(offsetof(JulianDay, value) == 8 && sizeof(JulianDay) == 16, "JulianDay layout drifted");

_Static_assert(offsetof(UT1Time, year) == 4 && offsetof(UT1Time, month) == 8 &&
               offsetof(UT1Time, day) == 12 && offsetof(UT1Time, fraction) == 16 &&
               sizeof(UT1Time) == 24, "UT1Time layout drifted");

_Static_assert(offsetof(SunCoordinate, lon) == 8 && offsetof(SunCoordinate, lat) == 16 &&
               offsetof(SunCoordinate, r) == 24 && sizeof(SunCoordinate) == 32,
               "SunCoordinate layout drifted");

_Static_assert(offsetof(MoonCoordinate, lon) == 8 && offsetof(MoonCoordinate, lat) == 16 &&
               offsetof(MoonCoordinate, r) == 24 && sizeof(MoonCoordinate) == 32,
               "MoonCoordinate layout drifted");

_Static_assert(offsetof(MoonIllumination, illumination) == 8 && offsetof(MoonIllumination, elongation_deg) == 16 &&
               sizeof(MoonIllumination) == 24, "MoonIllumination layout drifted");

_Static_assert(offsetof(MoonPositionAngle, angle_deg) == 8 && sizeof(MoonPositionAngle) == 16,
               "MoonPositionAngle layout drifted");

_Static_assert(offsetof(Discriminant, count) == 4 && sizeof(Discriminant) == 8, "Discriminant layout drifted");

_Static_assert(offsetof(JieqiMomentQuery, jq_idx) == 1 && offsetof(JieqiMomentQuery, y) == 4 &&
               offsetof(JieqiMomentQuery, m) == 8 && offsetof(JieqiMomentQuery, d) == 12 &&
               offsetof(JieqiMomentQuery, frac) == 16 && sizeof(JieqiMomentQuery) == 24,
               "JieqiMomentQuery layout drifted");

_Static_assert(offsetof(SupportedLunarYearRange, start) == 4 && offsetof(SupportedLunarYearRange, end) == 8 &&
               sizeof(SupportedLunarYearRange) == 12, "SupportedLunarYearRange layout drifted");

_Static_assert(offsetof(LunarYearInfo, year) == 4 && offsetof(LunarYearInfo, month) == 8 &&
               offsetof(LunarYearInfo, day) == 9 && offsetof(LunarYearInfo, leap_month) == 10 &&
               offsetof(LunarYearInfo, month_len) == 12 && sizeof(LunarYearInfo) == 16,
               "LunarYearInfo layout drifted");

_Static_assert(offsetof(DeltaT, value) == 8 && sizeof(DeltaT) == 16, "DeltaT layout drifted");

_Static_assert(offsetof(EquationOfTime, value) == 8 && sizeof(EquationOfTime) == 16,
               "EquationOfTime layout drifted");

_Static_assert(offsetof(SiderealTime, value) == 8 && sizeof(SiderealTime) == 16,
               "SiderealTime layout drifted");

_Static_assert(offsetof(ApparentSolarTime, year) == 4 && offsetof(ApparentSolarTime, month) == 8 &&
               offsetof(ApparentSolarTime, day) == 12 && offsetof(ApparentSolarTime, fraction) == 16 &&
               sizeof(ApparentSolarTime) == 24, "ApparentSolarTime layout drifted");

_Static_assert(offsetof(LunarDate, year) == 4 && offsetof(LunarDate, month) == 8 &&
               offsetof(LunarDate, is_leap) == 9 && offsetof(LunarDate, day) == 10 &&
               sizeof(LunarDate) == 12, "LunarDate layout drifted");

_Static_assert(offsetof(GregorianDate, year) == 4 && offsetof(GregorianDate, month) == 8 &&
               offsetof(GregorianDate, day) == 9 && sizeof(GregorianDate) == 12,
               "GregorianDate layout drifted");

_Static_assert(offsetof(ChartBodyV1, target_code) == 0 && offsetof(ChartBodyV1, present_fields) == 4 &&
               offsetof(ChartBodyV1, longitude_deg) == 8 && offsetof(ChartBodyV1, latitude_deg) == 16 &&
               offsetof(ChartBodyV1, distance_au) == 24 &&
               offsetof(ChartBodyV1, longitude_rate_deg_per_tt_day) == 32 &&
               sizeof(ChartBodyV1) == 40 && _Alignof(ChartBodyV1) == 8, "ChartBodyV1 layout drifted");

_Static_assert(offsetof(ChartHousesV1, ascendant_deg) == 0 && offsetof(ChartHousesV1, midheaven_deg) == 8 &&
               offsetof(ChartHousesV1, descendant_deg) == 16 && offsetof(ChartHousesV1, imum_coeli_deg) == 24 &&
               offsetof(ChartHousesV1, cusps_deg) == 32 && sizeof(ChartHousesV1) == 128 &&
               _Alignof(ChartHousesV1) == 8, "ChartHousesV1 layout drifted");

_Static_assert(offsetof(ChartSnapshotV1, valid) == 0 && offsetof(ChartSnapshotV1, jd_ut1) == 8 &&
               offsetof(ChartSnapshotV1, jde_tt) == 16 && offsetof(ChartSnapshotV1, bodies) == 24 &&
               offsetof(ChartSnapshotV1, houses) == 584 && sizeof(ChartSnapshotV1) == 712 &&
               _Alignof(ChartSnapshotV1) == 8, "ChartSnapshotV1 layout drifted");

_Static_assert((sizeof(((ChartSnapshotV1*)0)->bodies) / sizeof(ChartBodyV1) == 14 &&
               offsetof(ChartSnapshotV1, bodies[1]) - offsetof(ChartSnapshotV1, bodies[0]) == 40 &&
               offsetof(ChartSnapshotV1, bodies[13]) == 544) != 0, "ChartSnapshotV1 body array drifted");

/* #357: the capability records; the selectors mirror the chart target order and Delta T codes. */
_Static_assert(offsetof(CapabilityRecordV1, operation_code) == 0 && offsetof(CapabilityRecordV1, subject_code) == 4 &&
               offsetof(CapabilityRecordV1, status_code) == 8 && offsetof(CapabilityRecordV1, source_code) == 12 &&
               offsetof(CapabilityRecordV1, domain_start_jde_tt) == 16 && offsetof(CapabilityRecordV1, domain_end_jde_tt) == 24 &&
               offsetof(CapabilityRecordV1, maximum_span_tt_days) == 32 && offsetof(CapabilityRecordV1, flags) == 36 &&
               sizeof(CapabilityRecordV1) == 40 && _Alignof(CapabilityRecordV1) == 8, "CapabilityRecordV1 layout drifted");

_Static_assert(offsetof(ModelIdentityRecordV1, kind_code) == 0 && offsetof(ModelIdentityRecordV1, identity_code) == 4 &&
               offsetof(ModelIdentityRecordV1, status_code) == 8 && offsetof(ModelIdentityRecordV1, default_alias_code) == 12 &&
               offsetof(ModelIdentityRecordV1, domain_start_jde_tt) == 16 && offsetof(ModelIdentityRecordV1, domain_end_jde_tt) == 24 &&
               sizeof(ModelIdentityRecordV1) == 32 && _Alignof(ModelIdentityRecordV1) == 8,
               "ModelIdentityRecordV1 layout drifted");

_Static_assert(CAPABILITY_OPERATION_POSITION == 0 && CAPABILITY_OPERATION_LONGITUDE_RATE == 1 &&
               CAPABILITY_OPERATION_EVENTS == 2 && CAPABILITY_OPERATION_HOUSES == 3 &&
               CAPABILITY_OPERATION_MODEL_IDENTITY == 4, "Capability operation codes drifted");

_Static_assert(CAPABILITY_STATUS_SOURCE_WINDOW == 0 && CAPABILITY_STATUS_IMPLEMENTATION_WINDOW == 1 &&
               CAPABILITY_STATUS_UNBOUNDED_FINITE == 2, "Capability status codes drifted");

_Static_assert(CAPABILITY_SOURCE_SUN_VSOP87D == 0 && CAPABILITY_SOURCE_MOON_ELP2000_82B_TRUNC == 1 &&
               CAPABILITY_SOURCE_PLANET_VSOP87D == 2 && CAPABILITY_SOURCE_PLUTO_PLUTO95_APPARENT == 3 &&
               CAPABILITY_SOURCE_NODE_MEAN == 4 && CAPABILITY_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM == 5,
               "Capability source codes drifted");

_Static_assert(MODEL_KIND_DELTA_T == 0 && MODEL_KIND_NUTATION == 1 && MODEL_KIND_POSITION_SOURCE == 2 &&
               MODEL_STATUS_CURRENT == 0 && MODEL_STATUS_FROZEN == 1 &&
               CHART_DELTA_T_MODEL_DEFAULT == 0, "Model identity codes drifted");

_Static_assert((sizeof(((ChartHousesV1*)0)->cusps_deg) / sizeof(double) == 12 &&
               offsetof(ChartHousesV1, cusps_deg[1]) - offsetof(ChartHousesV1, cusps_deg[0]) == 8 &&
               offsetof(ChartHousesV1, cusps_deg[11]) == 120 &&
               offsetof(ChartSnapshotV1, houses.cusps_deg) == 616) != 0, "ChartHousesV1 cusp array drifted");

_Static_assert((_Generic(((ChartBodyV1*)0)->target_code, uint32_t: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->present_fields, uint32_t: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->longitude_deg, double: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->latitude_deg, double: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->distance_au, double: 1, default: 0) &&
               _Generic(((ChartBodyV1*)0)->longitude_rate_deg_per_tt_day, double: 1, default: 0)) != 0,
               "ChartBodyV1 field types drifted");

_Static_assert((_Generic(((ChartHousesV1*)0)->ascendant_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->midheaven_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->descendant_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->imum_coeli_deg, double: 1, default: 0) &&
               _Generic(((ChartHousesV1*)0)->cusps_deg, double*: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->valid, bool: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->jd_ut1, double: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->jde_tt, double: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->bodies, ChartBodyV1*: 1, default: 0) &&
               _Generic(((ChartSnapshotV1*)0)->houses, ChartHousesV1: 1, default: 0)) != 0,
               "ChartSnapshotV1 or ChartHousesV1 field types drifted");

_Static_assert(_Generic(&chart_snapshot_v1,
               ChartSnapshotV1 (*)(int32_t, uint32_t, uint32_t, double, uint32_t, double, double, uint32_t, uint32_t): 1,
               default: 0) != 0, "chart_snapshot_v1 signature drifted");

_Static_assert(CHART_CIVIL_SCALE_UTC == 0 && CHART_CIVIL_SCALE_UT1 == 1 &&
               CHART_HOUSE_SYSTEM_EQUAL == 0 && CHART_HOUSE_SYSTEM_WHOLE_SIGN == 1 &&
               CHART_HOUSE_SYSTEM_PLACIDUS == 2 && CHART_DELTA_T_MODEL_DEFAULT == 0 &&
               CHART_DELTA_T_MODEL_ALGO1 == 1 && CHART_DELTA_T_MODEL_ALGO2 == 2 &&
               CHART_DELTA_T_MODEL_ALGO3 == 3 && CHART_DELTA_T_MODEL_ALGO4 == 4 &&
               CHART_DELTA_T_MODEL_ALGO5 == 5, "Chart V1 selector codes drifted");

_Static_assert(CHART_TARGET_SUN == 0 && CHART_TARGET_MOON == 1 && CHART_TARGET_MERCURY == 2 &&
               CHART_TARGET_VENUS == 3 && CHART_TARGET_MARS == 4 && CHART_TARGET_JUPITER == 5 &&
               CHART_TARGET_SATURN == 6 && CHART_TARGET_URANUS == 7 && CHART_TARGET_NEPTUNE == 8 &&
               CHART_TARGET_PLUTO == 9 && CHART_TARGET_MEAN_ASCENDING == 10 &&
               CHART_TARGET_MEAN_DESCENDING == 11 && CHART_TARGET_TRUE_ASCENDING == 12 &&
               CHART_TARGET_TRUE_DESCENDING == 13 && CHART_PRESENT_LATITUDE == 1 &&
               CHART_PRESENT_DISTANCE == 2, "Chart V1 identity or presence codes drifted");

/* #303: the event targets mirror the chart identities — one target order for the whole ABI. */
_Static_assert(EVENT_TARGET_SUN == CHART_TARGET_SUN && EVENT_TARGET_MOON == CHART_TARGET_MOON &&
               EVENT_TARGET_MERCURY == CHART_TARGET_MERCURY && EVENT_TARGET_VENUS == CHART_TARGET_VENUS &&
               EVENT_TARGET_MARS == CHART_TARGET_MARS && EVENT_TARGET_JUPITER == CHART_TARGET_JUPITER &&
               EVENT_TARGET_SATURN == CHART_TARGET_SATURN && EVENT_TARGET_URANUS == CHART_TARGET_URANUS &&
               EVENT_TARGET_NEPTUNE == CHART_TARGET_NEPTUNE && EVENT_TARGET_PLUTO == CHART_TARGET_PLUTO &&
               EVENT_TARGET_MEAN_ASCENDING == CHART_TARGET_MEAN_ASCENDING &&
               EVENT_TARGET_MEAN_DESCENDING == CHART_TARGET_MEAN_DESCENDING &&
               EVENT_TARGET_TRUE_ASCENDING == CHART_TARGET_TRUE_ASCENDING &&
               EVENT_TARGET_TRUE_DESCENDING == CHART_TARGET_TRUE_DESCENDING,
               "Event target identities drifted from the chart targets");

_Static_assert(EVENT_DIRECTION_INCREASING == 0 && EVENT_DIRECTION_DECREASING == 1 &&
               EVENT_DIRECTION_TANGENT == 2, "Event direction codes drifted");

_Static_assert(EVENT_MOTION_DIRECT == 0 && EVENT_MOTION_RETROGRADE == 1, "Event motion codes drifted");

_Static_assert(offsetof(LongitudeEventRecord, jde_tt) == 0 &&
               offsetof(LongitudeEventRecord, longitude_deg) == 8 &&
               offsetof(LongitudeEventRecord, longitude_rate_deg_per_tt_day) == 16 &&
               offsetof(LongitudeEventRecord, direction_code) == 24 &&
               sizeof(LongitudeEventRecord) == 32 && _Alignof(LongitudeEventRecord) == 8,
               "LongitudeEventRecord layout drifted");

_Static_assert(offsetof(StationEventRecord, jde_tt) == 0 &&
               offsetof(StationEventRecord, longitude_deg) == 8 &&
               offsetof(StationEventRecord, longitude_rate_deg_per_tt_day) == 16 &&
               offsetof(StationEventRecord, motion_before_code) == 24 &&
               offsetof(StationEventRecord, motion_after_code) == 28 &&
               sizeof(StationEventRecord) == 32 && _Alignof(StationEventRecord) == 8,
               "StationEventRecord layout drifted");

_Static_assert(offsetof(LongitudeDifferenceEventRecord, jde_tt) == 0 &&
               offsetof(LongitudeDifferenceEventRecord, longitude_a_deg) == 8 &&
               offsetof(LongitudeDifferenceEventRecord, longitude_b_deg) == 16 &&
               offsetof(LongitudeDifferenceEventRecord, longitude_rate_a_deg_per_tt_day) == 24 &&
               offsetof(LongitudeDifferenceEventRecord, longitude_rate_b_deg_per_tt_day) == 32 &&
               offsetof(LongitudeDifferenceEventRecord, signed_difference_deg) == 40 &&
               offsetof(LongitudeDifferenceEventRecord, direction_code) == 48 &&
               sizeof(LongitudeDifferenceEventRecord) == 56 && _Alignof(LongitudeDifferenceEventRecord) == 8,
               "LongitudeDifferenceEventRecord layout drifted");

_Static_assert((_Generic(((LongitudeEventRecord*)0)->jde_tt, double: 1, default: 0) &&
                _Generic(((LongitudeEventRecord*)0)->longitude_deg, double: 1, default: 0) &&
                _Generic(((LongitudeEventRecord*)0)->longitude_rate_deg_per_tt_day, double: 1, default: 0) &&
                _Generic(((LongitudeEventRecord*)0)->direction_code, uint32_t: 1, default: 0) &&
                _Generic(((StationEventRecord*)0)->motion_before_code, uint32_t: 1, default: 0) &&
                _Generic(((StationEventRecord*)0)->motion_after_code, uint32_t: 1, default: 0) &&
                _Generic(((LongitudeDifferenceEventRecord*)0)->longitude_a_deg, double: 1, default: 0) &&
                _Generic(((LongitudeDifferenceEventRecord*)0)->longitude_b_deg, double: 1, default: 0) &&
                _Generic(((LongitudeDifferenceEventRecord*)0)->longitude_rate_a_deg_per_tt_day, double: 1, default: 0) &&
                _Generic(((LongitudeDifferenceEventRecord*)0)->longitude_rate_b_deg_per_tt_day, double: 1, default: 0) &&
                _Generic(((LongitudeDifferenceEventRecord*)0)->signed_difference_deg, double: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->operation_code, uint32_t: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->subject_code, uint32_t: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->status_code, uint32_t: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->source_code, uint32_t: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->domain_start_jde_tt, double: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->domain_end_jde_tt, double: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->maximum_span_tt_days, uint32_t: 1, default: 0) &&
                _Generic(((CapabilityRecordV1*)0)->flags, uint32_t: 1, default: 0) &&
                _Generic(((ModelIdentityRecordV1*)0)->kind_code, uint32_t: 1, default: 0) &&
                _Generic(((ModelIdentityRecordV1*)0)->identity_code, uint32_t: 1, default: 0) &&
                _Generic(((ModelIdentityRecordV1*)0)->status_code, uint32_t: 1, default: 0) &&
                _Generic(((ModelIdentityRecordV1*)0)->default_alias_code, uint32_t: 1, default: 0) &&
                _Generic(((ModelIdentityRecordV1*)0)->domain_start_jde_tt, double: 1, default: 0) &&
                _Generic(((ModelIdentityRecordV1*)0)->domain_end_jde_tt, double: 1, default: 0)) != 0,
               "Event record field types drifted");

/* Signature pins: every export's function type, so a signature drift fails this TU. */

_Static_assert(_Generic(&set_log_verbosity,
               bool (*)(uint8_t new_value): 1,
               default: 0) != 0, "set_log_verbosity signature drifted");

_Static_assert(_Generic(&last_error,
               const char * (*)(void): 1,
               default: 0) != 0, "last_error signature drifted");

_Static_assert(_Generic(&ut1_to_jd,
               JulianDay (*)(int32_t y, uint32_t m, uint32_t d, double fraction): 1,
               default: 0) != 0, "ut1_to_jd signature drifted");

_Static_assert(_Generic(&ut1_to_jde,
               JulianDay (*)(int32_t y, uint32_t m, uint32_t d, double fraction): 1,
               default: 0) != 0, "ut1_to_jde signature drifted");

_Static_assert(_Generic(&jde_to_ut1,
               UT1Time (*)(double jde): 1,
               default: 0) != 0, "jde_to_ut1 signature drifted");

_Static_assert(_Generic(&sun_apparent_geocentric_coord,
               SunCoordinate (*)(double jde): 1,
               default: 0) != 0, "sun_apparent_geocentric_coord signature drifted");

_Static_assert(_Generic(&moon_apparent_geocentric_coord,
               MoonCoordinate (*)(double jde): 1,
               default: 0) != 0, "moon_apparent_geocentric_coord signature drifted");

_Static_assert(_Generic(&moon_illumination,
               MoonIllumination (*)(double jde): 1,
               default: 0) != 0, "moon_illumination signature drifted");

_Static_assert(_Generic(&moon_position_angle,
               MoonPositionAngle (*)(double jde): 1,
               default: 0) != 0, "moon_position_angle signature drifted");

_Static_assert(_Generic(&moon_phase_moments,
               uint32_t (*)(int32_t year, uint8_t phase_kind, uint32_t *root_count, double *slots, uint32_t slot_count): 1,
               default: 0) != 0, "moon_phase_moments signature drifted");

_Static_assert(_Generic(&solar_lon_root_discriminant,
               Discriminant (*)(int32_t year, double longitude): 1,
               default: 0) != 0, "solar_lon_root_discriminant signature drifted");

_Static_assert(_Generic(&solar_lon_roots,
               uint32_t (*)(int32_t year, double longitude, double *slots, uint32_t slot_count): 1,
               default: 0) != 0, "solar_lon_roots signature drifted");

_Static_assert(_Generic(&new_moons_after_jde,
               uint32_t (*)(double jde, double *slots, uint32_t slot_count): 1,
               default: 0) != 0, "new_moons_after_jde signature drifted");

_Static_assert(_Generic(&new_moons_in_year,
               uint32_t (*)(int32_t year, uint32_t *root_count, double *slots, uint32_t slot_count): 1,
               default: 0) != 0, "new_moons_in_year signature drifted");

_Static_assert(_Generic(&equation_of_time,
               EquationOfTime (*)(double jde): 1,
               default: 0) != 0, "equation_of_time signature drifted");

_Static_assert(_Generic(&apparent_solar_time,
               ApparentSolarTime (*)(int32_t y, uint32_t m, uint32_t d, double fraction, double longitude): 1,
               default: 0) != 0, "apparent_solar_time signature drifted");

_Static_assert(_Generic(&local_apparent_sidereal_time,
               SiderealTime (*)(double jd_ut1, double longitude): 1,
               default: 0) != 0, "local_apparent_sidereal_time signature drifted");

_Static_assert(_Generic(&query_jieqi_moment,
               JieqiMomentQuery (*)(int32_t year, uint8_t jq_idx): 1,
               default: 0) != 0, "query_jieqi_moment signature drifted");

_Static_assert(_Generic(&get_jieqi_name,
               bool (*)(uint8_t jq_idx, char *buf, uint32_t buf_size): 1,
               default: 0) != 0, "get_jieqi_name signature drifted");

_Static_assert(_Generic(&get_supported_lunar_year_range,
               SupportedLunarYearRange (*)(uint8_t algo): 1,
               default: 0) != 0, "get_supported_lunar_year_range signature drifted");

_Static_assert(_Generic(&get_lunar_year_info,
               LunarYearInfo (*)(uint8_t algo, int32_t year): 1,
               default: 0) != 0, "get_lunar_year_info signature drifted");

_Static_assert(_Generic(&gregorian_to_lunar,
               LunarDate (*)(uint8_t algo, int32_t year, uint8_t month, uint8_t day): 1,
               default: 0) != 0, "gregorian_to_lunar signature drifted");

_Static_assert(_Generic(&lunar_to_gregorian,
               GregorianDate (*)(uint8_t algo, int32_t year, uint8_t month, bool is_leap, uint8_t day): 1,
               default: 0) != 0, "lunar_to_gregorian signature drifted");

_Static_assert(_Generic(&delta_t_algo1,
               DeltaT (*)(double year): 1,
               default: 0) != 0, "delta_t_algo1 signature drifted");

_Static_assert(_Generic(&delta_t_algo2,
               DeltaT (*)(double year): 1,
               default: 0) != 0, "delta_t_algo2 signature drifted");

_Static_assert(_Generic(&delta_t_algo3,
               DeltaT (*)(double year): 1,
               default: 0) != 0, "delta_t_algo3 signature drifted");

_Static_assert(_Generic(&delta_t_algo4,
               DeltaT (*)(double year): 1,
               default: 0) != 0, "delta_t_algo4 signature drifted");

_Static_assert(_Generic(&delta_t_algo5,
               DeltaT (*)(double year): 1,
               default: 0) != 0, "delta_t_algo5 signature drifted");

_Static_assert(_Generic(&delta_t,
               DeltaT (*)(double year): 1,
               default: 0) != 0, "delta_t signature drifted");


_Static_assert(_Generic(&longitude_events,
               uint32_t (*)(uint32_t target_code, double target_longitude_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, LongitudeEventRecord *slots, uint32_t slot_count): 1,
               default: 0) != 0, "longitude_events signature drifted");

_Static_assert(_Generic(&longitude_stations,
               uint32_t (*)(uint32_t target_code, double start_jde_tt, double end_jde_tt, uint32_t *event_count, StationEventRecord *slots, uint32_t slot_count): 1,
               default: 0) != 0, "longitude_stations signature drifted");

_Static_assert(_Generic(&longitude_difference_events,
               uint32_t (*)(uint32_t first_target_code, uint32_t second_target_code, double target_difference_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, LongitudeDifferenceEventRecord *slots, uint32_t slot_count): 1,
               default: 0) != 0, "longitude_difference_events signature drifted");

_Static_assert(_Generic(&celestial_capabilities,
               uint32_t (*)(uint32_t operation_code, uint32_t subject_code, uint32_t *record_count, CapabilityRecordV1 *slots, uint32_t slot_count): 1,
               default: 0) != 0, "celestial_capabilities signature drifted");

_Static_assert(_Generic(&celestial_model_identities,
               uint32_t (*)(uint32_t kind_code, uint32_t *record_count, ModelIdentityRecordV1 *slots, uint32_t slot_count): 1,
               default: 0) != 0, "celestial_model_identities signature drifted");

_Static_assert(_Generic(&celestial_capability_text,
               uint32_t (*)(uint32_t operation_code, uint32_t subject_code, char *buffer, uint32_t capacity): 1,
               default: 0) != 0, "celestial_capability_text signature drifted");
