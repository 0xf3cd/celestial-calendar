# Workflow and Dependency Maintenance

## Claude Workflows

Manual reviews run from the default branch and read the requested PR through `gh pr view`
and `gh pr diff`; they do not check out PR-controlled startup configuration. Findings go
to a top-level PR comment. Dispatch requires repository write access and the default-branch
ref. Do not dispatch a modified workflow definition from an untrusted ref.

The OWNER-only mention path retains the action's tag mode, request context and actor-filtered
comments. Its appended instructions and explicit tool denials constrain it to reading and
commenting. Tag mode still checks out the target branch and restores selected configuration
files from the PR base. That restoration does not cover every imported instruction or hook
target. Neither tool restrictions nor prompt instructions constitute a sandbox.

Both workflows pin the action, but accept its live `https://claude.ai/install.sh` bootstrap
before the pinned CLI starts (#213). The installer can change without a repository diff and
runs with credentials available. Checkout credential persistence is disabled; the action still
acquires an App token, configures Git authentication and attempts token revocation afterward.
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
