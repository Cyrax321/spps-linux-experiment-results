// block_e_compression.cpp — Compression Experiments (Block E)
// E1: SPPS + zstd on all three datasets
// E2: Compression across topologies at n=1M
// E3: Apples-to-apples compression: SPPS vs Protobuf vs FlatBuffers (all zstd)

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stack>
#include <zstd.h>

#include "tree.pb.h"
#include "tree_generated.h"
#include <google/protobuf/arena.h>

using namespace std;
using namespace std::chrono;

// =========================================================================
// TREE GENERATORS (same as benchmark.cpp)
// =========================================================================
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

vector<vector<int>> generatePathGraph(int n) {
    vector<vector<int>> children(n + 1);
    for (int i = 1; i < n; ++i) children[i].push_back(i + 1);
    return children;
}

vector<vector<int>> generateStarGraph(int n) {
    vector<vector<int>> children(n + 1);
    for (int i = 2; i <= n; ++i) children[1].push_back(i);
    return children;
}

vector<vector<int>> generateBalancedBinaryTree(int n) {
    vector<vector<int>> children(n + 1);
    for (int i = 2; i <= n; ++i) children[i / 2].push_back(i);
    return children;
}

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
// SPPS ENCODE (verbatim from benchmark.cpp)
// =========================================================================
vector<long long> sppsEncode(int n, const vector<vector<int>>& children) {
    if (n <= 1) return {};

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
    return S;
}

// =========================================================================
// COMPRESSION HELPERS
// =========================================================================
struct CompressionResult {
    size_t rawBytes;
    size_t compressedBytes;
    double compressionTimeMs;
    double decompressionTimeMs;
    double ratio;
    double bytesPerNode;
    int n;
};

CompressionResult compressWithZstd(const vector<long long>& data, int level, int n) {
    CompressionResult res;
    res.n = n;
    res.rawBytes = data.size() * sizeof(long long);

    // Compress
    size_t compBound = ZSTD_compressBound(res.rawBytes);
    vector<char> compBuffer(compBound);

    auto start = high_resolution_clock::now();
    size_t compSize = ZSTD_compress(compBuffer.data(), compBound,
                                     data.data(), res.rawBytes, level);
    auto end = high_resolution_clock::now();

    if (ZSTD_isError(compSize)) {
        cerr << "ZSTD compression error: " << ZSTD_getErrorName(compSize) << endl;
        return res;
    }

    res.compressedBytes = compSize;
    res.compressionTimeMs = duration_cast<duration<double, milli>>(end - start).count();
    res.ratio = (double)res.rawBytes / compSize;
    res.bytesPerNode = (double)compSize / n;

    // Decompress
    vector<long long> decompBuffer(data.size());
    start = high_resolution_clock::now();
    size_t decompSize = ZSTD_decompress(decompBuffer.data(), res.rawBytes,
                                        compBuffer.data(), compSize);
    end = high_resolution_clock::now();

    if (ZSTD_isError(decompSize) || decompSize != res.rawBytes) {
        cerr << "ZSTD decompression error!" << endl;
    }

    res.decompressionTimeMs = duration_cast<duration<double, milli>>(end - start).count();

    return res;
}

void printCompressionRow(const string& name, const CompressionResult& r) {
    cout << left << setw(25) << name
         << setw(12) << r.compressedBytes
         << fixed << setprecision(2)
         << setw(12) << r.bytesPerNode
         << setw(12) << r.compressionTimeMs
         << setw(12) << r.decompressionTimeMs
         << setw(10) << r.ratio
         << endl;
}

int main() {
    cout << "=================================================================" << endl;
    cout << " BLOCK E — COMPRESSION EXPERIMENTS" << endl;
    cout << " SPPS + zstd on all datasets and topologies" << endl;
    cout << "=================================================================" << endl;

    // =====================================================================
    // E1: SPPS + zstd on all three datasets
    // =====================================================================
    cout << "\n========== E1: SPPS + zstd on All Three Datasets ==========" << endl;

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
        if (n > 0) {
            datasets.push_back({"Django AST", "datasets/real_ast_benchmark.txt", n});
        }
    }

    // sqlite3 AST
    {
        int n = 0;
        auto tree = loadEdgeList("datasets/sqlite3_ast_edges.txt", n);
        if (n > 0) {
            datasets.push_back({"sqlite3 AST", "datasets/sqlite3_ast_edges.txt", n});
        }
    }

    // XMark XML
    {
        int n = 0;
        auto tree = loadEdgeList("datasets/xmark_edges.txt", n);
        if (n > 0) {
            datasets.push_back({"XMark XML", "datasets/xmark_edges.txt", n});
        }
    }

    cout << "\nDatasets loaded:" << endl;
    for (auto& ds : datasets) cout << "  " << ds.name << ": n=" << ds.n << endl;

    cout << "\n" << left << setw(25) << "Dataset/Level"
         << setw(12) << "Comp.Bytes"
         << setw(12) << "Bytes/Node"
         << setw(12) << "Comp.ms"
         << setw(12) << "Decomp.ms"
         << setw(10) << "Ratio"
         << endl;
    cout << string(83, '-') << endl;

    for (auto& ds : datasets) {
        int n = ds.n;
        auto tree = loadEdgeList(ds.file, n);
        auto S = sppsEncode(n, tree);

        // Raw SPPS
        size_t rawBytes = S.size() * sizeof(long long);
        double rawBPN = (double)rawBytes / n;
        cout << left << setw(25) << (ds.name + " (raw)")
             << setw(12) << rawBytes
             << fixed << setprecision(2)
             << setw(12) << rawBPN
             << setw(12) << "-"
             << setw(12) << "-"
             << setw(10) << "1.00"
             << endl;

        // zstd level 1 (fastest)
        auto r1 = compressWithZstd(S, 1, n);
        printCompressionRow(ds.name + " (zstd-1)", r1);

        // zstd level 3 (default)
        auto r3 = compressWithZstd(S, 3, n);
        printCompressionRow(ds.name + " (zstd-3)", r3);

        cout << endl;
    }

    // =====================================================================
    // E2: Compression across topologies at n=1M
    // =====================================================================
    cout << "\n========== E2: Compression Across Topologies (n=1M) ==========" << endl;

    int N_TOPO = 1000000;
    struct TopoData {
        string name;
        vector<long long> S;
    };

    vector<TopoData> topos;

    {
        auto tree = generatePathGraph(N_TOPO);
        topos.push_back({"Path Graph", sppsEncode(N_TOPO, tree)});
    }
    {
        auto tree = generateStarGraph(N_TOPO);
        topos.push_back({"Star Graph", sppsEncode(N_TOPO, tree)});
    }
    {
        int n = (1 << 20) - 1; // 1048575
        auto tree = generateBalancedBinaryTree(n);
        topos.push_back({"Balanced Binary", sppsEncode(n, tree)});
        N_TOPO = n; // use actual size for this one
    }
    {
        N_TOPO = 1000000;
        auto tree = generateASTLikeTree(N_TOPO);
        topos.push_back({"Random AST-Like", sppsEncode(N_TOPO, tree)});
    }

    cout << "\n" << left << setw(25) << "Topology"
         << setw(12) << "Raw B/node"
         << setw(12) << "zstd-1 B/n"
         << setw(12) << "zstd-3 B/n"
         << setw(10) << "Ratio-3"
         << endl;
    cout << string(71, '-') << endl;

    for (auto& t : topos) {
        int n = (int)t.S.size() + 1;
        double rawBPN = (double)(t.S.size() * sizeof(long long)) / n;

        auto r1 = compressWithZstd(t.S, 1, n);
        auto r3 = compressWithZstd(t.S, 3, n);

        cout << left << setw(25) << t.name
             << fixed << setprecision(2)
             << setw(12) << rawBPN
             << setw(12) << r1.bytesPerNode
             << setw(12) << r3.bytesPerNode
             << setw(10) << r3.ratio
             << endl;
    }

    // =====================================================================
    // E3: Apples-to-apples: compress SPPS, Protobuf, FlatBuffers payloads
    // =====================================================================
    cout << "\n========== E3: All-Format Compression Comparison ==========" << endl;
    cout << "(Django AST, n=2,325,575 — all formats through same zstd pipeline)" << endl;

    if (!datasets.empty()) {
        int n = datasets[0].n;
        auto tree = loadEdgeList(datasets[0].file, n);

        // --- SPPS payload ---
        auto S = sppsEncode(n, tree);
        size_t spps_raw = S.size() * sizeof(long long);
        auto spps_z1 = compressWithZstd(S, 1, n);
        auto spps_z3 = compressWithZstd(S, 3, n);

        // --- Protobuf payload (Arena) ---
        google::protobuf::Arena arena;
        PB_Bench::Node* pbRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&arena);
        struct PBFrame { int id; int ci; PB_Bench::Node* node; };
        stack<PBFrame> pbstk;
        pbRoot->set_id(1);
        pbstk.push({1, 0, pbRoot});
        while (!pbstk.empty()) {
            PBFrame& f = pbstk.top();
            if (f.ci < (int)tree[f.id].size()) {
                int child = tree[f.id][f.ci]; f.ci++;
                PB_Bench::Node* cn = f.node->add_children();
                cn->set_id(child);
                pbstk.push({child, 0, cn});
            } else pbstk.pop();
        }
        string pb_data;
        pbRoot->SerializeToString(&pb_data);
        size_t pb_raw = pb_data.size();

        // Compress PB via zstd
        vector<long long> pb_as_ll((pb_data.size() + sizeof(long long) - 1) / sizeof(long long));
        memcpy(pb_as_ll.data(), pb_data.data(), pb_data.size());

        // Use raw bytes compression helper
        auto compressRawBytes = [&](const char* data, size_t dataSize, int level, int nodes) -> CompressionResult {
            CompressionResult res;
            res.n = nodes;
            res.rawBytes = dataSize;
            size_t compBound = ZSTD_compressBound(dataSize);
            vector<char> compBuffer(compBound);
            auto start = high_resolution_clock::now();
            size_t compSize = ZSTD_compress(compBuffer.data(), compBound, data, dataSize, level);
            auto end = high_resolution_clock::now();
            if (ZSTD_isError(compSize)) { cerr << "ZSTD error: " << ZSTD_getErrorName(compSize) << endl; return res; }
            res.compressedBytes = compSize;
            res.compressionTimeMs = duration_cast<duration<double, milli>>(end - start).count();
            res.ratio = (double)dataSize / compSize;
            res.bytesPerNode = (double)compSize / nodes;
            // Decompress
            vector<char> decompBuffer(dataSize);
            start = high_resolution_clock::now();
            ZSTD_decompress(decompBuffer.data(), dataSize, compBuffer.data(), compSize);
            end = high_resolution_clock::now();
            res.decompressionTimeMs = duration_cast<duration<double, milli>>(end - start).count();
            return res;
        };

        auto pb_z1 = compressRawBytes(pb_data.data(), pb_data.size(), 1, n);
        auto pb_z3 = compressRawBytes(pb_data.data(), pb_data.size(), 3, n);

        // --- FlatBuffers payload ---
        flatbuffers::FlatBufferBuilder fbb(1024 * 1024);
        struct FBFrame { int id; int ci; vector<flatbuffers::Offset<FB_Bench::Node>> co; };
        stack<FBFrame> fbstk;
        fbstk.push({1, 0, {}});
        flatbuffers::Offset<FB_Bench::Node> fb_result;
        while (!fbstk.empty()) {
            FBFrame& f = fbstk.top();
            if (f.ci < (int)tree[f.id].size()) {
                int child = tree[f.id][f.ci]; f.ci++;
                fbstk.push({child, 0, {}});
            } else {
                auto off = FB_Bench::CreateNode(fbb, f.id, fbb.CreateVector(f.co));
                fbstk.pop();
                if (!fbstk.empty()) fbstk.top().co.push_back(off);
                else fb_result = off;
            }
        }
        fbb.Finish(fb_result);
        size_t fb_raw = fbb.GetSize();
        auto fb_z1 = compressRawBytes((const char*)fbb.GetBufferPointer(), fb_raw, 1, n);
        auto fb_z3 = compressRawBytes((const char*)fbb.GetBufferPointer(), fb_raw, 3, n);

        // --- Print comparison table ---
        cout << "\n" << left << setw(25) << "Format"
             << setw(15) << "Raw B/node"
             << setw(15) << "zstd-1 B/n"
             << setw(15) << "zstd-3 B/n"
             << setw(12) << "Ratio-3"
             << endl;
        cout << string(82, '-') << endl;

        auto printE3Row = [&](const string& name, size_t raw, const CompressionResult& z1, const CompressionResult& z3) {
            cout << left << setw(25) << name
                 << fixed << setprecision(2)
                 << setw(15) << ((double)raw / n)
                 << setw(15) << z1.bytesPerNode
                 << setw(15) << z3.bytesPerNode
                 << setw(12) << z3.ratio
                 << endl;
        };

        printE3Row("SPPS", spps_raw, spps_z1, spps_z3);
        printE3Row("Protobuf (Arena)", pb_raw, pb_z1, pb_z3);
        printE3Row("FlatBuffers", fb_raw, fb_z1, fb_z3);

        cout << "\n--- Summary ---" << endl;
        cout << "SPPS raw:           " << fixed << setprecision(2) << ((double)spps_raw / n) << " B/node" << endl;
        cout << "SPPS+zstd-3:        " << spps_z3.bytesPerNode << " B/node (" << spps_z3.ratio << "x compression)" << endl;
        cout << "Protobuf raw:       " << ((double)pb_raw / n) << " B/node" << endl;
        cout << "Protobuf+zstd-3:    " << pb_z3.bytesPerNode << " B/node (" << pb_z3.ratio << "x compression)" << endl;
        cout << "FlatBuffers raw:    " << ((double)fb_raw / n) << " B/node" << endl;
        cout << "FlatBuffers+zstd-3: " << fb_z3.bytesPerNode << " B/node (" << fb_z3.ratio << "x compression)" << endl;
    }

    cout << "\n=================================================================" << endl;
    cout << " BLOCK E COMPLETE" << endl;
    cout << "=================================================================" << endl;

    return 0;
}

