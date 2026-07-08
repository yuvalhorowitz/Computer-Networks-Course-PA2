#!/bin/sh
# PA2 Step 5 — root-crash recovery test. Run from your OWN terminal:
#     ! sh build/step5_run.sh
# Starts netproc + all 5 nodes, lets them converge to root 7416, KILLS node 7416
# (~5s), and checks the four survivors re-elect 15762 within ~ROOT_TIMEOUT.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s nodeproc

rm -f build/n_7416.log build/n_25824.log build/n_53021.log build/n_15762.log build/n_56908.log build/log_netproc5.txt
build/netproc "$TOPO" >build/log_netproc5.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

LT=18
./nodeproc 127.0.0.1 56908 $LT 23 76      >build/n_56908.log 2>&1 &
./nodeproc 127.0.0.1 25824 $LT 23 62 213  >build/n_25824.log 2>&1 &
./nodeproc 127.0.0.1 7416  $LT 62 155 136 >build/n_7416.log  2>&1 & P7416=$!
./nodeproc 127.0.0.1 53021 $LT 155 76 79  >build/n_53021.log 2>&1 &
./nodeproc 127.0.0.1 15762 $LT 213 136 79 >build/n_15762.log 2>&1 &

sleep 5
echo "=== t~5s: killing node 7416 (simulated root crash) ==="
kill -9 "$P7416" 2>/dev/null
sleep 13          # > ROOT_TIMEOUT; let survivors re-elect, then reach lifetime
kill $NP 2>/dev/null

echo
echo "===== survivors: first time they adopted Root=15762, and final line ====="
show() {
  first=$(grep -m1 'Root=15762' "build/n_$1.log" | sed -e 's/\t.*//')
  final=$(grep 'Root=' "build/n_$1.log" | tail -1 | sed -e 's/^time=[0-9.]*[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g')
  printf "  %-6s recovered@ %-9s final: %-35s oracle: %s\n" "$1" "${first:-never}" "$final" "$2"
}
show 15762 "Root=15762 parent=NULL distance=0"
show 53021 "Root=15762 parent=15762 distance=79"
show 56908 "Root=15762 parent=53021 distance=155"
show 25824 "Root=15762 parent=56908 distance=178"

echo
echo "(7416 was killed ~5s; ROOT_TIMEOUT=6s. Full transcripts: build/n_<id>.log)"
