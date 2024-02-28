#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/mangled-input.kubeka && failed

passed

