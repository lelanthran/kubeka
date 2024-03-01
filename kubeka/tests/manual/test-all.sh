#!/bin/bash

. tests/manual/tests.inc

TESTDIR=`dirname $0`

export TESTFILES="`ls -1 $TESTDIR/*.sh | grep -v test-all.sh | sort`"

export TESTSCRIPTS=""

for X in $TESTFILES; do
   TESTSCRIPTS="$TESTSCRIPTS `basename $X`"
done

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
