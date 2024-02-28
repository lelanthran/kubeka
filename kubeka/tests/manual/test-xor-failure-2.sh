#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/xor-failure-2.kubeka && failed

passed

