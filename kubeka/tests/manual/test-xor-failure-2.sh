#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/xor-failure-2.kubeka && failed

passed

