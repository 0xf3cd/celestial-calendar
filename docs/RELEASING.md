<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions between
    Gregorian and Chinese Lunar calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  SPDX-License-Identifier: MIT
-->

# Releasing CelestialCalendar

The release workflow freezes one candidate for GitHub Release, PyPI, and npm. It accepts three explicit producer
run IDs and rejects a tag, commit, run, artifact, or registry version that does not match exactly. npm-primary
(`@0xf3cd/celestial`) and npm-alias (`celestial-calendar`) are separate from the PyPI homonym.

## Release

The [one-time settings](#one-time-setup) must already be in place. Pending source work is not a release:
final API freeze, candidate selection and authorization are separate acceptance steps.

1. Prepare the version in a pull request merged before the tag:
   - set `BUILD_VERSION` in `project.py`; the Python and npm package versions derive from it, so do not edit
     `pyproject.toml` or the `package.json` files;
   - replace `docs/RELEASE_NOTES.md` with the notes of this release only;
   - finalize the first version heading and entries in `docs/CHANGELOG.md`, removing its unreleased disclaimer
     only when the release is authorized. Keep pending entries undated; historical dates come from tagged commits;
   - check historical "new in" notes and source-versus-published documentation warnings. The READMEs describe
     the current source without repeating a global version number.

   Then confirm `project.py`, `docs/RELEASE_NOTES.md`, and the first `docs/CHANGELOG.md` entry all name the same
   version.
2. Create and push the protected `vMAJOR.MINOR.PATCH` tag from a commit already contained in `main`.
3. Dispatch `build_and_test.yml`, `wasm.yml`, and `python-wheel.yml` on that tag. Wait for all three to succeed and
   record their run IDs. Each must be a `workflow_dispatch` run at the tagged commit.
4. Finish the remaining steps within the producer artifacts' 30-day retention window.

Dispatch each producer once:

```sh
tag=vMAJOR.MINOR.PATCH
commit=$(git rev-parse "$tag^{commit}")
gh workflow run build_and_test.yml --ref "$tag"
gh workflow run wasm.yml --ref "$tag"
gh workflow run python-wheel.yml --ref "$tag"
```

After they finish, list the dispatched runs at that exact commit. Record the `databaseId` for one successful run of
each workflow, then inspect each selected ID before continuing:

```sh
tag=vMAJOR.MINOR.PATCH
commit=$(git rev-parse "$tag^{commit}")
for workflow in build_and_test.yml wasm.yml python-wheel.yml; do
  gh run list --workflow "$workflow" --event workflow_dispatch --commit "$commit" \
    --json databaseId,workflowName,event,headSha,status,conclusion,url
done
gh run view RUN_ID --json databaseId,workflowName,event,headSha,status,conclusion,url
```

The selected runs must report `workflow_dispatch`, the value of `$commit`, `completed`, and `success`. A displayed
run number is not a run ID; use `databaseId` in the release inputs.

### Frozen Candidate

Dispatch the release workflow on the tag with the three recorded IDs:

```sh
gh workflow run release.yml --ref vMAJOR.MINOR.PATCH \
  -f native_run_id=<native-run-id> \
  -f wasm_run_id=<wasm-run-id> \
  -f python_run_id=<python-run-id>
```

Preparation validates the protected tag, main ancestry, producer runs, artifact API digests, archive contents,
and documentation. It then stages one candidate and classifies both npm identities:

- an absent version in an existing, correctly identified package requires OIDC publication;
- an absent version whose package also returns 404 records `bootstrap_required`;
- a byte-identical version is a verified no-op;
- any metadata, integrity, or byte mismatch stops before GitHub Release creation.

GitHub Release publishes first. Approve the `pypi` and `npm` jobs only after checking the candidate summary. The
final unprivileged job requires exactly four PyPI wheels and two npm tarballs, compares registry hashes and bytes,
and clean-installs PyPI plus npm-primary, npm-alias, and the npm pair from their respective registries.
The candidate has 30 files: 14 GitHub assets, four PyPI wheels, two npm tarballs and ten evidence files
(29 files recorded in the manifest, excluding the manifest itself). The WASM archive has ten members:
the raw module/glue, LICENSE/notices, and both tarball/metadata/SHA-256 triples.

The npm job checks out only the protected release commit and installs the hash-locked Requests closure to run
the versioned classifier. Immediately before each publication it rechecks that package against the same frozen
candidate, primary first and alias second. Exact bytes are a verified no-op; conflicts, invalid JSON, HTTP 429,
and transport failures stop rather than authorizing publication. Verification alone polls indexing and transient
transport states to a fixed ceiling and reports which of npm-primary, npm-alias, or PyPI is still pending.

After success, inspect the immutable GitHub Release, asset inventory and `verify_registries` results.
Do not repeat those installations manually on the same bytes merely to obtain another green result:

| Check | Automatic execution | Manual responsibility |
|---|---|---|
| Native archive/runtime floor | Native producer validates installed archive members and measured requirements | Confirm selected run/commit and candidate hashes |
| Four repaired Python wheels | Producer installs each same wheel on minimum/current interpreters; release verifies registry bytes and installed Python acceptance | Read all four wheel identities and successful acceptance evidence |
| Primary and alias npm tarballs | WASM producer tests exact named tarballs; release verifies registry bytes and installs each name alone and the same-version pair | Read both frozen package identities and all three registry-consumer results |
| Authorization | Protected environments pause publication | Approve only the manifest's tag, commit, producer IDs, filenames, sizes and hashes |

An exceptional recovery that cannot reach `verify_registries` must perform its missing checks against the
original frozen candidate, as described below. A no-op publication proves byte identity, not a live OIDC publish.

## Recovery

- If publication reports `bootstrap_required`, use the [historical bootstrap appendix](#appendix-completed-npm-bootstraps)
  as context and obtain separate account/publication authorization; do not substitute newly built bytes.
- If immutable GitHub Release creation fails, inspect the release first. Delete it only if it is still a draft,
  then rerun failed jobs against the same workflow artifact.
- An unambiguous PyPI failure before registry acceptance may use `gh run rerun RUN_ID --failed` after reviewing
  the evidence.
- For npm publication failure, inspect the evidence and retry only the npm job using
  `gh run rerun --job NPM_JOB_ID`. Fresh classification retains successful exact package bytes and continues
  with the remaining package. This also handles a command that failed after npm accepted its exact bytes.
- The unprivileged `verify_registries` job is idempotent. A transient verification failure after publication may use
  `gh run rerun RUN_ID --failed`; the publication jobs have already succeeded and are not rerun.
- If a PyPI publish command fails ambiguously but registry queries prove the exact candidate is present, leave that
  publication job red. Do not rerun it or use `skip-existing`; record the recovery and complete consumer validation
  manually.
- After any irreversible job succeeds, never use "Re-run all jobs". The original run is the identity of the frozen
  candidate.

### Ambiguous PyPI success: original-candidate verification

For the terminal PyPI ambiguous-success case, verify and consume the same candidate manually from a clean checkout of
the release tag. `RUN_ID` is the release workflow run, not a producer run:

```sh
(
  set -euo pipefail
  tag=vMAJOR.MINOR.PATCH
  version=${tag#v}
  commit=$(git rev-parse "$tag^{commit}")
  test ! -e candidate
  gh run download RUN_ID --name celestial-release-candidate --dir candidate

  python3 -m venv registry-verify
  registry-verify/bin/python -m pip install --require-hashes --only-binary=:all: -r Requirements-producer.txt
  registry-verify/bin/python toolbox/registry_verifier.py verify \
    --candidate candidate --version "$version" --commit "$commit"

  python3 -m venv registry-python
  registry-python/bin/python -m pip --isolated install \
    --index-url https://pypi.org/simple --only-binary=:all: --no-cache-dir --no-deps \
    "celestial-calendar==$version"
  root=$(pwd)
  work=$(mktemp -d)
  (cd "$work" && "$root/registry-python/bin/python" "$root/bindings/python/test/run_all.py")
  node bindings/javascript/test/registry/registry_consumer_test.mjs "$version"
)
```

Rehearse environment self-approval and same-run failed-job artifact recovery after workflow changes and before the
final release tag. The instrument is `.github/workflows/release-rehearsal.yml`: push a disposable `vX.Y.Z-rehearsal`
tag — never a plain `vX.Y.Z`, which is release identity — (the tag rulesets cover it: create via the Admin bypass,
remove afterwards via the ruleset recovery path recorded in #215), dispatch the workflow on that tag, approve both
environment gates against the staged manifest, then rerun the deliberately failed job with
`gh run rerun RUN_ID --failed` and confirm it recovers the same-run artifact. The v0.6.0 npm no-op cannot rehearse a
live OIDC publish.

<a id="one-time-setup"></a>
## One-Time Setup

Before creating a release tag:

1. Add an active tag ruleset for `v*`. Test `github.ref_protected` with a disposable matching tag, then remove it.
2. Create `pypi` and `npm` environments. The maintainer is the required reviewer and may self-approve after reading
   the candidate manifest; disable self-review prevention and admin bypass. Restrict deployment tags to `v*.*.*`.
3. Register the PyPI pending publisher with owner `0xf3cd`, repository `celestial-calendar`, workflow
   `release.yml`, and environment `pypi`.
4. Register separate Trusted Publishers for both npm names with the same owner, repository and workflow,
   using environment `npm`. Package existence alone does not prove publisher configuration or live OIDC readiness.

Environment approval is a deliberate manifest check, not independent review.

<a id="appendix-completed-npm-bootstraps"></a>
## Appendix: completed npm bootstraps

### Historical npm v0.6.0 Bootstrap

npm cannot register a Trusted Publisher before a package exists. The primary bootstrap selected the sole
`@0xf3cd/celestial@0.6.0` tarball named by `npm-pack.json` from the selected `celestial-wasm` run,
verified `npm-pack.sha256`, and published those exact bytes with `--access public --ignore-scripts`.
The same WASM run ID supplied the release workflow. No repack or glob selection was permitted.
The short-lived credential was revoked before registering the Trusted Publisher; credentials must never
appear in commands, shell history, output or the repository.

v0.6.0 has no OIDC-generated provenance. Its later byte-verified no-op proves package identity;
the first live primary OIDC publication is v0.6.1.

### Alias v0.7.0 Bootstrap

The alias bootstrap followed npm-primary 0.7.0 and required separate account/publication authorization.
It used the original release run's `celestial-release-candidate`, selecting exactly one plain
`celestial-calendar@0.7.0` filename from `candidate/evidence/npm-alias-pack.json` and checking the
tarball in `candidate/npm` against `candidate/evidence/npm-alias-pack.sha256`. Both frozen tarballs
stayed unchanged. Publication used `--access public --ignore-scripts`; the short-lived credential
was revoked immediately without printing or storing it in the repository.

After registering the alias Trusted Publisher for this repository's `release.yml` and `npm` environment,
only the original run's npm job was retried. Fresh classification verified both packages and skipped
exact versions. That no-op proves bytes, not live alias OIDC publication; account readiness and the
first live alias OIDC publication require separate evidence.
