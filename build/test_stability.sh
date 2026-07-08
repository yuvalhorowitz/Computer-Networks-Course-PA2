#!/bin/sh
# PA2 — stability + communication efficiency. Run from your OWN terminal:
#     ! sh build/test_stability.sh
# After convergence there should be NO churn (no further Root= changes) and sends
# should settle to ~one HELLO per HELLO_TIMEOUT per node. Prints, per node: how
# many state lines were emitted, the time of the LAST state change (should be
# small => converged early, no late flapping), and the total sends.
cd "$(dirname "$0")/.."
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
make -s nodeproc

rm -f build/s_*.log build/log_netproc_st.txt
build/netproc "$TOPO" >build/log_netproc_st.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null' EXIT
sleep 0.5

LT=12
./nodeproc 127.0.0.1 56908 $LT 23 76      >build/s_56908.log 2>&1 &
./nodeproc 127.0.0.1 25824 $LT 23 62 213  >build/s_25824.log 2>&1 &
./nodeproc 127.0.0.1 7416  $LT 62 155 136 >build/s_7416.log  2>&1 &
./nodeproc 127.0.0.1 53021 $LT 155 76 79  >build/s_53021.log 2>&1 &
./nodeproc 127.0.0.1 15762 $LT 213 136 79 >build/s_15762.log 2>&1 &

sleep $((LT + 1))
kill $NP 2>/dev/null

echo "===== stability/efficiency over ${LT}s (HELLO_TIMEOUT=2 => ~$((LT/2)) hello sends/node) ====="
printf "  %-6s %-12s %-16s %-8s %s\n" node state-lines last-change sends verdict
for id in 7416 25824 53021 15762 56908; do
  log="build/s_$id.log"
  states=$(grep -c 'Root=' "$log")
  sends=$(grep -c 'Message sent' "$log")
  lastt=$(grep 'Root=' "$log" | tail -1 | sed -e 's/time=\([0-9.]*\).*/\1/')
  # stable if last Root= change happened within the first 1.0s
  verdict=$(awk -v t="$lastt" 'BEGIN{print (t<=1.0)?"STABLE":"CHURN?"}')
  printf "  %-6s %-12s %-16s %-8s %s\n" "$id" "$states" "time=${lastt}" "$sends" "$verdict"
done
echo "(STABLE = converged within ~1s and no later Root= changes)"
