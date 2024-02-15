#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/xor-failure-3.kubeka && failed

passed

