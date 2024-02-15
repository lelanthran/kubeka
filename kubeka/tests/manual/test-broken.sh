#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -p tests/input/broken && failed

passed
