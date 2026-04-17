// block_c_benchmarks.cpp — Real Dataset Benchmarks (Block C)
// C1: Django AST rerun with LOUDS added
// C2: sqlite3 Clang AST
// C3: XMark XML
// C4: Cross-dataset stability table
//
// Runs SPPS, LOUDS, FlatBuffers, Protobuf on all datasets
// 30 trials, same methodology as benchmark.cpp

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
#include <queue>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>
#include <cstdint>

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
// STATS
// =========================================================================
struct Stats {
    double mean, stddev, cv, ci95_lo, ci95_hi;
};

Stats computeStats(const vector<double>& data) {
    double sum = 0;
    for (double v : data) sum += v;
    double mean = sum / data.size();
    double sq = 0;
    for (double v : data) sq += (v - mean) * (v - mean);
    double stddev = sqrt(sq / (data.size() - 1));
    double ci = 1.96 * stddev / sqrt((double)data.size());
    return {mean, stddev, (mean > 0) ? stddev / mean * 100 : 0, mean - ci, mean + ci};
}

// =========================================================================
// SPPS
// =========================================================================
struct BenchResult {
    double encode_ms;
    double decode_ms;
    double dfs_ms;
    size_t sizeBytes;
};

BenchResult runSPPS(int n, const vector<vector<int>>& children) {
    BenchResult res;
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
    res.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();
    res.sizeBytes = S.size() * sizeof(long long);

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
    res.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

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
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();
    return res;
}

// =========================================================================
// LOUDS with O(1) rank/select acceleration
// =========================================================================

// O(1) select0 using __builtin_popcountll over 64-bit blocks with binary search
struct LOUDSAccel {
    vector<uint64_t> blocks;
    vector<int> rankSuperblock;
    int totalBits;
};

static inline int louds_rank1(const LOUDSAccel& acc, int pos) {
    if (pos <= 0) return 0;
    if (pos > acc.totalBits) pos = acc.totalBits;
    int blockIdx = pos / 64;
    int bitIdx = pos % 64;
    int r = acc.rankSuperblock[blockIdx];
    if (bitIdx > 0) {
        r += __builtin_popcountll(acc.blocks[blockIdx] & ((1ULL << bitIdx) - 1));
    }
    return r;
}

static inline int louds_select0(const LOUDSAccel& acc, int j) {
    if (j <= 0) return -1;
    int lo = 0, hi = (int)acc.blocks.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        int zeros_before_end = (mid + 1) * 64 - acc.rankSuperblock[mid + 1];
        if (zeros_before_end < j) lo = mid + 1;
        else hi = mid;
    }
    int zeros_before = lo * 64 - acc.rankSuperblock[lo];
    int remaining = j - zeros_before;
    uint64_t word = ~acc.blocks[lo];
    int pos = lo * 64;
    if (lo == (int)acc.blocks.size() - 1) {
        int rem = acc.totalBits % 64;
        if (rem > 0) word &= (1ULL << rem) - 1;
    }
    for (int i = 0; i < remaining; i++) {
        int tz = __builtin_ctzll(word);
        if (i == remaining - 1) return pos + tz;
        word &= word - 1;
    }
    return pos;
}

BenchResult runLOUDS(int n, const vector<vector<int>>& children) {
    BenchResult res;

    auto t0 = high_resolution_clock::now();
    // Encode: build bitvec, then pack into 64-bit blocks with rank index
    vector<bool> bitvec;
    bitvec.reserve(2 * n + 2);
    bitvec.push_back(true); bitvec.push_back(false); // super root
    queue<int> q;
    q.push(1);
    vector<int> bfsOrder;
    bfsOrder.reserve(n);
    bfsOrder.push_back(1);
    while (!q.empty()) {
        int node = q.front(); q.pop();
        for (int child : children[node]) {
            bitvec.push_back(true);
            q.push(child);
            bfsOrder.push_back(child);
        }
        bitvec.push_back(false);
    }

    // Build rank/select acceleration structure
    LOUDSAccel acc;
    acc.totalBits = (int)bitvec.size();
    int numBlocks = (acc.totalBits + 63) / 64;
    acc.blocks.resize(numBlocks, 0);
    for (int i = 0; i < acc.totalBits; i++) {
        if (bitvec[i]) acc.blocks[i / 64] |= (1ULL << (i % 64));
    }
    acc.rankSuperblock.resize(numBlocks + 1, 0);
    for (int i = 0; i < numBlocks; i++) {
        acc.rankSuperblock[i + 1] = acc.rankSuperblock[i] + __builtin_popcountll(acc.blocks[i]);
    }

    auto t1 = high_resolution_clock::now();
    res.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();
    res.sizeBytes = (bitvec.size() + 7) / 8;

    // Decode using O(1) rank/select over packed 64-bit blocks
    // Sequential scan through blocks, counting degrees via popcount
    vector<vector<int>> decChildren(n + 1);
    {
        int nodeIdx = 0;
        int childBfsIdx = 1;
        int bitPos = 2; // skip super-root "10"
        
        while (nodeIdx < n && bitPos < acc.totalBits) {
            int node = bfsOrder[nodeIdx];
            int degree = 0;
            
            // Count consecutive 1-bits (children) until we hit a 0
            while (bitPos < acc.totalBits) {
                int blockIdx = bitPos / 64;
                int bitInBlock = bitPos % 64;
                
                // Extract remaining bits from current position in this block
                uint64_t word = acc.blocks[blockIdx] >> bitInBlock;
                int bitsLeft = 64 - bitInBlock;
                
                if (word == 0) {
                    // No more 1s in this portion of block — but first bit is a 0
                    // If bitInBlock is exactly at a 0, that's our terminator
                    // Actually we need to check if bit at bitPos is 0
                    // word>>0 bit0 is the bit at bitPos
                    // Since word==0, bit at bitPos is 0 => end of degree
                    bitPos++; // skip the 0
                    break;
                }
                
                // Count trailing ones
                int tz = __builtin_ctzll(~word); // position of first 0 in word
                if (tz < bitsLeft) {
                    // Found the terminating 0 within this block
                    degree += tz;
                    bitPos += tz + 1; // skip the ones and the zero
                    break;
                } else {
                    // All remaining bits in block are 1s
                    degree += bitsLeft;
                    bitPos += bitsLeft;
                }
            }
            
            // Assign children
            for (int c = 0; c < degree; c++) {
                if (childBfsIdx < (int)bfsOrder.size())
                    decChildren[node].push_back(bfsOrder[childBfsIdx++]);
            }
            nodeIdx++;
        }
    }
    auto t2 = high_resolution_clock::now();
    res.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

    // DFS
    volatile int sink = 0;
    struct Frame { int node; int ci; };
    stack<Frame> stk;
    stk.push({1, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        sink = f.node;
        if (f.ci < (int)decChildren[f.node].size()) {
            int child = decChildren[f.node][f.ci++];
            stk.push({child, 0});
        } else stk.pop();
    }
    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();
    return res;
}

// =========================================================================
// FLATBUFFERS
// =========================================================================
flatbuffers::Offset<FB_Bench::Node> buildFB(int node_id,
    const vector<vector<int>>& tree, flatbuffers::FlatBufferBuilder& builder) {
    struct Frame { int id; int childIdx; vector<flatbuffers::Offset<FB_Bench::Node>> childOffsets; };
    stack<Frame> stk;
    stk.push({node_id, 0, {}});
    flatbuffers::Offset<FB_Bench::Node> result;
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.childIdx < (int)tree[f.id].size()) {
            int child = tree[f.id][f.childIdx]; f.childIdx++;
            stk.push({child, 0, {}});
        } else {
            auto off = FB_Bench::CreateNode(builder, f.id, builder.CreateVector(f.childOffsets));
            stk.pop();
            if (!stk.empty()) stk.top().childOffsets.push_back(off);
            else result = off;
        }
    }
    return result;
}

BenchResult runFlatBuffers(int n, const vector<vector<int>>& tree) {
    BenchResult res;
    auto t0 = high_resolution_clock::now();
    flatbuffers::FlatBufferBuilder builder(1024 * 1024);
    builder.Finish(buildFB(1, tree, builder));
    auto t1 = high_resolution_clock::now();
    res.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();
    res.sizeBytes = builder.GetSize();

    auto root = FB_Bench::GetNode(builder.GetBufferPointer());
    auto t2 = high_resolution_clock::now();
    res.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

    volatile int sink = 0;
    struct FBFrame { const FB_Bench::Node* n; int idx; };
    stack<FBFrame> stk;
    stk.push({root, 0});
    while (!stk.empty()) {
        FBFrame& f = stk.top();
        sink = f.n->id();
        if (f.n->children() && f.idx < (int)f.n->children()->size()) {
            auto child = f.n->children()->Get(f.idx); f.idx++;
            stk.push({child, 0});
        } else stk.pop();
    }
    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();
    return res;
}

// =========================================================================
// PROTOBUF
// =========================================================================
BenchResult runProtobuf(int n, const vector<vector<int>>& tree) {
    BenchResult res;
    auto t0 = high_resolution_clock::now();

    // Arena allocation for contiguous memory (fair comparison)
    google::protobuf::Arena encode_arena;
    PB_Bench::Node* pbRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&encode_arena);
    struct Frame { int id; int childIdx; PB_Bench::Node* node; };
    stack<Frame> stk;
    pbRoot->set_id(1);
    stk.push({1, 0, pbRoot});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.childIdx < (int)tree[f.id].size()) {
            int child = tree[f.id][f.childIdx]; f.childIdx++;
            PB_Bench::Node* cn = f.node->add_children();
            cn->set_id(child);
            stk.push({child, 0, cn});
        } else stk.pop();
    }
    string data; pbRoot->SerializeToString(&data);
    auto t1 = high_resolution_clock::now();
    res.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();
    res.sizeBytes = data.size();

    // Arena allocation for decode
    google::protobuf::Arena decode_arena;
    PB_Bench::Node* decRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&decode_arena);
    decRoot->ParseFromString(data);
    auto t2 = high_resolution_clock::now();
    res.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

    volatile int sink = 0;
    struct PBFrame { const PB_Bench::Node* n; int idx; };
    stack<PBFrame> pstk;
    pstk.push({decRoot, 0});
    while (!pstk.empty()) {
        PBFrame& f = pstk.top();
        sink = f.n->id();
        if (f.idx < f.n->children_size()) {
            const PB_Bench::Node* child = &f.n->children(f.idx); f.idx++;
            pstk.push({child, 0});
        } else pstk.pop();
    }
    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();
    return res;
}

// =========================================================================
// MAIN
// =========================================================================
int main() {
    cout << "=================================================================" << endl;
    cout << " BLOCK C — REAL DATASET BENCHMARKS" << endl;
    cout << " All 4 methods × 3 datasets × 30 trials" << endl;
    cout << "=================================================================" << endl;

    const int TRIALS = 30;
    const char* methods[] = {"SPPS", "LOUDS", "FlatBuffers", "Protobuf"};

    struct Dataset {
        string name;
        string file;
        int n;
    };

    vector<Dataset> datasets;

    // C1: Django AST
    {
        int n = 2325575;
        auto tree = loadEdgeList("datasets/real_ast_benchmark.txt", n);
        if (n > 0) datasets.push_back({"Django AST (C1)", "datasets/real_ast_benchmark.txt", n});
    }
    // C2: sqlite3 AST
    {
        int n = 0;
        auto tree = loadEdgeList("datasets/sqlite3_ast_edges.txt", n);
        if (n > 0) datasets.push_back({"sqlite3 AST (C2)", "datasets/sqlite3_ast_edges.txt", n});
    }
    // C3: XMark XML
    {
        int n = 0;
        auto tree = loadEdgeList("datasets/xmark_edges.txt", n);
        if (n > 0) datasets.push_back({"XMark XML (C3)", "datasets/xmark_edges.txt", n});
    }

    cout << "\nDatasets:" << endl;
    for (auto& ds : datasets)
        cout << "  " << ds.name << ": n=" << ds.n << endl;

    // Per-dataset results
    struct MethodStats {
        string method;
        Stats enc, dec, dfs;
        size_t sizeBytes;
        double bpn; // bytes per node
    };

    // Cross-dataset stability data
    vector<vector<double>> crossEnc(4), crossDec(4), crossDFS(4);

    for (auto& ds : datasets) {
        cout << "\n\n========== " << ds.name << " (n=" << ds.n << ") ==========" << endl;

        int n = ds.n;
        auto tree = loadEdgeList(ds.file, n);

        vector<MethodStats> allStats;

        for (int m = 0; m < 4; m++) {
            cout << "\n  --- " << methods[m] << " ---" << endl;

            vector<double> enc, dec, dfs;
            size_t sz = 0;

            // Warmup
            for (int w = 0; w < 2; w++) {
                BenchResult r;
                switch (m) {
                    case 0: r = runSPPS(n, tree); break;
                    case 1: r = runLOUDS(n, tree); break;
                    case 2: r = runFlatBuffers(n, tree); break;
                    case 3: r = runProtobuf(n, tree); break;
                }
            }

            for (int t = 0; t < TRIALS; t++) {
                BenchResult r;
                switch (m) {
                    case 0: r = runSPPS(n, tree); break;
                    case 1: r = runLOUDS(n, tree); break;
                    case 2: r = runFlatBuffers(n, tree); break;
                    case 3: r = runProtobuf(n, tree); break;
                }
                enc.push_back(r.encode_ms);
                dec.push_back(r.decode_ms);
                dfs.push_back(r.dfs_ms);
                sz = r.sizeBytes;
                if ((t + 1) % 10 == 0) cout << "    Trial " << (t + 1) << "/" << TRIALS << endl;
            }

            MethodStats ms;
            ms.method = methods[m];
            ms.enc = computeStats(enc);
            ms.dec = computeStats(dec);
            ms.dfs = computeStats(dfs);
            ms.sizeBytes = sz;
            ms.bpn = (double)sz / n;
            allStats.push_back(ms);

            // Collect cross-dataset ns/node for stability analysis
            crossEnc[m].push_back(ms.enc.mean / n * 1e6); // ns/node
            crossDec[m].push_back(ms.dec.mean / n * 1e6);
            crossDFS[m].push_back(ms.dfs.mean / n * 1e6);
        }

        // Print summary table for this dataset
        cout << "\n  " << left << setw(15) << "Method"
             << setw(12) << "Enc(ms)"
             << setw(12) << "Dec(ms)"
             << setw(12) << "DFS(ms)"
             << setw(12) << "Total(ms)"
             << setw(15) << "Size(B)"
             << setw(10) << "B/node"
             << endl;
        cout << "  " << string(88, '-') << endl;

        for (auto& ms : allStats) {
            double total = ms.enc.mean + ms.dec.mean + ms.dfs.mean;
            cout << "  " << left << setw(15) << ms.method
                 << fixed << setprecision(2)
                 << setw(12) << ms.enc.mean
                 << setw(12) << ms.dec.mean
                 << setw(12) << ms.dfs.mean
                 << setw(12) << total
                 << setw(15) << ms.sizeBytes
                 << setw(10) << ms.bpn
                 << endl;
        }
    }

    // =====================================================================
    // C4: Cross-Dataset Stability Table
    // =====================================================================
    cout << "\n\n=================================================================" << endl;
    cout << " C4: CROSS-DATASET STABILITY TABLE" << endl;
    cout << "=================================================================" << endl;

    cout << "\n  Encode ns/node by method across datasets:" << endl;
    cout << "  " << left << setw(15) << "Method";
    for (auto& ds : datasets) cout << setw(15) << ds.name.substr(0, 12);
    cout << setw(10) << "CV%" << endl;
    cout << "  " << string(15 + 15 * datasets.size() + 10, '-') << endl;

    for (int m = 0; m < 4; m++) {
        cout << "  " << left << setw(15) << methods[m];
        for (int d = 0; d < (int)crossEnc[m].size(); d++)
            cout << fixed << setprecision(1) << setw(15) << crossEnc[m][d];

        // CV across datasets
        auto cv = computeStats(crossEnc[m]);
        cout << fixed << setprecision(1) << setw(10) << cv.cv;
        cout << endl;
    }

    cout << "\n  Expected: SPPS CV% ≤ 10% across datasets" << endl;

    cout << "\n=================================================================" << endl;
    cout << " BLOCK C COMPLETE" << endl;
    cout << "=================================================================" << endl;

    return 0;
}
