# PA2 — Commands Reference

> A growing, documented list of commands relevant to this project. Each entry has a one-line
> explanation. New sections are filled in as we implement.

## Conventions

- Run commands from the **repo root** unless noted:
  `/Users/adminaccount/Documents/personal/Computer Networks/Computer-Networks-Course-PA2`
- The authoritative provided sources live in `source files/PA2_resource_alpha/`
  (`netproc.c`, `bf.h`, `topology.csv`). These supersede the originals in `PA2_resources.zip`.
- `costs.sh` ships only in the original `PA2_resources.zip`; unzip it once if needed (see below).
- Build artifacts go in `build/` to keep the tree clean.

## Build the network emulator (`netproc`)

`netproc` is provided — we compile it only to test our node against it. It uses C11 threads
(`<threads.h>`). On Linux / the lab machines (the grading target) this just works; on macOS you may
need `-pthread` and a toolchain that ships `<threads.h>`.

```sh
mkdir -p build
cc -std=c11 -I"source files/PA2_resource_alpha" \
   -o build/netproc "source files/PA2_resource_alpha/netproc.c"
# macOS, if the above complains about threads:
# cc -std=c11 -pthread -I"source files/PA2_resource_alpha" \
#    -o build/netproc "source files/PA2_resource_alpha/netproc.c"
```

### macOS build (verified) — needs the threads shim

Apple's default clang ships **no `<threads.h>`**, so the plain build above fails
(`'threads.h' file not found`). Use the local shim at `build/shim/threads.h` (test-only; not in the
submission, and `netproc.c` is the course's file):
```sh
cc -std=c11 -Wall -pthread -Ibuild/shim -I"source files/PA2_resource_alpha" \
   -o build/netproc "source files/PA2_resource_alpha/netproc.c"
```

## Run the network emulator

```sh
./build/netproc "source files/PA2_resource_alpha/topology.csv"
```
Builds the graph from the topology, binds TCP port **6789**, and waits for node connections.
Prints connection/forwarding activity to stdout. Stop with Ctrl-C.

> **Running anything that opens a socket must be done in your own terminal.** The assistant's Bash
> sandbox blocks all TCP binds/connects (`EPERM`). Prefix a command with `!` in the chat to run it in
> your real shell, e.g. `! sh build/step1_probe.sh`.

## Step 1 — verify netproc's wire protocol (probe experiment)

```sh
# compile the throwaway probe
cc -std=c11 -Wall -Wextra -o build/probe build/probe.c

# run both experiments (unicast + broadcast) and print observed vs. predicted byte 0
! sh build/step1_probe.sh        # run from the chat so it executes in your real shell
```
Confirms: 128-byte framing, the byte-0 rewrite to the receiver's ingress link, 0-based link
asymmetry, broadcast fan-out, and verbatim payload. Results recorded in `LEARNING_NOTES.md` §1.3.

## Derive a node's link costs (`costs.sh`)

`costs.sh` prints a node's incident link costs in topology-file order — exactly the order `nodeproc`
expects on the command line.

```sh
# One-time: extract costs.sh from the original resources zip if not already present.
unzip -o "source files/PA2_resources.zip" costs.sh -d build

# Print the costs for a given node ID:
sh build/costs.sh "source files/PA2_resource_alpha/topology.csv" 7416
```

## Build the node (`nodeproc`)

```sh
make            # builds ./nodeproc from nodeproc.c + net_util.c (gnu11, -Wall -Wextra)
make clean      # removes nodeproc + object files
```
(Plain build — `nodeproc` uses no C11 threads, so no shim is needed; only `netproc` does.)

## Run a node

Form: `nodeproc <netproc_addr> <node_id> <lifetime_sec> <cost1> [cost2 ...]`. Costs are positional, in
topology-file (= link index) order. For the given `topology.csv` (local, 60 s):

```sh
nodeproc 127.0.0.1 56908 60 23 76          # links: 23->25824, 76->53021
nodeproc 127.0.0.1 25824 60 23 62 213      # links: 23->56908, 62->7416, 213->15762
nodeproc 127.0.0.1 7416  60 62 155 136     # links: 62->25824, 155->53021, 136->15762
nodeproc 127.0.0.1 53021 60 155 76 79      # links: 155->7416, 76->56908, 79->15762
nodeproc 127.0.0.1 15762 60 213 136 79     # links: 213->25824, 136->7416, 79->53021

# Or derive costs automatically:
nodeproc 127.0.0.1 7416 60 $(sh build/costs.sh "source files/PA2_resource_alpha/topology.csv" 7416)
```
(Run these in your own terminal — sockets are sandboxed on the assistant side. `netproc` must already
be listening.)

### Step 2 smoke test (lifecycle only)
```sh
! sh build/step2_run.sh    # netproc + one nodeproc (7416, 3s): connect, initial state, shutdown
```

### Step 3 message round-trip test (no sockets)
```sh
cc -std=gnu11 -Wall -Wextra -I. -o build/test_msg build/test_msg.c msg.c && ./build/test_msg
```
Verifies `bf_msg` encode→decode preserves all fields, the link byte, and zero padding (no network, so
it runs anywhere).

## Multi-node convergence run

Start `netproc`, then one `nodeproc` per node; all converge to `Root=7416` (oracle in
`LEARNING_NOTES.md` §0.5). Easiest via the harness:

```sh
! sh build/step4_run.sh    # netproc + all 5 nodes; prints final state per node vs oracle
```
Or manually: run `netproc` in one terminal and each `nodeproc … ` (commands above) in others. Per-node
transcripts are written to `build/n_<id>.log` by the harness.

### Root-crash recovery test (Step 5)
```sh
! sh build/step5_run.sh    # converge to 7416, kill it ~5s, survivors re-elect 15762 (~ROOT_TIMEOUT)
```
Confirms expTime aging + recovery: survivors converge to the 15762 oracle (15762:0, 53021:79,
56908:155, 25824:178).

### Extreme / generality tests (Step 6)
```sh
! sh build/step6_loop.sh     # 4-node graph with cycles + a cost tie (root=10); checks tie-break
! sh build/step6_rejoin.sh   # kill the root, recover to 15762, restart 7416 -> reclaims root
```

### Generic topology tester (any graph) — auto-checks vs a reference oracle
`build/oracle.py` computes the expected converged state (matching nodeproc's tie-break);
`build/run_topo.sh` launches netproc + one node per ID (costs auto-derived) and prints observed vs
oracle with an OK/XX marker. Works on any topology CSV.
```sh
! sh build/run_topo.sh build/topo_mesh.csv     # 6-node mesh: depth + an equal-cost tie (node 20)
! sh build/run_topo.sh build/topo_split.csv    # disconnected: two components -> two roots (100, 400)
! sh build/run_topo.sh build/topo_chain.csv    # 6-node line: deep multi-hop propagation
! sh build/run_topo.sh build/topo_zero.csv     # zero-cost links (all distance 0) + tie
! sh build/run_topo.sh build/topo_loop.csv     # ring + chord (same as step6_loop)

# Just print the expected oracle for a topology (no sockets):
python3 build/oracle.py build/topo_mesh.csv
```

### Dynamic / timing scenarios
```sh
! sh build/test_nonroot_fail.sh   # kill intermediate 25824; survivors reroute (56908: 85->231)
! sh build/test_late_join.sh      # 4 nodes converge, then 56908 joins late and slots in
! sh build/test_stability.sh      # after convergence: no churn + ~1 HELLO/interval/node
! sh build/test_robust.sh         # short lifetime (1s) + unknown-ID rejection handled gracefully
```

## Package for submission

The zip must contain ONLY these files (verified: a clean `make` builds `nodeproc` from them):
```sh
make clean
zip pa2_submission.zip README Makefile bf.h net_util.h net_util.c msg.h msg.c nodeproc.c
```
Everything under `build/` and the `*.md` working docs are intentionally excluded.
