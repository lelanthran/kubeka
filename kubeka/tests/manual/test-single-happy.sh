#!/bin/bash

. tests/manual/tests.inc

rm -f vg.txt
$PROG \
   -f  tests/input/single-happy.kubeka \
   -j  single-happy-2 \
   &> tests/output/single-happy.output || failed

diff\
   tests/expected/single-happy.output \
   tests/output/single-happy.output || failed

passed

