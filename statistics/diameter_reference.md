# Apparent-diameter references

The [numeric receipt](../src/test/provenance/diameter/calibration.json) retains
50 Horizons rows: J2000 for all ten bodies, three additional epochs per body,
six lunar topocentric rows and four daily-sampled Sun/Moon near-extrema.
Those daily samples locate small/large ranges on the recorded grid; they are
not solved extremum times. The [radius record](../src/test/provenance/diameter/radii.json)
pins the adopted source column independently of the runtime radius table.
Source-specific terms and the manuscript/resolution distinction are recorded in
[ATTRIBUTION](../src/test/provenance/diameter/ATTRIBUTION.md).

## Replay

```sh
python statistics/diameter_reference.py --check
```

Only Python's standard library and the cloned numeric receipts are needed.
The replay uses a tangent triangle, `2 atan2(R, sqrt(d-R) sqrt(d+R))`, not the
production diameter implementation. Radius is converted from km to AU using
the exact NIST/IAU value 149597870.7 km/AU.

The source-own geometry allowance is half the last printed angular digit plus
the maximum propagated change from half the last range digit and eight binary
ulps for arithmetic. In particular, the J2000 Sun's `1950.990` has a 0.0005
arcsecond half-digit allowance. The Moon's narrow allowance is retained per row.

The full-provider bounds are separate: three times the largest measured gap
per target/centre, rounded upward to one significant decimal digit. The native
range inputs were captured at `1caf4f97f641e889498bb3c2252b2a8118e0e115`, before
the diameter implementation existed. Maximum measured gaps include 0.171924
arcsec for the geocentric Moon and 0.132986 arcsec for the selected lunar sites.
These finite-dataset regression bounds are not accuracy guarantees.

## Source inputs

Each original request pins `TIME_TYPE=TT`, exact JDEs, observer centre/site,
airless output, range unit, actual header radius, API signature and the exposed
kernel/EOP labels. Horizons exposes labels, not immutable kernel revisions.
The topocentric rows use Q49's printed UT1-UTC and the held TT-UTC offsets
(64.184 s at J2000, 69.184 s in 2026) to recover the source UT1 inputs.
Library civil UTC/UT1 and explicit Delta T selections are tested separately.
Fresh service output can differ as kernels/EOP change.

The retained source data contain numeric inputs/outputs and source identities
only; raw response prose, paper pages and upstream implementations are excluded.
