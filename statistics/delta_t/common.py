import sys
import ctypes
from pathlib import Path


def dynamic_lib_ext() -> str:
  '''Return the library extension for the current platform.'''
  if sys.platform == 'win32':
    return '.dll'
  elif sys.platform == 'darwin':
    return '.dylib'
  else:
    return '.so'


# Define constants for paths.
PROJ_PATH        = Path(__file__).parent.parent.parent
USNO_DATA_PATH   = Path(__file__).parent / 'usno_data.txt'
BINDINGS_PATH    = PROJ_PATH / 'build' / 'python_bindings'
DELTA_T_LIB_PATH = BINDINGS_PATH / ('libdeltat' + dynamic_lib_ext())

assert PROJ_PATH.exists(),        f"Project path not found: {PROJ_PATH}"
assert USNO_DATA_PATH.exists(),   f"USNO data not found: {USNO_DATA_PATH}"
assert BINDINGS_PATH.exists(),    f"Bindings path not found: {BINDINGS_PATH}"
assert DELTA_T_LIB_PATH.exists(), f"Delta T library not found: {DELTA_T_LIB_PATH}"


# Define the argument and return types of the C functions.
DELTA_T_LIB = ctypes.CDLL(DELTA_T_LIB_PATH)

DELTA_T_LIB.delta_t_algo1.argtypes = [ctypes.c_double]
DELTA_T_LIB.delta_t_algo1.restype = ctypes.c_double

DELTA_T_LIB.delta_t_algo2.argtypes = [ctypes.c_double]
DELTA_T_LIB.delta_t_algo2.restype = ctypes.c_double

DELTA_T_LIB.delta_t_algo3.argtypes = [ctypes.c_double]
DELTA_T_LIB.delta_t_algo3.restype = ctypes.c_double

DELTA_T_LIB.delta_t_algo4.argtypes = [ctypes.c_double]
DELTA_T_LIB.delta_t_algo4.restype = ctypes.c_double


# Wrap C functions with Python functions, so that they can be called from Python.
def delta_t_algo1(year: float) -> float:
  if year < -4000:
    raise ValueError(f"Year {year} is not supported by algorithm 1.")
  return DELTA_T_LIB.delta_t_algo1(year)

def delta_t_algo2(year: float) -> float:
  return DELTA_T_LIB.delta_t_algo2(year)

def delta_t_algo3(year: float) -> float:
  if year >= 3000:
    raise ValueError(f"Year {year} is not supported by algorithm 3.")
  return DELTA_T_LIB.delta_t_algo3(year)

def delta_t_algo4(year: float) -> float:
  if year >= 2035:
    raise ValueError(f"Year {year} is not supported by algorithm 4.")
  return DELTA_T_LIB.delta_t_algo4(year)
