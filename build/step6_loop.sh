#!/bin/sh
# PA2 Step 6 — loop-topology test. Run from your OWN terminal:
#     ! sh build/step6_loop.sh
# A 4-node graph with cycles (ring 10-20-30-40-10 plus chord 20-40), root = 10.
# Checks convergence on a non-trivial topology, including a cost tie at node 30
# (paths via 20 and via 40 both cost 2 -> tie-break picks smaller next-hop, 20).
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="build/topo_loop.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s nodeproc

rm -f build/l_10.log build/l_20.log build/l_30.log build/l_40.log build/log_netproc6.txt
build/netproc "$TOPO" >build/log_netproc6.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

LT=5
./nodeproc 127.0.0.1 10 $LT 1 1   >build/l_10.log 2>&1 &   # links 0->20, 1->40
./nodeproc 127.0.0.1 20 $LT 1 1 1 >build/l_20.log 2>&1 &   # links 0->10, 1->30, 2->40
./nodeproc 127.0.0.1 30 $LT 1 1   >build/l_30.log 2>&1 &   # links 0->20, 1->40
./nodeproc 127.0.0.1 40 $LT 1 1 1 >build/l_40.log 2>&1 &   # links 0->30, 1->10, 2->20

sleep $((LT + 1))
kill $NP 2>/dev/null

echo "===== loop topology: final state vs oracle (root = 10) ====="
show() {
  obs=$(grep 'Root=' "build/l_$1.log" | tail -1 | sed -e 's/^time=[0-9.]*[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g')
  printf "  %-3s observed: %-33s oracle: %s\n" "$1" "$obs" "$2"
}
show 10 "Root=10 parent=NULL distance=0"
show 20 "Root=10 parent=10 distance=1"
show 40 "Root=10 parent=10 distance=1"
show 30 "Root=10 parent=20 distance=2"
echo "(node 30 has equal-cost paths via 20 and 40; tie-break selects parent=20)"
