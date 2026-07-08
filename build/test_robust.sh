#!/bin/sh
# PA2 — robustness edges. Run from your OWN terminal:
#     ! sh build/test_robust.sh
# (a) very short lifetime (1s): node must print initial state, then shut down on
#     time and exit cleanly.
# (b) unknown ID (not in topology): netproc rejects and closes the socket; the
#     node must exit gracefully (not hang or crash), well before its lifetime.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s nodeproc

rm -f build/rb_*.log build/log_netproc_rb.txt
build/netproc "$TOPO" >build/log_netproc_rb.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

echo "=== (a) short lifetime: bfproc 7416 with lifetime=1 ==="
./nodeproc 127.0.0.1 7416 1 62 155 136 >build/rb_short.log 2>&1
echo "exit=$?  (expect 0)"
cat build/rb_short.log

echo
echo "=== (b) unknown ID 99999 (not in topology), lifetime=5 ==="
./nodeproc 127.0.0.1 99999 5 10 >build/rb_unknown.log 2>&1 & UPID=$!
sleep 3
if kill -0 "$UPID" 2>/dev/null; then
  echo "STILL RUNNING after 3s — unexpected (should exit when netproc closes the socket)"
  kill -9 "$UPID" 2>/dev/null
else
  wait "$UPID"; echo "exited cleanly (exit=$?)"
fi
echo "--- node output ---"; cat build/rb_unknown.log
echo "--- netproc saw ---"; grep -iE 'do not know|ID' build/log_netproc_rb.txt | head

kill $NP 2>/dev/null
