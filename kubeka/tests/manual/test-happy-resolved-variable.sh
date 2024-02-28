#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/happy/happy-resolved-variable.kubeka || failed

passed

