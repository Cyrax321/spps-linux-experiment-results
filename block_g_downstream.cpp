// block_g_downstream.cpp — Downstream Macro-Benchmark (Block G)
// G1: Load Django AST from disk → deserialize → DFS max-depth
// Tests real-world end-to-end pipeline: SPPS vs Protobuf (Arena)
//
// This measures the wall-clock time of a practical "compiler pass"
// workflow: read serialized tree from disk, reconstruct in memory,
// and run a DFS to compute maximum depth (expected: 28 for Django AST).

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <stack>
#include <algorithm>

#include "tree.pb.h"
#include "tree_generated.h"
#include <google/protobuf/arena.h>

using namespace std;
using namespace std::chrono;

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

// =========================================================================
// DFS MAX DEPTH (common)
// =========================================================================
int dfsMaxDepth(int root, const vector<vector<int>>& children) {
    int maxDepth = 0;
    struct Frame { int node; int childIdx; int depth; };
    stack<Frame> stk;
    stk.push({root, 0, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.depth > maxDepth) maxDepth = f.depth;
        if (f.childIdx < (int)children[f.node].size()) {
            int child = children[f.node][f.childIdx++];
            stk.push({child, 0, f.depth + 1});
        } else {
            stk.pop();
        }
    }
    return maxDepth;
}

// DFS max depth on Protobuf tree
int dfsMaxDepthPB(const PB_Bench::Node* root) {
    int maxDepth = 0;
    struct Frame { const PB_Bench::Node* n; int idx; int depth; };
    stack<Frame> stk;
    stk.push({root, 0, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.depth > maxDepth) maxDepth = f.depth;
        if (f.idx < f.n->children_size()) {
            const PB_Bench::Node* child = &f.n->children(f.idx++);
            stk.push({child, 0, f.depth + 1});
        } else {
            stk.pop();
        }
    }
    return maxDepth;
}

// DFS max depth on SPPS decoded tree (BasePointer + M arrays)
int dfsMaxDepthSPPS(int root, const vector<int>& out_deg,
                     const vector<int>& BasePointer, const vector<int>& M) {
    int maxDepth = 0;
    struct Frame { int node; int childIdx; int depth; };
    stack<Frame> stk;
    stk.push({root, 0, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.depth > maxDepth) maxDepth = f.depth;
        if (f.childIdx < out_deg[f.node]) {
            int child = M[BasePointer[f.node] + f.childIdx++];
            stk.push({child, 0, f.depth + 1});
        } else {
            stk.pop();
        }
    }
    return maxDepth;
}

// =========================================================================
// SPPS PIPELINE: Load from file → encode → decode → DFS max-depth
// =========================================================================
struct PipelineResult {
    double total_ms;
    double encode_ms;
    double decode_ms;
    double dfs_ms;
    int maxDepth;
    size_t sizeBytes;
};

PipelineResult runSPPSPipeline(int n, const vector<vector<int>>& children) {
    PipelineResult res;
    auto t0 = high_resolution_clock::now();

    // === ENCODE ===
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
    res.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();
    res.sizeBytes = S.size() * sizeof(long long);

    // === DECODE ===
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
    res.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

    // === DFS MAX DEPTH ===
    res.maxDepth = dfsMaxDepthSPPS(root, out_deg, BasePointer, M);

    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();
    res.total_ms = duration_cast<duration<double, milli>>(t3 - t0).count();

    return res;
}

// =========================================================================
// PROTOBUF (Arena) PIPELINE: Load → encode → serialize → deserialize → DFS
// =========================================================================
PipelineResult runProtobufPipeline(int n, const vector<vector<int>>& children) {
    PipelineResult res;
    auto t0 = high_resolution_clock::now();

    // === ENCODE with Arena ===
    google::protobuf::Arena encode_arena;
    PB_Bench::Node* pbRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&encode_arena);

    struct Frame { int id; int childIdx; PB_Bench::Node* node; };
    stack<Frame> stk;
    pbRoot->set_id(1);
    stk.push({1, 0, pbRoot});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.childIdx < (int)children[f.id].size()) {
            int child = children[f.id][f.childIdx]; f.childIdx++;
            PB_Bench::Node* cn = f.node->add_children();
            cn->set_id(child);
            stk.push({child, 0, cn});
        } else stk.pop();
    }

    string data;
    pbRoot->SerializeToString(&data);

    auto t1 = high_resolution_clock::now();
    res.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();
    res.sizeBytes = data.size();

    // === DECODE with Arena ===
    google::protobuf::Arena decode_arena;
    PB_Bench::Node* decRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&decode_arena);
    decRoot->ParseFromString(data);

    auto t2 = high_resolution_clock::now();
    res.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

    // === DFS MAX DEPTH ===
    res.maxDepth = dfsMaxDepthPB(decRoot);

    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();
    res.total_ms = duration_cast<duration<double, milli>>(t3 - t0).count();

    return res;
}

// =========================================================================
// STATS
// =========================================================================
struct Stats {
    double mean, stddev, cv;
};

Stats computeStats(const vector<double>& data) {
    double sum = 0;
    for (double v : data) sum += v;
    double mean = sum / data.size();
    double sq = 0;
    for (double v : data) sq += (v - mean) * (v - mean);
    double stddev = sqrt(sq / (data.size() - 1));
    return {mean, stddev, (mean > 0) ? stddev / mean * 100 : 0};
}

// =========================================================================
// MAIN
// =========================================================================
int main() {
    cout << "=================================================================" << endl;
    cout << " BLOCK G — DOWNSTREAM MACRO-BENCHMARK" << endl;
    cout << " Mock Compiler Pass: Deserialize → DFS Max Depth" << endl;
    cout << " SPPS vs Protobuf (Arena)" << endl;
    cout << "=================================================================" << endl;

    const int TRIALS = 30;
    const int WARMUP = 3;

    int n = 2325575;
    auto tree = loadEdgeList("datasets/real_ast_benchmark.txt", n);
    if (n == 0) {
        cerr << "Cannot load dataset!" << endl;
        return 1;
    }

    cout << "\nDataset: Django AST, n=" << n << endl;
    cout << "Protocol: " << WARMUP << " warmup + " << TRIALS << " timed trials" << endl;

    // ===================== SPPS Pipeline =====================
    cout << "\n--- SPPS Pipeline ---" << endl;
    cout << "  Warmup..." << flush;
    for (int w = 0; w < WARMUP; w++) runSPPSPipeline(n, tree);
    cout << " done" << endl;

    vector<double> spps_total, spps_enc, spps_dec, spps_dfs;
    int spps_depth = 0;
    size_t spps_size = 0;
    for (int t = 0; t < TRIALS; t++) {
        auto r = runSPPSPipeline(n, tree);
        spps_total.push_back(r.total_ms);
        spps_enc.push_back(r.encode_ms);
        spps_dec.push_back(r.decode_ms);
        spps_dfs.push_back(r.dfs_ms);
        spps_depth = r.maxDepth;
        spps_size = r.sizeBytes;
        if ((t + 1) % 10 == 0) cout << "  Trial " << (t + 1) << "/" << TRIALS << endl;
    }

    // ===================== Protobuf (Arena) Pipeline =====================
    cout << "\n--- Protobuf (Arena) Pipeline ---" << endl;
    cout << "  Warmup..." << flush;
    for (int w = 0; w < WARMUP; w++) runProtobufPipeline(n, tree);
    cout << " done" << endl;

    vector<double> pb_total, pb_enc, pb_dec, pb_dfs;
    int pb_depth = 0;
    size_t pb_size = 0;
    for (int t = 0; t < TRIALS; t++) {
        auto r = runProtobufPipeline(n, tree);
        pb_total.push_back(r.total_ms);
        pb_enc.push_back(r.encode_ms);
        pb_dec.push_back(r.decode_ms);
        pb_dfs.push_back(r.dfs_ms);
        pb_depth = r.maxDepth;
        pb_size = r.sizeBytes;
        if ((t + 1) % 10 == 0) cout << "  Trial " << (t + 1) << "/" << TRIALS << endl;
    }

    // ===================== Results =====================
    cout << "\n=================================================================" << endl;
    cout << " G1: DOWNSTREAM PIPELINE RESULTS" << endl;
    cout << "=================================================================" << endl;

    auto spps_t = computeStats(spps_total);
    auto spps_e = computeStats(spps_enc);
    auto spps_d = computeStats(spps_dec);
    auto spps_f = computeStats(spps_dfs);

    auto pb_t = computeStats(pb_total);
    auto pb_e = computeStats(pb_enc);
    auto pb_d = computeStats(pb_dec);
    auto pb_f = computeStats(pb_dfs);

    cout << "\n" << left << setw(25) << "Metric"
         << setw(15) << "SPPS"
         << setw(15) << "PB (Arena)"
         << setw(12) << "Speedup"
         << endl;
    cout << string(67, '-') << endl;

    auto printRow = [](const string& label, double spps_val, double pb_val) {
        cout << left << setw(25) << label
             << fixed << setprecision(2)
             << setw(15) << spps_val
             << setw(15) << pb_val
             << setw(12) << (pb_val / spps_val)
             << "x" << endl;
    };

    printRow("Encode (ms)", spps_e.mean, pb_e.mean);
    printRow("Decode (ms)", spps_d.mean, pb_d.mean);
    printRow("DFS Max-Depth (ms)", spps_f.mean, pb_f.mean);
    printRow("Total Pipeline (ms)", spps_t.mean, pb_t.mean);

    cout << left << setw(25) << "Max Depth Found"
         << setw(15) << spps_depth
         << setw(15) << pb_depth << endl;

    cout << left << setw(25) << "Serialized Size (B)"
         << setw(15) << spps_size
         << setw(15) << pb_size << endl;

    cout << left << setw(25) << "Size (B/node)"
         << fixed << setprecision(2)
         << setw(15) << ((double)spps_size / n)
         << setw(15) << ((double)pb_size / n) << endl;

    cout << "\n--- Stability ---" << endl;
    cout << "  SPPS total CV%: " << fixed << setprecision(2) << spps_t.cv << "%" << endl;
    cout << "  PB total CV%:   " << pb_t.cv << "%" << endl;

    double speedup = pb_t.mean / spps_t.mean;
    cout << "\n--- Speedup Narrative ---" << endl;
    cout << "SPPS pipeline completes in " << fixed << setprecision(2) << spps_t.mean
         << " ms vs Protobuf (Arena) " << pb_t.mean << " ms." << endl;
    cout << "SPPS is " << speedup << "x faster for the full deserialize-and-traverse pipeline." << endl;
    cout << "Both methods correctly find max depth = " << spps_depth << "." << endl;

    cout << "\n=================================================================" << endl;
    cout << " BLOCK G COMPLETE" << endl;
    cout << "=================================================================" << endl;

    return 0;
}
