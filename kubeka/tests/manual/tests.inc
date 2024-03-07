RED='\e[0;31m'
GREEN='\e[0;32m'
REV='\e[7m'
NC='\e[0m'

failed() {
   echo -e ${RED}${REV}FAILED${NC} $0 $1
   exit 127
}

passed() {
   VGERR="`tail -n 1 vg.txt 2>/dev/null | cut -f 4 -d \  `"
   if [ ! -z "$VGERR" ] && [ "$VGERR" -ne "0" ]; then
      failed "valgrind report errors in vg.txt"
   fi
   echo -e ${GREEN}${REV}PASSED${NC} $0
   exit 0
}

happy() {
   VGERR="`tail -n 1 vg.txt 2>/dev/null | cut -f 4 -d \  `"
   if [ ! -z "$VGERR" ] && [ "$VGERR" -ne "0" ]; then
      failed "valgrind report errors in vg.txt"
   fi
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
   if [ `echo $PROG | grep -c 'gdb'` -gt 0 ]; then
      $PROG --lint \
         -f  tests/input/$1.kubeka \
         || failed
   else
      rm -f vg.txt
      $PROG --lint \
         -f  tests/input/$1.kubeka \
         &> tests/output/$1.output && eval $2
      diff tests/expected/$1.output tests/output/$1.output || failed
   fi
}

run_daemon() {
   if [ `echo $PROG | grep -c 'gdb'` -gt 0 ]; then
      $PROG --daemon \
         -f  tests/input/$1.kubeka \
         || failed
   else
      rm -f vg.txt
      $PROG --daemon \
         -f  tests/input/$1.kubeka \
         &> tests/output/$1.output || failed
      sleep 5s
      kill -INT `cat /tmp/kubeka.pid | cut -f 2 -d :` &> /dev/null
      diff tests/expected/$1.output tests/output/$1.output || failed
   fi
}

if [ -z "$INCLUDED" ]; then
   export INCLUDED="included"
   make debug || failed
fi
