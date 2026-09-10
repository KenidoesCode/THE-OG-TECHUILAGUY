#!/bin/bash
set -e

cd "$(dirname "$0")/.."

make clean
make

test -f techuilaguy-os

echo "OS BUILD TEST: PASS"
