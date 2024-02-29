#!/bin/bash

. tests/manual/tests.inc

export TESTSCRIPTS="\
   test-broken.sh\
   test-circular-dependency.sh\
   test-unresolved-variable.sh\
   test-duplicates.sh\
   test-happy-cline.sh\
   test-happy-environment-variables.sh\
   test-happy-resolved-variable.sh\
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

NFAILURES=0
NPASSES=0
for X in $TESTSCRIPTS; do
   $TESTDIR/$X $1 &> /tmp/$X.output
   if [ $? -ne 0 ]; then
      echo -e "${RED}${REV}✘${NC} $X"
      NFAILURES=$(($NFAILURES + 1))
      cat /tmp/$X.output
   else
      echo -e "${GREEN}✓${NC} $X"
      NPASSES=$(($NPASSES + 1))
   fi
done

echo $NFAILURES failed, $NPASSES passed

exit $NFAILURES
