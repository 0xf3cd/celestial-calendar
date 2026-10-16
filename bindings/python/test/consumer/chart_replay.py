# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Shared chart-example replay checks; no package or native-library import."""

from __future__ import annotations

import math


# Replay acceptance margins for the stored chart snapshot, not ephemeris accuracy bounds. They are the thresholds
# the snapshot test used when the example was added for #300 (9a7a692); #331 moved them here unchanged and made the
# longitude, axis and cusp comparisons circular. The measured wheel-golden margins in golden_replay.py (#217) are
# separate; see docs/VALIDATION.md#replay-margins.
TIME_MARGIN_DAYS = 1e-9
POSITION_MARGIN_DEG = 1e-8
DISTANCE_MARGIN_AU = 1e-10
RATE_MARGIN_DEG_PER_TT_DAY = 1e-9
HOUSE_MARGIN_DEG = 1e-5


def assert_chart_replay(actual: dict, expected: dict) -> dict[str, float]:
  """Check every numeric field and its presence, returning maximum absolute residuals."""
  residuals = {}

  def close(label: str, value: float, stored: float, margin: float, circular: bool = False) -> None:
    assert math.isfinite(value) and math.isfinite(stored), f"non-finite replay field: {label}"
    difference = abs(math.remainder(value - stored, 360.0) if circular else value - stored)
    assert difference <= margin, f"{label}: replay residual {difference:.17g} exceeds {margin:.17g}"
    group = label.split(".")[-1]
    residuals[group] = max(residuals.get(group, 0.0), difference)

  assert actual["times"].keys() == expected["times"].keys() == {"jd_ut1", "jde_tt"}
  for name, stored in expected["times"].items():
    close(f"times.{name}", actual["times"][name], stored, TIME_MARGIN_DAYS)

  assert len(actual["bodies"]) == len(expected["bodies"]) == 14
  for index, (body, stored) in enumerate(zip(actual["bodies"], expected["bodies"], strict=True)):
    assert body.keys() == stored.keys()
    assert body["target"] == stored["target"], f"target order at {index}"
    for name in ("longitude_deg", "latitude_deg"):
      close(f"body{index}.{name}", body[name], stored[name], POSITION_MARGIN_DEG, name == "longitude_deg")
    close(
      f"body{index}.longitude_rate_deg_per_tt_day",
      body["longitude_rate_deg_per_tt_day"],
      stored["longitude_rate_deg_per_tt_day"],
      RATE_MARGIN_DEG_PER_TT_DAY,
    )
    if stored["distance_au"] is None:
      assert body["distance_au"] is None, f"unexpected distance at {index}"
    else:
      assert body["distance_au"] is not None, f"missing distance at {index}"
      close(f"body{index}.distance_au", body["distance_au"], stored["distance_au"], DISTANCE_MARGIN_AU)

  assert actual["houses"].keys() == expected["houses"].keys()
  for name in ("ascendant_deg", "midheaven_deg", "descendant_deg", "imum_coeli_deg"):
    close(f"houses.{name}", actual["houses"][name], expected["houses"][name], HOUSE_MARGIN_DEG, True)
  assert len(actual["houses"]["cusps_deg"]) == len(expected["houses"]["cusps_deg"]) == 12
  for value, stored in zip(actual["houses"]["cusps_deg"], expected["houses"]["cusps_deg"], strict=True):
    close("houses.cusps_deg", value, stored, HOUSE_MARGIN_DEG, True)
  return residuals
