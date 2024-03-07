#!/bin/bash

. tests/manual/tests.inc

rm -f vg.txt
$PROG \
   -f  tests/input/single-happy.kubeka \
   -j  single-happy-2 \
   -d \
   &> tests/output/options-xor-jd1.output && failed

diff\
   tests/expected/options-xor-jd1.output \
   tests/output/options-xor-jd1.output || failed

passed


