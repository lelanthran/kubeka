#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -p tests/input/broken && failed

passed
