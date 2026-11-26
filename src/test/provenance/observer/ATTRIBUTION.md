# Observer numeric-receipt attribution

These three numeric-only projections retain the inputs and outputs needed to regenerate
the fixture and replay its independent calibration. Collection date: 2026-11-07.
[SOURCE_PINS.json](SOURCE_PINS.json) records source bytes, configuration, exact Horizons
requests and response hashes. The original response prose, upstream code, binaries and
book pages are not redistributed.

Source terms and the limits of this record are linked in
[Observer reference fixtures](../../../../statistics/observer_reference.md#source-terms).
The links and hashes identify sources; they do not grant reproduction rights or settle
the rights in generated output.

## V20: src/test/provenance/observer/sofa-observer-preflight.json

- Data path: `src/test/provenance/observer/sofa-observer-preflight.json`.
- SHA-256: `5dc83fe108cf0b7abed8b9514f8051a55a53894b84b3b80d45cf266728fc04c7`.
- Source: IAU SOFA issue 2023-10-11 iauGd2gce.
- Retained role: 72 WGS84 vectors; test-only, not a consumer-package member.
- Boundary: these retained values remain under their source terms, outside the project MIT grant.

## V15: src/test/provenance/observer/pymeeus-matched-constant-preflight.json

- Data path: `src/test/provenance/observer/pymeeus-matched-constant-preflight.json`.
- SHA-256: `dd854502b36f3a105c0035fd92e4d488cc40bda64f785f6bd020974e535f958d`.
- Source: PyMeeus commit c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92.
- Retained role: 128 controlled-distance WGS84 reductions; test-only, not a consumer-package member.
- Boundary: these retained values remain under their source terms, outside the project MIT grant.

## V06: src/test/provenance/observer/calibration-results.json

- Data path: `src/test/provenance/observer/calibration-results.json`.
- SHA-256: `5f6678d8f6e17989fa83611c58580ede925b387ef4b1bebb5ad6d7da001a840e`.
- Source: JPL Horizons and base-provider independent calibration.
- Retained role: 60 geocentric/topocentric input and frozen calibration rows; test-only, not a consumer-package member.
- Boundary: these retained values remain under their source terms, outside the project MIT grant.

The endpoint was `https://ssd.jpl.nasa.gov/api/horizons.api`; every source response
prints API 1.2. Source labels are recorded per response, not all treated as DE441.
The native-geocentric and GAST inputs came from base `4d9efd52ed8ef83946208b1e2d6abe91deaeac01`,
before the observer implementation. The independently computed reduction columns are
retained with those inputs; they are not measurements of the production observer.
