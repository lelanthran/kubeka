
failed() {
   echo FAILED: $0
   exit 127
}

passed() {
   echo PASSED: $0
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
