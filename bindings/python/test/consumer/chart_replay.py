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


def assert_chart_replay(actual: dict, expected: dict) -> dict[str, float]:
  """Check every numeric field and its presence, returning maximum absolute residuals."""
  # Replay acceptance margins for the stored snapshot, not physical accuracy bounds.
  # See docs/VALIDATION.md#replay-margins.
  margins = expected["replay_margins"]
  residuals = {}

  def close(label: str, value: float, stored: float, margin: float, circular: bool = False) -> None:
    assert math.isfinite(value) and math.isfinite(stored), f"non-finite replay field: {label}"
    difference = abs(math.remainder(value - stored, 360.0) if circular else value - stored)
    assert difference <= margin, f"{label}: replay residual {difference:.17g} exceeds {margin:.17g}"
    group = label.split(".")[-1]
    residuals[group] = max(residuals.get(group, 0.0), difference)

  assert actual["times"].keys() == expected["times"].keys() == {"jd_ut1", "jde_tt"}
  for name, stored in expected["times"].items():
    close(f"times.{name}", actual["times"][name], stored, margins["time_days"])

  assert len(actual["bodies"]) == len(expected["bodies"]) == 14
  for index, (body, stored) in enumerate(zip(actual["bodies"], expected["bodies"], strict=True)):
    assert body.keys() == stored.keys()
    assert body["target"] == stored["target"], f"target order at {index}"
    for name in ("longitude_deg", "latitude_deg"):
      close(f"body{index}.{name}", body[name], stored[name], margins["position_deg"], name == "longitude_deg")
    close(
      f"body{index}.longitude_rate_deg_per_tt_day",
      body["longitude_rate_deg_per_tt_day"],
      stored["longitude_rate_deg_per_tt_day"],
      margins["rate_deg_per_tt_day"],
    )
    if stored["distance_au"] is None:
      assert body["distance_au"] is None, f"unexpected distance at {index}"
    else:
      assert body["distance_au"] is not None, f"missing distance at {index}"
      close(f"body{index}.distance_au", body["distance_au"], stored["distance_au"], margins["distance_au"])

  assert actual["houses"].keys() == expected["houses"].keys()
  for name in ("ascendant_deg", "midheaven_deg", "descendant_deg", "imum_coeli_deg"):
    close(f"houses.{name}", actual["houses"][name], expected["houses"][name], margins["house_deg"], True)
  assert len(actual["houses"]["cusps_deg"]) == len(expected["houses"]["cusps_deg"]) == 12
  for value, stored in zip(actual["houses"]["cusps_deg"], expected["houses"]["cusps_deg"], strict=True):
    close("houses.cusps_deg", value, stored, margins["house_deg"], True)
  return residuals
