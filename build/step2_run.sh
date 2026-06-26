#!/bin/sh
# PA2 Step 2 — connection-lifecycle smoke test. Run from your OWN terminal:
#     ! sh build/step2_run.sh
# Starts netproc + one bfproc (node 7416, 3s lifetime) and shows that the node
# connects, prints its initial state, and shuts down on time. Test-only.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

# build (netproc needs the macOS threads shim; bfproc is plain)
[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s bfproc

rm -f build/log_netproc2.txt
build/netproc "$TOPO" >build/log_netproc2.txt 2>&1 &
NP=$!
sleep 0.6

echo "===== bfproc output (node 7416, lifetime 3s) ====="
./bfproc 127.0.0.1 7416 3 62 155 136

echo
echo "===== netproc connection log ====="
grep -iE 'connection|ID' build/log_netproc2.txt | head

kill $NP 2>/dev/null
