<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when a
     v*.*.* tag is pushed. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

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
