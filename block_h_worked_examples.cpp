// block_h_worked_examples.cpp — Worked Examples for Paper (Block H)
// H1: Complete encoding trace for n=7 standard tree (all d=+1)
// H2: Complete decoding trace for same n=7 tree

#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>

using namespace std;

// =========================================================================
// H1 — Complete Encoding Trace (n=7 standard tree)
// =========================================================================
void h1_encoding_trace() {
    cout << "=================================================================" << endl;
    cout << " H1: COMPLETE ENCODING TRACE (n=7)" << endl;
    cout << "=================================================================" << endl;

    // Clean 7-node tree, all edges parent->child (d=+1):
    //         1
    //        / \
    //       2   3
    //      / \   \
    //     4   5   6
    //             |
    //             7
    int n = 7;
    vector<vector<int>> children(n + 1);
    children[1] = {2, 3};
    children[2] = {4, 5};
    children[3] = {6};
    children[6] = {7};

    cout << "\n  INPUT TREE (n=7, root=1):" << endl;
    cout << "         1" << endl;
    cout << "        / \\" << endl;
    cout << "       2   3" << endl;
    cout << "      / \\   \\" << endl;
    cout << "     4   5   6" << endl;
    cout << "             |" << endl;
    cout << "             7" << endl;
    cout << "\n  Edge list: {(1,2), (1,3), (2,4), (2,5), (3,6), (6,7)}" << endl;

    // Step 1: Setup arrays
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

    cout << "\n  STEP 1: Compute parent[], ChildRank[], neighborSum[]" << endl;
    cout << "  " << left << setw(8) << "Node" << setw(10) << "Parent"
         << setw(12) << "ChildRank" << setw(16) << "NeighborSum" << endl;
    cout << "  " << string(46, '-') << endl;
    for (int i = 1; i <= n; i++) {
        cout << "  " << left << setw(8) << i << setw(10) << parent[i]
             << setw(12) << ChildRank[i] << setw(16) << neighborSum[i] << endl;
    }

    // Step 2: Virtual node injection
    long long N = n + 2; // = 9
    int v_virt = n + 1;  // = 8
    ChildRank[v_virt] = (int)children[r].size(); // = 2
    parent[v_virt] = r;
    neighborSum[r] += v_virt;
    neighborSum[v_virt] += r;

    cout << "\n  STEP 2: Virtual node injection" << endl;
    cout << "    v_virt = " << v_virt << " (node n+1)" << endl;
    cout << "    N = n + 2 = " << N << " (radix for encoding)" << endl;
    cout << "    Attach v_virt=" << v_virt << " as child of root r="
         << r << " at rank " << ChildRank[v_virt] << endl;
    cout << "    neighborSum[" << r << "] updated to " << neighborSum[r] << endl;

    // Step 3: Degree array
    vector<int> D(n + 2, 0);
    for (int i = 1; i <= n; i++)
        D[i] = (int)children[i].size() + 1;
    D[r] = (int)children[r].size() + 1;
    D[v_virt] = 1;

    cout << "\n  STEP 3: Degree array D[] (unrooted augmented tree)" << endl;
    cout << "    Formula: D[v] = |children(v)| + 1 (for parent edge)" << endl;
    cout << "    D[root] = |children(root)| + 1 (for v_virt edge)" << endl;
    cout << "    D[v_virt] = 1" << endl;
    cout << "  " << left << setw(10) << "Node" << setw(10) << "|children|"
         << setw(10) << "D[v]" << setw(15) << "Is leaf?" << endl;
    cout << "  " << string(45, '-') << endl;
    for (int i = 1; i <= n + 1; i++) {
        int ch = (i <= n) ? (int)children[i].size() : 0;
        cout << "  " << left << setw(10) << i << setw(10) << ch
             << setw(10) << D[i] << setw(15) << (D[i] == 1 ? "YES" : "")
             << endl;
    }

    // Step 4: Prüfer-like encoding
    cout << "\n  STEP 4: Prüfer-like encoding (leaf removal)" << endl;
    cout << "  " << left << setw(6) << "Step" << setw(8) << "Leaf"
         << setw(8) << "P" << setw(8) << "k" << setw(6) << "d"
         << setw(20) << "ω = d·(P·N+k)" << setw(30) << "Action" << endl;
    cout << "  " << string(76, '-') << endl;

    vector<long long> S;
    S.reserve(n);
    int ptr = 1;
    while (ptr <= n + 1 && D[ptr] != 1) ptr++;
    int leaf = ptr;
    int step = 0;

    for (int i = 1; i <= n; ++i) {
        if (D[leaf] == 0) break;
        long long P = neighborSum[leaf];
        long long d = (parent[leaf] == P) ? 1 : ((parent[P] == leaf) ? -1 : 1);
        long long k = ChildRank[leaf];
        long long omega = d * (P * N + k);
        S.push_back(omega);
        step++;

        string action = "Remove " + to_string(leaf) + ", D[" +
                        to_string((int)P) + "]=" + to_string(D[(int)P]) +
                        "->" + to_string(D[(int)P] - 1);

        cout << "  " << left << setw(6) << step << setw(8) << leaf
             << setw(8) << P << setw(8) << k << setw(6) << d
             << setw(20) << omega << action << endl;

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

    cout << "\n  Drop last entry (virtual root edge)." << endl;
    cout << "\n  FINAL SEQUENCE S (length " << S.size() << "):" << endl;
    cout << "    S = [";
    for (int i = 0; i < (int)S.size(); i++) {
        if (i > 0) cout << ", ";
        cout << S[i];
    }
    cout << "]" << endl;
    cout << "    (All values positive → all d=+1 → all edges parent→child)" << endl;

    // Now do H2 decoding from this sequence
    cout << "\n\n=================================================================" << endl;
    cout << " H2: COMPLETE DECODING TRACE (same n=7 tree)" << endl;
    cout << "=================================================================" << endl;

    cout << "\n  INPUT SEQUENCE S = [";
    for (int i = 0; i < (int)S.size(); i++) {
        if (i > 0) cout << ", ";
        cout << S[i];
    }
    cout << "]" << endl;
    cout << "  n = |S| + 1 = " << S.size() + 1 << endl;
    cout << "  N = n + 2 = " << (S.size() + 1 + 2) << endl;

    int n_dec = (int)S.size() + 1;
    long long N_dec = n_dec + 2;

    // Pass 1: Compute D_dec and out_deg
    vector<int> D_dec(n_dec + 2, 1);
    vector<int> out_deg(n_dec + 2, 0);

    cout << "\n  PASS 1: Extract parent labels, compute degrees" << endl;
    cout << "  " << left << setw(6) << "i" << setw(10) << "ω_i"
         << setw(10) << "|ω_i|" << setw(10) << "P_i"
         << setw(10) << "k_i" << setw(10) << "d_i" << endl;
    cout << "  " << string(56, '-') << endl;

    for (int i = 0; i < (int)S.size(); i++) {
        long long omega = S[i];
        long long absV = abs(omega);
        long long P_i = absV / N_dec;
        long long k_i = absV % N_dec;
        int d_i = (omega > 0) ? 1 : -1;
        D_dec[P_i]++;
        out_deg[P_i]++;

        cout << "  " << left << setw(6) << (i + 1) << setw(10) << omega
             << setw(10) << absV << setw(10) << P_i
             << setw(10) << k_i << setw(10) << d_i << endl;
    }

    cout << "\n  After Pass 1, D_dec[] (Prüfer degree = 1 + appearances):" << endl;
    for (int i = 1; i <= n_dec + 1; i++) {
        if (D_dec[i] > 0)
            cout << "    D_dec[" << i << "] = " << D_dec[i]
                 << (D_dec[i] == 1 ? "  (leaf)" : "") << endl;
    }

    // Compute BasePointer
    vector<int> BasePointer(n_dec + 2, 0);
    int offset = 0;
    for (int v = 1; v <= n_dec + 1; ++v) {
        BasePointer[v] = offset;
        offset += out_deg[v];
    }

    cout << "\n  BasePointer[] (CSR prefix sum from out_deg):" << endl;
    for (int v = 1; v <= n_dec + 1; v++) {
        if (out_deg[v] > 0)
            cout << "    BasePointer[" << v << "] = " << BasePointer[v]
                 << "  (out_deg=" << out_deg[v] << ")" << endl;
    }

    vector<int> M(offset + 1, 0);

    // Pass 2: Reconstruct
    cout << "\n  PASS 2: Reconstruct tree (fill CSR spatial map M[])" << endl;
    cout << "  " << left << setw(6) << "i" << setw(8) << "Leaf"
         << setw(8) << "P_i" << setw(8) << "k_i"
         << setw(25) << "M[BP[P]+k] = leaf" << endl;
    cout << "  " << string(55, '-') << endl;

    ptr = 1;
    while (ptr <= n_dec + 1 && D_dec[ptr] != 1) ptr++;
    leaf = ptr;

    // Reset D_dec for pass 2
    fill(D_dec.begin(), D_dec.end(), 1);
    for (long long V_i : S) {
        long long P_i = abs(V_i) / N_dec;
        D_dec[P_i]++;
    }

    ptr = 1;
    while (ptr <= n_dec + 1 && D_dec[ptr] != 1) ptr++;
    leaf = ptr;

    int step2 = 0;
    for (long long V_i : S) {
        long long absV = abs(V_i);
        long long P_i = absV / N_dec;
        long long k_i = absV % N_dec;
        step2++;

        string mEntry = "M[" + to_string(BasePointer[(int)P_i]) + "+" +
                        to_string((int)k_i) + "] = " + to_string(leaf);

        cout << "  " << left << setw(6) << step2 << setw(8) << leaf
             << setw(8) << P_i << setw(8) << k_i
             << setw(25) << mEntry << endl;

        M[BasePointer[(int)P_i] + (int)k_i] = leaf;
        D_dec[(int)P_i]--;
        if (D_dec[(int)P_i] == 1 && P_i < ptr) leaf = (int)P_i;
        else {
            ptr++;
            while (ptr <= n_dec + 1 && D_dec[ptr] != 1) ptr++;
            leaf = ptr;
        }
    }

    // Find root
    int u_dec = -1, v_dec = -1;
    for (int i = 1; i <= n_dec + 1; i++) {
        if (D_dec[i] == 1) {
            if (u_dec == -1) u_dec = i; else v_dec = i;
        }
    }
    int root = (u_dec == n_dec + 1) ? v_dec : u_dec;

    cout << "\n  Remaining degree-1 nodes: " << u_dec << " and " << v_dec << endl;
    cout << "  Virtual node = " << (n_dec + 1) << ", so root = " << root << endl;

    cout << "\n  FINAL CSR MAP M[]:" << endl;
    for (int v = 1; v <= n_dec; v++) {
        if (out_deg[v] > 0) {
            cout << "    Node " << v << " children: [";
            for (int k = 0; k < out_deg[v]; k++) {
                if (k > 0) cout << ", ";
                cout << M[BasePointer[v] + k];
            }
            cout << "]" << endl;
        }
    }

    // Verify
    cout << "\n  RECONSTRUCTED TREE:" << endl;
    for (int v = 1; v <= n_dec; v++) {
        if (out_deg[v] > 0) {
            cout << "    " << v << " -> {";
            for (int k = 0; k < out_deg[v]; k++) {
                if (k > 0) cout << ", ";
                cout << M[BasePointer[v] + k];
            }
            cout << "}" << endl;
        }
    }

    // Match original
    bool match = true;
    for (int v = 1; v <= 7; v++) {
        int origDeg = (int)children[v].size();
        if (origDeg != out_deg[v]) { match = false; break; }
        for (int k = 0; k < origDeg; k++) {
            if (children[v][k] != M[BasePointer[v] + k]) { match = false; break; }
        }
        if (!match) break;
    }

    cout << "\n  VERIFICATION: Original tree " << (match ? "MATCHES" : "DOES NOT MATCH")
         << " reconstructed tree → " << (match ? "PASS" : "FAIL") << endl;
}

int main() {
    cout << "=================================================================" << endl;
    cout << " BLOCK H — WORKED EXAMPLES FOR PAPER" << endl;
    cout << " Step-by-step encoding and decoding traces" << endl;
    cout << "=================================================================" << endl;

    h1_encoding_trace();

    cout << "\n=================================================================" << endl;
    cout << " BLOCK H COMPLETE" << endl;
    cout << "=================================================================" << endl;

    return 0;
}
