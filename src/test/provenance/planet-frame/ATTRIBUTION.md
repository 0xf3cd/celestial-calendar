# Planetary observation-frame reference

`FRAME_ROWS` in `src/test/astro/planet_test.cpp` was generated on 2026-10-12 by
`statistics/planet_frame_reference.py`. `SOURCE_PINS.json` identifies the inputs,
generated values, source counts and comparison thresholds. These coefficient-derived
values remain under their source terms and outside the project MIT grant;
`THIRD_PARTY_NOTICES.txt` records the VSOP87 source terms. No external implementation or coefficient
table is copied into this generator or added to production.

## Sources and regeneration

- AA+ v2.55 (29 April 2024), complete unfiltered `AAVSOP87D_*.cpp` tables for Earth
  and Mercury–Neptune. Source archive and individual unit hashes are in `SOURCE_PINS.json`.
- IMCCE's [VSOP87 specification](https://ftp.imcce.fr/pub/ephem/planets/vsop87/vsop87.doc),
  *Time scale*, *Reference system* and *Computation*: D coordinates use mean ecliptic
  and equinox axes at the evaluation date; dynamical time is treated as equal to TT.
- IMCCE's [substitution checks](https://ftp.imcce.fr/pub/ephem/planets/vsop87/vsop87.chk):
  all eight D-series J2000 position triples are checked against the printed digits.
- Jean Meeus, *Astronomical Algorithms*, second edition, pp.136–137,
  (21.5)/(21.7) and Example 21.c; Chapter 33, (33.1)–(33.4).

Supply an unmodified AA+ directory and the primary check file explicitly:

```sh
python3 statistics/planet_frame_reference.py \
  --reference-root /path/to/aaplus \
  --check-file /path/to/vsop87.chk > planet-frame-reference.json
```

The standard-library-only generator checks every declared coefficient array, term count,
power and ordered aggregate reference. It evaluates the unscaled coefficients with
`math.fsum`; production evaluates its separately imported scaled tables. It reports
source hashes, primary substitutions, the book example, every observation/emission
epoch, iteration count and vector. Output JDE inputs must round-trip to the same
binary64 values when inserted into C++; station inputs are the source decimal epoch
plus a binary64 `±0.1` day offset, not a newly rounded decimal epoch.

## Selection and constructions

No random seed is used. Every body receives January 1 at noon TT in 1885, 2000 and
2099, followed by the first station in the 2025 Horizons census in
`src/test/astro/planet_phenomena_test.cpp` at `±0.1` day.
Horizons selects those inputs; it does not supply the geometric expectations.
Mercury receives `2460737/2460738` and `2460764/2460765` for direct/retrograde wraps;
Neptune receives `2460764/2460765` for its direct wrap. There are 41 rows.

The direct construction rotates the emission-time target using active column-vector
`Rz(Π+p) Rx(-η) Rz(-Π)`, then subtracts fixed observation-time Earth. The second
construction rotates each operand back to fixed J2000 axes, subtracts there, and
rotates the result to observation axes. Each construction independently iterates the
emission epoch with `0.0057755183` day/AU, a ten-iteration cap and one observation-JD
ULP stopping criterion. Both start at the binary64 observation epoch, evaluate the vector
at the current binary64 emission epoch, and form the next epoch by binary64 subtraction
of the computed light time from the observation epoch. At the first step whose successive
epoch difference is at most one observation-JD ULP, they return the current epoch and
its vector, not the proposed next epoch. Neither calls production code.

Both returned vectors use observation-date axes; the fixed construction's intermediate
light-time vector uses J2000 axes. The rows pin this discrete iteration and emission selection.
They do not admit every neighbouring binary64 epoch satisfying the stopping inequality.

The two constructions share the Meeus model and VSOP87D theory. They test arithmetic,
transcription and composition, not a shared physical-model error. Chapter 21's
mean-frame transport is not asserted to be the exact dynamical precession law that
generated every VSOP87D date-series coefficient.

## Tolerances

The reference was generated before selecting the thresholds. Over these 41 rows,
the largest production-minus-reference gaps were `2.29250e-10°` longitude,
`3.09309e-12°` latitude and `1.01086e-12 AU` range. Direct versus fixed-axis independent
vectors differed by at most `1.44692e-14 AU`. The test thresholds are `1e-9°`, `2e-11°`
and `5e-12 AU`, leaving at least four times the measured arithmetic gap plus the
stored-value rounding for the specified discrete emission selection, not the entire
set of epochs within the ULP stop criterion.

These are implementation/reference regression thresholds, not physical uncertainty
estimates or whole-domain accuracy guarantees.
