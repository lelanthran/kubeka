#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/happy/happy-cline.kubeka || failed

passed

