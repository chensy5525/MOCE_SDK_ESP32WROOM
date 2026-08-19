#!/usr/bin/env bash
set -e

SDK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJECT_DIR=${1:-"$SDK_ROOT/examples_direct/tb6612_fixed_duty_test"}

source "$SDK_ROOT/env/export.sh"

cd "$PROJECT_DIR"

idf.py fullclean
