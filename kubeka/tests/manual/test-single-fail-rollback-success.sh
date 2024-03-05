#!/bin/bash

. tests/manual/tests.inc

rm -f vg.txt
$PROG \
   -f  tests/input/single-fail-rollback-success.kubeka \
   -j  single-fail-rollback-success-2 \
   &> tests/output/single-fail-rollback-success.output && failed

diff\
   tests/expected/single-fail-rollback-success.output \
   tests/output/single-fail-rollback-success.output || failed

passed

failed

