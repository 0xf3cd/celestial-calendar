<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v0.8.0]

### Charts and Ephemerides

- A chart snapshot of one instant is available in every language: C++ `astro::chart::calculate`, the C ABI's
  `chart_snapshot_v1`, Python `chart_snapshot()` and JavaScript `chart.snapshot()`. From a civil time declared as
  UTC or UT1, a location, a house system and an optional ΔT model, it returns UT1/TT times, positions and
  instantaneous longitude rates for the Sun, Moon, Mercury through Pluto and four lunar nodes, plus the four axes
  and twelve house cusps. Distances are in AU, the Moon's included; the nodes have none. Inputs outside the
  documented domains fail rather than return a partial result or another house system.
- C++ adds the parts behind the snapshot: apparent positions for Mercury through Neptune from the complete VSOP87D
  series, Pluto from the Meeus Chapter 37 model, mean and true lunar nodes, Equal, Whole Sign and Placidus houses,
  longitude rates, planetary illumination, and yearly searches for conjunctions, oppositions, quadratures,
  greatest elongations and stations. C++ also selects among the five ΔT models with an enum; the default is
  Algo5.

### Breaking Changes

- C++ headers include what they use. `jieqi.hpp` no longer brings in `astro.hpp`, `datetime.hpp` no longer brings
  in `<functional>` or `<cstddef>`, and `src/util/util.hpp` is removed; include the headers your code uses.
  `std::hash<calendar::Datetime>` is removed as well. The C ABI keeps every 0.7.0 export and record layout.

### Fixes

- C and C++ calls that take an integer year check it before narrowing it. An unrepresentable year used to alias to
  another year and return that year's result; it now fails, and the message names the original year. Python and
  JavaScript already rejected such years.
- The default C++ ΔT evaluation of a datetime no longer fails at year 32767.

### Documentation

- The root README and both package guides are now English-first. Chinese translations are in the repository's
  `README_ZH.md` files; the package pages on PyPI and npm show the English guide.
- Building, validation and design material lives in `docs/BUILDING.md`, `docs/VALIDATION.md` and the new
  `docs/DESIGN.md`, which explains the source layers, time scales and failure handling.
- The CHANGELOG gains an errata section for counts and wording that changed after earlier releases.
