#!/bin/sh
# PA2 — late joiner (non-root). Run from your OWN terminal:
#     ! sh build/test_late_join.sh
# Start 4 nodes (omit 56908), let them converge to root 7416, THEN start 56908
# late and confirm it joins (Root=7416, parent=25824, distance=85) while the
# others are unchanged. Compared to the full topology.csv oracle.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s nodeproc

rm -f build/j_*.log build/log_netproc_lj.txt
build/netproc "$TOPO" >build/log_netproc_lj.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

LT=15
./nodeproc 127.0.0.1 25824 $LT 23 62 213  >build/j_25824.log 2>&1 &
./nodeproc 127.0.0.1 7416  $LT 62 155 136 >build/j_7416.log  2>&1 &
./nodeproc 127.0.0.1 53021 $LT 155 76 79  >build/j_53021.log 2>&1 &
./nodeproc 127.0.0.1 15762 $LT 213 136 79 >build/j_15762.log 2>&1 &

sleep 5
echo "=== t~5s: 56908 joins late ==="
./nodeproc 127.0.0.1 56908 10 23 76 >build/j_56908.log 2>&1 &
sleep 8
kill $NP 2>/dev/null

python3 build/oracle.py "$TOPO" > build/oracle_lj.txt
echo "===== all nodes vs full oracle (after late join) ====="
for id in 7416 25824 53021 15762 56908; do
  obs=$(grep 'Root=' "build/j_$id.log" | tail -1 | sed -e 's/^time=[0-9.]*[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g')
  ora=$(grep "^$id " build/oracle_lj.txt | sed "s/^$id //")
  [ "$obs" = "$ora" ] && mark=OK || mark=XX
  printf "  %-6s %-34s %-34s [%s]\n" "$id" "$obs" "$ora" "$mark"
done
jt=$(grep -m1 'Root=7416' build/j_56908.log | sed 's/\t.*//')
echo "(56908 first adopted Root=7416 at ${jt:-?})"
