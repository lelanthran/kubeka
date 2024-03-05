#!/bin/bash

. tests/manual/tests.inc

rm -f vg.txt
$PROG \
   -f  tests/input/single-fail-rollback-failure.kubeka \
   -j  single-fail-rollback-failure-2 \
   &> tests/output/single-fail-rollback-failure.output && failed

diff\
   tests/expected/single-fail-rollback-failure.output \
   tests/output/single-fail-rollback-failure.output || failed

passed

