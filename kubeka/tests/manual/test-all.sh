#!/bin/bash

. tests/manual/tests.inc

export TESTSCRIPTS="\
   test-broken.sh\
   test-circular-dependency.sh\
   test-duplicates.sh\
   test-mangled-input.sh\
   test-missing-reference.sh\
   test-missing-required.sh\
   test-self-dependency.sh\
   test-unknown-node-type.sh\
   test-xor-failure-1.sh\
   test-xor-failure-2.sh\
   test-xor-failure-3.sh\
"

TESTDIR=`dirname $0`


for X in $TESTSCRIPTS; do
   $TESTDIR/$X &> /dev/null
   if [ $? -ne 0 ]; then
      echo FAILED: $X
   else
      echo PASSED: $X
   fi
done

