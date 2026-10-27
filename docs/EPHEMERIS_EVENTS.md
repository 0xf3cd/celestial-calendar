# Longitude events

The interval searches are available from C++ (`ephemeris_events.hpp`), the C ABI
(`celestial.h`) and Python (`celestial_calendar`); JavaScript exposes no event API.
All three surfaces share this page's contracts: half-open interval membership, deterministic
ordering, exact numerical contacts, domains, limits and the conditional
floating-point contract below. Include `ephemeris_events.hpp` with
`-I src/astro -I src/calendar -I src/util` for interval queries over the 14
`astro::ephemeris::Target` values. Times are JDE(TT), and rates are degrees per TT
day. Body longitudes are apparent, geocentric and tropical; node longitudes follow
[`lunar_node::position`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/lunar_node.hpp)
in the true ecliptic and equinox of date.

<a id="choosing-an-event-api"></a>
## Choosing an event API

Use the interval API for caller-chosen TT windows, seven-point-rate stations and
deterministic endpoint inclusion. The annual C++ operations retain their own
definitions, numerical policies and year domains:

| Operation | Definition and search | Computational domain |
|---|---|---|
| `planet::phenomena::events` | Longitude extrema for stations; planet-minus-Sun longitude crossings; great-circle separation maxima for greatest elongations | UTC years [1, 32766], or [1886, 2098] for Pluto |
| `moon_phase::phase_moments::moments` | Moon-minus-Sun apparent longitude at 0°, 90°, 180° or 270°, refined by Newton's method | UTC years [1, 32766] |
| `moon_phase::new_moon::moments` | Conjunctions with a local near-360° angular fold and acceptance of an exactly +0.0° right bracket endpoint | UTC years [1, 32766] |
| `sun::geocentric_coord::math::find_roots` | Apparent solar longitude at a supplied level, refined by Newton's method; also supplies Jieqi | UTC years [1, 32766] |
| Interval searches on this page | Longitude levels and zeros of `ephemeris::longitude_rate`: six position evaluations, with zero weight at the center | The target's `longitude_rate_domain`; at most 3660 TT days per query |

Annual windows run from Jan 1 to next Jan 1 in UTC, using the library's UT1
substitute before 1972. The annual planetary search detects candidates on a
one-day grid. It refines longitude extrema to a `1e-8`-day bracket and requires
a strict two-sided witness. Those extrema need not coincide with zeros of the
seven-point rate, so station times from the two APIs are not interchangeable.
Greatest elongations have no interval counterpart. See
[`planet_phenomena.hpp`](../src/astro/planet_phenomena.hpp) for the annual event labels.

Moon phases and solar levels use the same apparent-position models as
`longitude_difference_events(MOON, SUN, ...)` and `longitude_events(SUN, ...)`,
respectively, but their solvers and endpoint rules differ. An annual Moon
phase exactly at Jan 1 UTC has no defined owner. Conjunction and
principal-phase New Moon share a root flow while retaining different bracket
and angular-fold conventions; changing an existing conjunction caller to the
principal-phase operation can change success or failure at remote inputs.
The exact folds and endpoint tests are documented on `objective_at`, `miss_at` and `bracket_rejected` in
[`moon_phase.hpp`](../src/astro/moon_phase.hpp).

The annual operations do not delegate to interval searches, even inside the
common domain. This preserves their results, failure policies and supported
years, including the solar roots used by the lunar calendar. A supported
computational domain is not an astronomical accuracy guarantee. C++ callers
using the former low-level Moon-phase names should read the
[migration guide](MIGRATING_1_0.md#c-numerical-helpers-and-generators).

## C ABI and Python

The C exports `celestial_longitude_events`, `celestial_longitude_stations` and
`celestial_longitude_difference_events` mirror the three C++ searches over the single `CELESTIAL_TARGET_*`
code family shared with charts, positions and capabilities. One count+fill protocol copies results into caller-owned buffers:
`event_count` always receives the full total, caller-allocated `slots` receive the
first `min(total, slot_count)` records in chronological order, and the return value
is how many records were written. A truncated fill is therefore observable by
comparing the return value against `*event_count`. A zero total is a legal
no-event outcome; every failure also leaves `*event_count` at 0 and is told apart
from the empty success by `celestial_last_error_kind()`. A nonzero error kind means failure even with an empty
message. Both error readers preserve state; every other export clears or records the thread's kind/message.

```c
uint32_t total = 0;
CelestialLongitudeEventRecord slots[8];
const uint32_t written =
  celestial_longitude_events(CELESTIAL_TARGET_MERCURY, 0.0, 2460735.5, 2460785.5, &total, slots, 8);
if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE) {
  fprintf(stderr, "%s\n", celestial_last_error());
  return 1;
}
/* Consume slots[0..written); total > written reports truncation. */
```

```python
import celestial_calendar as celestial

events = celestial.longitude_events(celestial.Target.MERCURY, 0.0, 2460735.5, 2460785.5)
```

Python uses the shared `Target` enum with `EventDirection` and `StationMotion`, and returns frozen dataclasses.
Wrong types/shapes raise `TypeError`; invalid values, body pairs, intervals or provider-domain rejections raise
`ValueError`, including rejections found in native code. Computation/resource/internal failures and malformed
native success payloads raise `CelestialError`. A nonempty result uses a native count pass and fill pass, with
errors and totals checked after both. A legal empty result returns an empty tuple after the count pass.

```cpp
using namespace astro::ephemeris;

const auto crossings = longitude_events(
  Target::MERCURY, astro::toolbox::AngleDeg { 0.0 }, 2460735.5, 2460785.5
);
const auto turns = stations(Target::MERCURY, 2460735.5, 2460785.5);
const auto differences = longitude_difference_events(
  Target::MOON, Target::SUN, astro::toolbox::AngleDeg { -60.0 },
  2460676.5, 2460707.5
);
```

The difference is **first minus second**, canonicalized to `(-180°,180°]`.
Whole-turn target shifts are equivalent; `-180°` and `180°` select the same
level. Reversing the targets also reverses the requested angle and crossing
direction. The API does not assign names or interpretations to angles.

## Search terminology

- A **canonical cell** is a fixed interval on the target-domain grid. Its boundaries
  do not move when a caller clips or splits a query. The **owning cell** is the
  half-open cell containing an event's reported time; it reports that event once.
- A **seam** is a shared boundary between cells. A station there may need samples
  from neighboring cells to establish behavior on both sides.
- A **certificate** is a sufficient numerical test for exclusion, a unique root,
  or a two-sided contact under the stated error model. It is not an astronomical
  accuracy bound. Inconclusive tests subdivide or fail within the work limits.
- An **allowance** bounds evaluation, rounding or displacement error for a specified
  sample or region. Derivative bounds and the rate's truncation remainder have
  separate roles; one allowance cannot substitute for another.
- An **admitted center** is a time where the complete seven-point rate stencil fits
  inside the provider domain. If the requested time requires another center, its
  displacement is included in the observation's error allowance.

These terms describe numerical interval membership and checks. The caller owns C
output buffers; returned C++ and Python values remain independent of later calls.

## Results and interval membership

- `LongitudeEvent` contains `jde_tt`, `longitude`, `longitude_rate` and
  `direction` (`INCREASING`, `DECREASING` or `TANGENT`).
- `StationEvent` contains the time, longitude, seven-point rate, and
  `motion_before`/`motion_after` (`DIRECT` or `RETROGRADE`).
- `LongitudeDifferenceEvent` contains both longitudes, both rates, the
  canonical signed difference and the level's direction.

Results are strictly time-ordered. A reported event at `start_jde_tt` belongs
to the query; one at `end_jde_tt` does not. Canonical cells, analysis bands,
refinement and reporting do not depend on the caller's endpoints. Splitting
a successful query at a reported event assigns it to the right-hand query.
Unresolvable reported-time collisions throw rather than merge nearby events.

Stations are zeros of `ephemeris::longitude_rate`, the seven-point
rate with `h=1/8` TT day, not an analytic derivative or a minimum angular
separation. The reported rate need not be bitwise zero. Descending nodes use
their actual antipodal positions, while their rates retain the ascending
model's trajectory.
Near a station, its sign can differ from the one-day
`planet::geocentric_coord::is_retrograde` marker; see `LongitudeRate`'s
contract.

### Numerical contacts

A tangent is an exact numerical contact at a canonical stationary time:
the supplied target equals the longitude evaluated at that time, and
certified observations on both sides do not cross the level. Relative
stations use the difference of the two seven-point rates. A zero rate alone
does not establish a tangent.

For example, use a returned station's `longitude` as the target of
`longitude_events`. There is no angular proximity window: an adjacent
binary64 target is not that same contact. If the available error allowances
cannot distinguish a near miss from two crossings, the query throws.
Certification first tries the station's owning cell, then a fixed window
covering that cell and its immediate neighbors. The window is independent
of the query and uses allowances covering its full span. Certified contact
regions are excluded from every touched cell before the crossing search.
The constructed query can still throw when two-sided evidence cannot be
certified, including at a true provider-domain endpoint. Reusing a station's
longitude does not bypass the completeness checks.
Contact certification checks a continuous angular lift, derivative-side
margins larger than the rate's truncation remainder, and signed curvature
throughout the chosen region. The region may be wider or narrower than the
rate's initial isolation bracket; every accepted region is checked afresh.

Self-pairs are invalid. Same-model ascending/descending pairs are
structurally antipodal: a matching opposition target throws
`std::invalid_argument` because the relation holds throughout the interval;
a different target returns an empty list.

<a id="domains-limits-and-failures"></a>
## Domains, limits and failures

Each query must be nonempty, finite and inside `longitude_rate_domain` for
its target, or the intersection for a pair. The lower boundary is inclusive;
the upper query endpoint may equal the domain end. The rate domain already
reserves the full `±3/8`-day sampling reach. All 14 target domains remain
available; the Pluto model retains its narrower source window.

A query spans at most 3660 TT days. Searches allow at most 65536 provider
attempts and 4096 candidate slots. A rate observation makes six underlying
position calls; a pair may evaluate both providers. The shared limits bound
work, not a promise that every allowed span resolves within that budget.

Malformed/out-of-window input, self-pairs and continuously satisfied
relations throw `std::invalid_argument`. Numerical ambiguity, failed
iteration, unsupported floating-point execution or work-limit exhaustion
throws `std::runtime_error`. No failure returns a partial event list.

<a id="numerical-contract"></a>
## Numerical contract

Completeness is conditional on an explicit evaluation-error model. It is
not an unconditional statement about every C++ implementation or libm,
and it is not a bound on the astronomical model's physical accuracy.

The execution premises are IEEE-754 binary64 with `FLT_EVAL_METHOD == 0`, round-to-nearest-even,
gradual underflow, no unsafe algebraic reassociation or fast-math, and the
source's nearest-binary64 constants. `fabs`, `remainder`, `nextafter` and
power-of-two lattice operations must have their stated IEEE meanings.
The API checks the floating representation, active rounding mode and
gradual-underflow behavior and rejects fast-math builds. These guards do
not test every elementary-function call.

The budgets use `2^-48` for each named elementary-function error channel:

| Function | Error premise |
|---|---|
| `sin`, `cos` (including VSOP rows) | absolute error at most `2^-48` |
| `atan2`, `asin` | absolute angular error at most `2^-48` radians |
| `tan` | relative error at most `2^-48` on the certified finite latitude domains |
| two-/three-argument `hypot` | relative error at most `2^-48` |
| `pow` with exponents 0, 1, 2 or 3 | relative error at most `2^-48` on the source's admitted arguments; exponent 0 returns 1 exactly |

Zero results in relative channels must be exact. The domain-specific
evaluation budgets account separately for arithmetic, argument formation,
normalization, frame transport, finite light-time/inverse-FK5 iteration,
and conversion from the stored degree/radian scale to the smooth exact-π
reference. Selecting these premises does not establish them as platform
facts; deployments with a different math contract need new allowances.

Derivative bounds come from source coefficient sums and outward rational
majorants through order seven. A fixed J2000 `±100`-year analysis band
uses tighter bounds where an entire canonical cell and its sampling halo
fit; a contact window must also fit before using that band. The remaining
domain uses the full-window bounds. Band selection is
cell-based, so widening a query cannot change a station's definition.
`EVENT_BOUNDS_CENTURY` and `EVENT_BOUNDS_WIDE` hold these allowances;
[`statistics/ephemeris_event_bounds.py`](https://github.com/0xf3cd/celestial-calendar/blob/main/statistics/ephemeris_event_bounds.py)
checks their transcription from the
[exact records](https://github.com/0xf3cd/celestial-calendar/blob/main/src/test/provenance/ephemeris-events/BOUND_RECORDS.json).

The rate derivative bounds use the exact `23/15` integral-kernel norm.
The distinction between the smooth longitude derivative and the seven-point
rate is bounded by `M7*h^6/140`, using a nonnegative Peano kernel; the
[derivation below](#seven-point-operator-and-remainder) gives both constants.
This truncation remainder is **not** the floating-point error of evaluating the
six-sample rate. Both terms are retained for contact classification.

Refinement requests a `2^-10`-day time certificate (84.375 seconds), then
reports on a nearest-even `2^-30`-day lattice. The fine lattice makes
half-open interval membership deterministic; it does not claim microsecond physical
accuracy. A certified secant observation usually gives a substantially
better representative than the stopping criterion alone.

### Search policies

The quarter-day grid fixes which bracket owns an event. Its spacing is a
starting scale for certification, not a guarantee that a cell contains at
most one event. A cell that cannot be excluded or certified is subdivided.
The default 32-level cap would reduce a quarter-day cell below `6e-11` day;
an unresolved observation allowance or indistinguishable times can stop
the search earlier. Such regions remain unresolved and fail if they
intersect the query.

The default 96 refinement iterations bound work even when the requested
resolution is zero. The limit exceeds binary64's 53-bit significand width,
but does not guarantee convergence for every bracket or error allowance.
Refinement checks the actual time radius or observation certificate, and
throws on exhaustion without an extra acceptance check after the cap.
Contact certification tries at most 24 regions inside the owning cell,
then at most 24 in its fixed neighbor window. The dyadic radii change the
region being certified; they do not relax its sign or curvature tests.

The [admission and budget limits](#domains-limits-and-failures) are project resource policies, not
astronomical or completeness bounds. A smaller request may still exhaust
either budget. One mutable budget belongs to the whole query, including
recursive subdivision, refinement and contact-window probes; provider
attempts consume it even when the provider throws.

### Scalar cell certificates

These are the project's sufficient tests for a smooth scalar `f` on a
closed cell `[a,b]`, with `w=b-a`, `|f'|≤M1` and `|f''|≤M2`. Each sampled
value has an absolute allowance that includes any abscissa displacement.
The implementation rounds lower bounds downward and upper bounds upward
under the [numerical contract](#numerical-contract).

Let `m_a` and `m_b` be lower bounds for the magnitudes of same-sign
endpoint values. If a root existed at `r`, the derivative bound would give
`m_a≤M1(r-a)` and `m_b≤M1(b-r)`. Therefore `m_a+m_b>M1*w` excludes a root.
Independently, the linear interpolant `L` obeys
`|f(t)-L(t)|≤M2(t-a)(b-t)/2≤M2*w²/8`; same-sign endpoints with
`min(m_a,m_b)>M2*w²/8` also exclude a root.

For uniqueness, subtract the endpoint allowances from the observed
absolute change, then divide by `w` to obtain a lower bound `s` on the
absolute secant slope. Since the secant is the average of `f'`,

```text
|f'(t) - (f(b)-f(a))/w|
  ≤ M2 * ((t-a)² + (b-t)²) / (2w)
  ≤ M2*w/2.
```

Thus `s-M2*w/2>0` proves a nonzero derivative throughout the cell.
Opposite robust endpoint signs then certify one root; an exact endpoint
zero is assigned according to the cell's endpoint-ownership rule.
Without a positive derivative floor, a sign change alone is not a
unique-root certificate. Curvature certificates for contacts use the
second divided difference and the third-derivative bound, as described
in `event_search::curvature_sign`; the seven-point rate's separate
truncation remainder is retained when checking both sides of a station.

<a id="seven-point-operator-and-remainder"></a>
## Seven-point operator and remainder

For a seven-times continuously differentiable angular lift `g` whose
sample-pair changes fit in `(-180°,180°)`,
`ephemeris::detail::differentiate` evaluates

```text
D_h g(t) = Σ[j=1..3] c_j [g(t+jh) − g(t−jh)] / h
(c_1,c_2,c_3) = (3/4, −3/20, 1/60),  h = 1/8 TT day.
```

The coefficients are the [central finite-difference stencil](https://en.wikipedia.org/wiki/Finite_difference_coefficient#Central_finite_difference).
The fundamental theorem of calculus rewrites this as
`D_h g(t) = ∫[-3,3] w(s) g′(t+hs) ds`. The even, piecewise-constant
weight is `37/60` for `|s|≤1`, `−2/15` for `1<|s|≤2`, and
`1/60` for `2<|s|≤3`. Its absolute integral is
`2(37/60 + 2/15 + 1/60) = 23/15`. Differentiating the integral bounds
the first and second derivatives of `D_h g` by `23/15` times the
suprema of `|g″|` and `|g‴|` over the full sample halo.

The functional `D_1 g(0)−g′(0)` vanishes on polynomials through degree
six. Its seventh-order
[Peano kernel](https://en.wikipedia.org/wiki/Peano_kernel_theorem) is even;
for `x≥0`, it is

```text
K(x) = [45(1−x)_+^6 − 9(2−x)_+^6 + (3−x)_+^6] / 43200,
y_+ = max(y,0).
```

It vanishes outside `[-3,3]` and is nonnegative inside. On `[2,3]`
only the last term remains. On `[1,2)`, `(3−x)/(2−x)≥2` makes the
last term exceed the negative term. On `[0,1]`, set `u=1−x`. The
numerator is
`55+138u+105u²−20u³−75u⁴−42u⁵+37u⁶`, which is at least
`55+106u+37u⁶>0` because `0≤u≤1`.

Consequently `∫K = 2Σ c_j j⁷/7! = 1/140`, and scaling gives
`|D_h g−g′| ≤ (h⁶/140) sup|g⁽⁷⁾|`. This truncation term is separate
from the floating-point allowance for evaluating the six samples.
The exact record fields `position_seventh_derivative`,
`rate_to_position_derivative_remainder` and `rate_error` retain that
distinction.
