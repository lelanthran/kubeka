#!/bin/bash

. tests/manual/tests.inc

rm -f vg.txt
$PROG \
   -f  tests/input/single-happy.kubeka \
   &> tests/output/options-xor-jd2.output && failed

diff\
   tests/expected/options-xor-jd2.output \
   tests/output/options-xor-jd2.output || failed

passed


