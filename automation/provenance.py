# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import sys

from pathlib import Path

if __package__:
  from . import (
    astrotime_delta_t_provenance as astrotime,
    erfa_identity as erfa,
    hko_identity as hko,
    internal_provenance as internal,
    nasa_delta_t_provenance as nasa,
    retained_provenance as retained,
    sofa_identity as sofa,
    third_party_notices as notices,
    usno_identity as usno,
    vsop87d_import as vsop87d,
  )
  from .utils import green_print, red_print
else:
  import astrotime_delta_t_provenance as astrotime
  import erfa_identity as erfa
  import hko_identity as hko
  import internal_provenance as internal
  import nasa_delta_t_provenance as nasa
  import retained_provenance as retained
  import sofa_identity as sofa
  import third_party_notices as notices
  import usno_identity as usno
  import vsop87d_import as vsop87d
  from utils import green_print, red_print


REPO_ROOT = Path(__file__).resolve().parents[1]
VERIFIERS = (
  ("ERFA", lambda root: erfa.verify_erfa_identities(root, root / erfa.ERFA_ROOT.relative_to(erfa.REPO_ROOT))),
  ("SOFA", lambda root: sofa.verify_sofa_identities(root, root / sofa.SOFA_ROOT.relative_to(sofa.REPO_ROOT))),
  ("HKO", lambda root: hko.verify_hko_identity(root, root / hko.HKO_ROOT.relative_to(hko.REPO_ROOT))),
  ("USNO", lambda root: usno.verify_usno_identities(root, root / usno.USNO_ROOT.relative_to(usno.REPO_ROOT))),
  (
    "NASA Delta T",
    lambda root: nasa.verify_nasa_delta_t_provenance(root, root / nasa.NASA_ROOT.relative_to(nasa.REPO_ROOT)),
  ),
  (
    "AstroTime Delta T",
    lambda root: astrotime.verify_astrotime_delta_t_provenance(
      root, root / astrotime.ASTROTIME_ROOT.relative_to(astrotime.REPO_ROOT)
    ),
  ),
  ("Internal provenance", internal.verify_internal_provenance),
  ("VSOP87D", vsop87d.verify_repository),
  ("Retained sources and values", retained.verify_retained_provenance),
  ("Third-party notices", notices.verify_notices),
)


def check_provenance(repo_root: Path = REPO_ROOT) -> int:
  """Run the source-specific verifiers directly; retained-data checks are transitive."""
  for name, verify in VERIFIERS:
    try:
      verify(repo_root)
    except Exception as error:
      red_print(f"Provenance check failed [{name}]: {type(error).__name__}: {error}")
      return 1
    green_print(f"Provenance verified [{name}]")
  return 0


if __name__ == "__main__":
  sys.exit(check_provenance())
