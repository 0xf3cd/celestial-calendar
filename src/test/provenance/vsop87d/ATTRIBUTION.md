# VSOP87D Planetary-Series Attribution

- Source: Bretagnon and Francou's January 1996 IMCCE distribution, CDS VI/81.
- Upstream: `https://ftp.imcce.fr/pub/ephem/planets/vsop87/`.
- Applies to: the seven complete Mercury-Neptune coefficient headers under
  `src/astro/vsop87d/`, the 80 official VSOP87D position rows in
  `src/test/astro/vsop87d_check_data.hpp`, and any binary that includes those coefficient headers.
- Transformation: `automation/vsop87d_import.py` preserves the published phase and frequency
  spellings and shifts the amplitude decimal point by eight places, matching
  `astro::vsop87d::SCALING_FACTOR` without a binary floating-point conversion.
- Replay: supply the seven official files and `vsop87.chk` to the importer. The raw upstream files
  are not retained in this repository; routine builds and CI remain offline.

| Body | Source | Source SHA-256 | Series | Terms | Generated SHA-256 |
|---|---|---|---:|---:|---|
| Mercury | `VSOP87D.mer` | `f468481b5a05080a943ad4746ff7ea7e0ff6652b71a46d83c9c636cb69485e34` | 18 | 6,827 | `09e745e40b5cf3d9ad68d1715c3952c21153d1fbc8e05f1b4570252448439fcc` |
| Venus | `VSOP87D.ven` | `cb2f3a738289ed45f69fec1845e480baf4b32d481eccc21b8629a2d0d10e8261` | 18 | 1,682 | `acd5bdc121fca49c8aeedec17888207ec7eb341b14bc75102454576089b3e65a` |
| Mars | `VSOP87D.mar` | `b1184df9553d85ffcf904c16bd437ab668804fa98859f27fe2e7bf6cfa6bc07e` | 18 | 5,483 | `b590c01ecd2f132fb59033fb632e7533ec61624e301544f4c51b2f00faa74754` |
| Jupiter | `VSOP87D.jup` | `3f3dfbc7d117ecad2b2dadf2fc626b260a3cd5efa98e7d4c6b26cd682fc48090` | 18 | 3,483 | `3aacf1d698d3502bf35b9c4fa4099ac0f255bfbef26d2d14ea0d5ba01b33e14f` |
| Saturn | `VSOP87D.sat` | `2e49e19396f24c17298f0b667e7763ee5c28b60d549c89d72a17dfd5f8d46b05` | 18 | 5,759 | `ef0a66af47b52915daa32383e0dcc34ced959308c56024bb557f18d82831a317` |
| Uranus | `VSOP87D.ura` | `80eb3a778d53f450066d9b13f17e15b979e253437d22f33ec5a4604cdb2872a7` | 16 | 3,989 | `e5a67f411b593a1e139e1f3b35bff4608d1ead90e7f52f8ac1e8a875e1ba7c20` |
| Neptune | `VSOP87D.nep` | `3ff65a5cabc04c411f975888f77268b89e336ece0b75d198a3935090fdde3ae6` | 17 | 1,929 | `ec12eadf6b0dbe0bf906d67ca766d8a6daafd17fb2976fa1ed577b5c825df28b` |
| Official checks | `vsop87.chk` | `f8fa52449262be05a22a96840c1acbad0b35c8999e00b5c0477ba8a91a67a51a` | 80 blocks | 80 positions | `8bdedcccb48e6d0dcbd2d3e18430738d6ef4a45295e9149be53c72b93017308f` |

- Terms fact: no redistribution grant was located; the located scientific-context wording does
  not establish package redistribution permission.
- Disposition: retained under owner risk acceptance, outside the project MIT grant. Permission is
  not claimed.

This project-authored record supplies attribution and known facts only; it is not upstream licence
text or a permission grant.
