#!/bin/sh
# PA2 — non-root failure + reroute. Run from your OWN terminal:
#     ! sh build/test_nonroot_fail.sh
# Converge on topology.csv (root 7416), then KILL an intermediate node (25824).
# The root is unaffected; survivors must reroute. 56908 loses its 85-cost path
# via 25824 and reroutes to 231 via 53021. Compared to the oracle for the graph
# with 25824 removed.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s bfproc
grep -v '25824' "$TOPO" > build/topo_no25824.csv   # oracle reference: 25824 gone

rm -f build/f_*.log build/log_netproc_nf.txt
build/netproc "$TOPO" >build/log_netproc_nf.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

LT=18
./bfproc 127.0.0.1 56908 $LT 23 76      >build/f_56908.log 2>&1 &
./bfproc 127.0.0.1 25824 $LT 23 62 213  >build/f_25824.log 2>&1 & P25824=$!
./bfproc 127.0.0.1 7416  $LT 62 155 136 >build/f_7416.log  2>&1 &
./bfproc 127.0.0.1 53021 $LT 155 76 79  >build/f_53021.log 2>&1 &
./bfproc 127.0.0.1 15762 $LT 213 136 79 >build/f_15762.log 2>&1 &

sleep 5
echo "=== t~5s: killing intermediate node 25824 (root 7416 stays) ==="
kill -9 "$P25824" 2>/dev/null
sleep 12
kill $NP 2>/dev/null

python3 build/oracle.py build/topo_no25824.csv > build/oracle_nf.txt
echo "===== survivors after reroute  vs  oracle (25824 removed) ====="
for id in 7416 53021 15762 56908; do
  obs=$(grep 'Root=' "build/f_$id.log" | tail -1 | sed -e 's/^time=[0-9.]*[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g')
  ora=$(grep "^$id " build/oracle_nf.txt | sed "s/^$id //")
  [ "$obs" = "$ora" ] && mark=OK || mark=XX
  printf "  %-6s %-36s %-36s [%s]\n" "$id" "$obs" "$ora" "$mark"
done
echo "(56908 should move 85/25824 -> 231/53021)"
