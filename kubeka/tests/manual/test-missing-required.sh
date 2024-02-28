#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/missing-required.kubeka && failed

passed

