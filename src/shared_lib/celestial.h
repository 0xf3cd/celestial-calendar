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
 * Two deliberate decisions: the exported symbols carry no prefix, matching the exports
 * shipped since 0.3.0 — the collision risk is accepted and revisited at the next major
 * version. And the export set is exactly the entry points declared below (#91): each one
 * is marked `CELESTIAL_API`, and consumers need no defines — the macro reads as dllimport
 * on Windows and default visibility elsewhere; only the library's own build defines
 * `CELESTIAL_BUILDING_DLL` (CMake injects it privately) to get dllexport.
 *
 * ABI evolution policy (#129): within a major version, struct layouts never move —
 * sizes and offsets are pinned by `c_header_check.c` at compile time, and a struct is
 * frozen the moment it lands here. Changing a field (type, order, or count) or an
 * existing function's signature means a new major version, or a `_v2`-named function
 * returning a new, separately named struct. Adding a struct or a function never breaks
 * the ABI; widening what an existing function accepts (as `algo` was widened to 3)
 * breaks neither the ABI nor the contract; narrowing it keeps the ABI but breaks
 * the contract, and needs a new major version just the same.
 *
 * The chart snapshot carries `_v1` from its first release (`chart_snapshot_v1`,
 * `ChartSnapshotV1`): an incompatible change to its fields or body roster takes the
 * second route above, a separately named version beside V1.
 *
 * Error contract: every function is `noexcept` at the boundary. Struct-returning
 * functions signal failure with `valid = false`; the rest return `0` / `false`.
 * On failure every function except `last_error` also records a thread-local
 * message readable through it.
 *
 * Thread-safety contract: every entry point may be called concurrently from any
 * number of threads, with no host-side synchronization. `set_log_verbosity` turns
 * a process-wide knob: writes are atomic — a concurrent reader always sees one
 * whole level or another — but which of two racing writes wins is unspecified.
 * `last_error` is per-thread: it reports the calling thread's most recent call to an
 * exported function except `last_error`, and no thread ever observes another's message.
 * Two computations memoize per argument — the jieqi moments and the algo-2 lunar year
 * info. Both caches are shared process-wide and never erased: the first call with a given
 * argument pays for it, later calls from any thread reuse the result, and memory grows
 * monotonically with the number of distinct arguments ever queried. Two threads that
 * miss on the same argument at once both compute it, and one of the results is discarded.
 * Separately, the first `gregorian_to_lunar` or `lunar_to_gregorian` with `algo = 2`
 * runs the astronomical pipeline once to establish that algorithm's supported range;
 * unlike the caches above, that one blocks every thread that arrives while it is in
 * flight, and costs nothing afterwards. Everything else recomputes on each call, apart
 * from lookups of values already fixed before `main` — the supported ranges reported by
 * `get_supported_lunar_year_range`, which are settled at load time at the latest.
 *
 * Platform note: the library logs to stdout and swallows any logging failure. On
 * Windows (UCRT), however, writing to a closed stdout fail-fasts the process
 * (0xc0000409) through the invalid-parameter handler — not an exception path, so
 * the library cannot catch it. Hosts must not close the logging stream out from
 * under the library; an opt-out log sink is tracked on the backlog.
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


/* ---------- Global configuration ---------- */

/**
 * @brief Set the verbosity level of log printing.
 * @param new_value The new verbosity level (in `uint8_t`): 0 = none, 1 = info, 2 = debug.
 *                  The initial level is 0 (none) — logging is opt-in.
 * @returns `true` if the level was stored, `false` if `new_value` is out of range.
 */
CELESTIAL_API bool set_log_verbosity(uint8_t new_value);

/**
 * @brief Get the last-error message of the calling thread.
 * @returns A pointer to a thread-local C string, empty if there is no recorded error.
 *          Every exported function except `last_error` writes and clears the message,
 *          so the pointer refers to the most recent call on this thread. It stays valid
 *          until the next exported function call other than `last_error` on the same thread.
 * @note #97: FFI consumers get only a failure sentinel without this channel.
 */
CELESTIAL_API const char *last_error(void);


/* ---------- Julian Days ---------- */

typedef struct JulianDay {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The value. Either JD or JDE. */
} JulianDay;

/**
 * @brief Convert UT1 datetime to Julian Day Number (JD).
 * @param y The year, in [1, 32767].
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `JulianDay` struct. JD is based on UT1.
 */
CELESTIAL_API JulianDay ut1_to_jd(int32_t y, uint32_t m, uint32_t d, double fraction);

/**
 * @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE).
 * @param y The year, in [1, 32767]; the ΔT-shifted TT instant must also stay representable.
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `JulianDay` struct. JDE is based on TT.
 */
CELESTIAL_API JulianDay ut1_to_jde(int32_t y, uint32_t m, uint32_t d, double fraction);

typedef struct UT1Time {
  bool     valid;    /* Indicates if the result is valid. */
  int32_t  year;     /* The year. */
  uint32_t month;    /* The month. */
  uint32_t day;      /* The day. */
  double   fraction; /* The fraction of the day, in the range [0.0, 1.0). */
} UT1Time;

/**
 * @brief Convert Julian Ephemeris Day Number (JDE) to UT1 datetime.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `UT1Time` struct.
 */
CELESTIAL_API UT1Time jde_to_ut1(double jde);


/* ---------- Sun and Moon Apparent Geocentric Position ---------- */

typedef struct SunCoordinate {
  bool   valid; /* Indicates if the result is valid. */
  double lon;   /* The longitude. In degrees. */
  double lat;   /* The latitude. In degrees. */
  double r;     /* The radius. In AU. */
} SunCoordinate;

/**
 * @brief Calculate the apparent geocentric position of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `SunCoordinate` struct.
 */
CELESTIAL_API SunCoordinate sun_apparent_geocentric_coord(double jde);

typedef struct MoonCoordinate {
  bool   valid; /* Indicates if the result is valid. */
  double lon;   /* The longitude. In degrees. */
  double lat;   /* The latitude. In degrees. */
  double r;     /* The radius. In KM. */
} MoonCoordinate;

/**
 * @brief Calculate the apparent geocentric position of the Moon.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `MoonCoordinate` struct.
 */
CELESTIAL_API MoonCoordinate moon_apparent_geocentric_coord(double jde);

typedef struct MoonIllumination {
  bool   valid;          /* Indicates if the result is valid. */
  double illumination;   /* The illuminated fraction of the Moon's disk, in [0, 1]. */
  double elongation_deg; /* The apparent longitude difference Moon − Sun, in degrees, in [0, 360). */
} MoonIllumination;

/**
 * @brief Compute the Moon's illuminated fraction k and elongation at a JDE
 *        (Meeus ch. 48: k from (48.2)+(48.3)+(48.1)).
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `MoonIllumination` struct. `elongation_deg` is the apparent ecliptic longitude
 *          difference that defines the phases: 0° new, 90° first quarter, 180° full,
 *          270° last quarter.
 */
CELESTIAL_API MoonIllumination moon_illumination(double jde);

typedef struct MoonPositionAngle {
  bool   valid;     /* Indicates if the result is valid. */
  double angle_deg; /* Position angle of the Moon's bright limb, in [0, 360). */
} MoonPositionAngle;

/**
 * @brief Compute the position angle of the Moon's bright limb at a JDE (Meeus ch. 48, (48.5)).
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns A `MoonPositionAngle` struct; `angle_deg` is measured eastward from the north
 *          point of the disk, in [0, 360).
 */
CELESTIAL_API MoonPositionAngle moon_position_angle(double jde);


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
CELESTIAL_API uint32_t moon_phase_moments(int32_t year, uint8_t phase_kind, uint32_t *root_count, double *slots, uint32_t slot_count);


/* ---------- Solar Longitude Roots ---------- */

typedef struct Discriminant {
  bool     valid; /* Indicates if the result is valid. */
  uint32_t count; /* The count of the roots, which is 0, 1, or 2. */
} Discriminant;

/**
 * @brief Calculate the JDE discriminant — count how many times the Sun reaches the given
 *        geocentric `longitude` in the given `year`.
 * @param year The Gregorian year, in [1, 32766].
 * @param longitude The geocentric longitude.
 * @returns A `Discriminant` struct; `count` is 0, 1, or 2: the Sun won't reach the longitude
 *          in the year, reaches it once, or reaches it twice.
 */
CELESTIAL_API Discriminant solar_lon_root_discriminant(int32_t year, double longitude);

/**
 * @brief Find the JDE(s) at which the Sun reaches `longitude` in `year`, written to `slots`.
 * @param year The Gregorian year, in [1, 32766].
 * @param longitude The geocentric longitude.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
CELESTIAL_API uint32_t solar_lon_roots(int32_t year, double longitude, double *slots, uint32_t slot_count);


/* ---------- Sun Moon Conjunction ---------- */

/**
 * @brief Find the next `slot_count` new-moon JDE(s) after `jde`, written to `slots`.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
CELESTIAL_API uint32_t new_moons_after_jde(double jde, double *slots, uint32_t slot_count);

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
CELESTIAL_API uint32_t new_moons_in_year(int32_t year, uint32_t *root_count, double *slots, uint32_t slot_count);


/* ---------- Solar Time ---------- */

typedef struct EquationOfTime {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* E in degrees of hour angle; x240 for seconds of time. */
} EquationOfTime;

/**
 * @brief Compute the equation of time E = apparent solar time - mean solar time
 *        (Meeus ch. 28).
 * @param jde The julian ephemeris day number, which is based on TT.
 * @returns An `EquationOfTime` struct; `value` is E in degrees of hour angle
 *          (x240 = seconds of time).
 */
CELESTIAL_API EquationOfTime equation_of_time(double jde);

typedef struct ApparentSolarTime {
  bool     valid;    /* Indicates if the result is valid. */
  int32_t  year;     /* The year of the local apparent solar date. */
  uint32_t month;    /* The month of the local apparent solar date. */
  uint32_t day;      /* The day of the local apparent solar date. */
  double   fraction; /* The fraction of the local apparent solar day, in the range [0.0, 1.0). */
} ApparentSolarTime;

/**
 * @brief Convert a civil UTC moment to local apparent (true) solar time.
 * @param y The year (UTC), in [1, 32767]; the shifted TT instant must also stay representable.
 * @param m The month (UTC).
 * @param d The day (UTC).
 * @param fraction The fraction of the day (UTC). Must be in the range [0.0, 1.0).
 * @param longitude The observer's geographic longitude in degrees, positive east, in [-180, 180].
 * @returns An `ApparentSolarTime` struct; the local apparent solar date may differ from the
 *          input UTC date — a large enough longitude shifts the moment across midnight.
 */
CELESTIAL_API ApparentSolarTime apparent_solar_time(int32_t y, uint32_t m, uint32_t d, double fraction, double longitude);


/* ---------- Sidereal Time ---------- */

typedef struct SiderealTime {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The local apparent sidereal time, in degrees, in [0, 360). */
} SiderealTime;

/**
 * @brief Compute the Local Apparent Sidereal Time (LAST) for an observer.
 * @param jd_ut1 The julian day number, which is based on **UT1**. Declared domain:
 *        Gregorian years in [401, 32766].
 * @param longitude The observer's geographic longitude in degrees, positive east, in [-180, 180].
 * @returns A `SiderealTime` struct. The ΔT model is the library default (algo5) and the
 *          nutation model is IAU 1980 — both baked in, matching the other single-default
 *          entry points.
 */
CELESTIAL_API SiderealTime local_apparent_sidereal_time(double jd_ut1, double longitude);


/* ---------- Jieqi ---------- */

typedef struct JieqiMomentQuery {
  bool     valid;  /* Indicates if the result is valid. */
  uint8_t  jq_idx; /* The Jieqi index in [0, 24), in ecliptic order: 0 = 立春 (Lichun) … 23 = 大寒. */
  int32_t  y;      /* The year. */
  uint32_t m;      /* The month. */
  uint32_t d;      /* The day. */
  double   frac;   /* The fraction of the day, in the range [0.0, 1.0). */
} JieqiMomentQuery;

/**
 * @brief Query the accurate UT1 moment of the Jieqi in the given `year`.
 * @param year The Gregorian year, in [401, 32766].
 * @param jq_idx The Jieqi index in [0, 24), in ecliptic order: 0 = 立春 (Lichun) … 23 = 大寒.
 * @returns A `JieqiMomentQuery` struct.
 * @note The returned civil moment is UT1 (from TT through ΔT algo5), not UTC. While leap
 *       seconds are applied, |UT1 − UTC| < 0.9 s; past the last ΔAT table step the modelled
 *       UT1 − UTC is (ΔAT + 32.184 s) − ΔT, no longer bounded by 0.9 s (#115). The gap by
 *       era: `docs/DESIGN.md#time-scales`.
 * @note The returned date is not an east-eight wall date; rendering the same instant at a
 *       fixed UTC+8 offset can change its calendar date.
 */
CELESTIAL_API JieqiMomentQuery query_jieqi_moment(int32_t year, uint8_t jq_idx);

/**
 * @brief Write the Chinese name of the Jieqi to `buf`.
 * @param jq_idx The Jieqi index in [0, 24), in ecliptic order: 0 = 立春 (Lichun) … 23 = 大寒.
 * @param buf The name memory, allocated and freed by the caller.
 * @param buf_size Maximum bytes that can be written to `buf`.
 * @returns `true` if the name is successfully written to `buf`.
 */
CELESTIAL_API bool get_jieqi_name(uint8_t jq_idx, char *buf, uint32_t buf_size);


/* ---------- Lunar Calendar ---------- */

typedef struct SupportedLunarYearRange {
  bool    valid; /* Indicates if the result is valid. */
  int32_t start; /* The first supported lunar year. */
  int32_t end;   /* The last supported lunar year. */
} SupportedLunarYearRange;

/**
 * @brief Get the supported lunar year range of the algorithm.
 * @param algo The algorithm. Expected to be 1, 2, or 3.
 * @returns A `SupportedLunarYearRange` struct.
 * @details Algorithm profiles:
 *          - 1: preserves the published HKO calendar-date labels for 1901-2099;
 *          - 2: computation from VSOP87D/ELP2000-82B for 410-2500, with TT moments rendered
 *            through the library's UTC model and then a fixed east-eight offset; that model uses
 *            UT1 as a pre-1972 proxy, the ΔAT table from 1972 through its final announced step,
 *            and a frozen ΔAT of 37 s afterward;
 *          - 3: a baked table for 1600-2199, using algorithm 1 for 1901-2099 and algorithm 2
 *            for the other years.
 */
CELESTIAL_API SupportedLunarYearRange get_supported_lunar_year_range(uint8_t algo);

typedef struct LunarYearInfo {
  bool     valid;      /* Indicates if the result is valid. */
  int32_t  year;       /* Gregorian year of the first day of the lunar year. */
  uint8_t  month;      /* Gregorian month of the first day of the lunar year. */
  uint8_t  day;        /* Gregorian day of the first day of the lunar year. */
  uint8_t  leap_month; /* The leap month (1-12) in **traditional** numbering, or 0 if none. */
  uint16_t month_len;  /* Least 12/13 bits, one per month in calendar order (bit 0 = the first
                          month): 1 = 30-day month, 0 = 29-day month. A leap month occupies a
                          bit of its own, so with `leap_month = 2` the bits run 1, 2, leap 2,
                          3, … `gregorian_to_lunar` / `lunar_to_gregorian` below instead speak
                          traditional numbering + `is_leap`, where that leap month is
                          `month = 2, is_leap = true`. */
} LunarYearInfo;

/**
 * @brief Get the lunar year information for the given year.
 * @param algo The algorithm profile described by `get_supported_lunar_year_range`.
 * @param year The lunar year. Outside the range `get_supported_lunar_year_range` reports for
 *             `algo`, the result is `valid = false`.
 * @returns A `LunarYearInfo` struct.
 */
CELESTIAL_API LunarYearInfo get_lunar_year_info(uint8_t algo, int32_t year);

typedef struct LunarDate {
  bool    valid;   /* Indicates if the result is valid. */
  int32_t year;    /* The lunar year. */
  uint8_t month;   /* The lunar month, in traditional numbering (1-12). */
  bool    is_leap; /* Whether the month is the leap month. */
  uint8_t day;     /* The day of the lunar month. */
} LunarDate;

typedef struct GregorianDate {
  bool    valid; /* Indicates if the result is valid. */
  int32_t year;  /* The year. */
  uint8_t month; /* The month. */
  uint8_t day;   /* The day. */
} GregorianDate;

/**
 * @brief Convert a Gregorian date to a lunar date.
 * @param algo The algorithm profile described by `get_supported_lunar_year_range`.
 * @param year The Gregorian year, in [-32767, 32767]; an unrepresentable integer is rejected,
 *             never narrowed to an aliased year.
 * @param month The Gregorian month.
 * @param day The Gregorian day of the month.
 * @returns A `LunarDate` struct; `valid = false` when `algo` is unknown, the input is not a
 *          real Gregorian date, or it falls outside the Gregorian span the algorithm covers —
 *          that span runs from the first day of lunar year `start` to the last day of lunar
 *          year `end` (`start`/`end` as `get_supported_lunar_year_range` reports them), so it
 *          begins and ends mid-Gregorian-year at both ends.
 * @note The lunar month is in **traditional numbering** (1-12) plus the `is_leap` flag —
 *       a leap month carries its predecessor's number with `is_leap = true`: in lunar 2023
 *       (leap 2nd month), the leap 2nd month is `month = 2, is_leap = true`, and the
 *       traditional 3rd month is `month = 3, is_leap = false`. This is NOT the positional
 *       month index that `LunarYearInfo.month_len` is indexed by.
 * @note The Gregorian input is a calendar-date label on the selected algorithm's basis, not
 *       an instant.
 */
CELESTIAL_API LunarDate gregorian_to_lunar(uint8_t algo, int32_t year, uint8_t month, uint8_t day);

/**
 * @brief Convert a lunar date to a Gregorian date.
 * @param algo The algorithm profile described by `get_supported_lunar_year_range`.
 * @param year The lunar year.
 * @param month The lunar month, in traditional numbering (1-12) — see `gregorian_to_lunar`.
 * @param is_leap Whether the month is the leap month. `true` is accepted only on the year's
 *                actual leap month — `month = 2` in lunar 2023, and no month at all in a year
 *                that has none.
 * @param day The day of the lunar month.
 * @returns A `GregorianDate` struct; `valid = false` when the input does not name a real
 *          lunar date (bad `algo`, year out of range, no such leap month, day out of range).
 * @note The returned Gregorian value is a calendar-date label on the selected algorithm's
 *       basis, not an instant; see `gregorian_to_lunar`.
 */
CELESTIAL_API GregorianDate lunar_to_gregorian(uint8_t algo, int32_t year, uint8_t month, bool is_leap, uint8_t day);


/* ---------- Delta T ---------- */

typedef struct DeltaT {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The value of delta T. */
} DeltaT;

/**
 * @brief Compute delta T of a given moment using algorithm 1.
 * @param year A finite decimal Gregorian year, at least -4000.
 * @returns A `DeltaT` struct.
 * @note Frozen exhibit from Xu Jianwei's 2008 model, retained for historical comparison.
 */
CELESTIAL_API DeltaT delta_t_algo1(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 2.
 * @param year A finite decimal Gregorian year; no model-specific year bound.
 * @returns A `DeltaT` struct.
 * @note Frozen exhibit from Espenak and Meeus, NASA/TP-2006-214141, retained for historical
 *       comparison.
 */
CELESTIAL_API DeltaT delta_t_algo2(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 3.
 * @param year A finite decimal Gregorian year below 3000.
 * @returns A `DeltaT` struct.
 * @note Frozen exhibit from Fred Espenak's 2014 eclipse canon, retained for historical comparison.
 */
CELESTIAL_API DeltaT delta_t_algo3(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 4.
 * @param year A finite decimal Gregorian year below 2035.
 * @returns A `DeltaT` struct.
 * @note Frozen predecessor to algorithm 5, fitted from IERS Bulletin A observations and USNO
 *       predictions; retained for historical comparison.
 */
CELESTIAL_API DeltaT delta_t_algo4(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 5.
 * @param year A finite decimal Gregorian year; no model-specific year bound.
 * @returns A `DeltaT` struct.
 * @note The current project model and default: algorithm 2 before 2005, IERS Bulletin A fit
 *       through 2026.41, then an anchored long-term extrapolation.
 */
CELESTIAL_API DeltaT delta_t_algo5(double year);
/**
 * @brief Compute delta T of a given moment using the current default model (algorithm 5).
 * @param year A finite decimal Gregorian year; no model-specific year bound.
 * @returns A `DeltaT` struct.
 */
CELESTIAL_API DeltaT delta_t(double year);


/* ---------- Fixed-version Chart Snapshot ---------- */

#define CHART_CIVIL_SCALE_UTC UINT32_C(0)
#define CHART_CIVIL_SCALE_UT1 UINT32_C(1)

#define CHART_HOUSE_SYSTEM_EQUAL      UINT32_C(0)
#define CHART_HOUSE_SYSTEM_WHOLE_SIGN UINT32_C(1)
#define CHART_HOUSE_SYSTEM_PLACIDUS   UINT32_C(2)

#define CHART_DELTA_T_MODEL_DEFAULT UINT32_C(0)
#define CHART_DELTA_T_MODEL_ALGO1   UINT32_C(1)
#define CHART_DELTA_T_MODEL_ALGO2   UINT32_C(2)
#define CHART_DELTA_T_MODEL_ALGO3   UINT32_C(3)
#define CHART_DELTA_T_MODEL_ALGO4   UINT32_C(4)
#define CHART_DELTA_T_MODEL_ALGO5   UINT32_C(5)

#define CHART_TARGET_SUN             UINT32_C(0)
#define CHART_TARGET_MOON            UINT32_C(1)
#define CHART_TARGET_MERCURY         UINT32_C(2)
#define CHART_TARGET_VENUS           UINT32_C(3)
#define CHART_TARGET_MARS            UINT32_C(4)
#define CHART_TARGET_JUPITER         UINT32_C(5)
#define CHART_TARGET_SATURN          UINT32_C(6)
#define CHART_TARGET_URANUS          UINT32_C(7)
#define CHART_TARGET_NEPTUNE         UINT32_C(8)
#define CHART_TARGET_PLUTO           UINT32_C(9)
#define CHART_TARGET_MEAN_ASCENDING  UINT32_C(10)
#define CHART_TARGET_MEAN_DESCENDING UINT32_C(11)
#define CHART_TARGET_TRUE_ASCENDING  UINT32_C(12)
#define CHART_TARGET_TRUE_DESCENDING UINT32_C(13)

#define CHART_PRESENT_LATITUDE UINT32_C(1)
#define CHART_PRESENT_DISTANCE UINT32_C(2)

/** @brief One geocentric target in the true ecliptic and equinox of date. */
typedef struct ChartBodyV1 {
  uint32_t target_code; /* A CHART_TARGET_* code; records follow code order 0-13. */
  uint32_t present_fields; /* CHART_PRESENT_* bits; absent fields are unused storage, not sentinels. */
  double longitude_deg; /* In [0, 360). */
  double latitude_deg; /* Present for every target, including the nodes' known zero latitude. */
  double distance_au; /* Present for Sun, Moon and planets; absent for nodes. */
  double longitude_rate_deg_per_tt_day; /* Instantaneous signed rate; negative means retrograde at this instant. */
} ChartBodyV1;

/** @brief Four axes and twelve cusps in degrees, each in [0, 360); cusps are houses 1-12. */
typedef struct ChartHousesV1 {
  double ascendant_deg;
  double midheaven_deg;
  double descendant_deg;
  double imum_coeli_deg;
  double cusps_deg[12];
} ChartHousesV1;

/** @brief An owned complete V1 snapshot; consume the payload only when valid is true. */
typedef struct ChartSnapshotV1 {
  bool valid;
  double jd_ut1;
  double jde_tt;
  ChartBodyV1 bodies[14];
  ChartHousesV1 houses;
} ChartSnapshotV1;

/**
 * @brief Calculate a complete fixed-version chart at an explicitly scaled civil instant.
 * @param year Gregorian civil year in [1885, 2100).
 * @param month Gregorian month in [1, 12].
 * @param day A valid day of the Gregorian month.
 * @param fraction Finite fraction of the civil day in [0, 1); inserted leap seconds are outside the input.
 * @param civil_scale_code CHART_CIVIL_SCALE_UTC (from 1972-01-01) or CHART_CIVIL_SCALE_UT1.
 * @param latitude_deg North-positive latitude in (-90, 90).
 * @param longitude_deg East-positive longitude in [-180, 180].
 * @param house_system_code A CHART_HOUSE_SYSTEM_* code; Placidus rejects polar geometry.
 *                          All systems reject an Ascendant direction unresolved at round-off.
 * @param delta_t_model_code A CHART_DELTA_T_MODEL_* code; DEFAULT selects Algo5.
 * @returns A ChartSnapshotV1 by value. On failure valid is false and last_error reports the cause;
 *          invalid payload and padding bytes have no contract. There is no partial result.
 * @note JDE(TT) must be in [2409543.875, 2488069.125); Algo4 additionally requires JDE(TT) < 2464328.5.
 *       UT1 derived from UTC is modelled through the selected Delta T, not observed DUT1.
 *       Location affects houses only; timezone conversion is the caller's responsibility.
 */
CELESTIAL_API ChartSnapshotV1 chart_snapshot_v1(int32_t year, uint32_t month, uint32_t day, double fraction, uint32_t civil_scale_code, double latitude_deg, double longitude_deg, uint32_t house_system_code, uint32_t delta_t_model_code);


/* ---------- Ephemeris Events ---------- */

/*
 * #303: interval longitude events over the same fourteen targets the chart speaks
 * (CHART_TARGET_* order, duplicated as EVENT_TARGET_* so event consumers need no chart
 * vocabulary; c_header_check.c pins the two identities equal). Times are JDE(TT) and
 * rates are degrees per TT day, mirroring docs/EPHEMERIS_EVENTS.md. Angles carry no
 * names or interpretations — the API never speaks aspects or orbs.
 *
 * Ownership protocol: one call reports the total through `event_count` and writes up to
 * `slot_count` records into caller-owned `slots` (NULL only when `slot_count` is 0).
 * The return value is how many records were written; when the total exceeds the
 * capacity the fill is truncated and `event_count` still reports the full total, so the
 * overflow is observable, never silent. Records are in strict chronological order.
 * A zero total is a legal outcome; every failure also leaves `*event_count` at 0, and
 * is told apart from the empty success only through `last_error` (empty on success).
 */

#define EVENT_TARGET_SUN             UINT32_C(0)
#define EVENT_TARGET_MOON            UINT32_C(1)
#define EVENT_TARGET_MERCURY         UINT32_C(2)
#define EVENT_TARGET_VENUS           UINT32_C(3)
#define EVENT_TARGET_MARS            UINT32_C(4)
#define EVENT_TARGET_JUPITER         UINT32_C(5)
#define EVENT_TARGET_SATURN          UINT32_C(6)
#define EVENT_TARGET_URANUS          UINT32_C(7)
#define EVENT_TARGET_NEPTUNE         UINT32_C(8)
#define EVENT_TARGET_PLUTO           UINT32_C(9)
#define EVENT_TARGET_MEAN_ASCENDING  UINT32_C(10)
#define EVENT_TARGET_MEAN_DESCENDING UINT32_C(11)
#define EVENT_TARGET_TRUE_ASCENDING  UINT32_C(12)
#define EVENT_TARGET_TRUE_DESCENDING UINT32_C(13)

#define EVENT_DIRECTION_INCREASING UINT32_C(0)
#define EVENT_DIRECTION_DECREASING UINT32_C(1)
#define EVENT_DIRECTION_TANGENT    UINT32_C(2)

#define EVENT_MOTION_DIRECT    UINT32_C(0)
#define EVENT_MOTION_RETROGRADE UINT32_C(1)

/** @brief One longitude crossing or exact numerical contact at the supplied level. */
typedef struct LongitudeEventRecord {
  double   jde_tt;                        /* JDE(TT) of the event, on the reported 2^-30-day lattice. */
  double   longitude_deg;                 /* Apparent geocentric longitude at the event, in degrees; not
                                             normalized — values on either side of a branch cut stay raw. */
  double   longitude_rate_deg_per_tt_day; /* The published seven-point rate at the event. */
  uint32_t direction_code;                /* An EVENT_DIRECTION_* code. */
} LongitudeEventRecord;

/** @brief One stationary instant of the published seven-point longitude rate. */
typedef struct StationEventRecord {
  double   jde_tt;                        /* JDE(TT) of the stationary instant. */
  double   longitude_deg;                 /* Apparent geocentric longitude at the event, in degrees; not
                                             normalized — values on either side of a branch cut stay raw. */
  double   longitude_rate_deg_per_tt_day; /* The published rate at the event; need not be bitwise zero. */
  uint32_t motion_before_code;            /* An EVENT_MOTION_* code: certified motion before the instant. */
  uint32_t motion_after_code;             /* An EVENT_MOTION_* code: certified motion after the instant. */
} StationEventRecord;

/** @brief One event of the directed, canonical longitude difference first minus second. */
typedef struct LongitudeDifferenceEventRecord {
  double   jde_tt;                              /* JDE(TT) of the event. */
  double   longitude_a_deg;                     /* First target's longitude at the event, in degrees; not normalized. */
  double   longitude_b_deg;                     /* Second target's longitude at the event, in degrees; not normalized. */
  double   longitude_rate_a_deg_per_tt_day;     /* First target's published rate at the event. */
  double   longitude_rate_b_deg_per_tt_day;     /* Second target's published rate at the event. */
  double   signed_difference_deg;               /* Canonical (first - second) difference, in (-180, 180]. */
  uint32_t direction_code;                      /* An EVENT_DIRECTION_* code. */
} LongitudeDifferenceEventRecord;

/**
 * @brief Find the JDE(TT)s where `target`'s apparent geocentric longitude reaches
 *        `target_longitude_deg` in the half-open interval [start, end), written to `slots`.
 * @param target_code An EVENT_TARGET_* code.
 * @param target_longitude_deg A finite angle in degrees; whole-turn shifts denote the same level.
 * @param start_jde_tt Inclusive start, finite, within the target's longitude-rate domain.
 * @param end_jde_tt Exclusive end; the nonempty span must not exceed 3660 TT days.
 * @param event_count Where the total count of events is written. Must not be null.
 *                    On every failure `*event_count` is set to 0; a zero total is also
 *                    the legal no-event outcome, disambiguated by `last_error`.
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
CELESTIAL_API uint32_t longitude_events(uint32_t target_code, double target_longitude_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, LongitudeEventRecord *slots, uint32_t slot_count);

/**
 * @brief Find the JDE(TT)s where `target`'s published seven-point longitude rate is
 *        stationary in the half-open interval [start, end), written to `slots`.
 * @param target_code An EVENT_TARGET_* code.
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
 *       `longitude_events`.
 */
CELESTIAL_API uint32_t longitude_stations(uint32_t target_code, double start_jde_tt, double end_jde_tt, uint32_t *event_count, StationEventRecord *slots, uint32_t slot_count);

/**
 * @brief Find the JDE(TT)s where the directed difference (first - second) of the two
 *        targets' apparent geocentric longitudes reaches `target_difference_deg` in the
 *        half-open interval [start, end), written to `slots`.
 * @param first_target_code An EVENT_TARGET_* code.
 * @param second_target_code An EVENT_TARGET_* code, distinct from the first.
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
 *       target) is a legal empty list; a continuously satisfied relation throws.
 * @note Reversing the two targets reverses the requested angle and crossing direction.
 *       Self-pairs and continuously satisfied relations are invalid; numerical ambiguity
 *       or work-limit exhaustion fails the whole query. The floating-point premises are
 *       the same as for `longitude_events`.
 */
CELESTIAL_API uint32_t longitude_difference_events(uint32_t first_target_code, uint32_t second_target_code, double target_difference_deg, double start_jde_tt, double end_jde_tt, uint32_t *event_count, LongitudeDifferenceEventRecord *slots, uint32_t slot_count);


/* ---------- Capabilities and Model Identities ---------- */

#define CAPABILITY_OPERATION_POSITION       UINT32_C(0)
#define CAPABILITY_OPERATION_LONGITUDE_RATE UINT32_C(1)
#define CAPABILITY_OPERATION_EVENTS         UINT32_C(2)
#define CAPABILITY_OPERATION_HOUSES         UINT32_C(3)
#define CAPABILITY_OPERATION_MODEL_IDENTITY UINT32_C(4) /* Text lookup only; see celestial_capability_text. */

#define CAPABILITY_STATUS_SOURCE_WINDOW         UINT32_C(0)
#define CAPABILITY_STATUS_IMPLEMENTATION_WINDOW UINT32_C(1)
#define CAPABILITY_STATUS_UNBOUNDED_FINITE      UINT32_C(2)

#define CAPABILITY_SOURCE_SUN_VSOP87D               UINT32_C(0)
#define CAPABILITY_SOURCE_MOON_ELP2000_82B_TRUNC    UINT32_C(1)
#define CAPABILITY_SOURCE_PLANET_VSOP87D            UINT32_C(2)
#define CAPABILITY_SOURCE_PLUTO_PLUTO95_APPARENT    UINT32_C(3)
#define CAPABILITY_SOURCE_NODE_MEAN                 UINT32_C(4)
#define CAPABILITY_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM UINT32_C(5)

#define CAPABILITY_TARGET_ALL UINT32_C(0xFFFFFFFF)

#define MODEL_KIND_DELTA_T         UINT32_C(0)
#define MODEL_KIND_NUTATION        UINT32_C(1)
#define MODEL_KIND_POSITION_SOURCE UINT32_C(2)
#define MODEL_KIND_ALL             UINT32_C(0xFFFFFFFF)

#define MODEL_STATUS_CURRENT UINT32_C(0)
#define MODEL_STATUS_FROZEN  UINT32_C(1)

/**
 * @brief One operation's accepted-input description for one subject.
 * @note domain ends are NaN where no explicit JDE(TT) bound exists (Status
 *       CAPABILITY_STATUS_UNBOUNDED_FINITE); NaN is "unbounded", not a sentinel for
 *       a hidden value. The record never implies accuracy; measured validation coverage
 *       lives only in the prose of celestial_capability_text.
 */
typedef struct CapabilityRecordV1 {
  uint32_t operation_code;        /* A CAPABILITY_OPERATION_* code, MODEL_IDENTITY excluded. */
  uint32_t subject_code;          /* A CHART_TARGET_* code for target operations; unused (0) for HOUSES. */
  uint32_t status_code;           /* A CAPABILITY_STATUS_* code naming what kind of bound the domain is. */
  uint32_t source_code;           /* A CAPABILITY_SOURCE_* code; unused (0) for HOUSES. */
  double domain_start_jde_tt;     /* Inclusive; NaN = no lower bound. */
  double domain_end_jde_tt;       /* Exclusive; NaN = no upper bound. */
  uint32_t maximum_span_tt_days;  /* EVENTS: 3660; 0 = no span limit. */
  uint32_t flags;                 /* Reserved; written 0, ignored on read. */
} CapabilityRecordV1;

/** @brief One registered model identity with its applicability (#357). */
typedef struct ModelIdentityRecordV1 {
  uint32_t kind_code;           /* A MODEL_KIND_* code. */
  uint32_t identity_code;       /* Code within the kind; stable across releases. */
  uint32_t status_code;         /* MODEL_STATUS_CURRENT or MODEL_STATUS_FROZEN. */
  uint32_t default_alias_code;  /* Boundary code this identity answers to as the default; UINT32_MAX when none. */
  double domain_start_jde_tt;   /* Published applicability in JDE(TT); NaN = unbounded. */
  double domain_end_jde_tt;
} ModelIdentityRecordV1;

/**
 * @brief Describe what an operation accepts and why.
 * @param operation_code A CAPABILITY_OPERATION_* code (POSITION, LONGITUDE_RATE, EVENTS or HOUSES).
 * @param subject_code A CHART_TARGET_* code, or CAPABILITY_TARGET_ALL for every subject.
 * @param record_count Where the total count of records is written. Must not be null.
 *                     On every failure `*record_count` is set to 0; a zero total is also a
 *                     legal outcome for an operation with no subjects, disambiguated by `last_error`.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written, at most min(total, slot_count); records follow
 *          CHART_TARGET_* code order for target operations.
 * @note Every bound is derived from the operation's own defining constants, so this query
 *       and the computation it describes accept and reject the same inputs by construction.
 *       The reported domain is an acceptance boundary, not an accuracy promise.
 */
CELESTIAL_API uint32_t celestial_capabilities(uint32_t operation_code, uint32_t subject_code, uint32_t *record_count, CapabilityRecordV1 *slots, uint32_t slot_count);

/**
 * @brief Enumerate the registered model identities (Delta T, nutation, position sources).
 * @param kind_code A MODEL_KIND_* code, or MODEL_KIND_ALL for every kind.
 * @param record_count Where the total count of records is written. Must not be null.
 *                     On every failure `*record_count` is set to 0.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written, at most min(total, slot_count); records follow the
 *          registry order (Delta T algo1-5, nutation, position sources).
 * @note identity_code values are stable across releases; default_alias_code resolves the
 *       concrete meaning of a DEFAULT selection without recording an unresolved code 0.
 */
CELESTIAL_API uint32_t celestial_model_identities(uint32_t kind_code, uint32_t *record_count, ModelIdentityRecordV1 *slots, uint32_t slot_count);

/**
 * @brief Human-readable description behind one capability record or model identity.
 * @param operation_code A CAPABILITY_OPERATION_* code; CAPABILITY_OPERATION_MODEL_IDENTITY
 *                       addresses the identity registry (subject_code is the ordinal in
 *                       celestial_model_identities).
 * @param subject_code A CHART_TARGET_* code (a domain operation) or an identity ordinal
 *                     (CAPABILITY_OPERATION_MODEL_IDENTITY).
 * @param buffer Caller-owned text buffer; may be null to size the request first.
 * @param capacity The buffer size in bytes, including space for a null terminator.
 * @returns The required byte count excluding the null terminator. When `buffer` is not null
 *          and `capacity` is positive, the text is written truncated and null-terminated.
 * @note The text names origin, frame, units, corrections, source/version, status and role,
 *       and labels measured validation coverage as measured; it implies no unmeasured accuracy.
 */
CELESTIAL_API uint32_t celestial_capability_text(uint32_t operation_code, uint32_t subject_code, char *buffer, uint32_t capacity);


#ifdef __cplusplus
}
#endif

/* NOLINTEND(modernize-use-trailing-return-type, modernize-use-using) */

#endif /* CELESTIAL_CALENDAR_H */
