#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/self-dependency.kubeka && failed

passed

