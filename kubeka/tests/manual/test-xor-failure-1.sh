#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/xor-failure-1.kubeka && failed

passed

