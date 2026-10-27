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

#ifndef CELESTIAL_CALENDAR_H
#define CELESTIAL_CALENDAR_H

/*
 * The published C-ABI of `libcelestial_calendar` (#67). Pure C — consumable from C,
 * ctypes, and any FFI. The `lib*.cpp` implementations include this header, so the
 * struct layouts below are the single source of truth; `c_header_check.c` pins the
 * offsets at compile time.
 *
 * The export set is exactly the entry points declared below: each one is marked
 * `CELESTIAL_API`, and consumers need no defines — the macro reads as dllimport
 * on Windows and default visibility elsewhere; only the library's own build defines
 * `CELESTIAL_BUILDING_DLL` (CMake injects it privately) to get dllexport.
 *
 * Public functions use the `celestial_` prefix, records `Celestial`, and codes `CELESTIAL_`.
 * ABI evolution policy (#129): within a major version, existing layouts and signatures
 * are frozen; `c_header_check.c` pins sizes and offsets at compile time. An incompatible
 * change needs a new major version or an explicitly named parallel function and record.
 * Initial declarations need no version suffix. Adding a struct or a function never breaks
 * the ABI; widening what an existing function accepts (as `algo` was widened to 3)
 * breaks neither the ABI nor the contract; narrowing it keeps the ABI but breaks
 * the contract, and needs a new major version just the same.
 *
 * Gregorian and civil output labels use astronomical years in [-32767, 32767], with
 * year 0 denoting 1 BCE.
 *
 * Error contract: no function throws across the boundary. Struct-returning functions
 * signal failure with `valid = false`; count and sized-text calls return `0` on failure.
 * Every recording call first clears the calling thread's error kind and message.
 * The two error readers preserve both values. Check the error kind to distinguish a
 * legal empty result from failure, even when allocating the error message failed.
 * Among calls with a total-count output parameter, only celestial_longitude_events,
 * celestial_longitude_stations and celestial_longitude_difference_events permit a
 * successful zero total. Other such calls have a positive total on success.
 * celestial_solar_lon_roots may return zero roots, and celestial_new_moons_after_jde
 * permits a zero request; check the error kind after these calls too.
 *
 * Thread-safety contract: every entry point may be called concurrently from any
 * number of threads, with no host-side synchronization. Errors are per-thread:
 * no thread observes another thread's kind or message. The library does not print errors.
 * Two computations memoize per argument — the jieqi moments and the algo-2 lunar year
 * info. Both caches are shared process-wide and never erased: the first call with a given
 * argument pays for it, later calls from any thread reuse the result, and memory grows
 * monotonically with the number of distinct arguments ever queried. Two threads that
 * miss on the same argument at once both compute it, and one of the results is discarded.
 * Separately, the first `celestial_gregorian_to_lunar` or `celestial_lunar_to_gregorian`
 * with `algo = 2` runs the astronomical pipeline once to establish its supported range;
 * unlike the caches above, that one blocks every thread that arrives while it is in
 * flight, and costs nothing afterwards. Everything else recomputes on each call, apart
 * from lookups of values already fixed before `main` — the supported ranges reported by
 * `celestial_get_supported_lunar_year_range`, which are settled at load time at the latest.
 *
 * Export protocol/ownership metadata is maintained in the ABI tooling alongside the
 * declaration parser. New declarations must have matching metadata and independent C11 pins.
 */

#include <stdbool.h>
#include <stdint.h>

/* #91: export marker for the entry points below — contract in the header block above. */
#ifdef _WIN32
  #if defined(CELESTIAL_BUILDING_DLL)
    #define CELESTIAL_API __declspec(dllexport)
  #else
    #define CELESTIAL_API __declspec(dllimport)
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #define CELESTIAL_API __attribute__((visibility("default")))
#else
  #define CELESTIAL_API
#endif

/* NOLINTBEGIN(modernize-use-trailing-return-type, modernize-use-using):
 * this header must stay valid C — C has neither trailing return types nor `using`. */

#ifdef __cplusplus
extern "C" {
#endif


/* ---------- Errors ---------- */

#define CELESTIAL_ERROR_NONE          UINT32_C(0)
#define CELESTIAL_ERROR_INVALID_INPUT UINT32_C(1)
#define CELESTIAL_ERROR_COMPUTATION   UINT32_C(2)
#define CELESTIAL_ERROR_RESOURCE      UINT32_C(3)
#define CELESTIAL_ERROR_INTERNAL      UINT32_C(4)

/**
 * @brief Get the last-error message of the calling thread.
 * @returns A pointer to a thread-local C string, empty if there is no recorded error.
 *          The pointer remains valid until the next recording call on this thread.
 *          Neither error reader clears the kind or message.
 * @note A failure can have an empty message if message allocation failed; read the kind.
 */
CELESTIAL_API const char *celestial_last_error(void);

/** @brief Read the calling thread's CELESTIAL_ERROR_* code without clearing its error. */
CELESTIAL_API uint32_t celestial_last_error_kind(void);


/* ---------- Julian Days ---------- */

typedef struct CelestialJulianDay {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The value. Either JD or JDE. */
} CelestialJulianDay;

/**
 * @brief Convert UT1 datetime to Julian Day Number (JD).
 * @param y The year, in [1, 32767].
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `CelestialJulianDay` struct. JD is based on UT1.
 */
CELESTIAL_API CelestialJulianDay celestial_ut1_to_jd(int32_t y, uint32_t m, uint32_t d, double fraction);

/**
 * @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE).
 * @param y The year, in [1, 32767]; the ΔT-shifted TT instant must also stay representable.
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `CelestialJulianDay` struct. JDE is based on TT.
 */
CELESTIAL_API CelestialJulianDay celestial_ut1_to_jde(int32_t y, uint32_t m, uint32_t d, double fraction);

typedef struct CelestialUT1Time {
  bool     valid;    /* Indicates if the result is valid. */
  int32_t  year;     /* The year. */
  uint32_t month;    /* The month. */
  uint32_t day;      /* The day. */
  double   fraction; /* The fraction of the day, in the range [0.0, 1.0). */
} CelestialUT1Time;

/**
 * @brief Convert Julian Ephemeris Day Number (JDE) to UT1 datetime.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `CelestialUT1Time` struct.
 */
CELESTIAL_API CelestialUT1Time celestial_jde_to_ut1(double jde);


/* ---------- Sun and Moon Apparent Geocentric Position ---------- */

typedef struct CelestialSunCoordinate {
  bool   valid; /* Indicates if the result is valid. */
  double lon;   /* The longitude. In degrees. */
  double lat;   /* The latitude. In degrees. */
  double r;     /* The radius. In AU. */
} CelestialSunCoordinate;

/**
 * @brief Calculate the apparent geocentric position of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `CelestialSunCoordinate` struct.
 */
CELESTIAL_API CelestialSunCoordinate celestial_sun_apparent_geocentric_coord(double jde);

typedef struct CelestialMoonCoordinate {
  bool   valid; /* Indicates if the result is valid. */
  double lon;   /* The longitude. In degrees. */
  double lat;   /* The latitude. In degrees. */
  double r;     /* The radius. In KM. */
} CelestialMoonCoordinate;

/**
 * @brief Calculate the apparent geocentric position of the Moon.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `CelestialMoonCoordinate` struct.
 */
CELESTIAL_API CelestialMoonCoordinate celestial_moon_apparent_geocentric_coord(double jde);

typedef struct CelestialMoonIllumination {
  bool   valid;          /* Indicates if the result is valid. */
  double illumination;   /* The illuminated fraction of the Moon's disk, in [0, 1]. */
  double elongation_deg; /* The apparent longitude difference Moon − Sun, in degrees, in [0, 360). */
} CelestialMoonIllumination;

/**
 * @brief Compute the Moon's illuminated fraction k and elongation at a JDE
 *        (Meeus ch. 48: k from (48.2)+(48.3)+(48.1)).
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `CelestialMoonIllumination` struct. `elongation_deg` is the apparent ecliptic longitude
 *          difference that defines the phases: 0° new, 90° first quarter, 180° full,
 *          270° last quarter.
 */
CELESTIAL_API CelestialMoonIllumination celestial_moon_illumination(double jde);

typedef struct CelestialMoonPositionAngle {
  bool   valid;     /* Indicates if the result is valid. */
  double angle_deg; /* Position angle of the Moon's bright limb, in [0, 360). */
} CelestialMoonPositionAngle;

/**
 * @brief Compute the position angle of the Moon's bright limb at a JDE (Meeus ch. 48, (48.5)).
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `CelestialMoonPositionAngle` struct; `angle_deg` is measured eastward from the north
 *          point of the disk, in [0, 360).
 */
CELESTIAL_API CelestialMoonPositionAngle celestial_moon_position_angle(double jde);


/* ---------- Moon Phase Moments ---------- */

/**
 * @brief Find the moments of a given Moon phase in `year`; the total count goes to `root_count`,
 *        up to `slot_count` of them are written to `slots`.
 * @param year The Gregorian year, in [1, 32766].
 * @param phase_kind The phase kind: 0 = New Moon, 1 = First Quarter, 2 = Full Moon, 3 = Last Quarter.
 * @param root_count Where the total count of the roots is written. Must not be null.
 *                   On every failure `*root_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
CELESTIAL_API uint32_t celestial_moon_phase_moments(int32_t year, uint8_t phase_kind, uint32_t *root_count, double *slots, uint32_t slot_count);


/* ---------- Solar Longitude Roots ---------- */

typedef struct CelestialDiscriminant {
  bool     valid; /* Indicates if the result is valid. */
  uint32_t count; /* The count of the roots, which is 0, 1, or 2. */
} CelestialDiscriminant;

/**
 * @brief Calculate the JDE discriminant — count how many times the Sun reaches the given
 *        geocentric `longitude` in the given `year`.
 * @param year The Gregorian year, in [1, 32766].
 * @param longitude The geocentric longitude in degrees, in [0, 360).
 * @returns A `CelestialDiscriminant` struct; `count` is 0, 1, or 2: the Sun won't reach the longitude
 *          in the year, reaches it once, or reaches it twice.
 */
CELESTIAL_API CelestialDiscriminant celestial_solar_lon_root_discriminant(int32_t year, double longitude);

/**
 * @brief Find the JDE(s) at which the Sun reaches `longitude` in `year`, written to `slots`.
 * @param year The Gregorian year, in [1, 32766].
 * @param longitude The geocentric longitude in degrees, in [0, 360).
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
CELESTIAL_API uint32_t celestial_solar_lon_roots(int32_t year, double longitude, double *slots, uint32_t slot_count);


/* ---------- Sun Moon Conjunction ---------- */

/**
 * @brief Find the next `slot_count` new-moon JDE(s) after `jde`, written to `slots`.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
CELESTIAL_API uint32_t celestial_new_moons_after_jde(double jde, double *slots, uint32_t slot_count);

/**
 * @brief Find the new-moon JDE(s) in `year`; the total count goes to `root_count`,
 *        up to `slot_count` of them are written to `slots`.
 * @param year The Gregorian year, in [1, 32766].
 * @param root_count Where the total count of the roots is written. Must not be null.
 *                   On every failure `*root_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
CELESTIAL_API uint32_t celestial_new_moons_in_year(int32_t year, uint32_t *root_count, double *slots, uint32_t slot_count);


/* ---------- Solar Time ---------- */

typedef struct CelestialEquationOfTime {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* E in degrees of hour angle; x240 for seconds of time. */
} CelestialEquationOfTime;

/**
 * @brief Compute the equation of time E = apparent solar time - mean solar time
 *        (Meeus ch. 28).
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `CelestialEquationOfTime` struct; `value` is E in degrees of hour angle
 *          (x240 = seconds of time).
 */
CELESTIAL_API CelestialEquationOfTime celestial_equation_of_time(double jde);

typedef struct CelestialApparentSolarTime {
  bool     valid;    /* Indicates if the result is valid. */
  int32_t  year;     /* The year of the local apparent solar date. */
  uint32_t month;    /* The month of the local apparent solar date. */
  uint32_t day;      /* The day of the local apparent solar date. */
  double   fraction; /* The fraction of the local apparent solar day, in the range [0.0, 1.0). */
} CelestialApparentSolarTime;

/**
 * @brief Convert a civil UTC moment to local apparent (true) solar time.
 * @param y The year (UTC), in [1, 32767]; the shifted TT instant must also stay representable.
 * @param m The month (UTC).
 * @param d The day (UTC).
 * @param fraction The fraction of the day (UTC). Must be in the range [0.0, 1.0).
 * @param longitude The observer's geographic longitude in degrees, positive east, in [-180, 180].
 * @returns A `CelestialApparentSolarTime` struct; the local apparent solar date may differ from the
 *          input UTC date — a large enough longitude shifts the moment across midnight.
 */
CELESTIAL_API CelestialApparentSolarTime celestial_apparent_solar_time(int32_t y, uint32_t m, uint32_t d, double fraction, double longitude);


/* ---------- Sidereal Time ---------- */

typedef struct CelestialSiderealTime {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The local apparent sidereal time, in degrees, in [0, 360). */
} CelestialSiderealTime;

/**
 * @brief Compute the Local Apparent Sidereal Time (LAST) for an observer.
 * @param jd_ut1 The julian day number, which is based on **UT1**. Declared domain:
 *        Gregorian years in [401, 32766].
 * @param longitude The observer's geographic longitude in degrees, positive east, in [-180, 180].
 * @returns A `CelestialSiderealTime` struct. The ΔT model is fixed Algo5 and the
 *          nutation model is fixed IAU 1980.
 */
CELESTIAL_API CelestialSiderealTime celestial_local_apparent_sidereal_time(double jd_ut1, double longitude);


/* ---------- Jieqi ---------- */

typedef struct CelestialJieqiMomentQuery {
  bool     valid;  /* Indicates if the result is valid. */
  uint8_t  jq_idx; /* The Jieqi index in [0, 24), in ecliptic order: 0 = 立春 (Lichun) … 23 = 大寒. */
  int32_t  y;      /* The year. */
  uint32_t m;      /* The month. */
  uint32_t d;      /* The day. */
  double   frac;   /* The fraction of the day, in the range [0.0, 1.0). */
} CelestialJieqiMomentQuery;

/**
 * @brief Query the accurate UT1 moment of the Jieqi in the given `year`.
 * @param year The Gregorian year, in [401, 32766].
 * @param jq_idx The Jieqi index in [0, 24), in ecliptic order: 0 = 立春 (Lichun) … 23 = 大寒.
 * @returns A `CelestialJieqiMomentQuery` struct.
 * @note The returned civil moment is UT1 (from TT through ΔT algo5), not UTC. While leap
 *       seconds are applied, |UT1 − UTC| < 0.9 s; past the last ΔAT table step the modelled
 *       UT1 − UTC is (ΔAT + 32.184 s) − ΔT, no longer bounded by 0.9 s (#115). The gap by
 *       era: `docs/DESIGN.md#time-scales`.
 * @note The returned date is not an east-eight wall date; rendering the same instant at a
 *       fixed UTC+8 offset can change its calendar date.
 */
CELESTIAL_API CelestialJieqiMomentQuery celestial_query_jieqi_moment(int32_t year, uint8_t jq_idx);

/**
 * @brief Size or write the UTF-8 Chinese name of the Jieqi.
 * @param jq_idx The Jieqi index in [0, 24), in ecliptic order: 0 = 立春 (Lichun) … 23 = 大寒.
 * @param buf Caller-owned buffer; may be null only when buf_size is zero.
 * @param buf_size Available bytes, including the NUL terminator; zero queries the size.
 * @returns Required bytes including NUL, or zero on failure. Positive buf_size writes
 *          at most buf_size - 1 payload bytes and a terminating NUL, truncating if needed.
 * @note The current names need at most 7 bytes including NUL; bindings query the size dynamically.
 */
CELESTIAL_API uint32_t celestial_get_jieqi_name(uint8_t jq_idx, char *buf, uint32_t buf_size);


/* ---------- Lunar Calendar ---------- */

typedef struct CelestialSupportedLunarYearRange {
  bool    valid; /* Indicates if the result is valid. */
  int32_t start; /* The first supported lunar year. */
  int32_t end;   /* The last supported lunar year. */
} CelestialSupportedLunarYearRange;

/**
 * @brief Get the supported lunar year range of the algorithm.
 * @param algo The algorithm. Expected to be 1, 2, or 3.
 * @returns A `CelestialSupportedLunarYearRange` struct.
 * @details Algorithm profiles:
 *          - 1: preserves the published HKO calendar-date labels for 1901-2099;
 *          - 2: computation from VSOP87D/ELP2000-82B for 410-2500, with TT moments rendered
 *            through the library's UTC model and then a fixed east-eight offset; that model uses
 *            UT1 as a pre-1972 proxy, the ΔAT table from 1972 through its final announced step,
 *            and a frozen ΔAT of 37 s afterward;
 *          - 3: a baked table for 1600-2199, using algorithm 1 for 1901-2099 and algorithm 2
 *            for the other years.
 */
CELESTIAL_API CelestialSupportedLunarYearRange celestial_get_supported_lunar_year_range(uint8_t algo);

typedef struct CelestialLunarYearInfo {
  bool     valid;      /* Indicates if the result is valid. */
  int32_t  year;       /* Gregorian year of the first day of the lunar year. */
  uint8_t  month;      /* Gregorian month of the first day of the lunar year. */
  uint8_t  day;        /* Gregorian day of the first day of the lunar year. */
  uint8_t  leap_month; /* The leap month (1-12) in **traditional** numbering, or 0 if none. */
  uint16_t month_len;  /* Least 12/13 bits, one per month in calendar order (bit 0 = the first
                          month): 1 = 30-day month, 0 = 29-day month. A leap month occupies a
                          bit of its own, so with `leap_month = 2` the bits run 1, 2, leap 2,
                           3, … . `celestial_gregorian_to_lunar` and `celestial_lunar_to_gregorian`
                           use traditional numbering + `is_leap`, where that leap month is
                          `month = 2, is_leap = true`. */
} CelestialLunarYearInfo;

/**
 * @brief Get the lunar year information for the given year.
 * @param algo The algorithm profile described by `celestial_get_supported_lunar_year_range`.
 * @param year The lunar year. Outside the range `celestial_get_supported_lunar_year_range` reports for
 *             `algo`, the result is `valid = false`.
 * @returns A `CelestialLunarYearInfo` struct.
 */
CELESTIAL_API CelestialLunarYearInfo celestial_get_lunar_year_info(uint8_t algo, int32_t year);

typedef struct CelestialLunarDate {
  bool    valid;   /* Indicates if the result is valid. */
  int32_t year;    /* The lunar year. */
  uint8_t month;   /* The lunar month, in traditional numbering (1-12). */
  bool    is_leap; /* Whether the month is the leap month. */
  uint8_t day;     /* The day of the lunar month. */
} CelestialLunarDate;

typedef struct CelestialGregorianDate {
  bool    valid; /* Indicates if the result is valid. */
  int32_t year;  /* The year. */
  uint8_t month; /* The month. */
  uint8_t day;   /* The day. */
} CelestialGregorianDate;

/**
 * @brief Convert a Gregorian date to a lunar date.
 * @param algo The algorithm profile described by `celestial_get_supported_lunar_year_range`.
 * @param year The Gregorian year, in [-32767, 32767]; an unrepresentable integer is rejected,
 *             never narrowed to an aliased year.
 * @param month The Gregorian month.
 * @param day The Gregorian day of the month.
 * @returns A `CelestialLunarDate` struct; `valid = false` when `algo` is unknown, the input is not a
 *          real Gregorian date, or it falls outside the Gregorian span the algorithm covers —
 *          that span runs from the first day of lunar year `start` to the last day of lunar
 *          year `end` (`start`/`end` as `celestial_get_supported_lunar_year_range` reports them), so it
 *          begins and ends mid-Gregorian-year at both ends.
 * @note The lunar month is in **traditional numbering** (1-12) plus the `is_leap` flag —
 *       a leap month carries its predecessor's number with `is_leap = true`: in lunar 2023
 *       (leap 2nd month), the leap 2nd month is `month = 2, is_leap = true`, and the
 *       traditional 3rd month is `month = 3, is_leap = false`. This is NOT the positional
 *       month index that `CelestialLunarYearInfo.month_len` is indexed by.
 * @note The Gregorian input is a calendar-date label on the selected algorithm's basis, not
 *       an instant.
 */
CELESTIAL_API CelestialLunarDate celestial_gregorian_to_lunar(uint8_t algo, int32_t year, uint8_t month, uint8_t day);

/**
 * @brief Convert a lunar date to a Gregorian date.
 * @param algo The algorithm profile described by `celestial_get_supported_lunar_year_range`.
 * @param year The lunar year.
 * @param month The lunar month, in traditional numbering (1-12) — see `celestial_gregorian_to_lunar`.
 * @param is_leap Whether the month is the leap month. `true` is accepted only on the year's
 *                actual leap month — `month = 2` in lunar 2023, and no month at all in a year
 *                that has none.
 * @param day The day of the lunar month.
 * @returns A `CelestialGregorianDate` struct; `valid = false` when the input does not name a real
 *          lunar date (bad `algo`, year out of range, no such leap month, day out of range).
 * @note The returned Gregorian value is a calendar-date label on the selected algorithm's
 *       basis, not an instant; see `celestial_gregorian_to_lunar`.
 */
CELESTIAL_API CelestialGregorianDate celestial_lunar_to_gregorian(uint8_t algo, int32_t year, uint8_t month, bool is_leap, uint8_t day);


/* ---------- Shared Target, Model and Geometry Codes ---------- */

#define CELESTIAL_CIVIL_SCALE_UTC UINT32_C(0)
#define CELESTIAL_CIVIL_SCALE_UT1 UINT32_C(1)

#define CELESTIAL_HOUSE_SYSTEM_EQUAL      UINT32_C(0)
#define CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN UINT32_C(1)
#define CELESTIAL_HOUSE_SYSTEM_PLACIDUS   UINT32_C(2)

#define CELESTIAL_DELTA_T_MODEL_DEFAULT UINT32_C(0)
#define CELESTIAL_DELTA_T_MODEL_ALGO1   UINT32_C(1)
#define CELESTIAL_DELTA_T_MODEL_ALGO2   UINT32_C(2)
#define CELESTIAL_DELTA_T_MODEL_ALGO3   UINT32_C(3)
#define CELESTIAL_DELTA_T_MODEL_ALGO4   UINT32_C(4)
#define CELESTIAL_DELTA_T_MODEL_ALGO5   UINT32_C(5)

#define CELESTIAL_TARGET_SUN             UINT32_C(0)
#define CELESTIAL_TARGET_MOON            UINT32_C(1)
#define CELESTIAL_TARGET_MERCURY         UINT32_C(2)
#define CELESTIAL_TARGET_VENUS           UINT32_C(3)
#define CELESTIAL_TARGET_MARS            UINT32_C(4)
#define CELESTIAL_TARGET_JUPITER         UINT32_C(5)
#define CELESTIAL_TARGET_SATURN          UINT32_C(6)
#define CELESTIAL_TARGET_URANUS          UINT32_C(7)
#define CELESTIAL_TARGET_NEPTUNE         UINT32_C(8)
#define CELESTIAL_TARGET_PLUTO           UINT32_C(9)
#define CELESTIAL_TARGET_MEAN_ASCENDING  UINT32_C(10)
#define CELESTIAL_TARGET_MEAN_DESCENDING UINT32_C(11)
#define CELESTIAL_TARGET_TRUE_ASCENDING  UINT32_C(12)
#define CELESTIAL_TARGET_TRUE_DESCENDING UINT32_C(13)
#define CELESTIAL_TARGET_ALL             UINT32_C(0xFFFFFFFF)

#define CELESTIAL_POSITION_SOURCE_SUN_VSOP87D               UINT32_C(1)
#define CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC    UINT32_C(2)
#define CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D            UINT32_C(3)
#define CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT    UINT32_C(4)
#define CELESTIAL_POSITION_SOURCE_NODE_MEAN                 UINT32_C(5)
#define CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM UINT32_C(6)

#define CELESTIAL_NUTATION_MODEL_DEFAULT  UINT32_C(0)
#define CELESTIAL_NUTATION_MODEL_MEEUS    UINT32_C(1)
#define CELESTIAL_NUTATION_MODEL_IAU_1980 UINT32_C(2)

/* Latitude (or equatorial declination) is present in every chart/position record.
 * Distance is present only for physical targets; absent storage has no value semantics. */
#define CELESTIAL_PRESENT_LATITUDE UINT32_C(1)
#define CELESTIAL_PRESENT_DISTANCE UINT32_C(2)


/* ---------- Delta T ---------- */

typedef struct CelestialDeltaT {
  bool valid;                   /* Indicates if the result is valid. */
  uint32_t model_identity_code; /* Resolved CELESTIAL_DELTA_T_MODEL_* value, never DEFAULT. */
  double value;                 /* TT - UT1, in seconds. */
} CelestialDeltaT;

/**
 * @brief Compute TT - UT1 in seconds using the selected Delta T model.
 * @param year A finite decimal Gregorian year at which the model is evaluated.
 * @param model_code A CELESTIAL_DELTA_T_MODEL_* selector; DEFAULT resolves to the core default.
 * @returns The value and resolved model identity; valid is false on failure.
 * @note Algo1 requires year >= -4000, Algo3 year < 3000, and Algo4 year < 2035.
 *       These are evaluation-instant guards, not a universal bound on a converted TT value.
 */
CELESTIAL_API CelestialDeltaT celestial_delta_t(double year, uint32_t model_code);


/* ---------- Chart Snapshot ---------- */

/** @brief One geocentric target in the true ecliptic and equinox of date. */
typedef struct CelestialChartBody {
  uint32_t target_code;                 /* A CELESTIAL_TARGET_* code; records follow code order 0-13. */
  uint32_t source_code;                 /* Resolved CELESTIAL_POSITION_SOURCE_* identity. */
  uint32_t present_fields;              /* CELESTIAL_PRESENT_* bits; absent fields are unused storage, not sentinels. */
  double longitude_deg;                 /* In [0, 360). */
  double latitude_deg;                  /* Present for every target, including the nodes' known zero latitude. */
  double distance_au;                   /* Present for Sun, Moon and planets; absent for nodes. */
  double longitude_rate_deg_per_tt_day; /* Published seven-point signed rate; negative means retrograde at this instant. */
} CelestialChartBody;

/** @brief Four axes and twelve cusps in degrees, each in [0, 360); cusps are houses 1-12. */
typedef struct CelestialChartHouses {
  double ascendant_deg;
  double midheaven_deg;
  double descendant_deg;
  double imum_coeli_deg;
  double cusps_deg[12];
} CelestialChartHouses;

/** @brief A complete independent value; consume the payload only when valid is true. */
typedef struct CelestialChartSnapshot {
  bool valid;
  uint32_t delta_t_identity_code;  /* Resolved CELESTIAL_DELTA_T_MODEL_* value, never DEFAULT. */
  uint32_t nutation_identity_code; /* Resolved CELESTIAL_NUTATION_MODEL_* value for houses, never DEFAULT. */
  double jd_ut1;
  double jde_tt;
  CelestialChartBody bodies[14];
  CelestialChartHouses houses;
} CelestialChartSnapshot;

/**
 * @brief Calculate a complete chart at an explicitly scaled civil instant.
 * @param year Gregorian civil year in [1885, 2100).
 * @param month Gregorian month in [1, 12].
 * @param day A valid day of the Gregorian month.
 * @param fraction Finite fraction of the civil day in [0, 1); inserted leap seconds are outside the input.
 * @param civil_scale_code CELESTIAL_CIVIL_SCALE_UTC (from 1972-01-01) or CELESTIAL_CIVIL_SCALE_UT1.
 * @param latitude_deg North-positive latitude in (-90, 90).
 * @param longitude_deg East-positive longitude in [-180, 180].
 * @param house_system_code A CELESTIAL_HOUSE_SYSTEM_* code; Placidus rejects polar geometry.
 *                          All systems reject an Ascendant direction unresolved at round-off.
 * @param delta_t_model_code A CELESTIAL_DELTA_T_MODEL_* selector; DEFAULT resolves to the core default.
 * @returns A CelestialChartSnapshot by value. On failure valid is false; read
 *          celestial_last_error_kind and celestial_last_error for the cause. Invalid payload
 *          and padding bytes have no contract. There is no partial result.
 * @note JDE(TT) must be in [2409543.875, 2488069.125); Algo4 additionally requires JDE(TT) < 2464328.5.
 *       UT1 derived from UTC is modelled through the selected Delta T, not observed DUT1.
 *       Location affects houses only; timezone conversion is the caller's responsibility.
 *       Houses use the house operation's default nutation; bodies use fixed IAU 1980.
 */
CELESTIAL_API CelestialChartSnapshot celestial_chart_snapshot(int32_t year, uint32_t month, uint32_t day, double fraction, uint32_t civil_scale_code, double latitude_deg, double longitude_deg, uint32_t house_system_code, uint32_t delta_t_model_code);


/* ---------- Ephemeris Events ---------- */

/*
 * #303: interval longitude events over the same fourteen targets as the chart,
 * in CELESTIAL_TARGET_* order. Times are JDE(TT) and rates are degrees per TT day,
 * mirroring docs/EPHEMERIS_EVENTS.md. Angles carry no
 * names or interpretations — the API never speaks aspects or orbs.
 *
 * Buffer protocol: one call reports the total through `event_count` and writes up to
 * `slot_count` records into caller-owned `slots` (NULL only when `slot_count` is 0).
 * The return value is how many records were written; when the total exceeds the
 * capacity the fill is truncated and `event_count` still reports the full total, so the
 * overflow is observable, never silent. Records are in strict chronological order.
 * A zero total is a legal outcome; every failure also leaves `*event_count` at 0, and
 * is told apart from an empty success by `celestial_last_error_kind`.
 */

#define CELESTIAL_EVENT_DIRECTION_INCREASING UINT32_C(0)
#define CELESTIAL_EVENT_DIRECTION_DECREASING UINT32_C(1)
#define CELESTIAL_EVENT_DIRECTION_TANGENT    UINT32_C(2)

#define CELESTIAL_EVENT_MOTION_DIRECT     UINT32_C(0)
#define CELESTIAL_EVENT_MOTION_RETROGRADE UINT32_C(1)

/** @brief One longitude crossing or exact numerical contact at the supplied level. */
typedef struct CelestialLongitudeEventRecord {
  double   jde_tt;                        /* JDE(TT) of the event, on the reported 2^-30-day lattice. */
  double   longitude_deg;                 /* Apparent geocentric longitude at the event, in degrees; not
                                             normalized — values on either side of a branch cut stay raw. */
  double   longitude_rate_deg_per_tt_day; /* The seven-point ephemeris::longitude_rate at the event. */
  uint32_t direction_code;                /* A CELESTIAL_EVENT_DIRECTION_* code. */
} CelestialLongitudeEventRecord;

/** @brief One stationary instant of the seven-point ephemeris::longitude_rate. */
typedef struct CelestialStationEventRecord {
  double   jde_tt;                        /* JDE(TT) of the stationary instant. */
  double   longitude_deg;                 /* Apparent geocentric longitude at the event, in degrees; not
                                             normalized — values on either side of a branch cut stay raw. */
  double   longitude_rate_deg_per_tt_day; /* The published rate at the event; need not be bitwise zero. */
  uint32_t motion_before_code;            /* A CELESTIAL_EVENT_MOTION_* code: certified motion before the instant. */
  uint32_t motion_after_code;             /* A CELESTIAL_EVENT_MOTION_* code: certified motion after the instant. */
} CelestialStationEventRecord;

/** @brief One event of the directed, canonical longitude difference first minus second. */
typedef struct CelestialLongitudeDifferenceEventRecord {
  double   jde_tt;                              /* JDE(TT) of the event. */
  double   longitude_a_deg;                     /* First target's longitude at the event, in degrees; not normalized. */
  double   longitude_b_deg;                     /* Second target's longitude at the event, in degrees; not normalized. */
  double   longitude_rate_a_deg_per_tt_day;     /* First target's published rate at the event. */
  double   longitude_rate_b_deg_per_tt_day;     /* Second target's published rate at the event. */
  double   signed_difference_deg;               /* Canonical (first - second) difference, in (-180, 180]. */
  uint32_t direction_code;                      /* A CELESTIAL_EVENT_DIRECTION_* code. */
} CelestialLongitudeDifferenceEventRecord;

/**
 * @brief Find the JDE(TT)s where `target`'s apparent geocentric longitude reaches
 *        `target_longitude_deg` in the half-open interval [start, end), written to `slots`.
 * @param target_code A CELESTIAL_TARGET_* code.
 * @param target_longitude_deg A finite angle in degrees; whole-turn shifts denote the same level.
 * @param start_jde_tt Inclusive start, finite, within the target's longitude-rate domain.
 * @param end_jde_tt Exclusive end; the nonempty span must not exceed 3660 TT days.
 * @param event_count Where the total count of events is written. Must not be null.
 *                    On every failure `*event_count` is set to 0; a zero total is also
 *                    the legal no-event outcome, disambiguated by `celestial_last_error_kind`.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written, in chronological order, at most min(total, slot_count).
 * @note Longitudes are apparent, geocentric and tropical; node longitudes follow
 *       lunar_node::position. Exact numerical contacts are reported for a target equal
 *       to the query-independent stationary output, never within an angular window.
 *       Numerical ambiguity or work-limit exhaustion fails the whole query — no partial
 *       list is ever returned. See docs/EPHEMERIS_EVENTS.md for domains and the
 *       conditional floating-point contract.
 */
CELESTIAL_API uint32_t celestial_longitude_events(uint32_t target_code, double target_longitude_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, CelestialLongitudeEventRecord *slots, uint32_t slot_count);

/**
 * @brief Find the JDE(TT)s where `target`'s seven-point ephemeris::longitude_rate is
 *        stationary in the half-open interval [start, end), written to `slots`.
 * @param target_code A CELESTIAL_TARGET_* code.
 * @param start_jde_tt Inclusive start, finite, within the target's longitude-rate domain.
 * @param end_jde_tt Exclusive end; the nonempty span must not exceed 3660 TT days.
 * @param event_count Where the total count of events is written. Must not be null.
 *                    On every failure `*event_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written, in chronological order, at most min(total, slot_count).
 * @note Targets whose certified rate bounds exclude a station return a legal empty list.
 *       The motion codes describe the certified sign on each side; the reported rate
 *       need not be bitwise zero. The floating-point premises are the same as for
 *       `celestial_longitude_events`.
 */
CELESTIAL_API uint32_t celestial_longitude_stations(uint32_t target_code, double start_jde_tt, double end_jde_tt, uint32_t *event_count, CelestialStationEventRecord *slots, uint32_t slot_count);

/**
 * @brief Find the JDE(TT)s where the directed difference (first - second) of the two
 *        targets' apparent geocentric longitudes reaches `target_difference_deg` in the
 *        half-open interval [start, end), written to `slots`.
 * @param first_target_code A CELESTIAL_TARGET_* code.
 * @param second_target_code A CELESTIAL_TARGET_* code, distinct from the first.
 * @param target_difference_deg A finite signed angle in degrees, canonicalized to (-180, 180].
 * @param start_jde_tt Inclusive start, finite, within the intersection of the two targets'
 *                     longitude-rate domains.
 * @param end_jde_tt Exclusive end; the nonempty span must not exceed 3660 TT days.
 * @param event_count Where the total count of events is written. Must not be null.
 *                    On every failure `*event_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written, in chronological order, at most min(total, slot_count).
 * @note A constant mismatch (antipodal node pairs whose opposition angle is not the
 *       target) is a legal empty list; a continuously satisfied relation fails.
 * @note Reversing the two targets reverses the requested angle and crossing direction.
 *       Self-pairs and continuously satisfied relations are invalid; numerical ambiguity
 *       or work-limit exhaustion fails the whole query. The floating-point premises are
 *       the same as for `celestial_longitude_events`.
 */
CELESTIAL_API uint32_t celestial_longitude_difference_events(uint32_t first_target_code, uint32_t second_target_code, double target_difference_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, CelestialLongitudeDifferenceEventRecord *slots, uint32_t slot_count);


/* ---------- Capabilities and Model Identities ---------- */

#define CELESTIAL_OPERATION_POSITION       UINT32_C(0)
#define CELESTIAL_OPERATION_LONGITUDE_RATE UINT32_C(1)
#define CELESTIAL_OPERATION_EVENTS         UINT32_C(2)
#define CELESTIAL_OPERATION_HOUSES         UINT32_C(3)
#define CELESTIAL_OPERATION_EQUATORIAL     UINT32_C(4)
#define CELESTIAL_OPERATION_CHART          UINT32_C(5)
#define CELESTIAL_OPERATION_DELTA_T        UINT32_C(6)

#define CELESTIAL_STATUS_SOURCE_WINDOW           UINT32_C(0)
#define CELESTIAL_STATUS_IMPLEMENTATION_WINDOW   UINT32_C(1)
#define CELESTIAL_STATUS_FINITE_INPUT_EVALUATION UINT32_C(2)

#define CELESTIAL_MODEL_KIND_DELTA_T         UINT32_C(0)
#define CELESTIAL_MODEL_KIND_NUTATION        UINT32_C(1)
#define CELESTIAL_MODEL_KIND_POSITION_SOURCE UINT32_C(2)
#define CELESTIAL_MODEL_KIND_ALL             UINT32_C(0xFFFFFFFF)

#define CELESTIAL_MODEL_STATUS_CURRENT UINT32_C(0)
#define CELESTIAL_MODEL_STATUS_FROZEN  UINT32_C(1)

/* Explicit identities are 1-based within each CELESTIAL_MODEL_KIND_* kind; a model key
 * is the kind/identity pair. Selectable models use these same values as selectors.
 * DEFAULT (0) is input-only where an operation supports it; position sources have no DEFAULT.
 * Each operation describes its own model use and DEFAULT resolution. */
/**
 * @brief One TT operation's input bounds for one target.
 * @note Domain ends are NaN where no explicit JDE(TT) bound exists. With
 *       CELESTIAL_STATUS_FINITE_INPUT_EVALUATION, finite input is admitted to evaluation,
 *       but coordinate checks or computation can still fail. These bounds promise neither
 *       accuracy nor physical plausibility; validation coverage is described separately
 *       by celestial_capability_text. Civil houses use celestial_house_capability instead.
 */
typedef struct CelestialCapabilityRecord {
  uint32_t operation_code;        /* CELESTIAL_OPERATION_POSITION, LONGITUDE_RATE, EVENTS or EQUATORIAL. */
  uint32_t subject_code;          /* A CELESTIAL_TARGET_* code. */
  uint32_t status_code;           /* A CELESTIAL_STATUS_* code naming the kind of input bound. */
  uint32_t source_code;           /* A CELESTIAL_POSITION_SOURCE_* identity. */
  double domain_start_jde_tt;     /* Inclusive; NaN = no lower bound. */
  double domain_end_jde_tt;       /* Exclusive; NaN = no upper bound. */
  uint32_t maximum_span_tt_days;  /* EVENTS: 3660; 0 = no span limit. */
} CelestialCapabilityRecord;

/** @brief One registered model identity with its applicability.
 * @note Both domain ends are NaN where no explicit bound exists. Evaluation-instant guards
 *       do not imply a universal limit on converted TT output; see celestial_model_identity_text.
 */
typedef struct CelestialModelIdentityRecord {
  uint32_t kind_code;           /* A CELESTIAL_MODEL_KIND_* code, not ALL. */
  uint32_t identity_code;       /* Explicit model value within kind; never DEFAULT (0). */
  uint32_t status_code;         /* CELESTIAL_MODEL_STATUS_CURRENT or CELESTIAL_MODEL_STATUS_FROZEN. */
  double domain_start_jde_tt;   /* Inclusive evaluation-instant bound in JDE(TT); NaN = no explicit bound. */
  double domain_end_jde_tt;     /* Exclusive evaluation-instant bound; see the model text for decimal-year guards. */
} CelestialModelIdentityRecord;

/**
 * @brief List declared TT input bounds and sources for target operations.
 * @param operation_code CELESTIAL_OPERATION_POSITION, CELESTIAL_OPERATION_LONGITUDE_RATE,
 *                       CELESTIAL_OPERATION_EVENTS or CELESTIAL_OPERATION_EQUATORIAL.
 *                       Unknown codes and operations without TT target records fail.
 * @param subject_code A CELESTIAL_TARGET_* code, or CELESTIAL_TARGET_ALL for every target;
 *                     unknown target codes fail.
 * @param record_count Where the total count of records is written. Must not be null.
 *                     On every failure `*record_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written, at most min(total, slot_count); records follow
 *          CELESTIAL_TARGET_* code order.
 * @note Bounds come from the operation's defining constants; passing them does not promise
 *       successful computation or accuracy. For HOUSES use celestial_house_capability;
 *       HOUSES, CHART and DELTA_T model use is described by celestial_capability_text.
 */
CELESTIAL_API uint32_t celestial_capabilities(uint32_t operation_code, uint32_t subject_code, uint32_t *record_count, CelestialCapabilityRecord *slots, uint32_t slot_count);

/**
 * @brief Enumerate the registered model identities (Delta T, nutation, position sources).
 * @param kind_code A CELESTIAL_MODEL_KIND_* code, or CELESTIAL_MODEL_KIND_ALL for every kind.
 *                  Unknown kinds fail.
 * @param record_count Where the total count of records is written. Must not be null.
 *                     On every failure `*record_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written, at most min(total, slot_count); records follow the
 *          registry order (Delta T algo1-5, nutation, position sources).
 * @note Explicit identity values are stable within a major and start at 1 within each kind.
 *       DEFAULT (0) is never a returned identity; its resolution belongs to each operation.
 */
CELESTIAL_API uint32_t celestial_model_identities(uint32_t kind_code, uint32_t *record_count, CelestialModelIdentityRecord *slots, uint32_t slot_count);

/**
 * @brief Describe one model by the exact pair returned by celestial_model_identities.
 * @param kind_code A concrete CELESTIAL_MODEL_KIND_* value, not ALL.
 * @param identity_code A nonzero explicit value within that kind; unknown pairs fail.
 * @param buffer Caller-owned UTF-8 buffer; may be null only when capacity is zero.
 * @param capacity Available bytes, including the NUL terminator; zero queries the size.
 * @return Required bytes including NUL. Positive capacity writes at most capacity - 1
 *         payload bytes and a terminating NUL, truncating if needed.
 * @note Read the error kind after sizing or filling; zero required bytes signals failure.
 */
CELESTIAL_API uint32_t celestial_model_identity_text(uint32_t kind_code, uint32_t identity_code, char *buffer, uint32_t capacity);

/**
 * @brief Describe an operation's source, input policy and model use.
 * @param operation_code A CELESTIAL_OPERATION_* code. Target operations describe TT inputs;
 *                       HOUSES, CHART and DELTA_T describe their operation-specific model use.
 *                       Unknown operations fail. Model identity text uses celestial_model_identity_text.
 * @param subject_code Target operations require one CELESTIAL_TARGET_* code (not ALL).
 *                     HOUSES, CHART and DELTA_T require CELESTIAL_TARGET_ALL because they have no target.
 *                     Unknown or inapplicable subjects fail. Use kind/identity pairs with
 *                     celestial_model_identity_text to describe individual models.
 * @param buffer Caller-owned UTF-8 buffer; may be null only when capacity is zero.
 * @param capacity Available bytes, including the NUL terminator; zero queries the size.
 * @returns Required bytes including NUL, or zero on failure. Positive capacity writes
 *          at most capacity - 1 payload bytes and a terminating NUL, truncating if needed.
 * @note Coordinate descriptions name origin, frame, units and corrections. Model-use descriptions
 *       name source/version, status and role. Validation owners are cited separately;
 *       coordinate validity does not establish accuracy or physical plausibility.
 */
CELESTIAL_API uint32_t celestial_capability_text(uint32_t operation_code, uint32_t subject_code, char *buffer, uint32_t capacity);


/* ---------- Independent Position and House Queries ---------- */


/** @brief One selected target's apparent geocentric position in the true ecliptic of date. */
typedef struct CelestialPositionRecord {
  uint32_t target_code;    /* A CELESTIAL_TARGET_* code; records follow the caller's target order. */
  uint32_t present_fields; /* CELESTIAL_PRESENT_LATITUDE is set; DISTANCE marks distance_au. */
  double longitude_deg;    /* In [0, 360). */
  double latitude_deg;
  double distance_au;      /* Present for Sun, Moon, planets and Pluto; absent (unused storage) for nodes. */
  uint32_t source_code;    /* The resolved CELESTIAL_POSITION_SOURCE_* identity. */
} CelestialPositionRecord;

/** @brief One selected target's apparent geocentric equatorial position of date. */
typedef struct CelestialEquatorialPositionRecord {
  uint32_t target_code;       /* A CELESTIAL_TARGET_* code; records follow the caller's target order. */
  uint32_t present_fields;    /* CELESTIAL_PRESENT_LATITUDE marks declination; DISTANCE marks distance_au. */
  double right_ascension_deg; /* In [0, 360). */
  double declination_deg;     /* In [-90, 90]. */
  double distance_au;         /* Absent for mathematical nodes. */
  uint32_t source_code;       /* The resolved CELESTIAL_POSITION_SOURCE_* identity. */
} CelestialEquatorialPositionRecord;

/** @brief Independent house geometry with resolved explicit model identities. */
typedef struct CelestialHouseQueryResult {
  bool valid;                       /* false on failure; the payload and padding then have no contract. */
  uint32_t house_system_code;
  uint32_t delta_t_identity_code;   /* A CELESTIAL_DELTA_T_MODEL_* value, never DEFAULT. */
  uint32_t nutation_identity_code;  /* A CELESTIAL_NUTATION_MODEL_* value, never DEFAULT. */
  double jd_ut1;
  double jde_tt;
  CelestialChartHouses houses;
} CelestialHouseQueryResult;

/**
 * @brief Selected-target apparent geocentric positions at one JDE(TT) instant.
 * @param target_codes The caller's selection: an array of CELESTIAL_TARGET_* codes, allocated and
 *                     freed by the caller; must not be null. Output records follow this order.
 * @param target_count The selection size; must be positive, with no duplicated target.
 * @param jde_tt A finite JDE(TT). Each target's provider window applies (Pluto and the nodes
 *               publish one; see celestial_capabilities); no rate stencil is evaluated, so the
 *               snapshot joint window and the rate reach do not constrain this query.
 * @param record_count Where the total count of records is written. Must not be null.
 *                     On every failure `*record_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0 (a count-only pass).
 * @param slot_count The count of slots.
 * @returns How many slots are written, at most min(total, slot_count).
 * @note One failed target fails the whole query: no partial list is ever returned, and the
 *       legal empty outcome never occurs (target_count is positive). A subset query may succeed
 *       where a complete snapshot would reject an excluded target's window.
 */
CELESTIAL_API uint32_t celestial_positions(const uint32_t *target_codes, uint32_t target_count, double jde_tt, uint32_t *record_count, CelestialPositionRecord *slots, uint32_t slot_count);

/**
 * @brief Selected-target geocentric apparent RA/Dec at JDE(TT), in caller order.
 * @param target_codes A non-null, nonempty selection of distinct CELESTIAL_TARGET_* values.
 * @param target_count The number of selected targets.
 * @param jde_tt Finite JDE(TT); declared provider windows apply independently to each target.
 * @param record_count Non-null total-count output, set to zero on failure.
 * @param slots Caller-owned output, null only when slot_count is zero.
 * @param slot_count Output capacity in records.
 * @return Number written, at most min(total, slot_count); a failed target fails the whole query.
 * @note Fixed IAU 1980 longitude nutation and true obliquity; no rate, civil or house evaluation.
 *       Coordinate validity does not establish accuracy or physical plausibility at remote epochs.
 */
CELESTIAL_API uint32_t celestial_equatorial_positions(const uint32_t *target_codes, uint32_t target_count, double jde_tt, uint32_t *record_count, CelestialEquatorialPositionRecord *slots, uint32_t slot_count);

/**
 * @brief House geometry at a civil instant, independent of any body evaluation.
 * @param year Gregorian civil year in [1885, 2100) — the same civil contract as the snapshot.
 * @param month Gregorian month in [1, 12].
 * @param day A valid day of the Gregorian month.
 * @param fraction Finite fraction of the civil day in [0, 1); inserted leap seconds are outside the input.
 * @param civil_scale_code CELESTIAL_CIVIL_SCALE_UTC (from 1972-01-01) or CELESTIAL_CIVIL_SCALE_UT1.
 * @param delta_t_model_code A CELESTIAL_DELTA_T_MODEL_* selector; DEFAULT resolves to the core default.
 * @param nutation_model_code A CELESTIAL_NUTATION_MODEL_* selector; DEFAULT resolves to the
 *                            house operation's default.
 * @param latitude_deg North-positive latitude in (-90, 90).
 * @param longitude_deg East-positive longitude in [-180, 180].
 * @param house_system_code A CELESTIAL_HOUSE_SYSTEM_* code; Placidus rejects polar geometry, and
 *                          every system rejects an Ascendant direction unresolved at round-off.
 * @returns A CelestialHouseQueryResult by value. On failure valid is false; read
 *          celestial_last_error_kind and celestial_last_error for the cause. Invalid payload
 *          and padding bytes have no contract. There is no partial result.
 * @note The shared civil admission policy requires derived JDE(TT) < 2464328.5 for Algo4.
 *       No chart joint target/rate window applies; conversion and geometric failures remain possible.
 *       The result records the resolved Delta T and nutation identities.
 */
CELESTIAL_API CelestialHouseQueryResult celestial_house_query(int32_t year, uint32_t month, uint32_t day, double fraction, uint32_t civil_scale_code, uint32_t delta_t_model_code, uint32_t nutation_model_code, double latitude_deg, double longitude_deg, uint32_t house_system_code);

/** @brief The adopted civil-house admission policy, separate from geometric solvability. */
typedef struct CelestialCivilHouseCapability {
  bool valid;
  uint32_t status_code; /* CELESTIAL_STATUS_IMPLEMENTATION_WINDOW. */
  int32_t civil_start_year; /* Inclusive. */
  int32_t civil_end_year_exclusive; /* Exclusive. */
  uint32_t civil_scale_mask; /* Bit CELESTIAL_CIVIL_SCALE_* is set for an allowed scale. */
  int32_t utc_start_year;
  uint32_t utc_start_month;
  uint32_t utc_start_day;
  uint32_t delta_t_identity_code; /* Resolved CELESTIAL_DELTA_T_MODEL_* value, never DEFAULT. */
  bool has_derived_tt_cutoff; /* Whether the selected model constrains this civil policy's derived TT. */
  double derived_tt_end_jde_tt; /* Exclusive; meaningful only when has_derived_tt_cutoff. */
} CelestialCivilHouseCapability;

/**
 * @brief Describe the civil-house policy for one selected Delta T model.
 * @param delta_t_model_code A CELESTIAL_DELTA_T_MODEL_* selector; DEFAULT resolves to the core default.
 * @return Policy facts and resolved model identity; invalid on an unknown selector.
 * @note The policy is [1885, 2100), with UTC from 1972-01-01 and an exclusive derived-TT
 *       cutoff of 2464328.5 for Algo4 only. Admission does not promise geometric success.
 *       There is no chart target/rate window here; this is not a numeric TT house-domain record.
 */
CELESTIAL_API CelestialCivilHouseCapability celestial_house_capability(uint32_t delta_t_model_code);


#ifdef __cplusplus
}
#endif

/* NOLINTEND(modernize-use-trailing-return-type, modernize-use-using) */

#endif /* CELESTIAL_CALENDAR_H */
