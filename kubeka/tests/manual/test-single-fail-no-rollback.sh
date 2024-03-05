#!/bin/bash

. tests/manual/tests.inc


rm -f vg.txt
$PROG \
   -f  tests/input/single-fail-no-rollback.kubeka \
   -j  single-fail-no-rollback-2 \
   &> tests/output/single-fail-no-rollback.output && failed

diff\
   tests/expected/single-fail-no-rollback.output \
   tests/output/single-fail-no-rollback.output || failed

passed

