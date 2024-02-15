#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/duplicates.kubeka && failed

passed


