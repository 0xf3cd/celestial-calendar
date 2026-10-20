<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v0.9.0]

### Longitude Events

- Interval longitude-event searches are available from C++ (`ephemeris_events.hpp`), the C ABI and
  the Python package. `longitude_events` finds every crossing or exact numerical contact of a
  target's apparent geocentric longitude with a supplied level; `longitude_stations` finds the
  stationary instants of the published seven-point rate with certified motion on both sides;
  `longitude_difference_events` searches the directed, canonical difference of two targets.
  All fourteen targets — the Sun, Moon, planets, Pluto and the four lunar nodes — speak the same
  codes across the C ABI, Python and the chart snapshot. Times are JDE(TT) on the published
  2^-30-day reporting lattice; rates are degrees per TT day.
- The C ABI owns results through one count+fill call: an explicit total count, caller-allocated
  slots in deterministic chronological order, defined truncation behavior, and a thread-local
  error channel that separates a legal empty result from a failed query. Python mirrors the enums,
  fields, units and failures in frozen dataclasses and validates every input before native
  execution. Angles carry no aspect names or orb policy anywhere in the API.
- The private WASM ABI mirrors the expanded C surface — export list, binding metadata, layouts,
  manifests and ABI oracles — while JavaScript intentionally exposes no event API.
- Six shared fixtures (recorded with exact bit patterns in `toolbox/bindings_golden.json`) certify
  that the C++ core, the C ABI and Python agree on counts, ordering, timestamps, directions,
  longitudes and rates; the Python wheel suite and the WASM raw-protocol suite replay them on
  every build.

### Breaking Changes

- None. Every 0.8.0 C ABI export and record layout is unchanged; the three event exports and
  their record types are additive.

### Documentation

- [Longitude events](EPHEMERIS_EVENTS.md) now covers the C ABI and Python surfaces alongside
  the C++ searches: the count+fill ownership protocol, empty-result disambiguation, and the
  per-query cost of the two-pass Python protocol.
