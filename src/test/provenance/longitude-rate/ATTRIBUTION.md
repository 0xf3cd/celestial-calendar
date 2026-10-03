# Longitude-rate reference attribution

Applies to the 127 `GOLDEN_ROWS` in `src/test/astro/ephemeris_test.cpp`, collected
2026-10-03. `SOURCE_PINS.json` identifies the exact upstream versions and entry-point
bytes. Inputs are the printed binary64 JDE(TT) values; results are signed degrees per
TT day in the true ecliptic and equinox of date, with apparent corrections.

## Sources and licence boundaries

- **V44, AA+ v2.63:** SwiftAA commit `e70ab8d0a9a5c949eef00071bacfe3ca5731300b`,
  [Sources/aaplus-v2.63](https://github.com/onekiloparsec/SwiftAA/tree/e70ab8d0a9a5c949eef00071bacfe3ca5731300b/Sources/aaplus-v2.63).
  Supplies Sun, Moon, Mercury-Neptune and mean/five-term true-node positions. AA+
  permits binary use but restricts source redistribution. No AA+ source is retained here.
- **V20, SOFA:** unchanged issue 2023-10-11 `iauNut80`, full 106-term IAU 1980
  nutation. Source terms are recorded in `src/test/provenance/sofa/2023-10-11/`
  and `THIRD_PARTY_NOTICES.txt`. The input hash below identifies the compiled
  release source, independently of existing retained SOFA snapshot pins.
- **V15, corrected PyMeeus:** commit `c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92`,
  [architest/pymeeus](https://github.com/architest/pymeeus/tree/c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92).
  Its Earth VSOP87D positions supply the Pluto geocentric reference; PyMeeus retains
  its Lesser General Public License terms. Corrected mean/true nodes corroborate AA+
  but supply no retained node rate. The cubic node denominator is `467441`.
- **R38, Sonia Keys meeus v3:** `pluto.go` at commit
  `bfbd9ac2c7f709c94f1dee190c633d24a723f628`,
  [Chapter 37 Pluto evaluator](https://github.com/soniakeys/meeus/blob/bfbd9ac2c7f709c94f1dee190c633d24a723f628/v3/pluto/pluto.go).
  Supplies the independent Pluto reference's 43 terms and heliocentric model. Its
  MIT notice is already retained in `THIRD_PARTY_NOTICES.txt`.

The frozen numerical rows remain under their source terms and outside the project MIT
grant. This record adds no upstream permission claim. The test code and runtime
`src/astro/ephemeris.hpp` are project-authored; differentiation uses independent
standard mathematics. Upstream implementations are validation-only sources.

## Regeneration

At each printed epoch `t`, evaluate the following position at every reference sample:

| Target | Position call and correction |
|---|---|
| Sun | `CAASun::ApparentEclipticLongitude(t, true)` |
| Moon | `CAAMoon::EclipticLongitude(t)`; this one-argument call already includes truncated nutation |
| Mercury-Neptune | `CAAElliptical::Calculate(t, Object, true).ApparentGeocentricEclipticalLongitude` |
| Mean nodes | `CAAMoon::MeanLongitudeAscendingNode(t)` |
| True nodes | `CAAMoon::TrueLongitudeAscendingNode(t)`; Meeus five-term approximation |

For Sun, Moon and Mercury-Neptune, subtract
`CAANutation::NutationInLongitude(t)/3600` exactly once. For all targets, add the
longitude correction from `iauNut80(2451545.0, t - 2451545.0, ...)`, converted from
radians to degrees, then normalize to `[0, 360)`. Descending positions are ascending
plus 180 degrees; descending rates differentiate the ascending trajectory.

AA+ retains its variable Sun aberration coefficient `0.005775518` days/AU; the
project uses `0.0057755183`. Mercury-Neptune hold Earth at observation time and
evaluate only the target at retarded time, using its retarded-date VSOP87D frame
directly in subtraction. Aberration, FK5 and nutation use observation time. AA+
stops on successive target changes of `1e-5` degrees in L/B and `1e-6` AU in R;
the project's light-time residual stopping rule differs.

For Pluto, use the independent Chapter 37 evaluator and
`Earth.geometric_heliocentric_position(Epoch(t), tofk5=False)`. At each light-time
iteration, precess Pluto's J2000 ecliptic position to observation date (Meeus 21.7),
invert the FK5 correction (32.3), and subtract the observation-time Earth vector.
Update the retarded epoch with `t - 0.0057755183 * distance_au`, stopping when the
epoch change is at most one ULP of `t` (maximum ten iterations). Apply observation-time
annual aberration (23.2), forward FK5 correction and full SOFA nutation. Inverse FK5
uses at most eight fixed-point iterations and a `1e-15`-degree change criterion.

The rate reference is the derivative at the center of the **11-point Lagrange
interpolant**, with `h = 1/8 TT day` and samples `t + k*h`, `k = -5,...,5`.
Reject a sample grid unless every offset is represented exactly. Starting from the
center, lift consecutive circular differences into `[-180, 180]` on each side.
Accumulate the derivative with compensated summation. The positive pair coefficients
are `5/6, -5/21, 5/84, -5/504, 1/1260`, divided by `h`. These are reference coefficients,
not the production seven-point coefficients.

Diagnostics use the 11-point reference at `h=1/16`, the 9-point reference at `h=1/8`,
and an alternate 11-point grid at `h=3/16` with halving to `3/32`. The 9-point positive
pair coefficients are `4/5, -1/5, 4/105, -1/280`, divided by `h`.

## Epoch selection and validation corpus

The broad validation contains 14 targets at 89 training and 165 holdout epochs
(3,556 rates). Training uses 25 directed epochs and 64 C++ `mt19937_64{306}` draws
over `[2409545, 2488067.5]`. Holdout uses the 2025 Horizons station epochs already
in `src/test/astro/planet_phenomena_test.cpp`, 64 Python `random.Random(30602)` draws,
and reference-only Sun/Moon wraps and five-term true-node stations. Hashes of the
original corpora and grids are recorded in `SOURCE_PINS.json`; broad output is not
part of the compact runtime test fixture.

Selection uses roles and input seeds before reading production comparisons:

- Every target: early `2409545`, J2000 `2451545`, late `2488067.5`, and the first
  holdout seed draw `2426902.8825072921` (56 rows).
- Mean-node negative wrap: `2453905.5/2453906.5`; true-node negative wrap:
  `2453908.5/2453909.5`, including both orientations (8 rows).
- First reference Sun/Moon wrap of 2025, at offsets `-0.02, 0, 0.02` (6 rows).
- First 2025 station of each available direction per planet, at offsets
  `-0.25, 0, 0.25` from the recorded Horizons epoch (45 rows). Mars has only the
  direct station in this census. Horizons chooses epochs; the rates come from the
  above reference pipelines.
- First two reference five-term true-node stations of 2025, at offsets
  `-0.01, 0, 0.01`, including both orientations (12 rows).

No row is filtered by a production residual. Swiss rates supply no retained golden:
its osculating true nodes and default frame models have different definitions.

## Initial test tolerances

All numbers below are absolute degrees per TT day, measured over the complete
training plus holdout corpus. P is production seven-point `h=1/8` minus the primary
11-point reference; A compares the same production result to the alternate `h=3/16`
reference. H and AH are the respective reference step-halving differences.

| Target/family | Worst P | Worst A | Worst H | Worst AH | Initial test tolerance |
|---|---:|---:|---:|---:|---:|
| Sun | 1.1732786964202546e-9 | 8.865370659805194e-10 | 1.2913003999415196e-9 | 1.0008357476110064e-9 | 1e-8 |
| Moon | 3.2182612130782218e-10 | 1.2443912567050575e-9 | 2.2485995287979677e-9 | 1.547334704810055e-9 | 1e-8 |
| Mercury | 2.4778393503677165e-9 | 6.0423950287713524e-9 | 1.5115305701529635e-8 | 1.0265327476943753e-8 | 1e-7 |
| Venus | 4.576588885640831e-9 | 1.1624454954173302e-8 | 2.0158061331265742e-8 | 1.4361974803867028e-8 | 1e-7 |
| Mars | 1.206494210093112e-9 | 6.4485143891346297e-9 | 8.3710545994986063e-9 | 5.9973476185248842e-9 | 5e-8 |
| Jupiter | 1.5924597751570957e-10 | 4.2497944052755088e-10 | 8.0425452408938725e-10 | 4.9678416935705627e-10 | 5e-9 |
| Saturn | 7.9134143248643651e-11 | 1.3596224840428661e-10 | 2.9448625482597635e-10 | 2.0569311531826173e-10 | 2e-9 |
| Uranus | 6.041407651924402e-11 | 6.0754668657070354e-11 | 1.1147951312073801e-10 | 6.8824036747461292e-11 | 1e-9 |
| Neptune | 2.9309860094528517e-11 | 3.3969722867954744e-11 | 5.8905304245460144e-11 | 3.8508314592622384e-11 | 5e-10 |
| Pluto | 5.826637783368227e-12 | 2.2890276479836658e-11 | 4.7717794125767821e-11 | 4.0486291402741514e-11 | 5e-10 |
| Mean nodes, either orientation | 4.7149437132354421e-12 | 7.4398820437693303e-12 | 1.5890538884733019e-11 | 8.8276053133995447e-12 | 1e-10 |
| True nodes, either orientation | 2.6784602313867367e-11 | 5.8714796913328371e-11 | 1.0306822062489118e-10 | 9.458990535282652e-11 | 5e-10 |

Each new tolerance exceeds four times the maximum of P, A, H and AH, rounded up
to the shown threshold. This margin accounts for observed reference noise, the
stated apparent-reduction differences and cross-platform arithmetic. It is an
initial regression threshold for implementation/reference agreement, not a physical
accuracy claim or a certified error bound. It changes no existing test tolerance.

The planetary station-center check uses `3e-5`: the Chapter 37 Pluto model has a
nonzero rate up to `2.078300449320234e-5` at these independently chosen Horizons
epochs. True-node reference roots use `5e-10`, the tighter same-definition margin.
