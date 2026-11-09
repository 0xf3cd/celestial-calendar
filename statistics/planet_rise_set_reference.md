# Planetary rise/set references

The [numeric receipt](../src/test/provenance/planet-rise-set/calibration.json) holds twelve
fixed UT1 dates/sites and 32 independent Horizons event brackets for Mercury through Pluto.
The cases include both hemispheres, high-latitude days without crossings, nonzero elevation,
east/west longitude, the date line and the date/site of Meeus Example 15.a.
Inputs were fixed before running the new planetary adapter; there is no random seed.
The [attribution record](../src/test/provenance/planet-rise-set/ATTRIBUTION.md) states the
retained-material boundary. Numeric output remains under its source terms, outside the project MIT grant.

## Source conditions and event brackets

All 68 requests and response hashes are recorded. Horizons observer tables use geodetic
`coord@399`, east longitude/latitude/ellipsoidal kilometres, explicit TT, `AIRLESS`,
ICRF reference selection and quantities 2/4/7/30/49. The response labels identify the
actual target kernels and EOP file, not an assumed common DE441 provider for every planet.
The kernel files' own revisions are not exposed by those labels.

The [Horizons manual](https://ssd.jpl.nasa.gov/horizons/manual.html) defines the quantities and
RTS indicators. Ordinary RTS flags indicate an event in the previous output interval;
they are not precise event epochs. Here a one-minute scalar scan locates crossings of
airless centre altitude through -34 arcminutes and of apparent local hour angle through
zero. The latter is upper transit, not necessarily maximum altitude. Separate requests
refine each bracket to one second, then 0.1 and 0.01 seconds. The widest retained bracket
spans 0.010098516941070557 seconds using the JD values printed by Horizons.
Direction changes distinguish rise/set;
the 180-degree hour-angle wrap is not treated as transit.

Modern Horizons UT means UTC. Quantity 30 is TDB-UTC and quantity 49 is UT1-UTC;
their difference supplies local TDB-UT1. TT-UT1 differs by the TT/TDB offset, whose magnitude
the manual bounds at 2 ms. Endpoint DUT1 and printed epochs
are retained, and day ownership is checked using the resulting approximate UT1 values,
well away from the midnight boundary in this finite dataset. Coarse sampled altitude
extrema support the two no-crossing examples, not an arbitrary-interval completeness proof.

## Numerical, clock and physical-model comparisons

Before the new header existed, the unchanged engine was run with the proposed composition
of existing observer and default-time operations at commit `e79f3041f4004c0a73a2730d4dc5493c14617b8d`.
An independent bisection then used direct observer altitude/meridian residuals, without
calling the rise/set solver. Its final brackets bound the engine discrepancy by at most
0.00008046627044677734 seconds across these 32 events.

A second bisection holds the recorded source TDB-UT1 locally constant. Comparing the two
native-model brackets measures the clock contribution; comparing the source-clock bracket
with Horizons leaves the physical-model gap, plus TT/TDB, local EOP variation and printing
qualifications. Native observer reduction omits diurnal aberration, polar motion, solar light
deflection and an observer-specific light-time solve. A model mismatch is not a solver residual.

Per-target full-model regression allowances were frozen from the pre-feature maximum distance
to either source-bracket endpoint: three times that distance plus 0.0021 seconds, rounded
outward to one significant digit. They are Mercury 0.2, Venus 0.4, Mars 0.4, Jupiter 0.2,
Saturn 0.2, Uranus 0.3, Neptune 0.3 and Pluto 0.6 seconds. These finite-data allowances
are not accuracy promises, atmospheric error estimates or convergence tolerances.
The numerical comparison has its separate 0.0002-second allowance.

Meeus Chapter 15, p. 101, supplies the centre/point-source -34 arcminute convention.
Example 15.a is printed to minutes and uses its own geocentric/interpolated inputs;
the Horizons Boston row is a separate topocentric reference, not a transcription of that answer.

## Offline replay

From a clone, without network access or an installed library:

```sh
python3 statistics/planet_rise_set_reference.py --check
```

This verifies the receipt hash, bracket directions and widths, bisection comparisons,
clock decomposition and the frozen allowance formula, then regenerates the complete fixture.
`--output <path>` writes a copy. With original request/response pairs arranged as the
relative paths in `source_requests`, add `--raw-dir <directory>` to verify every response's
bytes/source labels and reconstruct final scalar endpoints. Original response prose is
not part of the public dataset. Recollecting from the recorded endpoint/parameters may
produce changed source labels or bytes; treat that as a new collection, not a way to
silently update the frozen regression allowances.
