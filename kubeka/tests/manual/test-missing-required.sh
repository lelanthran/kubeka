#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/missing-required.kubeka && failed

passed

