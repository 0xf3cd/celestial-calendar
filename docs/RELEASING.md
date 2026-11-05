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
(`@0xf3cd/celestial`) and npm-alias (`celestial-calendar`) are separate from the PyPI homonym. PyPI publishes
`celestial-calendar` plus its same-version `cele` and `wentian` forwarding packages.

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

GitHub Release publishes first. Approve the `pypi` and `npm` jobs only after checking the candidate summary and
the [initial Python alias setup](#python-alias-setup), if applicable. The final unprivileged job requires four
native PyPI wheels, one pure `cele` wheel, one pure `wentian` wheel and two npm tarballs. It compares registry
hashes and bytes, clean-installs all seven Python package combinations on floor/current interpreters, and
installs npm-primary, npm-alias, and the npm pair from their registry.
The candidate has 38 files: 18 GitHub assets, six PyPI wheels, two npm tarballs and twelve evidence files
(37 files recorded in the manifest, excluding the manifest itself). The WASM archive has ten members:
the raw module/glue, LICENSE/notices, and both tarball/metadata/SHA-256 triples.

The [Python Wheels recipe](BUILDING.md#python-wheels) describes the producer artifacts and alias build.
The candidate keeps all six wheels flat in `pypi/`, with strict, separate native and alias inventories.
Pure aliases carry the project MIT license; native wheels retain the third-party notices too.

`publish_pypi` uploads the primary, waits for its exact registry bytes, then uploads `cele`.
The dependent `publish_pypi_wentian` job uses the same protected `pypi` environment. Before each upload,
fresh classification verifies every existing file against the candidate and stages only missing wheel bytes:

- `absent`: the version endpoint returned 404; stage the complete distribution inventory;
- `partial`: every existing file matches, but some native wheels are missing; stage only those missing files;
- `exact`: the complete distribution already matches; skip the upload as a verified no-op.

Conflicting or extra files, invalid metadata, HTTP 429, transport failures and a 404 for a listed file stop the
job. They never authorize upload. Alias preparation requires the primary version to be exact first. A retry
does not rebuild, repack or use `skip-existing`. PyPI JSON can lag an upload: a stale 404/subset may cause an
already-existing-file rejection, which fails closed and can be retried after indexing settles. A deleted PyPI
filename cannot be re-uploaded; that situation can require a new version.

The npm job checks out only the protected release commit and installs the hash-locked Requests closure to run
the versioned classifier. Immediately before each publication it rechecks that package against the same frozen
candidate, primary first and alias second. Exact bytes are a verified no-op; conflicts, invalid JSON, HTTP 429,
and transport failures stop rather than authorizing publication. Verification alone polls indexing and transient
transport states to a fixed ceiling and reports which npm or Python distribution is still pending.

<a id="registry-verification"></a>
### Registry verification

After success, inspect the immutable GitHub Release, asset inventory and `verify_registries` results.
Do not repeat those installations manually on the same bytes merely to obtain another green result:

| Check | Automatic execution | Manual responsibility |
|---|---|---|
| Native archive/runtime floor | Native producer validates installed archive members and measured requirements | Confirm selected run/commit and candidate hashes |
| Four repaired Python wheels | Producer installs each same wheel on minimum/current interpreters; release verifies registry bytes and installed Python acceptance | Read all four wheel identities and successful acceptance evidence |
| Two pure Python aliases | Producer checks exact bytes, resolver conflicts, object identity and installed mypy; release resolves all seven Python package combinations | Read both alias identities, same-version dependencies and consumer evidence |
| Primary and alias npm tarballs | WASM producer tests exact named tarballs; release verifies registry bytes and installs each name alone and the same-version pair | Read both frozen package identities and all three registry-consumer results |
| Authorization | Protected environments pause publication | Approve only the manifest's tag, commit, producer IDs, filenames, sizes and hashes |

An exceptional recovery that cannot reach `verify_registries` must perform its missing checks against the
original frozen candidate, as described below. A no-op publication proves byte identity, not a live OIDC publish.

Before publication, the package validators compare README bytes and documentation URLs against the independently
selected release commit. Installed-package checks use that same source to verify what consumers receive.

<a id="release-documentation"></a>
### Release documentation

After `verify_registries` succeeds, obtain separate documentation-deployment authorization and dispatch
`docs-publish.yml` from the protected default branch with `release_tag=vMAJOR.MINOR.PATCH`.
Keep the [Pages environment approval](MAINTAINING.md#docs-publishing) separate from package
publication. The publisher resolves the published immutable tag; a newer default-branch workflow must still
build that release's exact source and version.

Retain the same-run Pages artifact ID and schema-2 manifest. After deployment, check the release version and full
source SHA on guide and API pages, desktop/mobile navigation, and entry links from all five package names (Python
`celestial-calendar`, `cele`, `wentian`; npm `@0xf3cd/celestial`, `celestial-calendar`). Package documentation links
use SHA-pinned source guides, so historical packages do not follow the latest hosted release. Code/build checks
alone do not complete live-site acceptance.

## Recovery

- If publication reports `bootstrap_required`, use the [historical bootstrap appendix](#appendix-completed-npm-bootstraps)
  as context and obtain separate account/publication authorization; do not substitute newly built bytes.
- If immutable GitHub Release creation fails, inspect the release first. Delete it only if it is still a draft,
  then rerun failed jobs against the same workflow artifact.
- For a PyPI publication failure, inspect registry and account state, then retry only the affected job with
  `gh run rerun --job PYPI_JOB_ID` or `gh run rerun --job WENTIAN_JOB_ID`. Fresh classification verifies existing
  exact bytes and stages only missing files from the same candidate, including partial primary uploads.
- For npm publication failure, inspect the evidence and retry only the npm job using
  `gh run rerun --job NPM_JOB_ID`. Fresh classification retains successful exact package bytes and continues
  with the remaining package. This also handles a command that failed after npm accepted its exact bytes.
- The unprivileged `verify_registries` job is idempotent. A transient verification failure after publication may use
  `gh run rerun RUN_ID --failed`; the publication jobs have already succeeded and are not rerun.
- If a PyPI command failed after accepting its exact bytes, the affected-job retry is a verified no-op and lets
  downstream checks continue. Never use `skip-existing` or substitute a newly built candidate.
- After any irreversible job succeeds, never use "Re-run all jobs". The original run is the identity of the frozen
  candidate.

### Exceptional recovery: original-candidate verification

If the automatic consumer job cannot be recovered, verify and consume the same candidate from a clean checkout of
the release tag. Run the Python consumer on both the floor and current interpreters from `python-wheel.yml`;
the example below shows one interpreter. `RUN_ID` is the release workflow run, not a producer run:

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

  python3 bindings/python-aliases/test/consumer.py "$version" --registry
  node bindings/javascript/test/registry/registry_consumer_test.mjs "$version"
)
```

Rehearse environment self-approval and same-run failed-job artifact recovery after workflow changes and before the
final release tag. The instrument is `.github/workflows/release-rehearsal.yml`: create an immutable `vX.Y.Z-rehearsal`
tag through the tag-creation Admin bypass, then dispatch the workflow on it. Retain the tag after the rehearsal;
the `v*` immutability rules prohibit moving or deleting it, just like formal release tags. Never use a plain
`vX.Y.Z` for a rehearsal: it is release identity. The rehearsal spelling matches the environments' `v*.*.*`
deployment-tag policy. Approve both
environment gates against the staged manifest, then confirm the dependent `gate_pypi_second` job waits for a
separate `pypi` approval. Record the actual behaviour rather than assuming an approval is requested again.
Then rerun the deliberately failed job with
`gh run rerun RUN_ID --failed` and confirm it recovers the same-run artifact. The v0.6.0 npm no-op cannot rehearse a
live OIDC publish.

For another source commit, choose an unused `vX.Y.Z-rehearsal` name; never move an existing tag.
Workflow reruns on the same tag keep the same source commit.

<a id="one-time-setup"></a>
## One-Time Setup

Before creating a release tag:

1. Add active creation and immutability rulesets for `v*`. Test `github.ref_protected` with an unused
   `vX.Y.Z-rehearsal` tag and retain it; do not weaken update or deletion protection for cleanup.
2. Create `pypi` and `npm` environments. The maintainer is the required reviewer and may self-approve after reading
   the candidate manifest; disable self-review prevention and admin bypass. Restrict deployment tags to `v*.*.*`.
3. Register the primary PyPI Trusted Publisher with owner `0xf3cd`, repository `celestial-calendar`, workflow
   `release.yml`, and environment `pypi`. For new alias projects use the sequential setup below.
4. Register separate Trusted Publishers for both npm names with the same owner, repository and workflow,
   using environment `npm`. Package existence alone does not prove publisher configuration or live OIDC readiness.

Environment approval is a deliberate manifest check, not independent review.

<a id="python-alias-setup"></a>
### Initial Python alias setup

PyPI permits only one pending publisher per owner/repository/workflow/environment identity
([Warehouse #16920](https://github.com/pypi/warehouse/issues/16920)). A pending row does not reserve a project name.
Keep the existing normal primary publisher and use the same fields above for both aliases:

1. Add the `cele` pending publisher. Before approving `publish_pypi`, inspect that row in the maintainer's account,
   verify its name and fields, and confirm `cele` is still absent on PyPI. A stale pending name can also block the
   primary's token exchange because PyPI resolves pending publishers first.
2. After the first job succeeds, confirm `cele` has its exact wheel and a normal Trusted Publisher. Add `wentian`
   as the next pending publisher, verify its fields and still-absent name, then approve `publish_pypi_wentian`.
3. If GitHub reused the first environment approval and the second job already failed, retain the candidate, finish
   the pending setup and retry only `WENTIAN_JOB_ID`. The pre-tag rehearsal must record whether the second gate pauses.

PyPI can create the project while minting the short-lived upload token, before an upload succeeds. If that happens,
inspect the new project's normal publisher rather than adding another pending row for an existing project.
Neither source inspection nor a no-op proves a live OIDC publication or account ownership. Confirm those in the
actual run and project settings. There remains a name-registration race before each first upload; the shared
identity cannot queue both names at once. Later releases use the three normal project associations without this
pending setup, and require no API token.

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
