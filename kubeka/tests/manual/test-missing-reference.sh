#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/missing-reference.kubeka && failed

passed

