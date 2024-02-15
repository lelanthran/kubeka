#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/circular-dependency.kubeka && failed

passed

