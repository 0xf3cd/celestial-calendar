# Changelog

## [v0.7.0]

### Changed

- Project-authored material is now licensed under MIT. Retained third-party material keeps its source terms and remains
  outside the project MIT grant; `THIRD_PARTY_NOTICES.txt` and the source-tree attribution boundaries identify the
  applicable exceptions.

## [v0.6.1] - 2026-08-24

### Fixed

- Python wheels now include the PEP 561 `py.typed` marker, so type checkers consume the annotations already present
  in the public package. Wheel verification and a clean-installed mypy consumer both pin the contract.

## [v0.6.0] - 2026-08-17

### Added

#### JavaScript / TypeScript

- `@0xf3cd/celestial` (#182): one ESM package now carries the complete 29-export `celestial.h` surface behind `config`, `time`, `sun`, `moon`, `jieqi`, and `lunar`. Import is side-effect free; one explicit `await init()` loads the package-owned WASM, and calculations stay synchronous afterwards. The hand-written declarations use string unions rather than runtime enum objects, keep JD/JDE/UT1 and units visible, and accept neither `Date` nor implicit coercions.
- JavaScript validates shape, finiteness, integer width, and each operation's domain before entering WASM. Bad input throws `TypeError` / `RangeError`; native failures become `CelestialError`, and only the seven recording C exports may contribute a native message. Legitimate absence remains `null` or `[]`.

#### Python

- `celestial-calendar` native wheels (#211) expose the complete 29-export C ABI as a flat `celestial_calendar` API built from enums, frozen dataclasses, scalars, tuples, and `CelestialError`; ctypes structs, count/fill protocols, `last_error`, and model-specific C symbols remain private. Inputs are checked before native calls, including a 4096-result cap on `new_moons_after`.
- One `py3` wheel is built and clean-installed on floor/current Python for manylinux_2_28 x86_64/aarch64, macOS 14 arm64, and Windows AMD64. The ABI gate reconciles 29 signatures, 16 layouts, and seven recording exports; every platform also replays the same 389-point native golden dataset as the WASM leg. Release collection requires exact duplicate-free inventories and publishes the same four wheel bytes to PyPI and GitHub Release, with SHA-256 sidecars on GitHub; no sdist is built.

#### Packaging and Verification

- The WASM recipe exports all 29 C entries plus internal allocation helpers and allows memory growth for the package singleton's bounded, never-evicted calendar caches. A test-only ABI oracle reconciles all 29 signatures, 16 struct layouts, and seven recording functions with the header, implementation, binding table, and built module.
- `toolbox/build_npm.py` produces one eight-file tarball whose version comes from `project.py`, with a 250,000-byte package cap and a 465,000-byte raw-WASM cap. The independent WASM leg checks 22 public methods, 31 edge/error cases, the shared 389-point native golden dataset, exact-tarball installation under Node 22 and the current Node, TypeScript declarations, and an Astro/Vite production build in Chrome. The `celestial-wasm` release artifact keeps the raw module and adds the exact tarball plus pack JSON/SHA-256 sidecars.
- The emsdk checkout is pinned to a commit included in the cache key (#220). Before publishing, the release collector verifies the WASM archive's exact members, npm package identity, and tarball sidecar; it separately verifies each native archive's members, build version, and runtime-library hashes. `release_downloader.py` repeats the archive-internal checks after download without comparing against the caller's checkout documentation.
- The JavaScript package's acceptance suite now pins the inclusive endpoints of the civil, calendar, Jieqi, longitude, ΔT, and lunar-algorithm domains (#223). The ABI oracle proves its own export, signature, layout, and recording checks with directed mutations, while an inventory keeps every ABI, Node, browser, and TypeScript test entry connected to the workflow, runner, or configuration that executes it.
- Registry publication freezes one candidate before any irreversible workflow step. GitHub Release publishes first; a protected OIDC job sends its exact four wheels to PyPI, while npm either publishes the exact tarball through OIDC or proves the v0.6.0 bootstrap byte-identical. An unprivileged verifier then checks registry inventories, SHA-256, npm SHA-512 integrity, downloaded bytes, and clean installs. Nothing is rebuilt in the release workflow.

### Changed

- Jieqi moment APIs now share the effective Gregorian-year domain `[401, 32766]` across C++, C, Python, and JavaScript (#219). `jieqi_ut1_moment` rejects earlier years with `std::out_of_range` before calculating or caching a JDE; the lower-level `jieqi_jde` and its cache retain their `[1, 32766]` domain.

## [v0.5.0] - 2026-08-15

### Added

#### Astronomical Calculation

- Precession (#57): `astro::earth::precession` carries both routes of Meeus ch.21 — the equatorial angles ζ/z/θ (21.2) with the rigorous transform (21.7), and the ecliptic η/Π/p with (λ₀, β₀) — and `EclipticCoord{λ, β}` joins the coordinate family. Verified three ways at 1e-6°: book Examples 21.a and 21.b, pymeeus cross-datasets (30 points per system, seed 42), and 12 points against `erfa.pmat76`, whose IAU 1976 matrix route is an independent implementation of the same transform. Both cross-checks measured ~1e-14°, so the tolerance is set by the book's printed digits, not by the agreement.
- Atmospheric refraction (#61): `astro::earth::refraction` implements Bennett (Meeus 16.3, from apparent altitude) and Saemundsson (16.4, from true altitude) under one T/P correction, `R(T,P) = R · (P/1010) · (283/(273 + T_c))`, with `at_horizon` for the horizon case. `rise_set::sun::h0_from(params)` returns −(horizon refraction + 16′); the defaults — 15 °C, 1013.25 hPa, Bennett — reproduce the previous `STANDARD_ALTITUDE`, so existing signatures and results are unchanged.
- General rise/transit/set (#62): `sunrise_sunset.hpp` is generalized into `rise_set.hpp`, whose engine takes a `BodyProvider` (jde_tt → equatorial coordinates) plus h₀ and knows nothing about which body it is solving for; solar and lunar conventions live in `rise_set::sun` and `rise_set::moon`. Transit rides ch.15's m₀ secant form with dα/dt measured from the provider rather than a per-body rate constant, and existence is settled by scanning the day on a 96-cell (15-minute) grid, probing the edge cells, and golden-section-refining each candidate extremum — rather than assuming the extremum sits at H = ±180°, an assumption with a blind band whenever dδ/dt ≠ 0. Polar topology moves into the type (`Polar{NONE, DAY, NIGHT}`) and `transit_jde` becomes optional, since a lunar UT day can have none. The Moon uses UT-day windows to match almanac practice and h₀ = 0.7275·Π − refraction, held to 181 USNO rows at ±2 min — five sites over 1999–2049, plus Tromsø's polar and double-event days and the routine dates on which the Moon simply does not rise.
- Moon phase (#33, #185): `phase_moments` solves the four principal phases (Sun–Moon elongation 0°, 90°, 180°, 270°), `position_angle` gives the bright limb's position angle (48.5), and `moon_illumination` returns the illuminated fraction with the elongation it came from (48.1–48.3). Meeus Example 48.a prints χ = 285°.0; this library's VSOP87D + truncated ELP2000-82B route gives 285.0442°, pymeeus 285.04435°.

#### C ABI & Downstream

- Lunar converter exports (#128, #129, #130): the year-level C entry points accept `algo3` (1600–2199) alongside 1 and 2, and `TraditionalMonth` with the failable `month_position` / `month_at_position` pair makes the two month numberings convertible in both directions — the helper rejects the mismatches (a `leap` flag on a non-leap month, a leap month in a year that has none) that a silent mapping would have swallowed. The converter's own doc blocks now say plainly that its "month" is a position index in which a leap month occupies its own slot.
- Two new exports (#185): `moon_illumination(jde)` for the illuminated fraction and elongation, and `local_apparent_sidereal_time(jd_ut1, longitude)` — the first C outlet for the `astro::sidereal` layer, which was already complete in C++.
- WASM artifact (#163, #185, #187): `toolbox/build_wasm.py` builds a browser/Node module with `em++ -Oz -fwasm-exceptions` from one recipe shared verbatim by the manual and CI paths. The shipped module exports the jieqi family, the Julian Day conversions, `moon_illumination`, `moon_position_angle`, `moon_phase_moments`, `local_apparent_sidereal_time` and `last_error`. `toolbox/bindings_golden.json` (schema `@2`) holds it to the native build across five sections totalling 389 entries — jieqi 204, moon 41, sidereal 43, position angle 41, phases 60 — and `wasm_check.mjs` checks value agreement, the sret struct layout, the exception path and module size, on a CI leg of its own with emsdk pinned to 6.0.6. Releases ship the module alongside the platform packages.
- Jieqi table JSON export (#164): `toolbox/jieqi_table.py` turns `query_jieqi_moment` into one static table (default 1950–2051, the last year being tail margin so every 1950–2050 moment keeps its successor in-table). Entries are `{year, idx, name_zh, unix_ms, iso_utc}`, sorted by moment rather than by ABI index; the timescale is stated as modelled UT1 with the per-era distance from UTC written into the table's own metadata. Held by a `./linter.py --jieqi-table` gate: count, strict ordering, year containment, the idx ↔ name mapping against an independent transcription, determinism, re-derivation through the second ctypes binding, and HKO almanac anchors.

### Changed

- `algo2`'s supported window narrows from [410, 5000] to [410, 2500] (#139). The old upper bound was a convention rather than a physical limit, and honouring it meant returning an answer that is essentially random at the far end. The new bound follows an error budget from three sources: past 1972 the ΔAT table is frozen at 37 s and future leap seconds are physically unknowable, so the systematic offset against real future time is ΔT − 69.184 s, with the extrapolation uncertainty taken from NASA's ΔT catalogue (612 s at 2500 against 12341 s at 5000); before 1972 the Morrison–Stephenson parabola governs; across the whole window the VSOP/ELP truncation and mean-element extrapolation add a second-order term.
- `Angle` and `Distance` (#48, #53): the value member becomes private and drops its `const`. A `const` member expresses non-assignability, not immutability — immutability was already carried by the interface, since every operator is `const` and returns a new value and there are no setters — while costing regularity: `std::optional<Angle<DEG>>` was already instantiated and only escaped breaking because no consumer had written an assignment. The member did not become public in exchange: a bare value outside its `Unit` cannot be read correctly, and `as<>()` / `deg()` / `rad()` state the unit at every read. Angle spellings are unified across the library in the same pass.
- Export surface closed to the published C ABI (#91): explicit `CELESTIAL_API` on every `celestial.h` entry point, hidden default visibility, `SOVERSION` = major.minor while 0.x, and release packaging via `cmake --install` — with a CI gate holding the exported symbol set to the header's entry points. Release archives change shape: the library now sits under `lib/` (`bin/` on Windows, with the import library in `lib/`) and the header under `include/`; the JSON build metadata stays at the top level.
- Release downloads now fail loud (#72, #211): artifacts must come from three explicitly selected, successful dispatches of the exact protected tag commit; duplicate, missing, extra, colliding, or failed downloads stop the release. The release workflow validates the selected run IDs and main ancestry before freezing its candidate.
- Logging is opt-in: the default verbosity is `NONE`, so loading the library no longer writes to the host's stdout unless asked.
- Toolchains: the Linux legs move from clang 18 to clang 22 (ubuntu-26.04) and Windows from choco LLVM 20.1.8 to 22.1.7, with clang-tidy pinned by the runner image it ships with.

### Fixed

- Jieqi year guard (#154): `calc_jieqi_jde` had no year check, and `to_ymd`'s `std::chrono::year` is a `short`. Three shapes were measured before the fix — `calc_jieqi_jde(65537, 春分)` did not throw and returned year 1's JDE, a silently wrong answer; `jieqi_jde(65537, …)` and `jieqi_jde(1, …)` returned the same value, because the cache keyed on the untruncated year while storing the truncated answer, poisoning the entry and leaving the key space unbounded; and `calc_jieqi_jde(100000, …)` wrapped around into the lower-bound guard and reported "The year -31072 is < 1". The input is now rejected as `out_of_range` before any of that.
- The last UT1 residue at year boundaries (#115): #84 moved `moments()`' year boundaries onto the leap-second-aware path, but the same pattern remained in `sun::find_roots`' year window and in `JieqiGenerator`'s start-year inference. Both now resolve through UTC, and the UT1/UTC model gap is documented at the points that consume it. No numerical consequence — the gap stays orders of magnitude below the ~4-day jieqi/New-Year clearance.
- `normalize_deg` / `normalize_rad` keep their documented half-open range (#88). One comparison, `_deg < 0.0`, was asked to hold three things at once and held none: a small negative input could return exactly 360.0, because `rem + 360.0` rounds up once `rem` falls within `ulp(360)/2`, and a whole-period negative input returned −0.0, because `-0.0 < 0.0` is false and the sign bit rode through untouched.
- `algo2` now enforces the window it advertises (#140). It was the only one of the three lunar algorithms without a year check — `algo1` and `algo3` both throw `out_of_range` — while `get_supported_lunar_year_range` advertised its bounds through the C ABI. Measured on the release build before the fix: `get_lunar_year_info(2, 409)` and `get_lunar_year_info(2, 5001)` both returned `valid=true`.
- Header self-containment and the ODR ground beneath it (#71): namespace-scope `constexpr` has internal linkage, so every translation unit was getting its own copy of the coefficient tables; they now have external linkage. Includes are audited per header and the self-containment gate runs on more than one platform, since a header leaning on a transitive include passes on one standard library and fails on another.
- `hash_combine`'s finalizer (#149): the old `v_hash *= 0x9e3779b9` only propagated low bits upward and was 32 bits wide — flipping input bit 63 flipped 7.7 output bits on average against an ideal of 32. It is now a splitmix64-shaped step, an xorshift bracketing a 64-bit golden-ratio multiply, which measures 27.8 at minimum; changing the constant alone was not enough (21.6), so the constant and the xorshift moved together.

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
