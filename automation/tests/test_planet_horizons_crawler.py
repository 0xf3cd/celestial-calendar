# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import importlib.util
import sys

from pathlib import Path


MODULE_NAME = "planet_horizons_crawler_test_subject"
MODULE_PATH = Path(__file__).resolve().parents[2] / "statistics" / "planet_horizons_crawler.py"
SPEC = importlib.util.spec_from_file_location(MODULE_NAME, MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
CRAWLER = importlib.util.module_from_spec(SPEC)
sys.modules[MODULE_NAME] = CRAWLER
SPEC.loader.exec_module(CRAWLER)


class Response:
  text = "mock response"

  @staticmethod
  def raise_for_status() -> None:
    pass


def test_fetch_horizons_quantizes_requested_and_expected_epochs(monkeypatch):
  captured = {}

  def get(url, *, params, timeout):
    captured["url"] = url
    captured["params"] = params
    captured["timeout"] = timeout
    return Response()

  def parse(target, text, required_columns, expected_epochs):
    captured["target"] = target
    captured["text"] = text
    captured["required_columns"] = required_columns
    captured["expected_epochs"] = expected_epochs
    return ()

  monkeypatch.setattr(CRAWLER.requests, "get", get)
  monkeypatch.setattr(CRAWLER, "parse_horizons_response", parse)

  target = CRAWLER.TARGETS[0]
  epochs = (2460676.500800741, 2460677.123456789)
  assert CRAWLER.fetch_horizons(target, epochs, "31") == ()
  assert captured == {
    "url": CRAWLER.HORIZONS_URL,
    "params": CRAWLER.horizons_params(target, (2460676.500801, 2460677.123457), "31"),
    "timeout": 60,
    "target": target,
    "text": "mock response",
    "required_columns": ("ObsEcLon", "ObsEcLat"),
    "expected_epochs": (2460676.500801, 2460677.123457),
  }
