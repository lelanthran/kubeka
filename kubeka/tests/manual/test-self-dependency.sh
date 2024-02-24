#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/self-dependency.kubeka && failed

passed

