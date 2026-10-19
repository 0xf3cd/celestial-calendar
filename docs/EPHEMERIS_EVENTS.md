# Longitude events

Include `ephemeris_events.hpp` with `-I src/astro -I src/calendar -I src/util`
for interval queries over the 14 `astro::ephemeris::Target` values. Times are
JDE(TT), and rates are degrees per TT day. Body longitudes are apparent,
geocentric and tropical; node longitudes follow
[`lunar_node::position`](https://github.com/0xf3cd/celestial-calendar/blob/main/src/astro/lunar_node.hpp) in the true ecliptic and
equinox of date.

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

## Results and ownership

- `LongitudeEvent` contains `jde_tt`, `longitude`, `longitude_rate` and
  `direction` (`INCREASING`, `DECREASING` or `TANGENT`).
- `StationEvent` contains the time, longitude, published rate, and
  `motion_before`/`motion_after` (`DIRECT` or `RETROGRADE`).
- `LongitudeDifferenceEvent` contains both longitudes, both rates, the
  canonical signed difference and the level's direction.

Results are strictly time-ordered. A reported event at `start_jde_tt` belongs
to the query; one at `end_jde_tt` does not. Canonical cells, analysis bands,
refinement and reporting do not depend on the caller's endpoints. Splitting
a successful query at a reported event assigns it to the right-hand query.
Unresolvable reported-time collisions throw rather than merge nearby events.

Stations are zeros of `ephemeris::longitude_rate`, the published seven-point
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
stations use the difference of the two published rates. A zero rate alone
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
The distinction between the smooth longitude derivative and the published
rate is bounded by `M7*h^6/140`, using a nonnegative Peano kernel; the
[derivation below](#seven-point-operator-and-remainder) gives both constants.
This truncation remainder is **not** the floating-point error of evaluating the
six-sample rate. Both terms are retained for contact classification.

Refinement requests a `2^-10`-day time certificate (84.375 seconds), then
reports on a nearest-even `2^-30`-day lattice. The fine lattice makes
half-open ownership deterministic; it does not claim microsecond physical
accuracy. A certified secant observation usually gives a substantially
better representative than the stopping criterion alone.

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
