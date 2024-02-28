RED='\e[0;31m'
GREEN='\e[0;32m'
REV='\e[7m'
NC='\e[0m'

failed() {
   echo -e ${RED}${REV}FAILED${NC} $0
   exit 127
}

passed() {
   echo -e ${GREEN}${REV}PASSED${NC} $0
   exit 0
}


if [ "$1" == "vg" ]; then
   VGFILE="--log-file=vg.txt"
   PROG="valgrind --leak-check=full --track-origins=yes $VGFILE"
fi

if [ "$1" == "gdb" ]; then
   PROG="gdb --args"
fi

PROG="$PROG recent/bin/kubeka.elf"

make debug || failed
