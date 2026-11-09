# Apparent-diameter source records

## Radius facts

The adopted IAU/JPL numeric radius facts in `src/astro/apparent_diameter.hpp` and
`radii.json` remain under their source terms and outside the project MIT grant.
The solar value is the exact nominal conversion scale of IAU 2015 Resolution B3,
as reproduced in Prsa et al. (2016), Table 1, arXiv:1605.09788v1. The retained
original was that author manuscript, not the unavailable resolution PDF.
The JPL SSD planetary equatorial column and satellite Moon mean column cite
Archinal et al. (2018), respectively reference D and reference 1.

All ten numeric radii also match the actual Horizons request headers recorded in
`calibration.json`. Those retained header facts use the existing V06/V08 output
boundary; the IAU/JPL table citations independently identify the adopted conventions.

Source links and collection records identify the facts; access and citation do
not imply a redistribution permission grant. No paper pages or table prose are
included here.

## V06: src/test/provenance/diameter/calibration.json

- Data path: `src/test/provenance/diameter/calibration.json`.
- SHA-256: `eaa29733328e791cadec3bee55792866a7e6a117e6d6239672e55f4705fe91b2`.
- Source: JPL Horizons API 1.2 and source-labelled kernels/EOP.
- Retained role: test-only numeric calibration, not a consumer-package member.
- Endpoint: `https://ssd.jpl.nasa.gov/api/horizons.api`.

JPL Horizons numeric output and request/source identities remain under their
source terms and outside the project MIT grant. The related fixture is
`src/test/astro/diameter_reference_data.hpp`.
See the [Horizons acknowledgements](https://ssd.jpl.nasa.gov/horizons/manual.html#acknowledgements)
and [JPL policies and important notices](https://www.jpl.nasa.gov/caltechjpl-privacy-policies-and-important-notices).
These are source/terms pointers, not an output-specific grant. Raw response prose,
HTML and binaries are not supplied.

## R34 exact AU definition

The local binding validation constants use NIST SP 330 (2019), Table 8, and
ERFA v2.0.1 `erfam.h`, `ERFA_DAU`, with the existing R34 source relation and ERFA
notice. The definition remains outside the project MIT grant; validation code is
project-authored. The source definition and terms are retained under
`src/test/provenance/nist/sp-330-2019/` and `src/test/provenance/erfa/v2.0.1/`.
