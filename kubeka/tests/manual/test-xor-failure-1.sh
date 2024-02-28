#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/xor-failure-1.kubeka && failed

passed

