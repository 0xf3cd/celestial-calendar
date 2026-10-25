# Interval-event reference values

The independent reference epochs in `src/test/astro/ephemeris_events_test.cpp`
were collected on 2026-10-19 from unmodified PyMeeus commit
[`c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92`](https://github.com/architest/pymeeus/tree/c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92).
The aggregate SHA-256 of its 20 ordered Python modules, formed from each
basename, a NUL separator and the file bytes, is
`af697956dfbb21908af6f5a0a07c6d6f57644c8681033f1f7d6654df0c0b0c67`.

The retained numerical epochs remain under their source terms and outside
the project MIT grant. PyMeeus's module headers specify version 3 of the
GNU Lesser General Public License, or any later version;
see the pinned [Mercury source notice](https://github.com/architest/pymeeus/blob/c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92/pymeeus/Mercury.py)
and the [GNU Lesser General Public License](https://www.gnu.org/licenses/lgpl-3.0.html).
No PyMeeus implementation or coefficient table is added to production.

## Generation and limitations

`statistics/ephemeris_event_reference.py --reference-root <source-tree>`
checks the module count and aggregate hash before collecting the references.
It imports only the supplied validation tree, never production code.

At each JDE(TT), call `Sun.apparent_geocentric_position` or
`Moon.apparent_ecliptical_pos`. For Mercury, call `geocentric_position`
and convert its apparent equatorial result back to ecliptic coordinates
with `equatorial2ecliptical` and `true_obliquity` at observation time.
The Mercury-minus-Uranus reference applies the same conversion to Uranus.
Meeus Example 33.a supplies an equatorial anchor before event collection.

The directed windows and target angles are the printed inputs in the test.
Lift consecutive one-day angular observations, enumerate the intervening
whole-turn levels and bisect each crossing to a bracket narrower than
`2e-8` day. The initial seven crossings comprise one solar level, three
Mercury passes, two oppositely oriented Moon-minus-Sun levels and one
Mercury-minus-Sun level. The station reference differentiates Mercury's
positions with an independent 11-point centered stencil, `h=1/16` day,
and bisects the printed direct-to-retrograde and retrograde-to-direct
brackets. No random seed is used.

These source meshes provide independent event epochs and counts for the
listed cases, not a proof of complete provider isolation. PyMeeus and the
library share astronomical theories. Mercury's reference retains PyMeeus's
retarded-epoch apparent reduction, whereas the library transports the
emission vector to observation axes. The observed crossing/station gaps
are below `9e-7` day; the new `5e-6`-day regression tolerance leaves room for
those model/evaluation differences without confusing separate branches.
It does not loosen any existing test tolerance.
The additional Mercury-minus-Uranus target at -55 degrees has three
passes, collected with 258 source-position evaluations. Its maximum
observed library/reference gap is below `9.1e-6` day; that new dataset uses
`3e-5` day without changing the initial reference tolerance.

The all-target dispatch, constructed contacts and hidden true-node station
tests are explicitly model-self-consistency regressions. Their inputs and
expectations are not independent physical reference columns.

## Elementary-function witnesses

`libm_reference.hpp` holds 516 mathematical reference rows: 64 each for
`sin`, `cos`, `tan`, `atan2`, `asin`, two-/three-argument `hypot`, and
integer-exponent `pow`, with four additional cubic-power witnesses. They were generated on 2026-10-19 with the
validation-only [mpmath 1.3.0](https://github.com/mpmath/mpmath/tree/1.3.0)
Python backend. `LIBM_REFERENCE_PINS.json` records the module identities,
generator, seed `20261019` and output digest. No oracle implementation is
retained, and no Python package is needed to build or run the C++ tests.
`statistics/ephemeris_event_bounds.py --check` verifies the collector and
output digests without importing mpmath.

`statistics/ephemeris_event_math_reference.py --output <directory>`
regenerates the rows with an explicitly installed mpmath 1.3.0. Actual
binary64 inputs are converted exactly; signed-zero results use the IEEE
function rules explicitly because mpmath does not retain the zero sign.
Every rounded result agrees at
80 and 112 decimal digits. The inputs include signed-zero branches,
trigonometric arguments through `±2^40`, latitude functions on `[-1,1]`,
norm coordinates across disparate magnitudes, and source-relevant powers
0, 1, 2 and 3. Rows are column-aligned over the entire table.

The C++ check allows the named `2^-48` absolute/relative premise plus the
stored reference's rounding. Its runtime inputs prevent constant-folding
the whole reference. These finite witnesses can reject a platform premise;
they cannot establish a uniform libm guarantee. No astronomical position
or rate was evaluated to generate them.

## Bound-record transcription

`BOUND_RECORDS.json` retains exact offline derivative/error bounds, signed
rounding directions and the numerical-source identities. The C++ arrays
can be reproduced by `statistics/ephemeris_event_bounds.py`; its `--check`
mode checks source pins and every emitted literal. The automation tests
reject understated errors, raised signed lower bounds, target reordering,
changed elementary-function premises and changed source identities.

Schema 2 uses the versioned `cpp-preprocessing-tokens` digest from
`automation/source_digest.py` for 21 complete headers, including `sun.hpp`.
It ignores comments and layout while retaining literal spellings, token
boundaries, directive endings and function-macro adjacency. Unsupported
preprocessing forms fail explicitly. Coverage is the recorded file list,
not its transitive includes; the 117 retained-data identities are separate.

This emitter checks transcription, not the coefficient-majorant derivation
or platform libm accuracy. The [event guide](../../../../docs/EPHEMERIS_EVENTS.md)
states those separate premises and the distinction between rate evaluation
error and the `M7*h^6/140` truncation remainder.
