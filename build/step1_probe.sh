#!/bin/sh
# PA2 Step 1 — empirical verification of netproc's wire protocol.
# Run this from your OWN terminal (the assistant's sandbox blocks TCP binds):
#     ! sh build/step1_probe.sh
# It (re)builds netproc + probe if needed, runs a unicast and a broadcast
# experiment, and prints what each receiver saw in byte 0 vs. the prediction.
#
# Throwaway test harness — not part of the submission.
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
SRC="source files/PA2_resource_alpha"
TOPO="$SRC/topology.csv"

# --- build (uses the local threads.h->pthreads shim for macOS) ---
[ -x build/netproc ] || cc -std=c11 -Wall -pthread -Ibuild/shim -I"$SRC" -o build/netproc "$SRC/netproc.c"
[ -x build/probe ]   || cc -std=c11 -Wall -Wextra -o build/probe build/probe.c

rm -f build/log_netproc.txt build/log_25824.txt build/log_53021.txt build/log_15762.txt

# --- run ---
build/netproc "$TOPO" >build/log_netproc.txt 2>&1 &
NP=$!
trap 'kill $NP 2>/dev/null; pkill -f "build/probe" 2>/dev/null' EXIT
sleep 0.6

# 7416's three neighbors listen
build/probe 127.0.0.1 25824 recv >build/log_25824.txt 2>&1 &
build/probe 127.0.0.1 53021 recv >build/log_53021.txt 2>&1 &
build/probe 127.0.0.1 15762 recv >build/log_15762.txt 2>&1 &
sleep 0.8

echo "=== Experiment A: 7416 unicast on its link 0 (-> 25824) ==="
build/probe 127.0.0.1 7416 send 0 2>/dev/null
sleep 0.8
echo "=== Experiment B: 7416 broadcast (0xFF = 255) ==="
build/probe 127.0.0.1 7416 send 255 2>/dev/null
sleep 1.0

kill $NP 2>/dev/null; pkill -f 'build/probe' 2>/dev/null; sleep 0.3

echo
echo "===== RECEIVER LOGS (byte0 = receiver's ingress link) ====="
echo "Prediction: A -> 25824 sees byte0=1 ; B -> 25824=1, 53021=0, 15762=1"
for n in 25824 53021 15762; do
  echo "--- node $n ---"
  grep '^\[recv' "build/log_$n.txt" || echo "(no frames)"
done
echo
echo "===== netproc forwarding trace ====="
grep -iE 'Received message|Sending to|Receiving from|connection|ID' build/log_netproc.txt | head -40
