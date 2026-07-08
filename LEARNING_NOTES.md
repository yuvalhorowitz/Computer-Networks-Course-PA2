# PA2 — Learning Notes

> A growing, step-by-step reference that mirrors how we build this project together.
> Goal: **learning and high-quality iterative implementation, not speed.**

## How to read this file

- Each **Step** is one chunk of work we tackled interactively.
- Inside a step, **concepts come first, then the code** that uses them.
- Every implementation step follows the same cadence:
  1. **Theory / concepts** — the ideas (grounded in the course lectures).
  2. **Implementation steps** — what we will build and how.
  3. **Confirmation** — we agree before writing code.
  4. **Implementation** — the actual code, documented.
- Companion docs: `PROJECT_OVERVIEW.md` (big-picture theory + plan) and `COMMANDS.md` (runnable
  commands). This file is a *working/learning aid* — it is **not** part of the submission zip
  (submission = `README`, `Makefile`, `*.c/*.h` only).

## Steps index

- [Step 0 — The assignment & the theory behind it](#step-0--the-assignment--the-theory-behind-it)
- [Step 1 — Verify netproc empirically](#step-1--verify-netproc-empirically) ✓
- [Step 2 — Scaffold + connection lifecycle](#step-2--scaffold--connection-lifecycle) ✓
- [Step 3 — Neighbor/link table + message encode/decode](#step-3--neighborlink-table--message-encodedecode) ✓
- [Step 4 — Bellman-Ford relaxation + send rule](#step-4--bellman-ford-relaxation--send-rule) ✓
- [Step 5 — expTime aging, root-expiry timer, root-crash recovery](#step-5--exptime-aging-root-expiry-timer-root-crash-recovery) ✓
- [Step 6 — Finalize: README, extreme tests, cleanup](#step-6--finalize-readme-extreme-tests-cleanup) ✓ (team info pending)

---

# Step 0 — The assignment & the theory behind it

**Status:** understanding only — no code. This step makes the assignment and its theory clear before
we write a single line of `nodeproc`.

## 0.1 What the assignment asks

We write **one program, `nodeproc`** (the "node process"). Many copies run at once — one per node in a
network. The nodes never talk directly; a provided program, **`netproc`**, simulates the network and
relays every message over TCP.

Each node knows **only the costs of its own links** (given on the command line). From that, plus the
messages its neighbors send, every node must **independently** converge on three values:

| Output | Meaning |
|---|---|
| **Root** | the smallest node ID in this node's connected component |
| **parent** | the next-hop neighbor on the shortest path toward the Root |
| **distance** | total cost of the shortest path to the Root |

> **`parent` ↔ root port (two ends of one edge).** A node's **root port** is the *local link* that
> leads toward the Root; the **parent** is the *neighbor node* at the far end of that link. They
> describe the same tree edge — the root port answers "which of my links?", the parent answers "which
> neighbor is on it?". PA2 prints `parent` as that **neighbor's ID** (not a port number), which is why
> our message carries `myID`. A node that is its own Root has no root port → `parent=NULL`.

Why this is non-trivial: there is **no central coordinator** and **no global map**. A node sees only
its neighbors. Yet the whole network must agree on one Root and each node must find its true shortest
distance to it — purely by exchanging small messages. This is exactly a **distributed algorithm**, and
the one we use is **distributed Bellman-Ford**, which is the engine inside the **Spanning Tree
Protocol (STP)**.

## 0.2 The theory it rests on (Lecture 4)

This is the heart of Step 0. The assignment is, in essence, the **control plane of STP** taught in
**Lecture 4** ("גשרים ופרוטוקול העץ הפורש"). The course builds it in layers; the same layers map
1-to-1 onto what `nodeproc` must do.

### Why a spanning tree at all?
Bridges connected with **redundant links** (good for reliability) create **loops**, and a broadcast
frame can **circulate forever**, eating all the bandwidth. We can't just delete cables. STP's fix: the
bridges run a **distributed protocol**, agree on a **loop-free, connected subgraph (a spanning tree)**,
and **disable** the leftover links. PA2 computes that tree's structure (each node's next hop toward the
root); it does not forward user data.

### The graph model
Network = weighted graph `G = (V, E)`: nodes have integer **IDs**; links have non-negative integer
**costs**; a path's cost is the sum of its links; the **distance** is the cheapest such cost. Costs
here are symmetric and non-negative — which is what lets the distributed algorithm converge.

### Step A — Elect the root (smallest ID)
Every node has an ID. Each repeatedly advertises the **smallest ID it has heard of so far**; the node
with the **globally smallest ID becomes the root**. Each node starts believing *it* is the root and is
corrected as smaller IDs propagate inward.

> **The crash problem → why `expTime` exists.** Lecture 4 flags the weakness explicitly: if the root
> *dies* (loses power), the others have **no way to know** — they keep believing the dead node is
> still root. PA2's fix is **message aging**: root information carries an expiration (`expTime`); if it
> isn't refreshed in time (`ROOT_TIMEOUT`), a node **discards it and falls back to being its own
> root**, letting the network re-elect the next-smallest ID. This is the single most subtle part of
> the assignment.

### Step B — Build the shortest-path tree with Bellman-Ford
Given a root `s`, each node keeps a distance estimate `dist` to it and applies the **Bellman
equation** (Lecture 4):
```
dist_s    = 0                                       (the root itself)
cost_v(u) = dist_u + W(u, v)        for a neighbor u of v
dist_v    = min { cost_v(u) : u is a neighbor of v }
```
In words: *my distance to the root is the cheapest over my neighbors of "the link to that neighbor +
that neighbor's own distance to the root."* The neighbor achieving the minimum is the **next hop** —
our `parent`. (Lecture 4 first uses `W = 1` for hop counts, then notes weights can be arbitrary link
costs — which is our case.)

Three properties Lecture 4 stresses, all of which shape our implementation:
1. **Runs continuously and stabilizes** — not one-shot; nodes keep exchanging until estimates settle.
2. **Works asynchronously** — no global clock; each node runs at its own pace (and `netproc` delivers
   messages asynchronously).
3. **Stabilizes from any initial values** — no dependence on history; this is *why* the spec says to
   test "with different initial distance values and with loops."

### Step C — Port roles (where loops actually get broken)
Each port becomes one of:
- **Root port** — the one port giving the best path *toward* the root (lowest distance; ties → smallest
  ID). **The neighbor on this port is our `parent`** (we print that neighbor's ID).
- **Designated port** — on each segment, the port of the node with the best advertisement for that
  segment; forwards *away* from the root.
- **Blocked port** — neither root nor designated; disabled, which breaks the loop.

PA2 only outputs the **root port** (`parent`), distance, and Root — it does not label
designated/blocked ports. (This is the source of the earlier "root port vs designated port" question.)

### The BPDU message + lexicographic comparison
Lecture 4 gives the exact message PA2 reuses — the **BPDU (Bridge Protocol Data Unit)**:
```
(myRoot, myDist, myID)        + expTime in PA2
```
Nodes compare advertisements **lexicographically**: smaller `myRoot` wins; tie → smaller `myDist`
(= our `myCost`/distance); tie → smaller `myID`. Every node's **default** belief, before hearing
anything better, is:
```
(myID, 0, myID)        "I am my own root, at distance 0."
```

### Aging, refresh, and the two planes
A bridge **forgets old messages** after a while; information must be **continually refreshed** or
discarded. This one idea underlies three PA2 requirements:
- **periodic re-advertisement** every `HELLO_TIMEOUT` (keep-alive) so neighbors' info stays fresh;
- **`expTime` aging** of root info → enables **root-crash recovery** *and* prevents stale routes from
  looping forever (classic distance-vector *count-to-infinity*);
- the split into a **control plane** (BPDU exchange building the tree) and a **data plane** (user data
  on the tree). PA2 implements the control plane.

### How the theory maps onto PA2

| Lecture-4 / STP concept | PA2 realization |
|---|---|
| Bridge running STP | one `nodeproc` process per node |
| Bridges talking over LAN segments | node processes exchanging 128-byte frames via `netproc` |
| BPDU `(myRoot, myDist, myID)` | the message payload (+ `expTime`) |
| Default `(myID, 0, myID)` | initial state `myRoot=myID, myCost=0, parent=NULL` |
| Lexicographic compare | relaxation order: smaller root → smaller cost → smaller ID |
| `dist_v = min(dist_u + W)` | `myCost = min over links (neighbor.myCost + cost[link])` |
| Root port | `parent` (next hop toward Root) |
| Message aging / root-crash fix | `expTime` + `ROOT_TIMEOUT` |
| Continuous, asynchronous convergence | event loop driven by `select()` + the HELLO timer |

## 0.3 The I/O contract

### Command line
```
nodeproc  <netproc_address>  <node_id>  <lifetime>  <cost1> [cost2 ...]
```
- `netproc_address` — IP where `netproc` runs (`127.0.0.1` if local); port is always **6789**.
- `node_id` — this node's integer ID. The PDF calls it "the ID you assign to the current process":
  that means *you choose, at launch, which topology node this process represents* — it is **not** an
  arbitrary process label. It must be an ID present in `topology.csv` (else `netproc` rejects the
  connection with `Error: Do not know ID`), and the `cost*` args must be that same node's link costs
  (what `costs.sh <id>` returns). This integer *is* the node's network identity: it becomes **`myID`**,
  the value used in **root election** (smallest ID wins), and — since every node starts as its own
  root — the **initial `myRoot`** (so the first printed line is `Root=<node_id> parent=NULL distance=0`).
- `lifetime` — seconds to run, then terminate gracefully (close sockets, free resources).
- `cost1, cost2, …` — costs of this node's incident links, in **link/port order** (see §0.4). Derive
  with `costs.sh`.

### Required stdout events (exact formats — graded)
First field is always the time. Print the **initial state** once, then on **every change** of
root/parent/distance, on **every send**, and at shutdown:
```c
printf("time=%.01f\tRoot=%d\tparent=%d\tdistance=%d\n", time_ms/1e3f, root_id, parent_id, distance);
printf("time=%.01f\tMessage sent to all neighbors\n",   time_ms/1e3f);
printf("time=%.01f\tLifetime expired. Shutting down.\n", time_ms/1e3f);
```
- No parent (a node that is its own root) → print the literal `parent=NULL`.
- `distance` = cost to Root. Time uses `clock_gettime(CLOCK_REALTIME)`.

### Constants (from the provided `bf.h` — do not redefine)
`PORT = 6789`, `HELLO_TIMEOUT = 2`, `ROOT_TIMEOUT = 3 × HELLO_TIMEOUT = 6` (seconds).

### How the BPDU drives the user interface
The node maintains one belief — its current best **BPDU `(myRoot, myCost, myID, expTime)`** — and the
UI is just the inputs that seed it and the printed events that report its changes.

**CLI inputs → state:** `node_id` becomes **`myID`** *and* the initial **`myRoot`** (each node starts
as its own root, default BPDU `(myID, 0, myID)`); the `cost1…` values are the link weights used in
relaxation (`candidate = neighbor.myCost + cost[link]`); `netproc_address`/port locate `netproc`;
`lifetime` sets the shutdown deadline.

**The printed `Root/parent/distance` line is the current best BPDU, displayed:** `Root = myRoot`,
`distance = myCost`, and `parent =` the **ID of the neighbor on the root port** (learned from that
neighbor's `myID`; `NULL` when self-root). **`Message sent to all neighbors`** = one broadcast (link
byte `0xFF`) of that BPDU. A node only ever advertises *its own* best BPDU — it never relays others'.

| Event inside the node | Printed line(s) |
|---|---|
| Startup — adopt default belief | `Root=<myID> parent=NULL distance=0` (initial state, once) |
| Receive BPDU → relax → `myRoot`/`myCost` **changed** | `Root=… parent=… distance=…` **then** broadcast → `Message sent to all neighbors` |
| Receive BPDU → relax → **no change** | (nothing printed) |
| HELLO timer fires (keep-alive, no change) | broadcast → `Message sent to all neighbors` |
| `expTime` expires → reset to self-root | `Root=<myID> parent=NULL distance=0` + broadcast → `Message sent…` |
| Lifetime reached | `Lifetime expired. Shutting down.` |

**PDF example transcript, decoded** (`nodeproc … 100 5432 10 7 3 1`):
```
time=120.0  Root=100 parent=NULL distance=0   ← initial belief = default BPDU (100,0,100)
time=120.1  Message sent to all neighbors     ← first broadcast of (100,0,100)        [view change]
time=123.1  Message sent to all neighbors     ← HELLO tick, nothing changed
time=125.3  Root=95  parent=1   distance=3    ← heard a better BPDU (root 95); adopted it
time=125.4  Message sent to all neighbors     ← view changed → re-advertise (95,3,100)
time=125.6  Root=95  parent=217 distance=2    ← heard (95, cheaper) via neighbor 217; same root, lower cost
time=130.0  Lifetime expired. Shutting down.  ← lifetime elapsed
```
The PDF confirms lines 2 & 5 are sends due to a *view change* and line 3 is a *HELLO* send; the
`10 7 3 1` costs and the `3`/`2` distances are illustrative, not arithmetic to match.

### The topology file format
The topology is a **CSV edge list** — one line per link:
```
<endpoint1>,<endpoint2>,<cost>
```
- **endpoint1/endpoint2** — integer **node IDs** at the link's two ends (same ID space as `node_id`).
- **cost** — non-negative integer weight, used in relaxation.

How `netproc` reads it (`network_init` → `fscanf("%d,%d,%d\n", ...)` → `network_add_link`) implies:
1. **One line = one link** (an edge), not a node. There is **no node list** — a node exists by
   appearing as an endpoint. (Our file: 7 link lines ⇒ 5 implied nodes.)
2. **Undirected, symmetric cost** — one line adds the edge in *both* directions with the same cost, so
   each link is written once. A node's **degree** = how many lines mention it ⇒ how many `cost*` args
   its command line takes.
   - The two columns are **not roles** — just "the two ends," in arbitrary order. A node may appear in
     endpoint1 on one line and endpoint2 on another (`network_add_link(u,v)` treats them symmetrically).
     Degree counts lines mentioning the node in **either** column (so does `costs.sh`:
     `$1==id || $2==id`). E.g. 15762 is endpoint1 in `15762,25824,213` and endpoint2 in `7416,15762,136`.
3. **Line order matters** — `netproc` appends to each node's `edgelist` in file order, which *is* the
   node's **link/port index order** (link 0,1,2…) and the order `costs.sh` emits costs. This is what
   makes the positional `cost1 cost2 …` line up with the right links.
4. **Single source of truth** — used by `netproc` (build the graph) and by `costs.sh` (produce a node's
   costs); both must use the *same* file for a consistent run.
5. No header, no comments; parsing stops at the first non-`int,int,int` line (e.g. the trailing blank).

This one file fixes the **node set**, the **links + costs**, and each node's **link indexing**.

## 0.4 How `netproc` works and how a node uses it

> Verified against the **alpha** source `source files/PA2_resource_alpha/netproc.c`, which is
> authoritative and supersedes the original in `PA2_resources.zip`.

### Startup
`netproc topology.csv` reads the topology (lines `endpoint1,endpoint2,cost`), builds an in-memory
graph, opens a TCP socket, `bind`s port **6789**, sets `SO_REUSEADDR`, and `listen`/`accept`s
(`main()`).

### Connect handshake (what a node does first)
1. `socket(AF_INET, SOCK_STREAM, 0)` then `connect()` to `netproc_address:6789`.
2. Send the node's **ID as a 4-byte `htonl(int)`**. `netproc` does `recv(..., 4, MSG_WAITALL)`,
   `ntohl`, looks the ID up (`network_find`); unknown ID → it closes that socket; known → binds the
   socket to that node's slot.
3. `netproc` sets **`TCP_NODELAY`**; our node should too (spec requires it) so small frames aren't
   delayed by Nagle.

### Message framing — fixed 128-byte frames
All messages are exactly **128 bytes**; `netproc` always `recv`/`send`s 128 with `MSG_WAITALL`. Because
TCP is a byte stream, our node must read/write **whole 128-byte frames** (a `readn`/`writen` loop) —
never assume one `read` returns a full frame.

**Sending**, the node's frame is:
```
byte 0      : destination link  (0-based local link index, or 0xFF = broadcast to all neighbors)
bytes 1..127: the 127-byte payload (our BPDU: myRoot, myCost, myID, expTime, + padding)
```
In `server_forward()`, `netproc` reads `buff[0]` as the sender's link index `i`, finds the target via
`node->edgelist[i]` (`i == 0xFF` → all neighbors).

### The link byte on delivery (the important alpha fix)
When forwarding, `netproc` **overwrites byte 0 with the *receiver's* link number** and copies the
payload after it:
```c
size_t edge = node->edgelist[i].edge;   // reverse-link index at the destination
w->buf[0] = edge;                       // receiver's link number
memcpy(w->buf + 1, buff + 1, 127);      // payload, unchanged
```
For **both** unicast and the `0xFF` broadcast. So a **received** frame is:
```
byte 0      : MY link number — which of *my* links this message arrived on
bytes 1..127: the sender's 127-byte payload
```
On receive, byte 0 tells the node which local link the message came in on → index `cost[link]`
directly when relaxing. (The *original* netproc omitted this byte, which would have forced inferring
the link from the sender's ID; the alpha version removes that complication.) The `edge` field added in
`network_add_link()` is what makes this possible: each directed edge stores its destination node index
*and* the reverse edge's index at that destination.

**Byte 0 is rewritten, the payload is not.** Its meaning differs per direction: leaving the sender it's
"which of *my* links to send out" (or `0xFF` = broadcast); arriving at the receiver it's "which of *my*
links this came in on." Sender and receiver link numbers usually **differ** (links are indexed
per node), and `0xFF` is never delivered as-is — a broadcast is fanned out into one frame per neighbor,
each stamped with *that* neighbor's own link number. Example — **7416 broadcasts** (byte0 `0xFF`);
7416's links are `0→25824,1→53021,2→15762`, so the copies arrive as:

| arrives at | its link back to 7416 | byte 0 delivered |
|---|---|---|
| 25824 (`0→56908,1→7416,2→15762`) | index 1 | `0x01` |
| 53021 (`0→7416,1→56908,2→15762`) | index 0 | `0x00` |
| 15762 (`0→25824,1→7416,2→53021`) | index 1 | `0x01` |

**Byte 0 is a link index on both ends — never a node ID.** At the sender it's the *destination* link;
at the receiver it's the *ingress* link (which of my links this arrived on). It identifies the source
neighbor only *indirectly* (each link has one neighbor). The source node's actual **ID comes from the
payload's `myID`**, not from byte 0. The receiver thus gets two complementary facts: **byte 0** → which
local link → charge `cost[byte0]`; **payload `myID`** → which neighbor → the `parent` ID if that link
is the root port. Together they bind "link `k` / `cost[k]`" ↔ "neighbor `myID`."

### Link ↔ cost ordering (why CLI costs line up)
`netproc` builds each node's `edgelist` in topology-file line order; `costs.sh` emits costs in that
same order. So **link index `k` (0-based) ↔ `cost(k+1)` on the command line** ↔ the `k`-th neighbor in
`netproc`'s `edgelist`. That consistent ordering turns "message arrived on link `k`" into "the cost to
that neighbor is `cost[k]`."

### Disconnect / teardown
If `send`/`recv` fails or returns 0, `netproc` prints `Removing socket of #<id>`, closes it, clears the
slot. That's how `netproc` notices a node that exited (e.g. after its lifetime) — relevant to the
root-crash / `expTime` recovery we must implement.

## 0.5 Worked walk-through (topology.csv)

Links (`a,b,cost`):
```
56908-25824:23   7416-25824:62   7416-53021:155   15762-25824:213
7416-15762:136   56908-53021:76   15762-53021:79
```
Nodes: 56908, 25824, 7416, 53021, 15762. Smallest ID = **7416**, so every node converges to
`Root = 7416`.

BPDU exchange sketch (`(myRoot, myCost, myID)`, lexicographic compare):
1. Each node starts advertising its default `(myID, 0, myID)`.
2. 7416 keeps advertising `(7416, 0, 7416)`. Neighbors 25824, 53021, 15762 hear a smaller root and
   adopt 7416 with `cost = link-cost` → 62, 155, 136; parent = 7416.
3. 56908 isn't adjacent to 7416. It hears 7416 via 25824 (`23 + 62 = 85`) and via 53021
   (`76 + 155 = 231`) → picks 85, parent 25824.

Converged state (our correctness oracle):

| node  | distance | parent |
|-------|----------|--------|
| 7416  | 0        | NULL   |
| 25824 | 62       | 7416   |
| 53021 | 155      | 7416   |
| 15762 | 136      | 7416   |
| 56908 | 85       | 25824  |

### Example commands + per-node link order (from this topology)
`costs.sh` prints, for an ID, the cost of every line where it appears, **in file order** — which is the
same order `netproc` builds that node's `edgelist`, so position = link index. The command line gives
**only costs** (positional); the node learns each neighbor's ID at runtime from incoming BPDUs.

| node_id | command (local, 60 s) | link0 | link1 | link2 |
|---|---|---|---|---|
| 56908 | `nodeproc 127.0.0.1 56908 60 23 76`     | 23→25824 | 76→53021 | — |
| 25824 | `nodeproc 127.0.0.1 25824 60 23 62 213` | 23→56908 | 62→7416  | 213→15762 |
| 7416  | `nodeproc 127.0.0.1 7416 60 62 155 136` | 62→25824 | 155→53021 | 136→15762 |
| 53021 | `nodeproc 127.0.0.1 53021 60 155 76 79` | 155→7416 | 76→56908 | 79→15762 |
| 15762 | `nodeproc 127.0.0.1 15762 60 213 136 79`| 213→25824 | 136→7416 | 79→53021 |

E.g. `nodeproc 127.0.0.1 7416 60 62 155 136`: play node 7416, connect to local `netproc:6789`, run 60 s,
with link costs 62/155/136. The "→neighbor" column is *not* passed on the command line — it's what the
node discovers via the link byte + each BPDU's `myID`.

## 0.6 Open questions to resolve when coding
- Confirm the alpha link-byte behavior and 0-based indexing **empirically** (Step 1: build `netproc` +
  a probe).
- Final 127-byte payload layout (`myRoot`, `myCost`, `myID`, `expTime`) and byte order, and whether
  `expTime` travels in seconds or milliseconds (Step 3).

## 0.7 Code
_None — Step 0 is understanding only. Code begins in Step 1 (verify netproc) per the plan._

---

# Step 1 — Verify netproc empirically

**Status:** ✅ done. We built the provided `netproc` and a throwaway probe and confirmed the wire
protocol live, so `nodeproc` is built on observed facts, not just a source reading.

## 1.1 Concepts (what we verify and why)
We *inferred* netproc's behavior from the alpha source (§0.4). Before building `nodeproc` on those
assumptions, confirm them by watching real bytes. Six things to check:
1. **Connect handshake** — connect + send ID as `htonl(int)` is accepted (wrong ID rejected).
2. **Frame size** — messages are exactly **128 bytes**.
3. **Byte-0 rewrite** — on receive, byte 0 = the *receiver's* ingress link, not what the sender wrote.
4. **0-based indexing + sender↔receiver asymmetry** as predicted in §0.4.
5. **Broadcast (`0xFF`) fan-out** — one send → one frame per neighbor, each with that neighbor's byte 0.
6. **Payload pass-through** — bytes 1–127 arrive unchanged.

## 1.2 Implementation steps (what we did)
1. **Built `netproc`** from the alpha sources. macOS clang has **no `<threads.h>`** (C11 threads), so we
   added a tiny **`threads.h`→pthreads shim** at `build/shim/threads.h` and compiled with
   `-Ibuild/shim -pthread`. The shim is *local-test only* — `netproc.c` is the course's file and the
   grader builds on Linux (real `<threads.h>`).
2. **Wrote a throwaway probe** `build/probe.c` (≈110 lines, not in the submission). Two modes:
   `recv` (connect, send `htonl(id)`, then dump byte 0 + first payload bytes of each 128-byte frame)
   and `send <link>` (connect, send a frame with `buf[0]=link` or `255`, payload marker
   `DE AD BE EF` + a fill pattern). It uses Stevens-style `readn`/`writen` loops, which also validates
   128-byte framing.
3. **Ran two experiments** via `build/step1_probe.sh`: one `netproc` + `recv` probes on 7416's three
   neighbors, then 7416 does **unicast** (`send 0`) and **broadcast** (`send 255`).
4. **Sandbox note:** the assistant's Bash environment blocks *all* TCP binds (`bind`/`connect` →
   `EPERM`, even for loopback), so the run must happen in the **user's own terminal** (`! sh
   build/step1_probe.sh`). Compilation and reading the resulting log files work fine inside the tool.

## 1.3 Results (observed = predicted ✅)
Predictions came straight from the §0.4 link table. Observed byte 0 at each receiver:

| experiment | receiver | byte 0 observed | predicted | payload |
|---|---|---|---|---|
| A — 7416 `send 0` (unicast → 25824) | 25824 | **1** | 1 | `DE AD BE EF…` intact |
| B — 7416 `send 255` (broadcast) | 25824 | **1** | 1 | intact |
| B | 53021 | **0** | 0 | intact |
| B | 15762 | **1** | 1 | intact |

`netproc`'s own trace confirmed the unicast (`…to send to #25824 (neighbor 0)`) and the broadcast
fan-out (`…to send to all neighbors` → `Sending to #25824 / #53021 / #15762`). All six hypotheses
held: handshake ✅, 128-byte frames ✅, **byte-0 rewrite to receiver's link** ✅, 0-based asymmetry
(7416 link 0 → 25824 link 1; broadcast 1/0/1) ✅, broadcast fan-out ✅, payload verbatim ✅.

## 1.4 Code (throwaway, lives in `build/`, not submitted)
- `build/shim/threads.h` — C11-threads→pthreads shim for building `netproc` on macOS.
- `build/probe.c` — the probe client (`readn`/`writen`, connect+`htonl(id)`+`TCP_NODELAY`, recv/send).
- `build/step1_probe.sh` — orchestrates the two experiments and prints observed vs. predicted.
- Build/run commands recorded in `COMMANDS.md`.

## 1.5 Takeaways that lock the `nodeproc` design
- **Send:** frame = `[dest-link (0-based) | 127-byte payload]`; `0xFF` = broadcast. Use `writen`.
- **Receive:** `readn` exactly 128; `frame[0]` = our ingress link → index `cost[frame[0]]`; decode the
  BPDU from `frame[1..]`. The sender's ID is in the payload (`myID`), *not* byte 0.
- Reuse the validated `readn`/`writen` pattern from the probe in our `net_util`.

---

# Step 2 — Scaffold + connection lifecycle

**Status:** ✅ done. First real submission code: a `nodeproc` that connects, registers, prints its
initial state, and shuts down at `lifetime` — no protocol logic yet.

## 2.1 Concepts
- **Connection lifecycle (happy path):** `connect()` → set `TCP_NODELAY` → send ID as `htonl(int)` →
  `netproc` registers us → print **initial state** → run until `lifetime` → print shutdown line →
  `close()`.
- **Initial state = the default BPDU, displayed:** `Root=<node_id> parent=NULL distance=0`, printed
  once (each node starts as its own root).
- **Time model (decided):** capture `t0` at startup (`clock_gettime(CLOCK_REALTIME)`); print **elapsed**
  seconds as `time_ms/1e3f`, so the first line is `time=0.0`. Reproducible; the PDF's `120.0`-style
  numbers are just their scenario clock.
- **Lifetime (decided):** whole seconds; converted to a ms deadline internally.
- **Deliberately *not* yet:** message send/receive, BPDU decode, relaxation, HELLO/expTime timers.

## 2.2 Implementation steps (what we built)
1. **`bf.h`** — provided constants + `FRAME_LEN/PAYLOAD_LEN/LINK_BROADCAST` + the `bf_msg` struct.
2. **`net_util.{h,c}`** — `readn`/`writen` (from the Step-1 probe), `connect_to_netproc()` (socket +
   `TCP_NODELAY` + connect), `send_id()` (`htonl`), and `clock_start()`/`now_ms()`.
3. **`nodeproc.c`** — parse `argv` (addr, id, lifetime, costs[]), connect, send ID, print initial state,
   then a `select()`-to-lifetime loop that drains (ignores) any frame, then the shutdown line + clean
   close.
4. **`Makefile`** — builds `nodeproc` from `nodeproc.c` + `net_util.c` with `-Wall -Wextra -std=gnu11`
   (`gnu11` exposes POSIX/BSD prototypes on both Linux and macOS without feature-test macros), + `clean`.
5. **`README`** — skeleton with team placeholders and the required section headings.

## 2.3 Results
- `make` builds cleanly, **no warnings** under `-Wall -Wextra`.
- Live run (`build/step2_run.sh`, node 7416, 3 s) — observed exactly:
  ```
  time=0.0        Root=7416       parent=NULL     distance=0
  time=3.0        Lifetime expired. Shutting down.
  ```
  and `netproc` logged `Accepted connection …` / `Received connection from ID: 7416`. Milestone met:
  connect + register + initial state + clean lifetime shutdown.

## 2.4 Code (submission files)
`bf.h`, `net_util.h`, `net_util.c`, `nodeproc.c`, `Makefile`, `README` (repo root). Run-test harness
`build/step2_run.sh` is throwaway. Notable choices: `std=gnu11` for portability; `select()` loop shape
chosen now so Step 5 only has to add deadlines/handlers, not restructure.

## 2.5 Next
Step 3 — define the 127-byte payload encode/decode and the per-link neighbor table; start parsing
received frames (`frame[0]` → link, `frame[1..]` → `bf_msg`).

---

# Step 3 — Neighbor/link table + message encode/decode

**Status:** ✅ done. Defined the wire serialization and the per-link table; `nodeproc` now parses and
stores received frames (no relaxation/sending yet).

## 3.1 Concepts
- **Explicit, portable serialization** — encode/decode the four `bf_msg` fields byte-by-byte with
  `htonl`/`ntohl` (via `memcpy`), so the wire format never depends on struct padding or host endianness.
- **`expTime` unit = milliseconds** (consistent with `now_ms`; never printed; `ROOT_TIMEOUT` → 6000 ms).
- **Frame helpers** mirror Step 1: `build_send_frame` sets `frame[0]=link` (`0xFF`=broadcast) then the
  payload; `parse_recv_frame` reads `frame[0]` as our ingress link and decodes `frame[1..]`.
- **Per-link neighbor table** indexed by link number — binds byte 0 (which link → `cost[link]`) with
  the payload's `myID` (which neighbor), exactly as in §0.4.

## 3.2 Implementation steps (what we built)
1. **`msg.{h,c}`** — `msg_encode`/`msg_decode` + `build_send_frame`/`parse_recv_frame`.
2. **`nodeproc.c`** — added `link_t`/`node_t`; fill `link[i].cost` from argv; init self-root state; the
   receive path now `parse_recv_frame` → stores `nbrID/nbrRoot/nbrCost/nbrExp/recvMs`, `heard=1`
   (with an optional `-DBF_DEBUG` stderr decode log). Graded stdout unchanged.
3. **`Makefile`** — added `msg.o`.
4. **FAQ-driven `net_util` fixes** (from `source files/more sources and lecturerer notes.txt`):
   `readn` now uses `recv(..., MSG_WAITALL)`, `writen` uses `send(..., MSG_NOSIGNAL)`, and `main()`
   does `signal(SIGPIPE, SIG_IGN)`. This stops a closed peer/`netproc` from killing the node with
   `SIGPIPE` — essential for the disconnect/root-crash "extreme scenarios". (`MSG_NOSIGNAL` is
   `#define`d to 0 on macOS, where the `SIGPIPE` ignore covers it.)
5. **`build/test_msg.c`** — throwaway no-socket round-trip test (runs in the assistant sandbox).

## 3.3 Packet structure (verified)
Frame = **128 bytes**: `[ byte 0 = link ][ bytes 1..127 = payload ]`. Payload (network byte order):

| bytes | field | example value | hex on wire |
|---|---|---|---|
| 0–3 | myRoot | 7416 | `00 00 1c f8` |
| 4–7 | myCost | 85 | `00 00 00 55` |
| 8–11 | myID | 56908 | `00 00 de 4c` |
| 12–15 | expTime (ms) | 6000 | `00 00 17 70` |
| 16–126 | padding | 0 | `00 …` |

## 3.4 Results
- `make` builds clean, no warnings. `build/test_msg` passes: round-trip preserves all four fields, the
  link byte, and zero padding — output matched the table above exactly.
- `nodeproc`'s receive path compiles and stores into the link table; it only *fires* once a neighbor
  actually sends (Step 4), where we'll confirm it live with `-DBF_DEBUG`.

## 3.5 Code (submission files)
Added `msg.h`, `msg.c`; updated `net_util.c` (socket flags), `nodeproc.c` (table + decode), `Makefile`.
Throwaway: `build/test_msg.c`.

## 3.6 Next
Step 4 — Bellman-Ford relaxation (over self + each `heard` link: `nbrRoot`, `nbrCost + cost[link]`;
pick by root↑ → cost↑ → next-hop-ID↑), update `myRoot/myCost/parent`, and **send** (broadcast `0xFF`
on change). This makes received data actually drive state — and exercises the Step-3 decode live.

---

# Step 4 — Bellman-Ford relaxation + send rule

**Status:** ✅ done. Received data now drives state; all five nodes converge to the §0.5 oracle.

## 4.1 Concepts
- **Relaxation (Bellman equation):** best candidate over **self** `(myID, 0)` and each **heard** link
  `j` `(nbrRoot, nbrCost + cost[j])`, chosen lexicographically by **root↑ → cost↑ → next-hop ID↑**.
  The winner sets `myRoot/myCost` and the parent (`-1`/NULL if self).
- **Send rule (spec + FAQ):** broadcast our own BPDU (`0xFF`) **iff** the view (`myRoot` or `myCost`)
  changed **or** the **HELLO** timer fired (every `HELLO_TIMEOUT`=2 s); every send resets HELLO.
- **HELLO belongs here:** without periodic resend a node whose view never changes (the root) would
  announce only once, and a slightly-late neighbor would never hear it → no convergence.
- **Output order:** on a view change, print the state line **then** send; on HELLO, send only; at
  startup, initial state + one announcement.
- **Deferred to Step 5:** real `expTime` aging + root-crash recovery (Step 4 `expTime` is a
  placeholder).

## 4.2 Implementation steps (`nodeproc.c`)
1. `relax(node)` — the comparison above; returns whether `(myRoot,myCost)` changed.
2. `print_state(node)` — the exact `Root/parent/distance` line (`parent=NULL` when self-root).
3. `send_update(node, fd, &helloDeadline)` — build BPDU → `build_send_frame(0xFF)` → `writen` → print
   "Message sent" → reset HELLO.
4. Startup announces once; event loop `select`s on `min(helloDeadline, lifetimeDeadline)`, relaxes on
   receipt (print+send on change), and HELLO-sends when due.

## 4.3 Results — converged to the oracle (all 5 nodes ✅)
Every node's final line matched the oracle. The 2-hop node 56908 shows the algorithm working
(intermediate estimates improving, then HELLO keep-alives, then shutdown):
```
time=0.0  Root=56908 parent=NULL  distance=0     ← self-root
time=0.0  Root=53021 parent=53021 distance=76    ← hears 53021's self-announce
time=0.0  Root=7416  parent=53021 distance=231   ← 53021 now advertises 7416 (155+76)
time=0.0  Root=7416  parent=25824 distance=85    ← better path via 25824 (62+23) → final
time=2.0  Message sent to all neighbors          ← HELLO
time=4.0  Message sent to all neighbors          ← HELLO
time=5.0  Lifetime expired. Shutting down.
```
(Convergence completes sub-100 ms, so it all prints at `time=0.0`; HELLO sends confirm the 2 s timer.)

## 4.4 Code (submission files)
Added `relax`, `print_state`, `send_update` to `nodeproc.c`; event loop now manages the HELLO + lifetime
deadlines. Throwaway: `build/step4_run.sh` (5-node convergence check).

## 4.5 Next
Step 5 — make `expTime` real: store an absolute expiry per learned root, send the *adjusted remaining*
time, refresh only on a *fresher* same-root message, add a **root-expiry** deadline to the loop, and on
expiry reset to self-root (root-crash recovery). Test: kill 7416 mid-run → survivors re-elect 15762
after `ROOT_TIMEOUT`.

---

# Step 5 — expTime aging, root-expiry timer, root-crash recovery

**Status:** ✅ done. The whole protocol now works, including recovery when the root crashes.

## 5.1 Concepts
- **`expTime` = freshness.** Per link we keep an absolute **`expDeadline`** = "when this neighbor's root
  info dies, on our clock." On receipt `(R,C,id,E)` at `now`: same root → `expDeadline =
  max(expDeadline, now+E)` (**increase only on a fresher message**); different root → `now+E`.
- **Liveness gate in `relax()`** — a link is a candidate only if `heard && now < expDeadline`. Expired
  links are ignored, so a dead root is dropped and the node falls back to **self** (`myRoot=myID,
  myCost=0`) — the spec's "expTime expires → reset to own ID".
- **Sent `expTime`** — self-root → `ROOT_TIMEOUT` (the "infinite" source, re-emitted each HELLO); else
  the remaining `expDeadline - now` (= received value minus how long we held it).
- **Root-expiry timer** — the loop blocks on `min(HELLO, root-expiry, lifetime)`, where root-expiry is
  the parent link's `expDeadline`; waking there forces a re-relax that drops the dead parent. (This is
  the spec's "queue of timeout values": HELLO / ROOT_TIMEOUT / lifetime.)
- Because `ROOT_TIMEOUT = 3×HELLO_TIMEOUT`, a live root keeps deadlines pushed ~6 s out; when it dies
  they lapse ≈ `last_emit + ROOT_TIMEOUT` network-wide and the tree re-forms.

## 5.2 Implementation steps (`nodeproc.c`)
1. Added `long expDeadline` to `link_t`.
2. Receive store applies the increase-only / new-root deadline rule.
3. `relax()` skips links with `now >= expDeadline`.
4. `root_expiry(node)` → parent's `expDeadline` or `LONG_MAX` (self-root).
5. `send_update()` sends the real `expTime` (self → `ROOT_TIMEOUT*1000`, else clamped remaining).
6. Event loop adds root-expiry to the `select` min and **re-relaxes every iteration** (so time-based
   expiry is caught with or without a message).

## 5.3 Results — recovery verified ✅
Killed node 7416 at ~5 s; the four survivors re-elected **15762** at `t≈10 s` (= last 7416 emission +
`ROOT_TIMEOUT`) and converged to the recovery oracle (15762:0/NULL, 53021:79, 56908:155, 25824:178).
Node 25824's transcript shows the entire cycle:
```
time=0.0   Root=25824 parent=NULL            ← self-root
time=0.0   Root=7416  parent=7416 distance=62 ← converge to original root
time=2..8  Message sent (HELLO keep-alives)
time=10.0  Root=25824 parent=NULL            ← 7416 info EXPIRED → reset to self
time=10.0  Root=15762 parent=15762 distance=213
time=10.0  Root=15762 parent=56908 distance=178 ← re-elect + re-converge (better path)
time=12..16 HELLO ; time=17.4 Lifetime expired. Shutting down.
```

## 5.4 Code (submission files)
`nodeproc.c`: `link_t.expDeadline`, liveness gate in `relax`, `root_expiry`, real `expTime` in
`send_update`, three-deadline loop with per-iteration re-relax. Throwaway: `build/step5_run.sh`.

## 5.5 Status
The protocol is **functionally complete**: convergence, HELLO keep-alive, lexicographic election,
expTime aging, and root-crash recovery all verified. Step 6 is finalization — README (team info,
multi-timeout writeup), more extreme tests (varied start order/lifetimes, link/loop cases), and a
submission-contents check.

---

# Step 6 — Finalize: README, extreme tests, cleanup

**Status:** ✅ done (only team names/IDs remain to be filled into the README).

## 6.1 Concepts
Functionally the protocol is finished; Step 6 is about **quality + submission readiness**: complete
documentation, broaden test coverage to the spec's "non-trivial topologies / loops / different initial
states", and ensure the zip contains exactly the required files.

## 6.2 Implementation steps (what we did)
1. **README finalized** — Algorithm, Data structures, Packet structure, Multiple timeouts, and
   **Notes/anything unusual** (the `nodeproc`→`nodeproc` name discrepancy, elapsed-time choice, ms
   `expTime`, endianness-independent serialization, SIGPIPE/`MSG_NOSIGNAL`/`MSG_WAITALL`, `gnu11`),
   plus a Testing summary. (Team line still a placeholder.)
2. **Extreme-test harnesses** (throwaway, `build/`):
   - `build/topo_loop.csv` + `build/step6_loop.sh` — 4-node graph with cycles (ring + chord), root 10,
     including an equal-cost tie at node 30.
   - `build/step6_rejoin.sh` — kill the root, recover to 15762, restart 7416, reclaim root.
3. **Submission hygiene** — clean `make` builds `nodeproc`; repo-root submission set is exactly
   `README, Makefile, bf.h, net_util.{h,c}, msg.{h,c}, nodeproc.c` (no strays); `zip` command in
   `COMMANDS.md`.

## 6.3 Results
- Clean build from scratch, no warnings.
- **Loop topology** converged to oracle (10:0/NULL, 20:1/10, 40:1/10, 30:2/**20**) — tie-break picks
  the smaller next-hop neighbor (20), confirming cycle handling + deterministic tie resolution.
- **Root rejoin** — survivors re-elected 15762 after the kill, then **reclaimed 7416** when it
  restarted (a smaller root ID arriving wins immediately); all final states matched the original
  oracle.

### Generic topology testing (reference oracle)
Added `build/oracle.py` (computes the converged state — per-component root, Dijkstra distances,
smallest-next-hop tie-break) and `build/run_topo.sh` (runs netproc + a node per ID and auto-compares
observed vs oracle, OK/XX per row). Works on any `endpoint,endpoint,cost` CSV. All passed (every row
OK):

| topology | what it stresses | result |
|---|---|---|
| `topo_mesh` (root 5) | path depth + equal-cost tie at node 20 (→parent 8) | all OK |
| `topo_split` | **disconnected** → two independent roots (100, 400) | all OK |
| `topo_chain` (root 1) | deep 6-hop propagation | all OK |
| `topo_zero` (root 10) | zero-cost links (all distance 0) | all OK |
| `topo_loop` (root 10) | ring + chord cycles + tie | all OK |
| `topo_star` (root 10) | star whose **root is a leaf** (hub relays) | all OK |
| `topo_complete` (root 11) | complete K4; node 33 best via 22, not direct | all OK |
| `topo_diamond` (root 1) | two equal shortest paths → tie at node 4 (→2) | all OK |
| `topo_bigmesh` (root 3) | 10 nodes, non-obvious deep paths | all OK |

The disconnected case confirms Root = smallest ID **in the connected component**, not globally.

### Dynamic / timing scenarios (bespoke scripts) — all passed
| script | scenario | result |
|---|---|---|
| `test_nonroot_fail.sh` | kill intermediate 25824; root unchanged, others reroute | ✅ 56908 85→231/53021 |
| `test_late_join.sh` | 4 converge, then 56908 joins late | ✅ joins to full oracle |
| `test_stability.sh` | post-convergence behavior | ✅ STABLE (last change `time=0.0`), ~1 HELLO/2 s/node, no send storms |
| `test_robust.sh` | short lifetime (1 s) + unknown ID | ✅ both exit cleanly (exit 0); netproc rejects unknown ID |

Note: a single *link* failure can't be tested (the provided netproc's topology is static — the only
failure primitive is killing a whole node). On an unexpected netproc disconnect the node currently
reuses the lifetime shutdown line — graceful, though the message is generic.

## 6.4 Remaining
Fill `## Team` in the README with members' names + ID numbers, then `make clean` and zip the eight
submission files. Everything else is submission-ready.






