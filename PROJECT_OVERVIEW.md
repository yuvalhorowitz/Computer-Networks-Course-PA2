# PA2 — Distributed Bellman-Ford: Project Overview & Plan

> Course: 512.4662 Introduction to Computer Networks (TAU), Boaz Patt-Shamir
> Assignment: Programming Assignment 2 — Distributed Bellman-Ford
> Due: **2026-07-14**

---

## 1. Theory background (from the course lectures)

This section follows the course's own treatment. The protocol the assignment asks us to build is, in
essence, the **control plane of the Spanning Tree Protocol (STP)** as taught in **Lecture 4** ("גשרים
ופרוטוקול העץ הפורש"), using the distributed **Bellman-Ford** algorithm presented there. Lecture 5
frames the same idea one layer up: *at the network layer, each destination is the root of its own
spanning tree, and the result is stored in routing tables.*

### 1.1 Where this comes from: bridges and the loop problem (Lecture 4)

Lecture 4 builds up the layer-2 devices: a **repeater** cleans and re-amplifies a signal; a **hub** is
a repeater with many ports that copies every signal to all legs (so everyone shares one collision
domain); a **bridge / Ethernet switch** is smarter — it works at the data-link layer and *learns*
which station sits on which port (storing `(source address, port)` in a table) so it only forwards
where needed.

The problem: if bridges are connected with **redundant links** (for reliability), a broadcast frame
can **circulate forever** around a loop, "eating up all the bandwidth." We can't just remove cables,
because redundancy is desirable. The solution Lecture 4 gives is the **Spanning Tree Protocol**: the
bridges run a *distributed* protocol, talk to each other, and agree on which ports to **disable** so
that the *active* topology is **connected and loop-free** (a spanning tree). Unused links stay
physically connected but carry no data — ready as backup.

### 1.2 The graph model

We treat the network as a weighted graph `G = (V, E)`:
- `V` = nodes (each `nodeproc` process, identified by an integer **ID**).
- `E` = links; each link `(u, v)` has a non-negative integer **cost** `c(u, v)`.
- A **path**'s cost is the sum of its link costs; the **distance** is the cheapest such cost.

Costs here are symmetric and non-negative, which is what lets the distributed algorithm converge.

### 1.3 STP step 1 — electing the root (lowest ID)

Lecture 4 splits STP into layered steps. **Step 1 is root election**: every bridge has an ID; each one
repeatedly advertises the **smallest ID it has heard of so far**, and the bridge with the **globally
smallest ID becomes the root**. Each node starts assuming *it* is the root and is corrected as smaller
IDs propagate in.

> **The crash problem (this is why `expTime` exists).** Lecture 4 explicitly notes the weakness: if
> the root loses power / dies, the others have **no way to know** — they keep believing the dead node
> is still the root. The assignment's **`expTime` / `ROOT_TIMEOUT`** mechanism is precisely the fix:
> root information is given a finite lifetime, and if it isn't refreshed in time, a node discards it
> and falls back to being its own root, letting the network re-elect the next-smallest ID.

### 1.4 STP step 2 — the distributed Bellman-Ford shortest-path tree

**Step 2: given a root, build a shortest-path (BFS-like) tree** toward it. Lecture 4 presents the
**Bellman-Ford** algorithm for this. Each node keeps a distance estimate `dist` to the root `s`:

```
dist_s = 0                                              (the root)
cost_v(u) = dist_u + W(u, v)        for a neighbor u of v
dist_v   = min { cost_v(u) : u is a neighbor of v }
```

(The lecture first uses `W(u,v) = 1` to get hop counts, then notes the weights can be arbitrary link
costs — which is the case in this assignment.) In words: *my distance to the root is the cheapest
over my neighbors of "the link to that neighbor + that neighbor's own distance to the root."* The
neighbor achieving the minimum becomes the **next hop** — the assignment's `parent`.

Lecture 4 highlights three properties that matter directly for our implementation:
1. **It runs continuously and stabilizes** — it is not a one-shot computation; nodes keep exchanging
   estimates and the system settles to the correct distances.
2. **It works asynchronously** — no global clock is needed; each node runs at its own pace. (Critical,
   since synchronizing a whole network is impractical — and `netproc` delivers messages
   asynchronously.)
3. **It stabilizes regardless of the initial values** — even if estimates don't start at infinity, the
   system still converges, so there is no dependence on history. (This is why the assignment says to
   test "with different initial distance values and with loops.")

### 1.5 STP step 3 — port roles (root / designated / blocked)

**Step 3** assigns each port a role, which is how loops actually get broken:
- **Root port** — the one port on a (non-root) node that gives the best path *toward* the root
  (lowest distance; ties broken by smallest ID). This is the assignment's `parent`.
- **Designated port** — on each segment, the port belonging to the node with the best advertisement
  *for that segment*; it forwards *away* from the root on behalf of the root.
- **Blocked port** — any port that is neither root nor designated is disabled, breaking the loop.

PA2 only requires each node to output its **root port** (`parent`), distance, and root — it does not
ask us to label designated/blocked ports — but this is where the earlier "root port vs. designated
port" distinction comes from.

### 1.6 The BPDU message and lexicographic comparison (the assignment's packet)

Lecture 4 gives the exact message format the assignment reuses — the **BPDU (Bridge Protocol Data
Unit)**:

```
(myRoot, myDist, myID)
```

Nodes **compare advertisements lexicographically**: first by `myRoot` (smaller root ID wins), then by
`myDist` (shorter distance wins), then by `myID` (smaller sender ID, as a tie-break). Every node's
**default message** — what it believes before hearing anything better — is:

```
(myID, 0, myID)        "I am my own root, at distance 0."
```

The assignment's packet is exactly this BPDU plus an `expTime` field (Section 3). `myDist` is the
assignment's `myCost`/`distance`.

### 1.7 Aging, refresh, and the two planes

Lecture 4 adds that a bridge **forgets old messages after a while** — information must be continually
**refreshed** or it is discarded. This single idea underlies three things the assignment requires:
- the **periodic re-advertisement** every `HELLO_TIMEOUT` (keep-alive), so neighbors' info stays fresh;
- the **`expTime` aging** of root information, which both enables **root-crash recovery** (1.3) and
  prevents stale routes from looping forever (the classic distance-vector *count-to-infinity* failure);
- the separation into a **control plane** (the BPDU exchange that builds the tree) and a **data plane**
  (which would forward user data only along the tree). PA2 implements the control plane.

### 1.8 How PA2 maps onto this theory

| Lecture-4 / STP concept | PA2 realization |
|---|---|
| Bridge running STP | one `nodeproc` process per node |
| Bridges talking over LAN segments | node processes exchanging 127-byte messages via `netproc` |
| BPDU `(myRoot, myDist, myID)` | the update message payload (+ `expTime`) |
| Default `(myID, 0, myID)` | initial state `myRoot=myID, myCost=0, parent=NULL` |
| Lexicographic compare | relaxation order: smaller root → smaller cost → smaller ID |
| Bellman-Ford `dist_v = min(dist_u + W)` | `myCost = min over neighbors (neighbor.myCost + cost[link])` |
| Root port | `parent` (next hop toward root) |
| Message aging / root-crash fix | `expTime` + `ROOT_TIMEOUT` |
| Continuous, asynchronous convergence | event loop driven by `select()` + the `HELLO` timer |

---

## 2. What the project is

We implement the **node process** of a distributed routing protocol. The protocol is a distributed
Bellman-Ford computation that behaves like the spanning-tree protocol (STP / IEEE 802.1): every node
cooperatively discovers, with no central coordinator,

1. a **root** — the smallest node ID in its connected component,
2. its **parent** — the next-hop neighbor on the shortest path toward that root, and
3. its **distance** — the total cost of that shortest path.

The network emulator, **`netproc`**, is *provided* by the course. We do **not** write it. We only
write the node executable, which must be built under the name **`nodeproc`**.

### How the system runs
1. `netproc` is started with a topology file and listens on TCP port **6789**.
2. Each node is started as a separate `nodeproc` process. It `connect()`s to `netproc` and sends its
   ID as a 4-byte `htonl(int)`.
3. Nodes never talk to each other directly — every message goes **through `netproc`**, which relays
   it to the correct neighbor over TCP.

### The topology (`topology.csv`)
Each line is `endpoint1,endpoint2,cost`. Our test topology:

```
56908-25824 : 23      7416-25824 : 62      7416-53021 : 155
15762-25824 : 213     7416-15762 : 136     56908-53021 : 76
15762-53021 : 79
```

Nodes: `56908, 25824, 7416, 53021, 15762`. The smallest ID is **7416**, so every node must converge
to `Root = 7416`.

### Command line
```
nodeproc <netproc_address> <node_id> <lifetime> <cost1> [cost2 ...]
```
- `netproc_address` — IP of the machine running `netproc` (`127.0.0.1` if local).
- `node_id` — this node's integer ID.
- `lifetime` — seconds to run before graceful shutdown.
- `cost1, cost2, ...` — costs of the incident links, in port order. The provided `costs.sh` script
  extracts these from the topology file:
  `nodeproc 127.0.0.1 7416 60 $(./costs.sh topology.csv 7416)`

---

## 3. The protocol in detail

### Update messages (like STP's BPDU)
Each message carries:
- `myRoot` — smallest ID the sender knows.
- `myCost` — sender's cost to `myRoot`.
- `myID` — sender's ID.
- `expTime` — time remaining before this information expires.

A node sends a message **iff**:
- its view (`myRoot` or `myCost`) **changed**, or
- a **`HELLO_TIMEOUT`** period elapsed since its last send (keep-alive).

### Expiration & root-crash recovery
- Each message carries `expTime`. On forwarding, it's reduced by the time the node held it
  (e.g. received at t=3 with expTime=100, resent at t=12 → expTime = 100 − (12 − 3) = 91).
- `expTime` may **increase** only when a *fresher* message (larger `expTime`) arrives for the **same
  root ID**.
- When `expTime` reaches 0, the node assumes the root is gone: it resets `myRoot` to its own ID and
  `myCost` to 0, then re-computes from its neighbors. This lets the network re-elect a new root.
- A node that *is* the root sends `expTime = ROOT_TIMEOUT` ("infinity").

### Constants (from the provided `bf.h`)
```
PORT          = 6789
HELLO_TIMEOUT = 2 seconds
ROOT_TIMEOUT  = 3 × HELLO_TIMEOUT = 6 seconds
```

### Output format (graded — must match exactly)
```
time=<t>	Root=<id>	parent=<id>	distance=<d>
time=<t>	Message sent to all neighbors
time=<t>	Lifetime expired. Shutting down.
```
The initial state is printed at startup; updates are printed on every change. `parent=NULL` when the
node is its own root. Time is computed with `clock_gettime(CLOCK_REALTIME)`.

---

## 4. Key technical finding (shapes the whole design)

The spec text says: *"In the incoming message, the leading byte will be the link number at the
receiver."* **But the provided `netproc.c` does not actually do this.**

In `server_forward()`, `netproc`:
- reads a 128-byte frame from the sender,
- treats byte 0 as the **sender's** link index (0-based — it indexes `edgelist[i]`),
- copies only the remaining payload (`memcpy(w->buf, buf + 1, 128)`) and forwards it,
- so the **receiver gets no link-number byte** — just the 127-byte payload (padded to 128).

**Consequence:** a node cannot tell *which of its links* a message arrived on from a header byte. It
must instead identify the sender by the `myID` field carried **inside** the message. This is why our
message format embeds `myID`, and why our very first implementation step is to *empirically verify*
the exact bytes a receiver sees before locking the format.

---

## 5. Design decisions (agreed)

| Decision | Choice | Why |
|---|---|---|
| Language | **C** | Matches `netproc.c`, `bf.h`, and the spec's `printf` examples. |
| Concurrency | **Single-threaded `select()`** with a timeout queue | Spec's primary suggestion; no locking; one timer drives HELLO, ROOT, and lifetime deadlines. |
| Link mapping | **Verify `netproc` behavior first**, then lock format | Resolves the receiver-link-byte ambiguity above before writing real code. |

### Message format (127-byte payload inside the 128-byte frame)
```c
typedef struct {
    uint32_t myRoot;   // smallest ID known to sender
    uint32_t myCost;   // sender's cost to myRoot
    uint32_t myID;     // sender's ID — lets the receiver identify the neighbor
    uint32_t expTime;  // ms until expiration; ROOT_TIMEOUT when sender is its own root
} bf_msg;              // 16 bytes, network byte order; rest of the 127 bytes zero-padded
```
- **Send:** 1 dest-link byte (`0xFF` = broadcast to all neighbors) + 127-byte payload, via Stevens'
  `writen()`.
- **Receive:** read a full 128-byte frame via `readn()`, parse the first 16 bytes.

### Algorithm (Bellman-Ford relaxation)
For each known neighbor `j` (cost `cost[j]`) plus "self", the candidate path is
`root = neighbor.myRoot`, `cost = neighbor.myCost + cost[j]` (self offers `root=myID, cost=0`).
Pick the best by: **smallest root → smallest total cost → smallest next-hop ID**. Update
`myRoot`, `myCost`, `parent`, and re-broadcast if the view changed.

### Timer model
One `select()` call blocks on the socket with a timeout equal to the nearest of three absolute
deadlines (HELLO, root-expiry, lifetime). On wake-up: handle any received frame, then fire whichever
deadlines have passed (send HELLO / expire root / shut down).

---

## 6. Files to be produced (submission is a flat zip)

| File | Purpose |
|---|---|
| `bf.h` | Provided constants, extended with the `bf_msg` struct and frame/protocol defines. |
| `net_util.h` / `net_util.c` | Stevens `writen`/`readn`, connect-to-netproc + `TCP_NODELAY` helper. |
| `nodeproc.c` | argv parsing, connect, neighbor table, relaxation, expiry, `select()` event loop, output. |
| `Makefile` | Builds the `nodeproc` executable; `clean` target. |
| `README` | Team names + IDs, algorithm summary, packet structure, multi-timeout design. |

---

## 7. Implementation steps

0. **Verify the wire protocol** — compile `netproc`, write a throwaway probe client, and confirm:
   is there a receiver link byte? what is the link index base (0 vs 1)? what frame length to read?
1. **`bf.h` + `net_util`** — constants, message struct, `writen`/`readn`, connect helper.
2. **`nodeproc.c`** — full algorithm, timers, event loop, exact output formatting.
3. **`Makefile` + `README`**.
4. **End-to-end verification** (below).

---

## 8. Verification

Expected steady state for `topology.csv` (Root = 7416 everywhere):

| node  | distance | parent (next hop) |
|-------|----------|-------------------|
| 7416  | 0        | NULL              |
| 25824 | 62       | 7416              |
| 53021 | 155      | 7416              |
| 15762 | 136      | 7416              |
| 56908 | 85       | 25824             |

Test procedure:
1. `make`; build `netproc` from the provided resources.
2. Start `netproc topology.csv`.
3. Start one `nodeproc` per node using `costs.sh`.
4. Confirm convergence to the table above; confirm "Message sent" appears on changes and every ~2s.
5. **Recovery test:** kill node `7416` mid-run; survivors should re-elect `15762` after
   `ROOT_TIMEOUT`.
6. Confirm each node prints the shutdown line and exits cleanly at its `lifetime`.
