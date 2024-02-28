#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/broken/unknown-node-type.kubeka && failed

passed

