#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/unresolved-variable.kubeka && failed

passed

