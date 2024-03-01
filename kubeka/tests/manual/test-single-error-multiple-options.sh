#!/bin/bash

. tests/manual/tests.inc

rm -f vg.txt
$PROG \
   -f  tests/input/single-error-multiple-options-1.kubeka \
   -f  tests/input/single-error-multiple-options-2.kubeka \
   -j  single-error-multiple-options-1-2 \
   -j  single-error-multiple-options-2-2 \
   &> tests/output/single-error-multiple-options-1.output && failed

diff\
   tests/expected/single-error-multiple-options-1.output \
   tests/output/single-error-multiple-options-1.output || failed

passed

