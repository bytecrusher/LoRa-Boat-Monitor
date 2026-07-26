#!/usr/bin/env sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEST_BINARY="${TMPDIR:-/tmp}/loraboatmonitor-version-test"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -I"${PROJECT_DIR}/include" \
  "${PROJECT_DIR}/src/versionComparator.cpp" \
  "${PROJECT_DIR}/tests/versionComparator_test.cpp" \
  -o "${TEST_BINARY}"
"${TEST_BINARY}"
