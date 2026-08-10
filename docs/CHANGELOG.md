# Changelog

## [Unreleased]

### Added

- Jieqi table JSON export (#164): `toolbox/jieqi_table.py` turns `query_jieqi_moment` into one static table (default 1950–2051, the last year being tail margin so every 1950–2050 moment keeps its successor in-table). Entries are `{year, idx, name_zh, unix_ms, iso_utc}`, sorted by moment rather than by ABI index; the timescale is stated as modelled UT1 with the per-era distance from UTC written into the table's own metadata. Held by a `./linter.py --jieqi-table` gate: count, strict ordering, year containment, the idx ↔ name mapping against an independent transcription, determinism, re-derivation through the second ctypes binding, and HKO almanac anchors.

### Changed

- Export surface closed to the published C ABI (#91): explicit `CELESTIAL_API` on every `celestial.h` entry point, hidden default visibility, `SOVERSION` = major.minor while 0.x, and release packaging via `cmake --install` — with a CI gate holding the exported symbol set to the header's entry points. Release archives change shape: the library now sits under `lib/` (`bin/` on Windows, with the import library in `lib/`) and the header under `include/`; the JSON build metadata stays at the top level.
- Release downloads now fail loud (#72): artifacts must come from a successful `build_and_test` run of the exact tagged commit, and a single failed artifact download fails the whole download. Cutting a release is now three deliberate steps — push the tag, dispatch `build_and_test` on it, rerun the release workflow.
- Logging is opt-in: the default verbosity is `NONE`, so loading the library no longer writes to the host's stdout unless asked.

## [v0.4.0] - 2026-07-29

### Added

#### Astronomical Calculation

- Leap-second-aware UTC time scale: the ΔAT table (28 entries, machine-checked against pyerfa), `utc_to_tt` / `tt_to_utc` conversions, a UTC Julian Day family, and carry-aware `add_seconds`; year/month boundary enumeration switched from its UT1 proxy to true UTC — a 1972–5000 scan surfaced 12 year-attribution flips (all in 3312+), pinned by directed goldens.
- Equation of time & local apparent solar time (Meeus ch. 28): `equation_of_time` on the apparent-right-ascension route (28.1) with the (28.2) mean-longitude polynomial, and `apparent(utc, longitude)` riding the new UTC machinery. Verified against five independent families — book example 28.a, pymeeus internals (worst 0.0007 s), a Skyfield/DE440s definitional oracle (worst 0.22 s over 1900–2100), NOAA solcalc formulas, and the in-repo transit route (worst 0.17 s) — plus exact-rational τ = ±1 anchors pinning the high-order polynomial terms.

#### Calibration

- Moon golden regression against JPL Horizons/DE441: 41 epochs × 3 columns with banded tolerances. The long-standing lunar-distance doubt is closed: worst residual 45.689 km ≡ the ELP2000-82B truncation envelope (30–46 km, flat across ten millennia) — zero transcription bias.
- Sun & solar-term golden regression: 43 Horizons epochs (core band 0.105″ / 0.018″ / 1.4e-8 AU), and dual-axis jieqi verification — 76 DE441 instants in pure TT (worst 2.76 s) × 168 HKO almanac values through the full chain including ΔT (worst 0.525 min ≈ table rounding). The solar-term surface consumed by downstream calendrics is now verified to the second.

#### C ABI

- Published C header `celestial.h`: every export with its documented error contract, struct-layout `_Static_assert`s, and a pure-C compile check in CI; release artifacts now ship the header. New `equation_of_time` / `apparent_solar_time` exports, and a thread-local `last_error` pilot on the Julian Day exports.

### Fixed

- C-ABI boundary hardening: exceptions can no longer cross `extern "C"` (catch-all handlers → `valid = false`); inputs are validated before narrowing casts with NaN-proof range guards, replacing `assert`s that are dead code in Release builds; `GLOBAL_VERBOSITY` is atomic; the lunar-algorithm bounds cache initializes lazily instead of running a full astronomical pipeline during library load.
- `jd_to_ut1` upper-bound guard, closing a JD → `uint32` overflow path.

### Changed

- Retired legacy comparison datasets lacking provenance or mixing time scales (TAMU / timeanddate diff tests, UT1-labelled jieqi rows) in favor of the external-authority goldens above.
- Windows note in `celestial.h`: the UCRT fail-fasts (`0xc0000409`) on writes to a closed stdout — a process kill, not a catchable exception.

## [v0.3.0] - 2026-07-27

### Added

#### Astronomical Calculation

- Sunrise & sunset line (Meeus, "Astronomical Algorithms" 2nd ed.): mean/true obliquity (ch. 22), ecliptic ↔ equatorial → horizontal coordinate transforms (ch. 13), GMST/GAST/LAST sidereal time with strict UT1/TT separation (ch. 12), apparent solar equatorial coordinates (ch. 25), and `sunrise_sunset.hpp` — sunrise/sunset/upper transit, civil/nautical/astronomical twilight, and polar day/night detection (ch. 15).
- End-to-end golden regression against external authorities (USNO rstt API × NOAA solcalc × Skyfield/JPL DE421): 143 pinned values across 7 sites × 4 seasonal dates including Tromsø polar cases; measured worst residual 0.55 min against the ±2 min contract.
- ΔT algo5, now the default: a single polynomial fitted on IERS Bulletin A observations (2005–2026.4) anchored to the Stephenson–Morrison–Hohenkerk long-term curve for extrapolation — removes the 2035 prediction cliff of the previous default.

### Fixed

- Newton root finder returns the best-residual iterate instead of the last one, fixing silently wrong roots beyond JD 2²² (year 6771+: 2357 wrong sun-longitude roots in 6772–9999 → 0) and a silent truncation of moon-phase enumeration; bracket construction refined (margin 0.67° → 6.1°).
- ELP2000-82B coefficient transcription errors; aberration switched to Meeus (25.11) variable form.
- `julian_day` input validation unified to a throw contract, with the 401 CE lower bound corrected.
- `util` function cache rewritten thread-safe (TSAN data races 7 → 0); copies share the cache.

### Changed

- CI resurrected after two years of bit rot: build matrix slimmed 8 → 2 with native ARM runners; clang-tidy gate aligned; automated AI code review on pull requests.
- Golden datasets now carry mandatory provenance (source, collection date, tolerance rationale) and regression tests are mutation-verified for detection power.

## [v0.2.0] - 2024-08-17

### Added

#### Calendar

- Supported more algorithms for lunar date conversion.
- Added more functions for Jieqi calculation.
- Used adaptive step size in Newton's method.

#### Utilities

- Supported hash and cache for arbitrary function, which is currently used in astro-calculation, jieqi calculation, and lunar date conversions.

#### Statistics

- Added a new notebook in folder `statistics`:
  - `lunar_calendar.ipynb` for inspecting the accurate lunar month start moments, also for inspecting the differences between algo1 and algo2.

#### Test

- Implemented unit tests with GTest, covering lunar date conversions using different algorithms.

#### Linter

- Added `.ruff.toml` which configures ruff. Also change Python codes as per the linter suggestions.

## [v0.1.0] - 2024-08-05

### Added

#### Astronomical Calculation

- Added theory ELP2000-82B (truncated version, from Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 47).
- Supported calculations of the Geocentric Coordinate of the Moon, with corrections (perturbation, nutation, ...).
- Applied Newton's method to approximate the moments of Sun-Moon conjunctions (i.e. New Moons).
- Added other math utilities, e.g., `Distance`.

#### Statistics

- Added more notebooks in folder `statistics`:
  - `moon_longitude.ipynb` for exploring Newton's Method on finding New Moon moments.
  - `new_moon.ipynb` to compare calculated New Moon moments with other data sources.

#### Test

- Implemented unit tests with GTest, covering ELP2000-82B, Moon position calculation, and Sun-Moon conjunction moment prediction.

#### Automation

- Added `linter.py` to run `ruff` and `clang-tidy`.
- Created the `toolbox` folder, including:
  - `release_downloader.py` to download the assets from latest release.

## [v0.0.0] - 2024-08-01

### Added

#### Astronomical Calculation

- Supported conversions between JD (Julian Day) and UT1, and conversions between JDE (Julian Ephemeris Day) and TT.
- Supported calculations of Delta T.
- Supported conversions between UT1 and TT time scales.
- Supported calculations of the Heliocentric Coordinate of Earth.
- Supported calculations of the Geocentric Coordinate of the Sun, with corrections (FK5 system correction, nutation correction, etc.).
- Added other math utilities, e.g., `Angle`.

#### Statistics

- Added folder `statistics`, for statistical analysis to evaluate different astronomical algorithms

#### Calendar

- Added `Datetime`, a struct to hold a date and an accurate time, representing a UT1 or UTC moment.
- Supported conversions between Lunar dates and Gregorian dates.
  - At this release, only Gregorian years between 1901 and 2099 were supported — v0.2.0 brought more algorithms, and the README states the range each one covers.
- Applied Newton's method to approximate the moment when the Sun reaches a certain longitude.
- Supported queries of the Jieqi (节气) moments in given Gregorian years.

#### Test

- Implemented unit tests with GTest, covering core functionalities.

#### Automation

- Implemented the `automation` Python package to manage the project in an automated manner.
- Added `project.py` as the entry point for building and testing the project.
- Created the `toolbox` folder, including:
  - `artifact_downloader.py` to download the latest shared libraries.
  - `build_info.py` to pack the platform, architecture, and shared lib info with a build.
  - `compiler_finder.py` to find the C and C++ compilers that satisfy a certain standard.
