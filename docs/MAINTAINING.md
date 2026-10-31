# Workflow and Dependency Maintenance

<a id="ci-jobs"></a>
## CI Jobs and Local Commands

`core_tests.yml` runs on every push and on pull requests from forks. `build_and_test.yml`, `python-wheel.yml` and
`wasm.yml` run on pull requests to `main` and on manual dispatch; `random_soak.yml` runs weekly. Run the local
commands from the source root in the build virtual environment. Workflow files and local composite actions own
the tool versions; the Windows STL exception is described under [Toolchain Feature Tracking](#todo).

| Workflow · job | What CI runs | Local command |
|---|---|---|
| `core_tests.yml` · `api-docs` | `project.py --docs` and `--site`; uploads `celestial-api-html` and `celestial-docs-site` separately | The same commands ([Documentation](BUILDING.md#api-docs)) |
| `core_tests.yml` · `linters-and-static-analysis` | `checks.py --ai-workflows`, `--action-pins`, `--pytest`, `--provenance`, `--signature-layout`; then `project.py --clean --cmake` and `checks.py --ruff --clang-tidy --abi-layout` | The same commands ([Static Checks](BUILDING.md#lint)); the workflow pins the Ruff version |
| `core_tests.yml` · `test-ubuntu-gcc14`, `test-ubuntu-clang` | The local `native-build` action runs `project.py --clean --cmake --cores all --build --test -v 1`; the clang job then runs `checks.py --ctypes-smoke --jieqi-table` and `--export-surface` | The same build command with `CXX=g++-14` or `CXX=clang++`, then the same `checks.py` flags, which need that build |
| `core_tests.yml` · `sanitizers` | An ASan + UBSan build, then every binary in `build/test/` run directly, with the test count reconciled against the `TEST` / `TEST_F` macros | Build with the job's `CXXFLAGS` and `python project.py --clean --cmake --build`, then run the reconciliation in [`AGENTS.md`](../AGENTS.md) ("Acceptance: run the binaries, and reconcile the count") |
| `core_tests.yml` · `self-contained-headers` (libstdc++, libc++, MSVC STL) | `checks.py --self-contained` | `python checks.py --self-contained`, for the local standard library only |
| `core_tests.yml` · `cxx-feature-probe` (same three) | `checks.py --features <leg>` | `python checks.py --features` ([Toolchain Feature Tracking](#todo)) |
| `build_and_test.yml` · `linux-docker` (amd64, arm64) | The `Dockerfile` build and test in a pinned manylinux image, then install-layout checks | CI only |
| `build_and_test.yml` · `macos`, `windows` | `project.py --clean --cmake --cores all --build --test -v 1`, then `cmake --install` with library-name and SOVERSION checks; Windows also enforces its link evidence | Build and test: `python project.py --all`. Install and link-evidence checks: CI only |
| `python-wheel.yml` · `manylinux` (x86_64, aarch64), `macos-arm64`, `windows-amd64` | `cibuildwheel` with the workflow's settings, an inspection of the repaired wheel, then `bindings/python/test/run_all.py` against the installed wheel from an unrelated directory; the manylinux x86_64 job also runs `bindings/python/test/types/consumer.py` | Release wheels: CI only ([Python Wheels](BUILDING.md#python-wheels)). To rerun the tests, install a wheel from [CI artifacts](#artifacts), then run `run_all.py`, and `types/consumer.py` with `bindings/python/requirements-mypy.txt` installed, from a directory outside the checkout |
| `python-wheel.yml` · `macos-15-floor` | The macOS arm64 wheel from the same run, installed and tested on macOS 15 | CI only |
| `wasm.yml` · `wasm` | The four build commands, Node package and type tests, Chrome and Firefox smoke tests, then Node consumers on the minimum supported version | The build and test commands in [WebAssembly and npm Packages](BUILDING.md#wasm) |
| `wasm.yml` · `node-consumer` (Windows, macOS) | The pair of packages from the same run, installed and tested without another build | CI only |
| `random_soak.yml` (three soak jobs) | The same `native-build` action with the run ID as `CELESTIAL_TEST_SEED`, then a direct RNG-using binary invocation that checks its reported seed | `CELESTIAL_TEST_SEED=<run ID> python project.py --test` replays a failure |
| `random_soak.yml` · `weekly-cxx-feature-probe` (three platforms) | `checks.py --features <leg>` on weekly and manual runs, in addition to the per-PR probes | `python checks.py --features` |
| `release.yml`, `release-rehearsal.yml`, `claude.yml`, `claude-review.yml` | Release and review automation | CI only; releases follow [`RELEASING.md`](RELEASING.md) |

<a id="docs-publishing"></a>
## Documentation publishing

`docs-publish.yml` is manual only. A metadata-only job rejects dispatches outside the protected default
branch before source checkout. The read-only build checks out the dispatched SHA, builds and verifies the
development site, then uploads one fixed-name immutable Pages artifact. The deploy job has only Pages/OIDC
write permissions and runs the pinned official deploy action, with no checkout or repository commands.
The action selects exactly one `github-pages` artifact from that workflow run and deploys its immutable ID.

The workflow file does not configure Pages or environment protection. Before enabling the Actions Pages
source or dispatching this workflow, separately authorise and complete these operations in order:

1. Configure and API-verify required reviewers and a default-branch deployment restriction on the
   `github-pages` environment.
2. Verify the protected default branch and the intended workflow/source SHA.
3. Enable the custom-Actions Pages source, then dispatch from that branch.
4. Retain the actual artifact ID, manifest and member digests; check the deployed revision, external links
   and desktop/mobile navigation before promoting its URL in repository or package metadata.

No live site, environment protection or deployment is established by adding the YAML file. Hosted acceptance
and URL promotion remain separate from the code/build change.

<a id="artifacts"></a>
## CI Artifact Downloads

This section is for contributors and for maintainers who run CI. Users should take the
[public release downloads](BUILDING.md#releases), which need no token. In GitHub, open the native,
[WASM](https://github.com/0xf3cd/celestial-calendar/actions/workflows/wasm.yml) or
[Python wheel](https://github.com/0xf3cd/celestial-calendar/actions/workflows/python-wheel.yml) workflow you need,
and download the artifacts from a completed run that built the target commit.

For automated downloads, use `toolbox/artifact_downloader.py`. From the source root, in an environment with the
`Requirements.txt` dependencies installed, supply GitHub authentication through the `GITHUB_TOKEN` environment
variable and run:

```sh
python3 toolbox/artifact_downloader.py -s build/artifacts --unzip
python3 toolbox/artifact_downloader.py --help
```

By default it selects successful `workflow_dispatch` runs that built the local checkout's HEAD and validates
their complete native, WASM and Python artifact inventories. PR and push runs are not eligible for that lookup.
To select the three producers explicitly, supply `--native-run-id`, `--wasm-run-id` and `--python-run-id` together.
These IDs must also identify successful `workflow_dispatch` runs of the expected workflows built from HEAD.
Alternatively, `-id` downloads a single run without release-inventory validation; it can also download PR runs.
`-s` is required and names the destination directory; without `--unzip` the archives stay zipped. `--help`
documents the remaining options. The tool can also be called as `python3 -m toolbox.artifact_downloader`.

<a id="todo"></a>
## Toolchain Feature Tracking

The code waits for full toolchain support before it adopts:

- modules;
- ranges extensions such as `std::views::enumerate` / `pairwise`;
- `std::generator` in the moon_phase / jieqi Newton iterations.

`./checks.py --features` checks these features by actually compiling them. CI runs it against libstdc++, libc++
and the MSVC STL separately, and reports when a toolchain already supports a feature while the code still keeps
its hand-written replacement, so this list does not go stale.

Windows uses the hosted runner's MSVC STL with the pinned LLVM compiler. The STL is not separately exact-pinned;
CI reports compiler versions and capability results; native link evidence records the selected toolchain and
libraries. Header or capability regressions still fail their gates. macOS compiling jobs select Xcode 26.

## Claude Workflows

Manual reviews run from the default branch and read the requested PR through `gh pr view`
and `gh pr diff`; they do not check out PR-controlled startup configuration. Findings go
to a top-level PR comment. Dispatch requires repository write access and the default-branch
ref; other refs are skipped. Do not dispatch a modified workflow definition from an untrusted ref.

The OWNER-only mention path retains the action's tag mode, request context and actor-filtered
comments. Its appended instructions and explicit tool denials constrain it to reading and
commenting. Tag mode still checks out the target branch and restores selected configuration
files from the PR base. That restoration does not cover every imported instruction or hook
target. Neither tool restrictions nor prompt instructions constitute a sandbox.

Both workflows pin the action, but accept its live `https://claude.ai/install.sh` bootstrap
before the pinned CLI starts (#213). The installer can change without a repository diff and
runs with credentials available. Checkout credential persistence is disabled; the action still
writes its App token into the origin URL in `.git/config` and attempts token revocation afterward.
Workflow permissions alone do not describe the App token's effective permissions.

Re-evaluate this boundary when the installer origin or integrity mechanism changes, the
action/CLI changes configuration loading, triggers or permissions expand, or an upstream
security incident affects these paths. Do not treat an action SHA as an installer-byte pin.

## Dependency Updates

Dependabot checks GitHub Actions, JavaScript development dependencies and the root/Python-wheel
pip manifests monthly in groups. Review updates before merging. Inline tool and runtime pins
remain deliberate workflow maintenance; a pip/npm manifest scan does not cover them.

`Requirements-statistics.txt` is excluded from scheduled version updates: the analysis
environment has no compatibility baseline in this maintenance scope. This exclusion does not
disable security alerts or promise exclusion from security updates.

Keep the existing producer and wheel `.in` inputs, hashed `.txt` closures and bootstrap
constraints consistent. Their headers contain the regeneration commands; existing dependency
tests check the result. A bot PR that updates only part of a closure needs that regeneration,
not relaxed assertions. General development requirements remain version-pinned without hashes.

GitHub-owned actions may retain major tags outside the credential-sensitive AI/release paths;
third-party actions require full SHAs. Python's macOS producer retains `macos-latest`, with a
pinned Xcode major/deployment target and a separate floor consumer. Runner-image drift remains
a risk, not a fixed defect. Before adding a self-hosted macOS producer, filter serial numbers
and hardware UUIDs from its build metadata.
