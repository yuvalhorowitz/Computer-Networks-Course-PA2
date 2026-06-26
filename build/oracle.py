#!/usr/bin/env python3
"""Reference oracle for PA2: compute each node's converged (Root, parent, distance).

Mirrors bfproc's selection rule so test harnesses can auto-check observed output:
  - Root = smallest ID in the node's CONNECTED COMPONENT (so disconnected
    components each elect their own root).
  - distance = shortest-path cost to that root (non-negative weights).
  - parent = among neighbors on a shortest path (dist[nbr] + cost == dist[x]),
    the one with the smallest neighbor ID (bfproc's tie-break); NULL at the root.

Usage: python3 build/oracle.py <topology.csv>
Prints one line per node: "<id> Root=<r> parent=<p|NULL> distance=<d>"
"""
import sys
import heapq
from collections import defaultdict


def main():
    path = sys.argv[1]
    adj = defaultdict(list)
    nodes = set()
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            u, v, c = (int(x) for x in line.split(","))
            adj[u].append((v, c))
            adj[v].append((u, c))
            nodes.add(u)
            nodes.add(v)

    # Connected components -> each component's root is its smallest ID.
    comp_root = {}
    seen = set()
    for n in nodes:
        if n in seen:
            continue
        stack, comp = [n], []
        seen.add(n)
        while stack:
            x = stack.pop()
            comp.append(x)
            for y, _ in adj[x]:
                if y not in seen:
                    seen.add(y)
                    stack.append(y)
        r = min(comp)
        for x in comp:
            comp_root[x] = r

    # Dijkstra from each root over its component.
    INF = float("inf")
    dist = {}
    for root in set(comp_root.values()):
        d = {root: 0}
        pq = [(0, root)]
        while pq:
            dd, x = heapq.heappop(pq)
            if dd > d.get(x, INF):
                continue
            for y, c in adj[x]:
                nd = dd + c
                if nd < d.get(y, INF):
                    d[y] = nd
                    heapq.heappush(pq, (nd, y))
        dist.update(d)

    for x in sorted(nodes):
        root = comp_root[x]
        if x == root:
            print(f"{x} Root={root} parent=NULL distance=0")
            continue
        parent = None
        for y, c in adj[x]:
            if comp_root.get(y) == root and dist[y] + c == dist[x]:
                if parent is None or y < parent:
                    parent = y
        print(f"{x} Root={root} parent={parent} distance={dist[x]}")


main()
