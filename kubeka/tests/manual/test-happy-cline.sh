#!/bin/bash

. tests/manual/tests.inc

make debug && $PROG \
   -f tests/input/happy/happy-cline.kubeka || failed

passed

