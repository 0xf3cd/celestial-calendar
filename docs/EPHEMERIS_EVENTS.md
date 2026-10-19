# Longitude events

Include `astro/ephemeris_events.hpp` for interval queries over the 14
`astro::ephemeris::Target` values. Times are JDE(TT), longitudes are apparent,
geocentric and tropical, and rates are degrees per TT day.

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

Stations are zeros of the published seven-point rate with `h=1/8` TT day,
not an analytic derivative or a minimum angular separation. The reported
rate need not be bitwise zero. Descending nodes use their actual antipodal
positions, while their rates retain the ascending model's trajectory.

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
The constructed query can also throw when a two-sided contact region cannot
be certified, including a stationary time exactly on a canonical cell or
provider-domain boundary. Reusing a station's longitude does not bypass
the completeness checks.
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
fit; the remaining domain uses the full-window bounds. Band selection is
cell-based, so widening a query cannot change a station's definition.

The rate derivative bounds use the exact `23/15` integral-kernel norm.
The distinction between the smooth longitude derivative and the published
rate is bounded by `M7*h^6/140`, using a one-sign Peano kernel. This
truncation remainder is **not** the floating-point error of evaluating the
six-sample rate. Both terms are retained for contact classification.

Refinement requests a `2^-10`-day time certificate (84.375 seconds), then
reports on a nearest-even `2^-30`-day lattice. The fine lattice makes
half-open ownership deterministic; it does not claim microsecond physical
accuracy. A certified secant observation usually gives a substantially
better representative than the stopping criterion alone.
