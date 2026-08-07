# Agent Instructions — CelestialCalendar

> **This file is the single source of truth (SSOT) for agent instructions.**
> `CLAUDE.md` is only an import pointer to this file. If anything elsewhere
> (README, PR comments, habit) conflicts with this file, this file wins — or fix
> the file, don't fork the convention.

## Project Overview

**CelestialCalendar** — A C++23 astronomical-calculation library that converts among
Gregorian, Lunar, and Chinese Ganzhi calendars, and computes accurate Jieqi (节气)
moments, sun/moon positions, and sunrise/sunset. 天文计算与历法转换.

Repository: `https://github.com/0xf3cd/celestial-calendar`
License: GNU General Public License v3.0

This file tells any AI assistant (Claude, Kimi, Codex, Gemini, ...) how to write code
**in the author's style**, so contributions stay indistinguishable from hand-written
code. For *style* questions, read a neighbouring header and imitate it rather than
inventing a convention.

## Human-in-the-loop (read first)

Stop and confirm with the author before proceeding whenever anything is unclear or
non-obvious — an ambiguous requirement, a design fork with no precedent in the code, an
algorithm/reference you're unsure of, an unexpected test failure, or any change whose
intent you can't verify from existing code. Prefer asking over guessing.

## Correctness is numerical, measured against references

This is precision astronomy, not vibes. Every algorithm traces to a named reference
(Meeus, VSOP87D, ELP2000-82B, SOFA, USNO ΔT — see README §References).

- **Name cross-formula physical constants** as `constexpr` (UPPER_CASE) — e.g.
  `SIDEREAL_RATE_DEG_PER_DAY`, shared between implementation and tests. Single-use
  coefficients of a cited reference polynomial stay **inline in the formula** with the
  `@ref` alongside (like `obliquity::mean`, `gen_eval_θ`, the ΔT segments): naming each
  coefficient of a quoted equation adds indirection without audit value. Dense coefficient
  tables (e.g. `MEEUS_NUTATION_COEFFS`) keep raw literals with a source comment.
  `.clang-tidy` disables the magic-number checks for exactly these two cases.
- Never "simplify" a formula or drop a term for cleaner code — the constants ARE the algorithm.
- Correctness is proven by **golden datasets**: tests hold high-precision reference values
  and `ASSERT_NEAR` to a per-column tolerance. New numerics ⇒ add a reference dataset + tolerance.
- **Never loosen an existing tolerance on your own.** Before touching a tolerance: talk to the
  author, debug it carefully, and confirm case-by-case that it's an algorithm/reference issue,
  not a code bug. The default is to surface the discrepancy, not to make the test green.
- Cite the source of any new algorithm / coefficients in a comment.
- **Comments: sparse but load-bearing.** Every key spot gets one — segment boundaries, magic
  constants, bug fixes (with the issue number) — and stays at 1-2 lines of *why*. No restating
  the code, no quoting the old expression, no measurement reports (those live in the PR/issue).
  Re-read and slim your added comments before wrapping up; that pass is part of the change.
- **ΔT provenance lives out-of-repo**: `delta_t.hpp` algo5 (the default) is trained in
  [AstroTime-Analysis](https://github.com/0xf3cd/AstroTime-Analysis) (`DeltaT/algo5.py`): one
  polynomial fitted on Bulletin A *observations* 2004.85–2026.41, then the
  Stephenson–Morrison–Hohenkerk integrated-lod curve anchored at the last observation (no upper
  bound; below 2005.0 it delegates to algo2). Refresh ritual: run the AstroTime-Analysis
  downloaders, re-run `algo5.py`, update the coefficients + anchor here — coefficient updates
  stay inside algo5; a new algoN is only for a methodology change. algo1–algo4 are frozen
  exhibits and comparison baselines (algo4: `DeltaT/models.ipynb`, two segments, the 2024.0+
  one fitted on USNO *predictions*; its drift vs truth is measured in #104). In-repo
  `statistics/` holds the evaluation notebooks and the golden-dataset crawlers — no model
  training. That repo also carries the raw IERS/USNO EOP data and the full VSOP87 coefficient
  files (coefficient-level audits under #94).

## Tech Stack

- **C++23** — Core library (`src/`); clang++ (LLVM 18+) and g++ 14+ are supported.
- **CMake ≥ 3.22** — Build system.
- **Python 3** — Build/test/lint automation (`project.py`, `linter.py`, `automation/`,
  `toolbox/`). All build/lint/CI is Python-orchestrated.
- **GoogleTest** — Fetched at configure time by CMake (`src/test/CMakeLists.txt`).
- **clang-tidy** — C++ static analysis; **ruff** — Python linting/formatting.

## Build, Test, and Lint

Preferred entry point is `project.py`. Set `CXX` (and `CC` on Windows) to a C++23-capable
compiler before running.

```sh
# macOS / Linux example
export CXX=clang++

# Full setup + configure + build + test
./project.py --all

# Individual steps
./project.py --setup      # Install missing Python deps and verify toolchain
./project.py --cmake      # Run CMake configuration
./project.py --build      # Build shared library and tests
./project.py --test       # Run all tests
./project.py --test -k integration -v 1  # Filtered, verbose tests
./project.py --bench      # Build and run the benchmarks
./project.py --clean      # Remove build/ directory
```

Benchmarks are opt-in and `--all` leaves them out: their targets are `EXCLUDE_FROM_ALL`, so no
CI leg pays to compile them. They live in `src/bench/`, not `src/test/` — that directory turns
every `.cpp` into a GoogleTest target that runs at build time, and anything under `build/test/` is
taken to be a GoogleTest binary whose test count reconciles with the `TEST(` macros. Adding a
`src/bench/bench_*.cpp` is the whole job of adding a benchmark: the glob is `CONFIGURE_DEPENDS`
so a new file is picked up without a separate `--cmake`, and the runner finds the binary by
directory. `--bench` clears stale binaries first, so a renamed or deleted benchmark stops running.

A benchmark's absolute nanoseconds are never comparable across machines, and across runs only
when a round is long enough for its fixed cost to amortize away — `bench_jieqi.cpp` runs the cache
hit a second time at 24000 iterations for exactly that reason. Otherwise read the paired ratios
inside one run. `src/bench/harness.hpp` explains what it does about measurement bias and why (#81).

Windows PowerShell:

```powershell
$env:CXX = "clang++"
$env:CC  = "clang"
python3 ./project.py --all
```

### Test randomness (#69)

Randomized tests draw from a shared, seeded engine (`util/random.hpp`): default seed 42,
override with the `CELESTIAL_TEST_SEED` env var, effective seed printed once per test
process (`[ util::random ] seed = ...`). The PR/push gate pins the default;
`random_soak.yml` (weekly / dispatch) draws a fresh seed per run. A soak failure is never
a flake: replay the failing ctest entry with the same seed, then bake the find into a
directed regression test.

### Direct CMake (optional)

```sh
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build/test
```

Either path works since #72 added the top-level `enable_testing()`. Without it only the
subdirectory registered, so `ctest --test-dir build` ran **zero** tests and still exited 0 —
a green light that checked nothing, which is why that call is load-bearing.

### Lint

- `clang-tidy` runs with **`WarningsAsErrors: '*'`** — zero warnings. `.clang-tidy` is the
  config; respect its deliberate disables (magic-numbers, identifier-length, identifier-naming)
  instead of "fixing" code to satisfy a disabled check.
  Selected check suites: `clang-analyzer-*`, `performance-*`, `portability-*`, `bugprone-*`,
  `cppcoreguidelines-*`, `readability-*`, `google-*`, `modernize-*`.
- Python: ruff (`./linter.py --ruff` or `ruff check .` / `ruff format .`), config `.ruff.toml`
  (line length 120, 2-space indent, rules `E`/`F`/`B`/`Q`, double quotes).
- Run `./linter.py --all` to check both C++ and Python.

## Code Style

### Why this style exists

规则背后的三条脑内声音，规则冲突时按此裁决：
1. **"C++ 就该这么用"** — const、编译期、concept 是默认态，不是洁癖。
2. **"代码读起来像公式"** — 实现忠于文献原文；audit 价值永远高于代码整洁。
3. **"这里要一点浪漫"** — λ/β、UDL、phi 这类笔触是项目的签名，禁止 bland 化。

分工：转录/脚手架/CI 可放手；API 形状、类型设计、数值核心、容差判断留给作者。

### Naming

Nothing enforces this — `readability-identifier-naming` is deliberately off (#72). The convention
lives here:

`lower_case` — variables, functions, parameters, members, methods, namespaces, and any
`const inline auto` that is called rather than read.
`CamelCase` — classes, structs, **and enum types** (`Algo`, `AngleUnit`, `Jieqi`).
`UPPER_CASE` — `inline constexpr` constants and enumerators.

**Domain notation overrides all of it.** An identifier that stands for a symbol in the source keeps
the source's spelling: `cos_λ`, `argL`, `A1`, `θCoeffs`, `gen_eval_θ`, and `Jieqi`'s 立春/冬至.
Same discipline as the `@ref` rule above — the code should read next to the book.

### Comment language

Narrative in English; domain terms keep their own spelling (`Jieqi::冬至`), glossed once in
parentheses where a reader who has only the English needs a handle — `the Chinese Jieqi (节气)`,
`a Jie (节)`. Translating an entity loses it. Outside the lunar subtree, keep new full-Chinese runs
to named or quoted material rather than explanation; the stray restatement that predates this
paragraph can stay where it is.

The lunar subtree is where Chinese restatement is concentrated, and stays that way: its Doxygen
carries a Chinese line after or under the English, on `@brief` / `@param` / `@return` / `@note`
and member docs alike (`@param year The Lunar year. 阴历年份。`). Match the neighbours when
editing there — do not spread the habit outward, do not strip it inside.

Punctuation follows the language of the run it sits in: full-width inside a Chinese phrase,
half-width everywhere else.

### Header-only, and the code reads like the maths

Header-only is a **deliberate design choice** — the no-link, orthogonal, self-contained feel
is intentional. Keep it. That buys a discipline:

- All logic in `src/**/*.hpp` with `#pragma once`; every function `inline`, shared constants
  `inline constexpr`. `.cpp` exists only in `shared_lib/` (C-ABI exports), `test/` and `bench/` (entry points).
  New logic → a header.
  `inline` on namespace-scope constants is not cosmetic: plain `constexpr` has internal linkage,
  so an external-linkage inline entity that odr-uses it (binds a reference, takes a span,
  subscripts an array, range-for's over it) is IFNDR (#71).
- **Every header compiles alone**, and CI enforces it per-header with `-fsyntax-only`.
  Self-containment is a property of the *standard library*, not of the code: a header that
  leans on a transitive include passes on the implementation that happens to provide it and
  fails on one that doesn't. `converter.hpp` used `assert` with no `<cassert>` and compiled
  fine on MSVC STL for months while failing outright on libc++ (#71). **The gate therefore
  has to run on more than one leg — a single green platform proves nothing.**
- **No `using namespace` and no namespace-scope `using` declarations in headers** — they leak
  into every including TU and cause conflicts / ambiguity. Put `using` inside function bodies
  (as `sun.hpp` already does with `using namespace astro::toolbox::literals;`) or fully-qualify.
  Closed-set forms that *are* allowed: `using X = Y` type aliases, and using-declarations that
  import a named template from another namespace (see `datetime.hpp` for the chrono set —
  class templates for CTAD, plus matching shape for stdlib alias templates). Test-only headers
  may still carry function-body or file-local usings (e.g. `delta_t_test_helper.hpp`).
- **Nested `lower_case` namespaces** by domain (`astro::earth::nutation`,
  `lib::`, `calendar::`); close with a `} // namespace …` comment.
- **Trailing return types**: `inline auto f(const double jde) -> SphericalCoordinate`.
- **Unicode math identifiers are deliberate** (`λ`, `β`, `Δψ`, `Ω`, `Mp`). Name symbols as the
  papers do; do NOT ASCII-ise them.
- **Strong physical types** over bare doubles (`Angle<RAD>`, `Distance<AU>`, `SphericalCoordinate`)
  with designated initializers (`.λ = …`). Keep units in the type system.
- `const`-correct; modern C++ (`<ranges>`, `<span>`, `std::array`).
- 2-space indentation. Compiler flags: `-Wall -Wextra -Werror -Wpedantic -Wnull-dereference
  -Wunreachable-code`; the optimization level is `CMAKE_BUILD_TYPE`'s (Release = `-O3 -DNDEBUG`).
- **Multi-line call layout**: when an argument is itself a call, or the call grows long, put
  each argument on its own line and the closing `);` on its own line at the call's
  indentation — matching the existing `upper_bound` / `Datetime`-construction sites:
  ```cpp
  calendar::add_seconds(
    tt_dt,
    -tt_minus_utc(tt_dt.ymd)
  );
  ```
  Trivial short calls stay on one line.
- Doxygen `/** @brief / @param / @return / @note / @ref */` on public functions; inline `//`
  comments explain the physics, not the syntax — with order-of-magnitude arguments
  ("ΔT ≈ 69 s ≈ 0.29° of rotation"), not bare claims ("this is accurate"). `@ref` cites the
  exact formula/table number of the named reference.

### Modern C++ posture

- **Immutability by default**: locals are `const`; value types (`Angle`, `Distance`) are
  immutable — operations return new values. No setters anywhere.
- **Composition over inheritance** — in fact, no inheritance in the library at all: structs
  are plain data carriers; "polymorphism" is values + table lookup
  (`nutation::Model` → `find_model` → `std::span`), never vtables.
- **Free functions over classes**: algorithms are `inline` free functions in nested
  namespaces; classes/structs exist only to carry data.
- **Declarative over imperative**: ranges pipelines (`| std::views::transform` → `std::reduce`)
  describe *what*, not *how*.
- **Functional core**: algorithms are pure functions — no global or mutable shared state,
  same inputs ⇒ same outputs (this is what makes golden-dataset testing meaningful).
  Higher-order functions where natural (`gen_eval_θ(jc)` returns the θ-evaluator closure);
  map/fold pipelines over manual loops; data and behavior stay separate (plain structs +
  free functions over them); enum + table lookup is the sum-type idiom (`nutation::Model`
  → `find_model`), not vtables.
- **Compile-time over runtime**: `constexpr` wherever the language allows (but don't force it —
  `std::cos` / `std::pow` / `std::remainder` are only constexpr from C++26); constrain
  templates with `requires` clauses, never SFINAE.
- **Names as contracts**: parameter names carry semantics that make misuse visible at the
  call site — e.g. the `jd_ut1` / `jde_tt` suffixes are the UT1/TT guard (issue #41).
- **Suppressions carry reasons**: every `NOLINT` / `NOLINTNEXTLINE` has a trailing comment
  explaining why the warning does not apply here.

### Small functions, honest errors, no premature abstraction

- **Functions stay short and single-purpose** — compose thin layers that each do one thing
  (`greenwich_mean` → `greenwich_apparent` → `local_apparent`). The doc comment is routinely
  longer than the function body: the physics and the citation are the hard part, the code is
  the easy part. If a function needs a long body, split it before commenting around it.
- **Fail fast with informative errors**: throw with context-rich messages
  (`throw std::out_of_range { std::vformat("Year {} is not supported …", …) }`), never
  silently clamp, swallow, or return a sentinel.
- **KISS / minimal abstraction**: three similar lines beat a premature abstraction. Generalize
  when a second or third real caller demands it, not in anticipation.

### File header

Every file opens with the full GPL-3.0 block comment (copy from any src file), verbatim except
the year: `Copyright (C) <year> Ningqi Wang (0xf3cd)`. New files use the current year; never
change an existing file's year — except a wholesale rewrite, which may set a
`<original>-<current>` range (user-directed, e.g. `util/random.hpp` 2024-2026). GPL-3.0
project — keep the header.

## Tests

- GoogleTest `TEST(Suite, Case)` in `src/test/**`, mirroring `src/**`
  (`astro/earth.hpp` → `test/astro/earth_test.cpp`), namespace `…::test`.
- **Tests are auto-discovered:** `src/test/CMakeLists.txt` globs every `*.cpp` under `src/test/`
  into a separate executable. Discovery timeout is 180s to accommodate slow Docker/ARM CI runners.
- Data-driven: inline a column-aligned dataset of reference values (whitespace-padded,
  right-aligned columns like `earth_test.cpp`); `ASSERT_NEAR` with a per-column tolerance;
  header-comment the columns. Alignment is computed **per column over the whole block**:
  every row pads each column (including the input/JDE column) to that column's widest
  cell, so the same field sits at the same character column on every row. Never pad
  per-row — regenerating or editing rows must re-align the entire block.
- **Provenance is mandatory**: every golden dataset states where its values came from —
  book example number (tests mirror worked examples value-by-value, e.g. Meeus Example 12.a),
  or the external source + collection date + generation seed (e.g. "USNO API, 2026-07-19,
  seed 42") — plus the rationale for the tolerance (measured margin, model gap). A dataset
  must be regenerable and auditable by a reader.
- Dataset **input columns must reproduce the reference source's actual inputs** — including
  any quantization the source applied (e.g. USNO evaluating LAST at 4-decimal longitudes).
  Otherwise tolerances silently absorb the mismatch and the dataset loses discriminating power.

### Acceptance: run the binaries, and reconcile the count

**Do not accept on ctest's numbers.** ctest reports on what it was told about, and that
registration goes stale in both directions: `ctest --test-dir build` finds *zero* tests and
reports success (the registry lives in `build/test`), and a `--build --test` run after adding a
source file silently omits it, because the `GLOB` that discovers tests expands at configure time.

Run the binaries directly, then reconcile the total against the `TEST` macros:

```sh
total=0; for exe in build/test/*; do [ -x "$exe" ] && [ -f "$exe" ] || continue
  out=$("$exe" 2>&1) || echo "FAIL $exe"
  n=$(echo "$out" | grep -oE "^\[==========\] [0-9]+ test" | grep -oE "[0-9]+" | head -1)
  total=$((total + ${n:-0})); done
echo "$total"; grep -rhoE '^\s*TEST(_F)?\(' src/test --include=*.cpp | wc -l   # must be equal
```

**The reconciliation is the load-bearing half.** Running the binaries proves that what ran
passed; only the count proves that everything that should have run *did*. A stale binary skews
it in the greener direction — the one nobody goes looking for — which is why the build now
clears `build/test/` before re-creating it (#155).

## Project Layout

```
src/
  astro/        Astronomical algorithms (VSOP87D, ELP2000-82B, Sun, Moon, ΔT, Julian Day, ...)
  bench/        Benchmarks, built only by `--bench` (see “Build, Test, and Lint”)
  calendar/     Calendar logic: datetime, lunar conversion algorithms, Jieqi
  shared_lib/   C++ shared-library wrapper over core algorithms
  test/         GoogleTest-based tests (auto-discovered by CMake)
  util/         Utility headers (hash, cache, random, YMD, ...)
automation/     Python modules used by project.py and linter.py
toolbox/        Helper scripts for artifacts, releases, build info
```

## Project-Specific Rules and Gotchas

1. **Single source of truth:** this `AGENTS.md` is the only agent-instruction file with
   content; `CLAUDE.md` just imports it. Don't create new per-tool instruction files —
   extend this one.
2. **C++23 features — "not yet", not "never":** basic C++20 ranges/views are in active use
   (`std::views::transform` etc.). Features the weakest CI toolchain doesn't support yet —
   **modules**, `std::generator`, and C++23 ranges additions like `std::views::enumerate` /
   `pairwise` — wait for compiler support, then get adopted (README §7 tracks the wishlist).
   **Availability is settled by compiling a real use of the feature, never by reading a
   feature-test macro** (#131: libc++ ships `std::ranges::fold_left` without defining
   `__cpp_lib_ranges_fold`). `./linter.py --features LEG` holds each CI leg to the state in
   `automation/feature_probe.py`; an unlock fails CI, and names the waiting sites it can see --
   which means the ones tagged `TODO` with the feature's name, not untagged hand-rolling.
3. **Shared library target:** `src/shared_lib/CMakeLists.txt` builds `libcelestial_calendar`
   from `lib*.cpp`. Version is injected via the `BUILD_VERSION` environment variable
   (defaults to `0.0.0`).
4. **CI produces cross-platform artifacts:** GitHub Actions builds on macOS, Windows, and
   two Linux architectures (x86_64 and arm64), each in Docker on a *native* runner — the
   8-platform QEMU matrix was retired in 2026-07 (#46). Do not change compiler or Docker
   base images without checking matrix impact.
5. **Sensitive files:** Do not read or surface `.env`, `credentials.json`, or any file
   containing tokens/keys.
6. **`build/` is gitignored.** Generated artifacts and `compile_commands.json` live there;
   do not commit them.
7. **Use `project.py` for routine work.** It handles environment checks, version propagation,
   and test filtering consistently across platforms.
8. **Assertions:** the default build is `Release` (`automation/build.py`), so `assert` is dead
   code in production. Test targets strip `NDEBUG` (`src/test/CMakeLists.txt`, #89), so asserts
   ARE live inside test binaries — `build_integrity_test.cpp` guards this. The error contract
   (target state; #77, #67 and #64 are closed, #70 is the last one open): public input
   validation must `throw` (fail-fast, independent of build type); `assert` is only
   for internal invariants, never as an input guard.

9. **CI toolchains are pinned — every one that can emit a diagnostic.** A tool that updates
   itself turns "the code changed" and "the tool changed" into the same red X, on a day nobody
   touched the repository; a pin that goes stale instead fails loudly with "no such version",
   which says what to do. Pinned: clang-tidy and clang at **18** on the Linux legs, choco LLVM at
   **20** on Windows (not the 18 the Linux legs use — a pre-existing split these pins record
   rather than create), the Xcode **major** (a major carries an Apple Clang major, and `-Werror`
   makes any new diagnostic in it a red build), and ruff. Exact versions live in the workflows,
   not here — there is no gate reconciling two copies. Chocolatey's `make` is deliberately outside
   this: it drives the build rather than diagnosing it, so a new version cannot turn `-Werror` red.
   Bump deliberately, never incidentally (#72, #73).

## AI do / don't

- DON'T round / drop astronomical constants or loosen tolerances to pass CI (see above).
- DON'T ASCII-ise unicode identifiers, move logic out of headers, or add namespace-scope
  `using` to a header.
- DON'T add a dependency or build step outside the `project.py` / `linter.py` flow.
- Match the neighbouring header's texture; internal consistency > external "best practice".

## Common Commands Reference

| Task | Command |
|------|---------|
| Full setup + build + test | `./project.py --all` |
| Configure only | `./project.py --cmake` |
| Build only | `./project.py --build` |
| Run tests, verbose | `./project.py --test -v 1` |
| Run benchmarks | `./project.py --bench` |
| Filtered tests | `./project.py --test -k <keyword>` |
| Clean | `./project.py --clean` |
| Python lint/format | `./linter.py --ruff` |
| C++ lint | `./linter.py --clang-tidy` |
| Show version | `./project.py --version` |
