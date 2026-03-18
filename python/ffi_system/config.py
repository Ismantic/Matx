import os
import sys

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILD_DIR = os.path.join(BASE_DIR, "build")
SRC_DIR = os.path.join(BASE_DIR, "src")
LIBCASE_PATH = os.path.join(BUILD_DIR, "libcase.so")
CASE_EXT_PATH = os.path.join(BASE_DIR, "case_ext.so")

PYTHON_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if PYTHON_DIR not in sys.path:
    sys.path.insert(0, PYTHON_DIR)
