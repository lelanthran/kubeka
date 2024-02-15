#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/missing-reference.kubeka && failed

passed

