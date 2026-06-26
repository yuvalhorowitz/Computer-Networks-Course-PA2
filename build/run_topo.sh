#!/bin/sh
# PA2 — generic topology test. Run from your OWN terminal:
#     ! sh build/run_topo.sh build/topo_mesh.csv
#     ! sh build/run_topo.sh build/topo_split.csv 6
# Launches netproc + one bfproc per node (costs auto-derived from the topology in
# link order), lets them converge, then prints each node's final line next to the
# reference oracle (build/oracle.py) with an OK/XX match marker. Test-only.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$1"
LT="${2:-5}"
[ -n "$TOPO" ] || { echo "usage: sh build/run_topo.sh <topology.csv> [lifetime]"; exit 2; }

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s bfproc

nodes=$(awk -F, 'NF>=3{print $1"\n"$2}' "$TOPO" | sort -un)
rm -f build/run_*.log build/log_topo.txt
build/netproc "$TOPO" >build/log_topo.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

for id in $nodes; do
  costs=$(awk -F, -v t="$id" '$1==t || $2==t {print $3}' "$TOPO" | tr '\n' ' ')
  ./bfproc 127.0.0.1 "$id" "$LT" $costs >"build/run_$id.log" 2>&1 &
done

sleep $((LT + 1))
kill $NP 2>/dev/null

python3 build/oracle.py "$TOPO" > build/oracle_out.txt
echo "topology: $TOPO     (OK = observed matches oracle)"
printf "  %-7s %-42s %-42s %s\n" node observed oracle match
for id in $nodes; do
  obs=$(grep 'Root=' "build/run_$id.log" | tail -1 | sed -e 's/^time=[0-9.]*[[:space:]]*//' -e 's/[[:space:]][[:space:]]*/ /g')
  ora=$(grep "^$id " build/oracle_out.txt | sed "s/^$id //")
  [ "$obs" = "$ora" ] && mark=OK || mark=XX
  printf "  %-7s %-42s %-42s [%s]\n" "$id" "$obs" "$ora" "$mark"
done
