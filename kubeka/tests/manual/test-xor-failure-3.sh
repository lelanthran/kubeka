#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/xor-failure-3.kubeka && failed

passed

