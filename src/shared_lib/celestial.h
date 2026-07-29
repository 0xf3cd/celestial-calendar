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
 * version. And no dllexport/dllimport macros: `WINDOWS_EXPORT_ALL_SYMBOLS` exports
 * everything, and imports resolve implicitly against the import library.
 *
 * Error contract: every function is `noexcept` at the boundary. Struct-returning
 * functions signal failure with `valid = false`; the rest return `0` / `false`.
 * On failure the Julian Day functions also record a thread-local message readable
 * through `last_error` (#97 pilot).
 */

#include <stdbool.h>
#include <stdint.h>

/* NOLINTBEGIN(modernize-use-trailing-return-type, modernize-use-using):
 * this header must stay valid C — C has neither trailing return types nor `using`. */

#ifdef __cplusplus
extern "C" {
#endif


/* ---------- Global configuration ---------- */

/**
 * @brief Set the verbosity level of log printing.
 * @param new_value The new verbosity level (in `uint8_t`): 0 = none, 1 = info, 2 = debug.
 *                  The initial level is 2 (debug).
 * @returns `true` if the level was stored, `false` if `new_value` is out of range.
 */
bool set_log_verbosity(uint8_t new_value);

/**
 * @brief Get the last-error message of the calling thread.
 * @returns A pointer to a thread-local C string, empty if there is no recorded error.
 *          Only the Julian Day functions (`ut1_to_jd`, `ut1_to_jde`, `jde_to_ut1`) write
 *          and clear the message; other functions neither set nor clear it, so the pointer
 *          always refers to the most recent Julian Day call on this thread. It stays valid
 *          until the next Julian Day call on the same thread.
 * @note #97 pilot: only the Julian Day functions record messages for now.
 */
const char *last_error(void);


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
JulianDay ut1_to_jd(int32_t y, uint32_t m, uint32_t d, double fraction);

/**
 * @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE).
 * @param y The year.
 * @param m The month.
 * @param d The day.
 * @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
 * @returns A `JulianDay` struct. JDE is based on TT.
 */
JulianDay ut1_to_jde(int32_t y, uint32_t m, uint32_t d, double fraction);

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
UT1Time jde_to_ut1(double jde);


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
SunCoordinate sun_apparent_geocentric_coord(double jde);

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
MoonCoordinate moon_apparent_geocentric_coord(double jde);


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
Discriminant solar_lon_root_discriminant(int32_t year, double longitude);

/**
 * @brief Find the JDE(s) at which the Sun reaches `longitude` in `year`, written to `slots`.
 * @param year The year.
 * @param longitude The geocentric longitude.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
uint32_t solar_lon_roots(int32_t year, double longitude, double *slots, uint32_t slot_count);


/* ---------- Sun Moon Conjunction ---------- */

/**
 * @brief Find the next `slot_count` new-moon JDE(s) after `jde`, written to `slots`.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
uint32_t new_moons_after_jde(double jde, double *slots, uint32_t slot_count);

/**
 * @brief Find the new-moon JDE(s) in `year`; the total count goes to `root_count`,
 *        up to `slot_count` of them are written to `slots`.
 * @param year The Gregorian year.
 * @param root_count Where the total count of the roots is written. Must not be null.
 * @param slots The output slots, allocated and freed by the caller; may be null only when
 *              `slot_count` is 0.
 * @param slot_count The count of slots.
 * @returns How many slots are written.
 */
uint32_t new_moons_in_year(int32_t year, uint32_t *root_count, double *slots, uint32_t slot_count);


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
EquationOfTime equation_of_time(double jde);

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
ApparentSolarTime apparent_solar_time(int32_t y, uint32_t m, uint32_t d, double fraction, double longitude);


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
 * @param year The year, in gregorian calendar.
 * @param jq_idx The index of the Jieqi. Expected to be in the range [0, 24).
 * @returns A `JieqiMomentQuery` struct.
 */
JieqiMomentQuery query_jieqi_moment(int32_t year, uint8_t jq_idx);

/**
 * @brief Write the Chinese name of the Jieqi to `buf`.
 * @param jq_idx The index of the Jieqi. Expected to be in the range [0, 24).
 * @param buf The name memory, allocated and freed by the caller.
 * @param buf_size Maximum bytes that can be written to `buf`.
 * @returns `true` if the name is successfully written to `buf`.
 */
bool get_jieqi_name(uint8_t jq_idx, char *buf, uint32_t buf_size);


/* ---------- Lunar Calendar ---------- */

typedef struct SupportedLunarYearRange {
  bool    valid; /* Indicates if the result is valid. */
  int32_t start; /* The first supported lunar year. */
  int32_t end;   /* The last supported lunar year. */
} SupportedLunarYearRange;

/**
 * @brief Get the supported lunar year range of the algorithm.
 * @param algo The algorithm. Expected to be 1 or 2.
 * @returns A `SupportedLunarYearRange` struct.
 */
SupportedLunarYearRange get_supported_lunar_year_range(uint8_t algo);

typedef struct LunarYearInfo {
  bool     valid;      /* Indicates if the result is valid. */
  int32_t  year;       /* Gregorian year of the first day of the lunar year. */
  uint8_t  month;      /* Gregorian month of the first day of the lunar year. */
  uint8_t  day;        /* Gregorian day of the first day of the lunar year. */
  uint8_t  leap_month; /* The leap month (1-12), or 0 if there is none. */
  uint16_t month_len;  /* Least 12/13 bits: 1 = 30-day month, 0 = 29-day month. */
} LunarYearInfo;

/**
 * @brief Get the lunar year information for the given year.
 * @param algo The algorithm. Expected to be 1 or 2.
 * @param year The lunar year.
 * @returns A `LunarYearInfo` struct.
 */
LunarYearInfo get_lunar_year_info(uint8_t algo, int32_t year);


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
DeltaT delta_t_algo1(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 2.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
DeltaT delta_t_algo2(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 3.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
DeltaT delta_t_algo3(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 4.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
DeltaT delta_t_algo4(double year);
/**
 * @brief Compute delta T of a given moment using algorithm 5.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
DeltaT delta_t_algo5(double year);
/**
 * @brief Compute delta T of a given moment, using the best algorithm.
 * @param year The year.
 * @returns A `DeltaT` struct.
 */
DeltaT delta_t(double year);


#ifdef __cplusplus
}
#endif

/* NOLINTEND(modernize-use-trailing-return-type, modernize-use-using) */

#endif /* CELESTIAL_CALENDAR_H */
