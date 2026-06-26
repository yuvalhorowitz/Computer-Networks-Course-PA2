#!/bin/sh
# PA2 Step 6 — root rejoin test. Run from your OWN terminal:
#     ! sh build/step6_rejoin.sh
# Converge to root 7416; kill 7416 (~5s) so survivors re-elect 15762; then RESTART
# 7416 (~14s) and confirm the network reclaims 7416 as root (a smaller ID arriving
# wins immediately). Demonstrates recovery in both directions.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s bfproc

rm -f build/r_7416.log build/r_25824.log build/r_53021.log build/r_15762.log build/r_56908.log build/log_netproc7.txt
build/netproc "$TOPO" >build/log_netproc7.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

./bfproc 127.0.0.1 56908 25 23 76      >build/r_56908.log 2>&1 &
./bfproc 127.0.0.1 25824 25 23 62 213  >build/r_25824.log 2>&1 &
./bfproc 127.0.0.1 53021 25 155 76 79  >build/r_53021.log 2>&1 &
./bfproc 127.0.0.1 15762 25 213 136 79 >build/r_15762.log 2>&1 &
./bfproc 127.0.0.1 7416  25 62 155 136 >build/r_7416.log  2>&1 & P7416=$!

sleep 5
echo "=== t~5s: kill 7416 (survivors should re-elect 15762) ==="
kill -9 "$P7416" 2>/dev/null
sleep 9
echo "=== t~14s: restart 7416 (network should reclaim 7416 as root) ==="
./bfproc 127.0.0.1 7416 11 62 155 136 >>build/r_7416.log 2>&1 &
sleep 9
kill $NP 2>/dev/null

echo
echo "===== final state vs ORIGINAL oracle (root = 7416 again) ====="
show() {
  obs=$(grep 'Root=' "build/r_$1.log" | tail -1 | sed -e 's/^time=[0-9.]*[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g')
  printf "  %-6s observed: %-34s oracle: %s\n" "$1" "$obs" "$2"
}
show 7416  "Root=7416 parent=NULL distance=0"
show 25824 "Root=7416 parent=7416 distance=62"
show 53021 "Root=7416 parent=7416 distance=155"
show 15762 "Root=7416 parent=7416 distance=136"
show 56908 "Root=7416 parent=25824 distance=85"
echo "(transcripts: build/r_<id>.log; 7416's log holds both its runs)"
