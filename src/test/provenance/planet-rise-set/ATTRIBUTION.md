# Planetary rise/set source records

## V06: src/test/provenance/planet-rise-set/calibration.json

- Data path: `src/test/provenance/planet-rise-set/calibration.json`.
- SHA-256: `638c558d0b119ff344d603c89a4a74c6e5e817e200fa1c16d5b3f931e23c5c7e`.
- Source: JPL Horizons API 1.2, collected 2026-11-09, per-response kernels/EOP.
- Endpoint: `https://ssd.jpl.nasa.gov/api/horizons.api`.
- Retained role: test-only numeric event brackets, requests, source identities and pre-feature calibration;
  not a consumer-package member.

The JPL Horizons numeric output remains under its source terms, outside the project MIT grant.
The generated fixture is `src/test/astro/planet_rise_set_reference_data.hpp`.
See the [Horizons acknowledgements](https://ssd.jpl.nasa.gov/horizons/manual.html#acknowledgements)
and [JPL policies and important notices](https://www.jpl.nasa.gov/caltechjpl-privacy-policies-and-important-notices).
These identify the source and terms, not an output-specific permission grant.
No raw response prose, book pages, HTML or binaries are distributed with these records.

`statistics/planet_rise_set_reference.py` is project-authored replay code. Its emitted numeric
fixtures retain the boundary above. The calibration records separate independent Horizons
brackets, an independent bisection of the existing native position model, and clock shifts;
the native comparison does not turn into an independent astronomical reference.
