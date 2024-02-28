#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/circular-dependency.kubeka && failed

passed

