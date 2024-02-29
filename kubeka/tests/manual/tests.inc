RED='\e[0;31m'
GREEN='\e[0;32m'
REV='\e[7m'
NC='\e[0m'

failed() {
   echo -e ${RED}${REV}FAILED${NC} $0 $1
   exit 127
}

passed() {
   VGERR="`tail -n 1 vg.txt | cut -f 4 -d \  `"
   if [ ! -z "$VGERR" ] && [ "$VGERR" -ne "0" ]; then
      failed "valgrind report errors in vg.txt"
   fi
   echo -e ${GREEN}${REV}PASSED${NC} $0
   exit 0
}

if [ "$1" == "vg" ]; then
   VGFILE="--log-file=vg.txt"
   ERRORCODE="--error-exitcode=0"
   PROG="valgrind $ERRORCODE --leak-check=full --track-origins=yes $VGFILE"
fi

if [ "$1" == "gdb" ]; then
   PROG="gdb --args"
fi

PROG="$PROG recent/bin/kubeka.elf"

single_test() {
   rm -f vg.txt
   $PROG \
      -f  tests/input/$1.kubeka \
      &> tests/output/$1.output && eval $2
            diff tests/expected/$1.output tests/output/$1.output || failed
}

if [ -z "$INCLUDED" ]; then
   export INCLUDED="included"
   make debug || failed
fi
