#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/mangled-input.kubeka && failed

passed

