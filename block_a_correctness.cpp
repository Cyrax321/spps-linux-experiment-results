// =================================================================
// SPPS Experiment Suite — Block A: Correctness Proofs
// 12,006 automated encode-decode-verify tests
// =================================================================
// block_a_correctness.cpp — SPPS Correctness Proofs (Block A)
// Tests A1–A5: General fuzzing, directed edge stress, sibling order stress,
// directed edge worked example, boundary cases.
// Algorithm identical to benchmark.cpp — no modifications.

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>

using namespace std;

// =========================================================================
// SPPS ENCODE (from benchmark.cpp, verbatim)
// Input: children[v] = ordered list of children of node v, root = 1
// Output: sequence S of signed integers
// =========================================================================
struct EncodedTree {
    vector<long long> S;     // SPPS sequence
    int n;                   // number of nodes
};

EncodedTree sppsEncode(int n, const vector<vector<int>>& children) {
    EncodedTree result;
    result.n = n;

    if (n == 1) {
        // Single node, empty sequence
        return result;
    }

    vector<int> ChildRank(n + 2, 0);
    vector<int> parent(n + 2, 0);
    vector<long long> neighborSum(n + 2, 0);
    int r = 1;

    for (int u = 1; u <= n; ++u) {
        for (int k = 0; k < (int)children[u].size(); ++k) {
            int v = children[u][k];
            ChildRank[v] = k;
            parent[v] = u;
            neighborSum[u] += v;
            neighborSum[v] += u;
        }
    }

    // Virtual root injection
    long long N = n + 2;
    int v_virt = n + 1;
    ChildRank[v_virt] = (int)children[r].size();
    parent[v_virt] = r;
    neighborSum[r] += v_virt;
    neighborSum[v_virt] += r;

    // Degree array
    vector<int> D(n + 2, 0);
    for (int i = 1; i <= n; i++)
        D[i] = (int)children[i].size() + 1;
    D[r] = (int)children[r].size() + 1;
    D[v_virt] = 1;

    vector<long long> S;
    S.reserve(n);
    int ptr = 1;
    while (ptr <= n + 1 && D[ptr] != 1) ptr++;
    int leaf = ptr;

    for (int i = 1; i <= n; ++i) {
        if (D[leaf] == 0) break;
        long long P = neighborSum[leaf];
        long long d = (parent[leaf] == P) ? 1 : ((parent[P] == leaf) ? -1 : 1);
        long long k = ChildRank[leaf];
        S.push_back(d * (P * N + k));
        D[P]--;
        neighborSum[P] -= leaf;

        if (D[P] == 1 && P < ptr) leaf = (int)P;
        else {
            ptr++;
            while (ptr <= n + 1 && D[ptr] != 1) ptr++;
            leaf = ptr;
        }
    }
    S.pop_back(); // Remove virtual root edge

    result.S = S;
    return result;
}

// =========================================================================
// SPPS DECODE (from benchmark.cpp, verbatim)
// Input: sequence S
// Output: root, children map (CSR: M, BasePointer, out_deg)
// =========================================================================
struct DecodedTree {
    int root;
    int n;
    vector<int> M;           // CSR child array
    vector<int> BasePointer;
    vector<int> out_deg;
    vector<int> parent;      // parent[v] = parent of v (0 if root)
    vector<int> childRank;   // childRank[v] = position among siblings
    vector<int> direction;   // direction[v] = +1 or -1 for edge to parent
};

DecodedTree sppsDecode(const vector<long long>& S) {
    DecodedTree result;
    int m = (int)S.size();
    int n = m + 1;
    result.n = n;
    long long N = n + 2;

    if (m == 0) {
        // Single node
        result.root = 1;
        result.M = {};
        result.BasePointer = {0, 0};
        result.out_deg = {0, 0};
        result.parent = {0, 0};
        result.childRank = {0, 0};
        result.direction = {0, 0};
        return result;
    }

    vector<int> D_dec(n + 2, 1);
    vector<int> out_deg(n + 2, 0);

    for (long long V_i : S) {
        long long P_i = abs(V_i) / N;
        D_dec[P_i]++;
        out_deg[P_i]++;
    }

    // BasePointer from out_deg
    vector<int> BasePointer(n + 2, 0);
    int offset = 0;
    for (int v = 1; v <= n + 1; ++v) {
        BasePointer[v] = offset;
        offset += out_deg[v];
    }

    vector<int> M(offset + 1, 0);

    // Track parent/rank/direction for verification
    vector<int> par(n + 2, 0);
    vector<int> cRank(n + 2, 0);
    vector<int> dir(n + 2, 0);

    int ptr = 1;
    while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++;
    int leaf = ptr;

    for (long long V_i : S) {
        long long absV = abs(V_i);
        long long P_i = absV / N;
        long long k_i = absV % N;
        int d_i = (V_i > 0) ? 1 : -1;

        M[BasePointer[P_i] + k_i] = leaf;

        // Record parent/rank/direction
        if (d_i == 1) {
            // leaf's parent is P_i (upward edge)
            par[leaf] = (int)P_i;
            cRank[leaf] = (int)k_i;
            dir[leaf] = 1;
        } else {
            // P_i's parent is leaf (downward edge)
            par[(int)P_i] = leaf;
            cRank[(int)P_i] = (int)k_i;
            dir[(int)P_i] = -1;
        }

        D_dec[P_i]--;
        if (D_dec[P_i] == 1 && P_i < ptr) leaf = (int)P_i;
        else {
            ptr++;
            while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++;
            leaf = ptr;
        }
    }

    // Identify root
    int u_dec = -1, v_dec = -1;
    for (int i = 1; i <= n + 1; i++) {
        if (D_dec[i] == 1) {
            if (u_dec == -1) u_dec = i;
            else v_dec = i;
        }
    }
    int root = (u_dec == n + 1) ? v_dec : u_dec;

    result.root = root;
    result.M = M;
    result.BasePointer = BasePointer;
    result.out_deg = out_deg;
    result.parent = par;
    result.childRank = cRank;
    result.direction = dir;

    return result;
}

// =========================================================================
// HELPER: Build children list from decoded tree
// =========================================================================
vector<vector<int>> getDecodedChildren(const DecodedTree& dt) {
    vector<vector<int>> children(dt.n + 2);
    for (int v = 1; v <= dt.n + 1; ++v) {
        for (int k = 0; k < dt.out_deg[v]; ++k) {
            int child = dt.M[dt.BasePointer[v] + k];
            if (child > 0 && child <= dt.n) {
                children[v].push_back(child);
            }
        }
    }
    return children;
}

// =========================================================================
// HELPER: Generate random tree with uniform branching
// =========================================================================
vector<vector<int>> generateRandomTree(int n, mt19937& rng) {
    vector<vector<int>> children(n + 1);
    for (int i = 2; i <= n; ++i) {
        uniform_int_distribution<int> dist(1, i - 1);
        int p = dist(rng);
        children[p].push_back(i);
    }
    return children;
}

// =========================================================================
// A1 — General Fuzzing (10,000 random trees)
// =========================================================================
int testGeneralFuzzing() {
    cout << "\n========== A1: GENERAL FUZZING (10,000 random trees) ==========" << endl;
    int pass = 0, fail = 0;
    mt19937 rng(12345);

    for (int t = 0; t < 10000; ++t) {
        // Size between 10 and 10000, uniform
        uniform_int_distribution<int> sizeDist(10, 10000);
        int n = sizeDist(rng);

        // Generate random tree
        vector<vector<int>> children = generateRandomTree(n, rng);

        // Encode
        EncodedTree enc = sppsEncode(n, children);

        // Decode
        DecodedTree dec = sppsDecode(enc.S);

        // Verify root identity
        if (dec.root != 1) {
            if (fail < 5) cerr << "  FAIL tree " << t << " n=" << n << ": root=" << dec.root << " expected 1" << endl;
            fail++;
            continue;
        }

        // Verify n matches
        if (dec.n != n) {
            if (fail < 5) cerr << "  FAIL tree " << t << " n=" << n << ": decoded n=" << dec.n << endl;
            fail++;
            continue;
        }

        // Verify topology: for each node, children match exactly
        bool topOk = true;
        vector<vector<int>> decChildren = getDecodedChildren(dec);
        for (int v = 1; v <= n; ++v) {
            if (children[v].size() != decChildren[v].size()) {
                topOk = false;
                break;
            }
            for (int k = 0; k < (int)children[v].size(); ++k) {
                if (children[v][k] != decChildren[v][k]) {
                    topOk = false;
                    break;
                }
            }
            if (!topOk) break;
        }

        if (topOk) pass++;
        else {
            if (fail < 5) cerr << "  FAIL tree " << t << " n=" << n << ": topology mismatch" << endl;
            fail++;
        }

        if ((t + 1) % 1000 == 0) cout << "  Progress: " << (t + 1) << "/10000" << endl;
    }

    cout << "  RESULT: " << pass << "/10000 passed (" << fail << " failed)" << endl;
    cout << "  STATUS: " << (pass == 10000 ? "PASS" : "FAIL") << endl;
    return pass;
}

// =========================================================================
// A2 — Directed Edge Stress Test (1,000 trees)
// =========================================================================
int testDirectedEdgeStress() {
    cout << "\n========== A2: DIRECTED EDGE STRESS TEST (1,000 trees) ==========" << endl;
    int pass = 0, fail = 0;
    mt19937 rng(54321);

    for (int t = 0; t < 1000; ++t) {
        uniform_int_distribution<int> sizeDist(10, 1000);
        int n = sizeDist(rng);

        // Generate random tree
        vector<vector<int>> children = generateRandomTree(n, rng);

        // Encode — direction is computed inside sppsEncode per the algorithm:
        // d = (parent[leaf] == P) ? 1 : -1
        // The d values are embedded in the sequence via sign(V_i)
        EncodedTree enc = sppsEncode(n, children);

        // Decode
        DecodedTree dec = sppsDecode(enc.S);

        // Verify that every edge direction is preserved
        // For each entry in sequence, the sign encodes direction
        // After decoding, check that parent relationships match original
        bool dirOk = true;
        for (int v = 2; v <= n; ++v) {
            // Find original parent of v
            int origParent = 0;
            for (int u = 1; u <= n; ++u) {
                for (int c : children[u]) {
                    if (c == v) { origParent = u; break; }
                }
                if (origParent) break;
            }
            // In decoded tree, v should be a child of origParent at some position
            bool found = false;
            for (int k = 0; k < dec.out_deg[origParent]; ++k) {
                if (dec.M[dec.BasePointer[origParent] + k] == v) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                dirOk = false;
                if (fail < 5) cerr << "  FAIL tree " << t << ": node " << v << " not child of " << origParent << endl;
                break;
            }
        }

        if (dirOk) pass++;
        else fail++;

        if ((t + 1) % 200 == 0) cout << "  Progress: " << (t + 1) << "/1000" << endl;
    }

    cout << "  RESULT: " << pass << "/1000 passed (" << fail << " failed)" << endl;
    cout << "  STATUS: " << (pass == 1000 ? "PASS" : "FAIL") << endl;
    return pass;
}

// =========================================================================
// A3 — Sibling Order Stress Test (1,000 trees)
// =========================================================================
int testSiblingOrderStress() {
    cout << "\n========== A3: SIBLING ORDER STRESS TEST (1,000 trees) ==========" << endl;
    int pass = 0, fail = 0;
    mt19937 rng(99999);

    for (int t = 0; t < 1000; ++t) {
        // Build tree where each parent has 2-20 children
        uniform_int_distribution<int> childDist(2, 20);
        int n = 1;
        vector<vector<int>> children;
        children.resize(2); // At least root

        // BFS-style construction
        vector<int> frontier = {1};
        while (!frontier.empty() && n < 500) {
            vector<int> nextFrontier;
            for (int p : frontier) {
                int numChildren = childDist(rng);
                if (n + numChildren > 1000) numChildren = max(0, 1000 - n);
                for (int c = 0; c < numChildren && n < 1000; ++c) {
                    n++;
                    if ((int)children.size() <= n) children.resize(n + 1);
                    children[p].push_back(n);
                    nextFrontier.push_back(n);
                }
            }
            frontier = nextFrontier;
            if (n >= 500) break;
        }

        // Make sure all nodes have entries
        children.resize(n + 1);

        // Encode
        EncodedTree enc = sppsEncode(n, children);

        // Decode
        DecodedTree dec = sppsDecode(enc.S);

        // Verify sibling ordering: for every node v, its children must
        // appear in the exact same left-to-right order as in the original
        bool orderOk = true;
        vector<vector<int>> decChildren = getDecodedChildren(dec);

        for (int v = 1; v <= n; ++v) {
            if (children[v].size() != decChildren[v].size()) {
                orderOk = false;
                if (fail < 5) cerr << "  FAIL tree " << t << ": node " << v
                    << " children count mismatch (" << children[v].size()
                    << " vs " << decChildren[v].size() << ")" << endl;
                break;
            }
            for (int k = 0; k < (int)children[v].size(); ++k) {
                if (children[v][k] != decChildren[v][k]) {
                    orderOk = false;
                    if (fail < 5) cerr << "  FAIL tree " << t << ": node " << v
                        << " child[" << k << "] = " << decChildren[v][k]
                        << " expected " << children[v][k] << endl;
                    break;
                }
            }
            if (!orderOk) break;
        }

        if (orderOk) pass++;
        else fail++;

        if ((t + 1) % 200 == 0) cout << "  Progress: " << (t + 1) << "/1000" << endl;
    }

    cout << "  RESULT: " << pass << "/1000 passed (" << fail << " failed)" << endl;
    cout << "  STATUS: " << (pass == 1000 ? "PASS" : "FAIL") << endl;
    return pass;
}

// =========================================================================
// A4 — Directed Edge Worked Example (n=9)
// =========================================================================
bool testDirectedEdgeWorkedExample() {
    cout << "\n========== A4: DIRECTED EDGE WORKED EXAMPLE (n=9) ==========" << endl;

    // Manually constructed 9-node tree:
    //         1
    //        / \
    //       2   3
    //      /|   |\
    //     4  5  6  7
    //        |     |
    //        8     9
    //
    // Edges with direction assignments:
    //   1->2 (d=+1), 1->3 (d=+1)
    //   2->4 (d=+1), 2->5 (d=-1)
    //   3->6 (d=-1), 3->7 (d=+1)
    //   5->8 (d=-1), 7->9 (d=+1)
    //
    // 4 edges d=+1: (1,2), (1,3), (2,4), (3,7)  -- wait, let me recount
    // Actually d is computed by the algorithm based on Prüfer leaf removal order,
    // not manually assigned. The d value captures whether the removed leaf's
    // neighbor is its parent (d=+1) or its child (d=-1).
    // Let me just build the tree and let the algorithm assign d values.

    int n = 9;
    vector<vector<int>> children(n + 1);
    children[1] = {2, 3};
    children[2] = {4, 5};
    children[3] = {6, 7};
    children[5] = {8};
    children[7] = {9};

    cout << "  Input tree (n=9, root=1):" << endl;
    cout << "    1 -> {2, 3}" << endl;
    cout << "    2 -> {4, 5}" << endl;
    cout << "    3 -> {6, 7}" << endl;
    cout << "    5 -> {8}" << endl;
    cout << "    7 -> {9}" << endl;
    cout << endl;

    // Manual encode with trace
    vector<int> ChildRank(n + 2, 0);
    vector<int> parent(n + 2, 0);
    vector<long long> neighborSum(n + 2, 0);
    int r = 1;

    for (int u = 1; u <= n; ++u) {
        for (int k = 0; k < (int)children[u].size(); ++k) {
            int v = children[u][k];
            ChildRank[v] = k;
            parent[v] = u;
            neighborSum[u] += v;
            neighborSum[v] += u;
        }
    }

    long long N = n + 2; // = 11
    int v_virt = n + 1;  // = 10
    ChildRank[v_virt] = (int)children[r].size();
    parent[v_virt] = r;
    neighborSum[r] += v_virt;
    neighborSum[v_virt] += r;

    vector<int> D(n + 2, 0);
    for (int i = 1; i <= n; i++)
        D[i] = (int)children[i].size() + 1;
    D[r] = (int)children[r].size() + 1;
    D[v_virt] = 1;

    cout << "  Initial degrees (augmented tree, v_virt=" << v_virt << ", N=" << N << "):" << endl;
    for (int i = 1; i <= n + 1; i++)
        cout << "    D[" << i << "] = " << D[i] << endl;
    cout << endl;

    cout << "  ENCODING TRACE:" << endl;
    cout << "  " << left << setw(6) << "Step" << setw(6) << "Leaf"
         << setw(6) << "P" << setw(6) << "k" << setw(6) << "d"
         << setw(16) << "omega" << "Formula" << endl;
    cout << "  " << string(70, '-') << endl;

    vector<long long> S;
    S.reserve(n);
    int ptr = 1;
    while (ptr <= n + 1 && D[ptr] != 1) ptr++;
    int leaf = ptr;
    int step = 0;

    int d_plus_count = 0, d_minus_count = 0;

    for (int i = 1; i <= n; ++i) {
        if (D[leaf] == 0) break;
        long long P = neighborSum[leaf];
        long long d = (parent[leaf] == P) ? 1 : ((parent[P] == leaf) ? -1 : 1);
        long long k = ChildRank[leaf];
        long long omega = d * (P * N + k);
        S.push_back(omega);
        step++;

        if (d == 1) d_plus_count++; else d_minus_count++;

        cout << "  " << left << setw(6) << step << setw(6) << leaf
             << setw(6) << P << setw(6) << k << setw(6) << d
             << setw(16) << omega
             << d << " * (" << P << " * " << N << " + " << k << ") = " << omega
             << endl;

        D[P]--;
        neighborSum[P] -= leaf;

        if (D[P] == 1 && P < ptr) leaf = (int)P;
        else {
            ptr++;
            while (ptr <= n + 1 && D[ptr] != 1) ptr++;
            leaf = ptr;
        }
    }
    S.pop_back(); // Remove virtual root edge
    step--;

    cout << endl;
    cout << "  Direction counts: d=+1: " << d_plus_count << ", d=-1: " << d_minus_count << endl;
    cout << "  Final sequence (length " << S.size() << "): [";
    for (int i = 0; i < (int)S.size(); i++) {
        if (i > 0) cout << ", ";
        cout << S[i];
    }
    cout << "]" << endl;

    // Decode
    cout << "\n  DECODING..." << endl;
    DecodedTree dec = sppsDecode(S);

    cout << "  Decoded root: " << dec.root << endl;
    cout << "  Decoded edges:" << endl;
    vector<vector<int>> decChildren = getDecodedChildren(dec);
    for (int v = 1; v <= dec.n; ++v) {
        if (!decChildren[v].empty()) {
            cout << "    " << v << " -> {";
            for (int k = 0; k < (int)decChildren[v].size(); k++) {
                if (k > 0) cout << ", ";
                cout << decChildren[v][k];
            }
            cout << "}" << endl;
        }
    }

    // Verify
    bool ok = true;
    if (dec.root != 1) { cerr << "  Root mismatch!" << endl; ok = false; }

    for (int v = 1; v <= n; ++v) {
        if (children[v].size() != decChildren[v].size()) {
            cerr << "  Children count mismatch at node " << v << endl;
            ok = false;
            break;
        }
        for (int k = 0; k < (int)children[v].size(); ++k) {
            if (children[v][k] != decChildren[v][k]) {
                cerr << "  Child order mismatch at node " << v << " pos " << k << endl;
                ok = false;
                break;
            }
        }
        if (!ok) break;
    }

    cout << "\n  STATUS: " << (ok ? "PASS" : "FAIL") << endl;
    return ok;
}

// =========================================================================
// A5 — Boundary Cases
// =========================================================================
bool testBoundaryCases() {
    cout << "\n========== A5: BOUNDARY CASES ==========" << endl;
    bool allOk = true;

    // --- n=1: single node ---
    {
        int n = 1;
        vector<vector<int>> children(n + 1);
        EncodedTree enc = sppsEncode(n, children);
        cout << "  n=1: sequence length = " << enc.S.size() << " (expected 0)" << endl;
        bool ok = (enc.S.size() == 0);
        cout << "  n=1: " << (ok ? "PASS" : "FAIL") << endl;
        if (!ok) allOk = false;
    }

    // --- n=2, d=+1: root 1 -> child 2 ---
    {
        int n = 2;
        vector<vector<int>> children(n + 1);
        children[1] = {2};
        EncodedTree enc = sppsEncode(n, children);
        cout << "\n  n=2 (1->2): sequence length = " << enc.S.size()
             << " (expected 1), S = [";
        for (auto v : enc.S) cout << v;
        cout << "]" << endl;

        DecodedTree dec = sppsDecode(enc.S);
        bool ok = (dec.root == 1 && dec.n == 2);

        // Verify child
        vector<vector<int>> dc = getDecodedChildren(dec);
        ok = ok && (dc[1].size() == 1 && dc[1][0] == 2);

        cout << "  n=2 (d=+1): root=" << dec.root << " " << (ok ? "PASS" : "FAIL") << endl;
        if (!ok) allOk = false;
    }

    // --- n=2, reversed (2 -> 1 as parent): Still root=1 since node 1 is root ---
    // Actually in our encoding root is always 1. d=+1/-1 is determined by
    // the algorithm based on leaf removal. Let's test with root=1, child=2.
    // The direction d is not user-controllable — it's computed by the algorithm.
    // For n=2: leaf=2 is removed, its neighbor is 1 (parent), so d=+1.
    {
        cout << "\n  n=2 direction check:" << endl;
        int n = 2;
        vector<vector<int>> children(n + 1);
        children[1] = {2};
        EncodedTree enc = sppsEncode(n, children);
        if (enc.S.size() == 1) {
            long long V = enc.S[0];
            int d = (V > 0) ? 1 : -1;
            cout << "    omega=" << V << ", d=" << d
                 << " (expected d=+1 since leaf 2's neighbor is parent 1)" << endl;
            bool ok = (d == 1);
            cout << "    direction preserved: " << (ok ? "PASS" : "FAIL") << endl;
            if (!ok) allOk = false;
        }
    }

    // --- n=3, chain: 1->2->3 ---
    {
        cout << "\n  n=3 chain (1->2->3):" << endl;
        int n = 3;
        vector<vector<int>> children(n + 1);
        children[1] = {2};
        children[2] = {3};

        EncodedTree enc = sppsEncode(n, children);
        DecodedTree dec = sppsDecode(enc.S);

        cout << "    Sequence length: " << enc.S.size() << " (expected 2)" << endl;
        cout << "    S = [";
        for (int i = 0; i < (int)enc.S.size(); i++) {
            if (i > 0) cout << ", ";
            cout << enc.S[i];
        }
        cout << "]" << endl;

        vector<vector<int>> dc = getDecodedChildren(dec);
        bool ok = (dec.root == 1) &&
                  (dc[1].size() == 1 && dc[1][0] == 2) &&
                  (dc[2].size() == 1 && dc[2][0] == 3);
        cout << "    root=" << dec.root << " topology: " << (ok ? "PASS" : "FAIL") << endl;
        if (!ok) allOk = false;
    }

    // --- n=3, star: 1->2, 1->3 ---
    {
        cout << "\n  n=3 star (1->{2,3}):" << endl;
        int n = 3;
        vector<vector<int>> children(n + 1);
        children[1] = {2, 3};

        EncodedTree enc = sppsEncode(n, children);
        DecodedTree dec = sppsDecode(enc.S);

        cout << "    Sequence length: " << enc.S.size() << " (expected 2)" << endl;
        cout << "    S = [";
        for (int i = 0; i < (int)enc.S.size(); i++) {
            if (i > 0) cout << ", ";
            cout << enc.S[i];
        }
        cout << "]" << endl;

        vector<vector<int>> dc = getDecodedChildren(dec);
        bool ok = (dec.root == 1) &&
                  (dc[1].size() == 2 && dc[1][0] == 2 && dc[1][1] == 3);
        cout << "    root=" << dec.root << " children[1]={";
        for (int i = 0; i < (int)dc[1].size(); i++) {
            if (i) cout << ",";
            cout << dc[1][i];
        }
        cout << "} " << (ok ? "PASS" : "FAIL") << endl;
        if (!ok) allOk = false;
    }

    cout << "\n  OVERALL: " << (allOk ? "ALL PASS" : "SOME FAILED") << endl;
    return allOk;
}

// =========================================================================
// MAIN
// =========================================================================
int main() {
    cout << "=================================================================" << endl;
    cout << " BLOCK A — SPPS CORRECTNESS PROOFS" << endl;
    cout << " Pure C++ · No external dependencies" << endl;
    cout << "=================================================================" << endl;

    bool allPass = true;

    // A1
    int a1 = testGeneralFuzzing();
    if (a1 != 10000) allPass = false;

    // A2
    int a2 = testDirectedEdgeStress();
    if (a2 != 1000) allPass = false;

    // A3
    int a3 = testSiblingOrderStress();
    if (a3 != 1000) allPass = false;

    // A4
    bool a4 = testDirectedEdgeWorkedExample();
    if (!a4) allPass = false;

    // A5
    bool a5 = testBoundaryCases();
    if (!a5) allPass = false;

    cout << "\n=================================================================" << endl;
    cout << " BLOCK A SUMMARY" << endl;
    cout << "=================================================================" << endl;
    cout << "  A1 General Fuzzing:        " << a1 << "/10000 " << (a1 == 10000 ? "PASS" : "FAIL") << endl;
    cout << "  A2 Directed Edge Stress:   " << a2 << "/1000  " << (a2 == 1000 ? "PASS" : "FAIL") << endl;
    cout << "  A3 Sibling Order Stress:   " << a3 << "/1000  " << (a3 == 1000 ? "PASS" : "FAIL") << endl;
    cout << "  A4 Worked Example (n=9):   " << (a4 ? "PASS" : "FAIL") << endl;
    cout << "  A5 Boundary Cases:         " << (a5 ? "PASS" : "FAIL") << endl;
    cout << "\n  OVERALL: " << (allPass ? "ALL BLOCKS PASS" : "SOME BLOCKS FAILED") << endl;
    cout << "=================================================================" << endl;

    return allPass ? 0 : 1;
}
