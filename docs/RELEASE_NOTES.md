<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when a
     v*.*.* tag is pushed. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

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
