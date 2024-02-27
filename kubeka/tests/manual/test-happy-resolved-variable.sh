#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/happy/happy-resolved-variable.kubeka || failed

passed

