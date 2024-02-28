#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/unresolved-variable.kubeka && failed

passed

