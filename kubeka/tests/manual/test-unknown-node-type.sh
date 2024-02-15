#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/broken/unknown-node-type.kubeka && failed

passed

