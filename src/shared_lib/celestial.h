/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
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
 * Error contract: every function is `noexcept` at the boundary. Struct-returning
 * functions signal failure with `valid = false`; the rest return `0` / `false`.
 * On failure every function except `last_error` also records a thread-local
 * message readable through it.
 *
 * Thread-safety contract: every entry point may be called concurrently from any
 * number of threads, with no host-side synchronization. `set_log_verbosity` turns
 * a process-wide knob: writes are atomic — a concurrent reader always sees one
 * whole level or another — but which of two racing writes wins is unspecified.
 * `last_error` is per-thread: it reports the calling thread's most recent call to a
 * exported function except `last_error`, and no thread ever
 * observes another's message. Two computations memoize
 * per argument — the jieqi moments and the algo-2 lunar year info. Both caches are
 * shared process-wide and never erased: the first call with a given argument pays for
 * it, later calls from any thread reuse the result, and memory grows monotonically
 * with the number of distinct arguments ever queried. Two threads that miss on the
 * same argument at once both compute it, and one of the two results is discarded.
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
 * @param y The year.
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `JulianDay` struct. JD is based on UT1.
 */
CELESTIAL_API JulianDay ut1_to_jd(int32_t y, uint32_t m, uint32_t d, double fraction);

/**
 * @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE).
 * @param y The year.
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
 * @note This is a recording function: on failure the reason is readable through `last_error()`.
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
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @returns A `Discriminant` struct; `count` is 0, 1, or 2: the Sun won't reach the longitude
 *          in the year, reaches it once, or reaches it twice.
 */
CELESTIAL_API Discriminant solar_lon_root_discriminant(int32_t year, double longitude);

/**
 * @brief Find the JDE(s) at which the Sun reaches `longitude` in `year`, written to `slots`.
 * @param year The year.
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
 * @param y The year (UTC).
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
  uint8_t  jq_idx; /* The index of the Jieqi, in the range [0, 24). */
  int32_t  y;      /* The year. */
  uint32_t m;      /* The month. */
  uint32_t d;      /* The day. */
  double   frac;   /* The fraction of the day, in the range [0.0, 1.0). */
} JieqiMomentQuery;

/**
 * @brief Query the accurate UT1 moment of the Jieqi in the given `year`.
 * @param year The Gregorian year, in [401, 32766].
 * @param jq_idx The index of the Jieqi. Expected to be in the range [0, 24).
 * @returns A `JieqiMomentQuery` struct.
 * @note The returned civil moment is UT1; while leap seconds are applied it matches UTC
 *       to within DUT1 (±0.9 s), so modern-era civil consumers may treat it as UTC.
 *       Past the ΔAT table freeze the modelled gap follows ΔT−(ΔAT+32.184) (#115).
 */
CELESTIAL_API JieqiMomentQuery query_jieqi_moment(int32_t year, uint8_t jq_idx);

/**
 * @brief Write the Chinese name of the Jieqi to `buf`.
 * @param jq_idx The index of the Jieqi. Expected to be in the range [0, 24).
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
 * @param algo The algorithm. Expected to be 1, 2, or 3.
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
 * @param algo The algorithm. Expected to be 1, 2, or 3.
 * @param year The Gregorian year.
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
 */
CELESTIAL_API LunarDate gregorian_to_lunar(uint8_t algo, int32_t year, uint8_t month, uint8_t day);

/**
 * @brief Convert a lunar date to a Gregorian date.
 * @param algo The algorithm. Expected to be 1, 2, or 3.
 * @param year The lunar year.
 * @param month The lunar month, in traditional numbering (1-12) — see `gregorian_to_lunar`.
 * @param is_leap Whether the month is the leap month. `true` is accepted only on the year's
 *                actual leap month — `month = 2` in lunar 2023, and no month at all in a year
 *                that has none.
 * @param day The day of the lunar month.
 * @returns A `GregorianDate` struct; `valid = false` when the input does not name a real
 *          lunar date (bad `algo`, year out of range, no such leap month, day out of range).
 */
CELESTIAL_API GregorianDate lunar_to_gregorian(uint8_t algo, int32_t year, uint8_t month, bool is_leap, uint8_t day);


/* ---------- Delta T ---------- */

typedef struct DeltaT {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The value of delta T. */
} DeltaT;

/**
 * @brief Compute delta T of a given moment using algorithm 1.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
CELESTIAL_API DeltaT delta_t_algo1(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 2.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
CELESTIAL_API DeltaT delta_t_algo2(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 3.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
CELESTIAL_API DeltaT delta_t_algo3(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 4.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
CELESTIAL_API DeltaT delta_t_algo4(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 5.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
CELESTIAL_API DeltaT delta_t_algo5(double year);
/**
 * @brief Compute delta T of a given moment, using the best algorithm.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
CELESTIAL_API DeltaT delta_t(double year);


#ifdef __cplusplus
}
#endif

/* NOLINTEND(modernize-use-trailing-return-type, modernize-use-using) */

#endif /* CELESTIAL_CALENDAR_H */
