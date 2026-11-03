<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v1.1.1]

### Python aliases

- Install `cele` or `wentian` from PyPI and use `import cele` or `import wentian`. Both pure-Python wheels depend on
  the exact same-version `celestial-calendar` package and expose its public top-level functions, types and exceptions.
- The three names can coexist and use one native implementation. Existing `import celestial_calendar` and its
  submodule imports remain supported; the aliases provide top-level imports only.
- Future releases publish the four platform wheels and both aliases from one tested candidate.

### Release recovery

- PyPI publication proceeds primary first, then `cele`, then `wentian`. A retry verifies already-published bytes and
  uploads only missing candidate wheels, including interrupted subsets of the four platform wheels.
- The initial alias setup uses sequential Trusted Publishers and protected publication jobs, accommodating PyPI's
  one-pending-publisher constraint for a shared workflow identity.

### Compatibility

The original Python package, both npm names, C/C++ APIs, platform minimums and astronomical calculations are unchanged.
This release does not redeploy the hosted documentation.

The [changelog](CHANGELOG.md) retains the full development history.
