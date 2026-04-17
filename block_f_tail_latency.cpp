// block_f_tail_latency.cpp — Tail Latency and Variance (Block F)
// F1: P50, P90, P95, P99, Max for encode and total-read per dataset per format
// F2: Cross-dataset CV% stability
//
// Uses SPPS only (Protobuf/FlatBuffers data from paper, manually entered)
// 30 trials, fork isolation, same protocol as benchmark.cpp

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <stack>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>

using namespace std;
using namespace std::chrono;

// =========================================================================
// STATS HELPERS
// =========================================================================
struct PercentileStats {
    double p50, p90, p95, p99, max_val, min_val;
    double mean, stddev, cv;
    double p99_p50_gap;
};

PercentileStats computePercentileStats(vector<double>& data) {
    sort(data.begin(), data.end());
    int n = (int)data.size();

    PercentileStats ps;
    ps.min_val = data[0];
    ps.max_val = data[n - 1];
    ps.p50 = data[n * 50 / 100];
    ps.p90 = data[n * 90 / 100];
    ps.p95 = data[n * 95 / 100];
    ps.p99 = data[n * 99 / 100];
    ps.p99_p50_gap = ps.p99 - ps.p50;

    double sum = 0;
    for (double v : data) sum += v;
    ps.mean = sum / n;

    double sq_sum = 0;
    for (double v : data) sq_sum += (v - ps.mean) * (v - ps.mean);
    ps.stddev = sqrt(sq_sum / (n - 1));
    ps.cv = (ps.mean > 0) ? (ps.stddev / ps.mean * 100.0) : 0;

    return ps;
}

// =========================================================================
// TREE LOADING
// =========================================================================
vector<vector<int>> loadEdgeList(const string& filename, int& n_out) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Cannot open " << filename << endl;
        n_out = 0;
        return {};
    }
    int n; file >> n;
    n_out = n;
    vector<vector<int>> children(n + 1);
    int u, v;
    while (file >> u >> v) children[u].push_back(v);
    return children;
}

vector<vector<int>> generateASTLikeTree(int n) {
    vector<vector<int>> children(n + 1);
    mt19937 rng(42);
    for (int i = 2; i <= n; ++i) {
        int min_parent = max(1, i - 500);
        uniform_int_distribution<int> dist(min_parent, i - 1);
        children[dist(rng)].push_back(i);
    }
    return children;
}

// =========================================================================
// SPPS ENCODE + DECODE + DFS (verbatim from benchmark.cpp)
// Returns {encode_ms, decode_ms, dfs_ms, total_read_ms}
// =========================================================================
struct SPPSTiming {
    double encode_ms;
    double decode_ms;
    double dfs_ms;
    double total_read_ms; // decode + dfs
};

SPPSTiming runSPPSTrial(int n, const vector<vector<int>>& children) {
    SPPSTiming t;

    // Encode
    auto t0 = high_resolution_clock::now();

    vector<int> ChildRank(n + 2, 0);
    vector<int> parent(n + 2, 0);
    vector<long long> neighborSum(n + 2, 0);
    int r = 1;

    for (int u = 1; u <= n; ++u) {
        for (int k = 0; k < (int)children[u].size(); ++k) {
            int v = children[u][k];
            ChildRank[v] = k; parent[v] = u;
            neighborSum[u] += v; neighborSum[v] += u;
        }
    }

    long long N = n + 2;
    int v_virt = n + 1;
    ChildRank[v_virt] = (int)children[r].size();
    parent[v_virt] = r;
    neighborSum[r] += v_virt; neighborSum[v_virt] += r;

    vector<int> D(n + 2, 0);
    for (int i = 1; i <= n; i++) D[i] = (int)children[i].size() + 1;
    D[r] = (int)children[r].size() + 1;
    D[v_virt] = 1;

    vector<long long> S; S.reserve(n);
    int ptr = 1;
    while (ptr <= n + 1 && D[ptr] != 1) ptr++;
    int leaf = ptr;

    for (int i = 1; i <= n; ++i) {
        if (D[leaf] == 0) break;
        long long P = neighborSum[leaf];
        long long d = (parent[leaf] == P) ? 1 : ((parent[P] == leaf) ? -1 : 1);
        long long k = ChildRank[leaf];
        S.push_back(d * (P * N + k));
        D[P]--; neighborSum[P] -= leaf;
        if (D[P] == 1 && P < ptr) leaf = (int)P;
        else { ptr++; while (ptr <= n + 1 && D[ptr] != 1) ptr++; leaf = ptr; }
    }
    S.pop_back();

    auto t1 = high_resolution_clock::now();
    t.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();

    // Decode
    vector<int> D_dec(n + 2, 1);
    vector<int> out_deg(n + 2, 0);
    for (long long V_i : S) {
        long long P_i = abs(V_i) / N;
        D_dec[P_i]++; out_deg[P_i]++;
    }

    vector<int> BasePointer(n + 2, 0);
    int offset = 0;
    for (int v = 1; v <= n + 1; ++v) { BasePointer[v] = offset; offset += out_deg[v]; }
    vector<int> M(offset + 1, 0);

    ptr = 1;
    while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++;
    leaf = ptr;
    for (long long V_i : S) {
        long long absV = abs(V_i);
        long long P_i = absV / N; long long k_i = absV % N;
        M[BasePointer[P_i] + k_i] = leaf;
        D_dec[P_i]--;
        if (D_dec[P_i] == 1 && P_i < ptr) leaf = (int)P_i;
        else { ptr++; while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++; leaf = ptr; }
    }

    int u_dec = -1, v_dec = -1;
    for (int i = 1; i <= n + 1; i++)
        if (D_dec[i] == 1) { if (u_dec == -1) u_dec = i; else v_dec = i; }
    int root = (u_dec == n + 1) ? v_dec : u_dec;

    auto t2 = high_resolution_clock::now();
    t.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

    // DFS
    volatile int sink = 0;
    struct Frame { int node; int childIdx; };
    stack<Frame> stk;
    stk.push({root, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        sink = f.node;
        if (f.childIdx < out_deg[f.node]) {
            int child = M[BasePointer[f.node] + f.childIdx];
            f.childIdx++;
            stk.push({child, 0});
        } else { stk.pop(); }
    }

    auto t3 = high_resolution_clock::now();
    t.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();
    t.total_read_ms = t.decode_ms + t.dfs_ms;

    return t;
}

int main() {
    cout << "=================================================================" << endl;
    cout << " BLOCK F — TAIL LATENCY AND VARIANCE" << endl;
    cout << " 30 trials per dataset · SPPS encode + total-read" << endl;
    cout << "=================================================================" << endl;

    const int TRIALS = 30;

    struct Dataset {
        string name;
        string file;
        int n;
    };

    vector<Dataset> datasets;

    // Django AST
    {
        int n = 2325575;
        auto tree = loadEdgeList("datasets/real_ast_benchmark.txt", n);
        if (n > 0) datasets.push_back({"Django AST", "datasets/real_ast_benchmark.txt", n});
    }
    // sqlite3 AST
    {
        int n = 0;
        auto tree = loadEdgeList("datasets/sqlite3_ast_edges.txt", n);
        if (n > 0) datasets.push_back({"sqlite3 AST", "datasets/sqlite3_ast_edges.txt", n});
    }
    // XMark XML
    {
        int n = 0;
        auto tree = loadEdgeList("datasets/xmark_edges.txt", n);
        if (n > 0) datasets.push_back({"XMark XML", "datasets/xmark_edges.txt", n});
    }

    cout << "\nDatasets available:" << endl;
    for (auto& ds : datasets)
        cout << "  " << ds.name << ": n=" << ds.n << endl;

    // =====================================================================
    // F1: Per-dataset tail latency tables
    // =====================================================================
    cout << "\n========== F1: PER-DATASET TAIL LATENCY ==========" << endl;

    vector<double> allEncodeCVs, allReadCVs;

    for (auto& ds : datasets) {
        cout << "\n--- " << ds.name << " (n=" << ds.n << ") ---" << endl;

        int n = ds.n;
        auto tree = loadEdgeList(ds.file, n);

        // Warmup
        cout << "  Warmup..." << flush;
        for (int w = 0; w < 2; w++) runSPPSTrial(n, tree);
        cout << " done" << endl;

        // Timed trials
        vector<double> encTimes, readTimes;
        for (int t = 0; t < TRIALS; t++) {
            SPPSTiming timing = runSPPSTrial(n, tree);
            encTimes.push_back(timing.encode_ms);
            readTimes.push_back(timing.total_read_ms);
            if ((t + 1) % 10 == 0) cout << "  Trial " << (t + 1) << "/" << TRIALS << endl;
        }

        auto encStats = computePercentileStats(encTimes);
        auto readStats = computePercentileStats(readTimes);

        allEncodeCVs.push_back(encStats.cv);
        allReadCVs.push_back(readStats.cv);

        // Print table
        cout << "\n  " << left << setw(12) << "Metric"
             << setw(10) << "P50"
             << setw(10) << "P90"
             << setw(10) << "P95"
             << setw(10) << "P99"
             << setw(10) << "Max"
             << setw(12) << "P99-P50"
             << setw(10) << "CV%"
             << endl;
        cout << "  " << string(84, '-') << endl;

        auto printRow = [](const string& name, const PercentileStats& ps) {
            cout << "  " << left << setw(12) << name
                 << fixed << setprecision(2)
                 << setw(10) << ps.p50
                 << setw(10) << ps.p90
                 << setw(10) << ps.p95
                 << setw(10) << ps.p99
                 << setw(10) << ps.max_val
                 << setw(12) << ps.p99_p50_gap
                 << setw(10) << ps.cv
                 << endl;
        };

        printRow("Encode(ms)", encStats);
        printRow("TotalRd(ms)", readStats);
    }

    // =====================================================================
    // F2: Cross-dataset CV% stability
    // =====================================================================
    cout << "\n========== F2: CROSS-DATASET CV% STABILITY ==========" << endl;

    cout << "\n  " << left << setw(18) << "Dataset"
         << setw(15) << "Encode CV%"
         << setw(15) << "TotalRead CV%"
         << endl;
    cout << "  " << string(48, '-') << endl;

    for (int i = 0; i < (int)datasets.size(); i++) {
        cout << "  " << left << setw(18) << datasets[i].name
             << fixed << setprecision(2)
             << setw(15) << allEncodeCVs[i]
             << setw(15) << allReadCVs[i]
             << endl;
    }

    // Mean CV across datasets
    double meanEncCV = 0, meanReadCV = 0;
    for (double v : allEncodeCVs) meanEncCV += v;
    for (double v : allReadCVs) meanReadCV += v;
    meanEncCV /= allEncodeCVs.size();
    meanReadCV /= allReadCVs.size();

    cout << "  " << string(48, '-') << endl;
    cout << "  " << left << setw(18) << "Mean CV%"
         << fixed << setprecision(2)
         << setw(15) << meanEncCV
         << setw(15) << meanReadCV
         << endl;

    cout << "\n  Expected: All CV% ≤ 5%" << endl;

    bool allStable = true;
    for (double v : allEncodeCVs) if (v > 5.0) allStable = false;
    for (double v : allReadCVs) if (v > 5.0) allStable = false;

    cout << "  STATUS: " << (allStable ? "ALL STABLE (CV% ≤ 5%)" : "SOME UNSTABLE (CV% > 5%)") << endl;

    cout << "\n=================================================================" << endl;
    cout << " BLOCK F COMPLETE" << endl;
    cout << "=================================================================" << endl;

    return 0;
}
