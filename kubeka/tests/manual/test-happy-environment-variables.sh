#!/bin/bash

. tests/manual/tests.inc

$PROG \
   -f tests/input/happy/happy-environment-variables.kubeka || failed

passed

