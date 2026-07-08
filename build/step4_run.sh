#!/bin/sh
# PA2 Step 4 — convergence test. Run from your OWN terminal:
#     ! sh build/step4_run.sh
# Launches netproc + all five bfproc nodes, lets them converge, then prints each
# node's final Root/parent/distance line next to the expected oracle. Test-only.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s nodeproc

rm -f build/n_7416.log build/n_25824.log build/n_53021.log build/n_15762.log build/n_56908.log build/log_netproc4.txt
build/netproc "$TOPO" >build/log_netproc4.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

LT=5   # lifetime seconds
./nodeproc 127.0.0.1 56908 $LT 23 76      >build/n_56908.log 2>&1 &
./nodeproc 127.0.0.1 25824 $LT 23 62 213  >build/n_25824.log 2>&1 &
./nodeproc 127.0.0.1 7416  $LT 62 155 136 >build/n_7416.log  2>&1 &
./nodeproc 127.0.0.1 53021 $LT 155 76 79  >build/n_53021.log 2>&1 &
./nodeproc 127.0.0.1 15762 $LT 213 136 79 >build/n_15762.log 2>&1 &

sleep $((LT + 1))   # let them converge and reach lifetime
kill $NP 2>/dev/null

echo "===== final state (last Root= line per node)  vs  oracle ====="
show() {
  obs=$(grep 'Root=' "build/n_$1.log" | tail -1 | sed -e 's/^time=[0-9.]*[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g')
  printf "  %-6s observed: %-34s  oracle: %s\n" "$1" "$obs" "$2"
}
show 7416  "Root=7416 parent=NULL distance=0"
show 25824 "Root=7416 parent=7416 distance=62"
show 53021 "Root=7416 parent=7416 distance=155"
show 15762 "Root=7416 parent=7416 distance=136"
show 56908 "Root=7416 parent=25824 distance=85"

echo
echo "(full per-node transcripts in build/n_<id>.log)"
