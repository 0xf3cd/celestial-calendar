# Lunar-Node Oracle Attribution

- Applies to: the 11 composite mean-ascending and true-ascending lunar-node rows in
  `src/test/astro/lunar_node_test.cpp`.
- Raw node source: AA+ v2.63 from SwiftAA commit
  `e70ab8d0a9a5c949eef00071bacfe3ca5731300b` (`AAMoon.cpp`). AA+ permits binary use but
  restricts source redistribution; no AA+ source is copied into the runtime or test.
- Frame-correction source: canonical SOFA release 2023-10-11 `iauNut80`. Its retained source and
  licence are already recorded under `src/test/provenance/sofa/2023-10-11/` and
  `THIRD_PARTY_NOTICES.txt`.
- Corroboration only: corrected PyMeeus commit
  `c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92`; no PyMeeus output supplies a retained column.
- Reproduction: evaluate AA+ mean and five-term true node at each recorded JDE(TT), call unchanged
  SOFA `iauNut80(2451545.0, jde_tt - 2451545.0, ...)`, add its longitude correction, and normalize
  to `[0 deg, 360 deg)`.
- Boundary: the retained numerical rows remain under their source terms and outside the project MIT
  grant. Runtime `lunar_node.hpp` is an independent implementation of the cited printed equations.
