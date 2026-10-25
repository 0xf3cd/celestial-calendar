# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Native error translation, count/fill queries and sized UTF-8 strings."""

from __future__ import annotations

import ctypes
import math
from collections.abc import Callable
from typing import TypeVar

from . import _abi, _binding
from ._codes import constant

_MAX_BUFFER_BYTES = 64 * 1024 * 1024


class CelestialError(RuntimeError):
  """Native computation/resource/internal failure or a malformed successful payload.

  operation identifies the Python operation. recorded is true when native reported
  an error kind, including an error whose message could not be allocated.
  """

  def __init__(self, operation: str, message: str, *, recorded: bool) -> None:
    super().__init__(message)
    self.operation = operation
    self.recorded = recorded


def malformed(operation: str, detail: str) -> CelestialError:
  return CelestialError(operation, f"Malformed native {operation}: {detail}", recorded=False)


def check_error(operation: str) -> None:
  kind = _binding.last_error_kind()
  if kind == constant("ERROR_NONE"):
    return
  detail = _binding.last_error_text() or f"{operation} failed (native error kind {kind})"
  if kind == constant("ERROR_INVALID_INPUT"):
    raise ValueError(detail)
  if kind not in {constant("ERROR_COMPUTATION"), constant("ERROR_RESOURCE"), constant("ERROR_INTERNAL")}:
    raise malformed(operation, f"unknown error kind {kind}")
  raise CelestialError(operation, detail, recorded=True)


def valid(result: object, operation: str) -> object:
  check_error(operation)
  if not getattr(result, "valid", False):
    raise malformed(operation, "invalid result without a native error")
  return result


_Record = TypeVar("_Record", bound=ctypes.Structure | ctypes.c_double)


def _count_fill_protocol(export: str, argument_count: int, operation: str) -> dict:
  """Require the generated policy to match this consumer's count/slots/capacity tail."""
  protocols = getattr(_abi, "PROTOCOLS", None)
  protocol = protocols.get(export) if isinstance(protocols, dict) else None
  parameters = _abi.C_PARAMETERS.get(export)
  roles = ("count_parameter", "slots_parameter", "capacity_parameter")
  policy_matches = (
    isinstance(protocol, dict)
    and protocol.get("kind") == "count-fill"
    and type(protocol.get("legal_empty")) is bool
    and protocol.get("total_must_match") is True
    and protocol.get("written_policy") == "min-total-capacity"
    and protocol.get("null_slots_requires_zero_capacity") is True
    and protocol.get("error_channel") == "kind"
  )
  if not policy_matches or parameters is None or len(parameters) != argument_count + 3:
    raise malformed(operation, f"invalid count/fill protocol for {export}")
  if tuple(protocol.get(role) for role in roles) != tuple(parameters[-3:]):
    raise malformed(operation, f"invalid count/fill parameter roles for {export}")
  return protocol


def count_fill(
  export: str,
  args: tuple[object, ...],
  record_type: type[_Record],
  operation: str,
  *,
  expected_count: int | None = None,
) -> ctypes.Array[_Record]:
  """Require a full fill under the generated export's legal-empty contract."""
  protocol = _count_fill_protocol(export, len(args), operation)
  allows_empty = protocol["legal_empty"]
  total = ctypes.c_uint32()
  written = _binding.call(export, *args, ctypes.byref(total), None, 0)
  check_error(operation)
  count = total.value
  if written != 0 or (not allows_empty and count == 0) or (expected_count is not None and count != expected_count):
    raise malformed(operation, "unexpected count-pass totals")
  if count * ctypes.sizeof(record_type) > _MAX_BUFFER_BYTES:
    raise CelestialError(operation, "native result buffer exceeds the binding's 64 MiB cap", recorded=False)
  try:
    records = (record_type * count)()
  except (MemoryError, OverflowError) as error:
    raise CelestialError(operation, "cannot allocate native result buffer", recorded=False) from error
  if count == 0:
    return records
  fill_total = ctypes.c_uint32()
  written = _binding.call(export, *args, ctypes.byref(fill_total), records, count)
  check_error(operation)
  if written != count or fill_total.value != count:
    raise malformed(operation, "changed total or incomplete fill")
  return records


def sized_string(export: str, args: tuple[object, ...], operation: str) -> str:
  """Read required bytes including NUL, then require an exact UTF-8 payload."""
  required = _binding.call(export, *args, None, 0)
  check_error(operation)
  if type(required) is not int or not 1 <= required <= 0xFFFFFFFF:
    raise malformed(operation, "invalid string size")
  if required > _MAX_BUFFER_BYTES:
    raise CelestialError(operation, "native text buffer exceeds the binding's 64 MiB cap", recorded=False)
  try:
    buffer = (ctypes.c_char * required)()
  except (MemoryError, OverflowError) as error:
    raise CelestialError(operation, "cannot allocate native text buffer", recorded=False) from error
  ctypes.memset(buffer, 0xFF, required)
  written = _binding.call(export, *args, buffer, required)
  check_error(operation)
  raw = bytes(buffer)
  if written != required or raw[-1:] != b"\0" or b"\0" in raw[:-1]:
    raise malformed(operation, "changed size or invalid NUL termination")
  try:
    return raw[:-1].decode("utf-8")
  except UnicodeDecodeError as error:
    raise malformed(operation, "invalid UTF-8") from error


_Decoded = TypeVar("_Decoded")


def decode(operation: str, decoder: Callable[..., _Decoded], *args: object) -> _Decoded:
  try:
    return decoder(*args)
  except (AttributeError, TypeError, ValueError, KeyError, OverflowError) as error:
    raise malformed(operation, str(error)) from error


def finite(value: float, name: str) -> float:
  if not math.isfinite(value):
    raise ValueError(f"{name} is not finite")
  return value


def longitude(value: float, name: str) -> float:
  result = finite(value, name)
  if not 0.0 <= result < 360.0:
    raise ValueError(f"{name} is outside [0, 360)")
  return result
