# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

# This file regenerates the FK5-correction dataset in src/test/astro/sun_test.cpp (#68):
# it evaluates Meeus (25.9) with mpmath at 50-digit precision,
#   λ' = λ − (1.397 + 0.00031·T)·T                          [deg; T = Julian century from J2000.0]
#   Δλ = −0.09033 + 0.03916·(cos λ' + sin λ')·tan β         [arcsec]
#   Δβ = +0.03916·(cos λ' − sin λ')                         [arcsec]
# so the dataset is an independent transcription check of `fk5_correction` (sun.hpp).
# The λ/β inputs come from the library's own `sun::geocentric_coord::vsop87d`, whose
# downstream apparent() pipeline is validated against JPL DE441 (sun_horizons_golden_test.cpp).
# The 137 epochs live in fk5_correction_epochs.txt (next to this script), so the flow is
# one-way regenerable: epochs -> probe -> dataset rows.
#
# Regeneration flow (run from the repo root):
#   1. `python3 statistics/fk5_correction_dataset.py jdes > jdes.txt`
#   2. Build this one-off header-only probe and run it on the JDE list (stdin -> TSV):
#
#        // probe.cpp
#        #include <cstdio>
#        #include "sun.hpp"
#        int main() {
#          namespace sg = astro::sun::geocentric_coord;
#          double jde;
#          while (std::scanf("%lf", &jde) == 1) {
#            const auto coord = sg::vsop87d(jde);
#            const auto corr = sg::fk5_correction(jde, coord);
#            std::printf("%.17g %.17g %.17g %.17g %.17g\n",
#                        jde, coord.λ.deg(), coord.β.deg(), corr.Δλ.deg(), corr.Δβ.deg());
#          }
#        }
#
#        clang++ -std=c++23 -O2 -I src/astro -I src/calendar -I src/util probe.cpp -o probe
#        ./probe < jdes.txt > probe.tsv
#
#   3. `python3 statistics/fk5_correction_dataset.py gen probe.tsv` — emits the aligned C++
#      dataset rows (stdout) and the audit report (stderr): mpmath version, row count, and
#      the measured mpmath-vs-C++ double-rounding gaps. The test's tolerance rationale lives
#      next to the tolerance in the test file; the chosen 1e-15 deg sits ~1e5x above the
#      measured gaps, so any transcription slip (~1e-5 deg scale) is caught by ~10 orders.
# Dependency: mpmath (the used version is printed to stderr with each run).
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar


import sys
from pathlib import Path

from mpmath import __version__ as MPMATH_VERSION
from mpmath import cos, mp, mpf, radians, sin, tan

DPS = 50
J2000 = mpf("2451545.0")
DAYS_PER_CENTURY = mpf(36525)
EPOCHS_FILE = Path(__file__).resolve().with_name("fk5_correction_epochs.txt")


def load_epochs() -> list[str]:
  """Read the epoch list (one JDE literal per line; '#' comments and blanks skipped)."""
  lines = EPOCHS_FILE.read_text(encoding="utf-8").splitlines()
  return [stripped for line in lines if (stripped := line.strip()) and not stripped.startswith("#")]


def fk5_meeus_25_9(jde: str, lam_deg: str, beta_deg: str) -> tuple[mpf, mpf]:
  """Evaluate Meeus (25.9) at 50-digit precision (self-contained via `workdps`).
  String inputs preserve the probe's %.17g double round-trip; the residual
  decimal-vs-binary gap is ~1e-17 relative, far below the test tolerance.
  Angles are in degrees, returned corrections in degrees."""
  with mp.workdps(DPS):
    t = (mpf(jde) - J2000) / DAYS_PER_CENTURY
    lam_dash = mpf(lam_deg) - (mpf("1.397") + mpf("0.00031") * t) * t
    lam_dash_rad = radians(lam_dash)
    beta_rad = radians(mpf(beta_deg))

    d_lam_arcsec = mpf("-0.09033") + mpf("0.03916") * (cos(lam_dash_rad) + sin(lam_dash_rad)) * tan(beta_rad)
    d_beta_arcsec = mpf("0.03916") * (cos(lam_dash_rad) - sin(lam_dash_rad))
    return d_lam_arcsec / 3600, d_beta_arcsec / 3600


def emit_rows(probe_tsv: str) -> None:
  """Emit the aligned C++ dataset rows (stdout) and the audit report (stderr)."""
  epochs = load_epochs()

  max_gap_lam = mpf(0)
  max_gap_beta = mpf(0)
  rows = []
  with open(probe_tsv, encoding="utf-8") as fh:
    for line in fh:
      jde, lam, beta, cpp_lam, cpp_beta = line.split()
      d_lam, d_beta = fk5_meeus_25_9(jde, lam, beta)
      # Parsed at the default 53-bit precision on purpose: mpf() then lands exactly on the
      # C++ double, so the gap measures the formula's double-rounding, not print residue.
      max_gap_lam = max(max_gap_lam, abs(d_lam - mpf(cpp_lam)))
      max_gap_beta = max(max_gap_beta, abs(d_beta - mpf(cpp_beta)))
      # %.17g family: round-trip the double, with the repo's two-digit exponent style.
      rows.append((jde, f"{float(d_lam):.17g}", f"{float(d_beta):.17g}"))

  probe_jdes = [r[0] for r in rows]
  if len(rows) != len(epochs):
    sys.exit(f"epoch/probe row count mismatch: {len(epochs)} vs {len(rows)}")
  for epoch, probe_jde in zip(epochs, probe_jdes, strict=True):
    if float(epoch) != float(probe_jde):
      sys.exit(f"epoch/probe JDE mismatch: {epoch} vs {probe_jde}")

  # Per-column alignment over the whole block (AGENTS.md): JDE column keeps the original
  # literals from the epochs file (e.g. 2451545.0), not the probe's %.17g reprints.
  rows = [(epoch, lam, beta) for (epoch, (_, lam, beta)) in zip(epochs, rows, strict=True)]
  w_jde = max(len(r[0]) for r in rows)
  w_lam = max(len(r[1]) for r in rows)
  w_beta = max(len(r[2]) for r in rows)

  print(f"    // {'JDE':<{w_jde}}   {'Longitude Delta':<{w_lam}}  Latitude Delta")
  for jde, lam, beta in rows:
    print(f"    {{ {jde:>{w_jde}}, {{ {lam:>{w_lam}}, {beta:>{w_beta}} }} }},")

  print(f"mpmath {MPMATH_VERSION}, {len(rows)} rows", file=sys.stderr)
  print(f"max |d_lam gap|  = {mp.nstr(max_gap_lam, 5)} deg", file=sys.stderr)
  print(f"max |d_beta gap| = {mp.nstr(max_gap_beta, 5)} deg", file=sys.stderr)


def main() -> None:
  if len(sys.argv) == 2 and sys.argv[1] == "jdes":
    print("\n".join(load_epochs()))
    return
  if len(sys.argv) == 3 and sys.argv[1] == "gen":
    emit_rows(sys.argv[2])
    return
  sys.exit("usage: fk5_correction_dataset.py jdes | gen PROBE_TSV")


if __name__ == "__main__":
  main()
