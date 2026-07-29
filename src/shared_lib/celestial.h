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

/** @brief Set the verbosity level of log printing. `false` if `new_value` is out of range. */
bool set_log_verbosity(uint8_t new_value);

/**
 * @brief Get the last-error message of the calling thread.
 * @returns A pointer to a thread-local C string, empty if there is no recorded error.
 *          The pointer stays valid until the next C-ABI call on the same thread.
 * @note #97 pilot: only the Julian Day functions record messages for now.
 */
const char *last_error(void);


/* ---------- Julian Days ---------- */

typedef struct JulianDay {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The value. Either JD or JDE. */
} JulianDay;

/** @brief Convert UT1 datetime to Julian Day Number (JD). `fraction` must be in [0.0, 1.0). */
JulianDay ut1_to_jd(int32_t y, uint32_t m, uint32_t d, double fraction);

/** @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE, TT-based). */
JulianDay ut1_to_jde(int32_t y, uint32_t m, uint32_t d, double fraction);

typedef struct UT1Time {
  bool     valid;    /* Indicates if the result is valid. */
  int32_t  year;     /* The year. */
  uint32_t month;    /* The month. */
  uint32_t day;      /* The day. */
  double   fraction; /* The fraction of the day, in the range [0.0, 1.0). */
} UT1Time;

/** @brief Convert Julian Ephemeris Day Number (JDE, TT-based) to UT1 datetime. */
UT1Time jde_to_ut1(double jde);


/* ---------- Sun and Moon Apparent Geocentric Position ---------- */

typedef struct SunCoordinate {
  bool valid; /* Indicates if the result is valid. */
  double lon; /* The longitude. In degrees. */
  double lat; /* The latitude. In degrees. */
  double   r; /* The radius. In AU. */
} SunCoordinate;

/** @brief Calculate the apparent geocentric position of the Sun. */
SunCoordinate sun_apparent_geocentric_coord(double jde);

typedef struct MoonCoordinate {
  bool valid; /* Indicates if the result is valid. */
  double lon; /* The longitude. In degrees. */
  double lat; /* The latitude. In degrees. */
  double   r; /* The radius. In KM. */
} MoonCoordinate;

/** @brief Calculate the apparent geocentric position of the Moon. */
MoonCoordinate moon_apparent_geocentric_coord(double jde);


/* ---------- Sun Position ---------- */

typedef struct Discriminant {
  bool     valid; /* Indicates if the result is valid. */
  uint32_t count; /* The count of the roots, which is 0, 1, or 2. */
} Discriminant;

/** @brief Count how many times the Sun reaches `longitude` in `year` (0, 1, or 2). */
Discriminant solar_lon_root_discriminant(int32_t year, double longitude);

/**
 * @brief Find the JDE(s) at which the Sun reaches `longitude` in `year`, written to `slots`.
 * @returns How many slots are written. `slots` may be null only when `slot_count` is 0.
 */
uint32_t solar_lon_roots(int32_t year, double longitude, double *slots, uint32_t slot_count);


/* ---------- Sun Moon Conjunction ---------- */

/**
 * @brief Find the next `slot_count` new-moon JDE(s) after `jde`, written to `slots`.
 * @returns How many slots are written. `slots` may be null only when `slot_count` is 0.
 */
uint32_t new_moons_after_jde(double jde, double *slots, uint32_t slot_count);

/**
 * @brief Find the new-moon JDE(s) in `year`; the total count goes to `root_count`,
 *        up to `slot_count` of them are written to `slots`.
 * @returns How many slots are written. Neither `root_count` nor `slots` may be null
 *          (`slots` may be null only when `slot_count` is 0).
 */
uint32_t new_moons_in_year(int32_t year, uint32_t *root_count, double *slots, uint32_t slot_count);


/* ---------- Jieqi ---------- */

typedef struct JieqiMomentQuery {
  bool     valid;  /* Indicates if the result is valid. */
  uint8_t  jq_idx; /* The index of the Jieqi, in the range [0, 24). */
  int32_t  y;      /* The year. */
  uint32_t m;      /* The month. */
  uint32_t d;      /* The day. */
  double   frac;   /* The fraction of the day, in the range [0.0, 1.0). */
} JieqiMomentQuery;

/** @brief Query the accurate UT1 moment of Jieqi `jq_idx` in `year`. */
JieqiMomentQuery query_jieqi_moment(int32_t year, uint8_t jq_idx);

/** @brief Write the Chinese name of Jieqi `jq_idx` to `buf`. `false` on any failure. */
bool get_jieqi_name(uint8_t jq_idx, char *buf, uint32_t buf_size);


/* ---------- Lunar Calendar ---------- */

typedef struct SupportedLunarYearRange {
  bool    valid; /* Indicates if the result is valid. */
  int32_t start; /* The first supported lunar year. */
  int32_t end;   /* The last supported lunar year. */
} SupportedLunarYearRange;

/** @brief Get the supported lunar year range of algorithm `algo` (1 or 2). */
SupportedLunarYearRange get_supported_lunar_year_range(uint8_t algo);

typedef struct LunarYearInfo {
  bool     valid;      /* Indicates if the result is valid. */
  int32_t  year;       /* Gregorian year of the first day of the lunar year. */
  uint8_t  month;      /* Gregorian month of the first day of the lunar year. */
  uint8_t  day;        /* Gregorian day of the first day of the lunar year. */
  uint8_t  leap_month; /* The leap month (1-12), or 0 if there is none. */
  uint16_t month_len;  /* Least 12/13 bits: 1 = 30-day month, 0 = 29-day month. */
} LunarYearInfo;

/** @brief Get the lunar year information for `year`, using algorithm `algo` (1 or 2). */
LunarYearInfo get_lunar_year_info(uint8_t algo, int32_t year);


/* ---------- Solar Time ---------- */

typedef struct EquationOfTime {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* E in degrees of hour angle; x240 for seconds of time. */
} EquationOfTime;

/** @brief Compute the equation of time E = apparent solar time - mean solar time (Meeus ch. 28). */
EquationOfTime equation_of_time(double jde);

typedef struct SolarTime {
  bool     valid;    /* Indicates if the result is valid. */
  int32_t  year;     /* The year. */
  uint32_t month;    /* The month. */
  uint32_t day;      /* The day. */
  double   fraction; /* The fraction of the day, in the range [0.0, 1.0). */
} SolarTime;

/**
 * @brief Convert a civil UTC moment to local apparent (true) solar time.
 *        `longitude` is positive east, in [-180, 180].
 */
SolarTime apparent_solar_time(int32_t y, uint32_t m, uint32_t d, double fraction, double longitude);


/* ---------- Delta T ---------- */

typedef struct DeltaT {
  bool   valid; /* Indicates if the result is valid. */
  double value; /* The value of delta T. */
} DeltaT;

/** @brief Compute delta T of a given moment using algorithm 1. */
DeltaT delta_t_algo1(double year);
/** @brief Compute delta T of a given moment using algorithm 2. */
DeltaT delta_t_algo2(double year);
/** @brief Compute delta T of a given moment using algorithm 3. */
DeltaT delta_t_algo3(double year);
/** @brief Compute delta T of a given moment using algorithm 4. */
DeltaT delta_t_algo4(double year);
/** @brief Compute delta T of a given moment using algorithm 5. */
DeltaT delta_t_algo5(double year);
/** @brief Compute delta T of a given moment, using the best algorithm. */
DeltaT delta_t(double year);


#ifdef __cplusplus
}
#endif

/* NOLINTEND(modernize-use-trailing-return-type, modernize-use-using) */

#endif /* CELESTIAL_CALENDAR_H */
