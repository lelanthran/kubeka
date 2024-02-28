#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/duplicates.kubeka && failed

passed


