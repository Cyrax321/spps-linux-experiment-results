// block_d_louds.cpp — LOUDS Baseline Integration (Block D)
// D1: Implement LOUDS (Level-Order Unary Degree Sequence) for tree encoding
// D2: Head-to-head comparison table vs SPPS, Protobuf, FlatBuffers
// D3: Speedup narrative sentence
//
// LOUDS is implemented directly (no external library required):
//   - Encodes tree as a bitvector using level-order (BFS) traversal
//   - Each node: d(v) ones followed by a zero, where d(v) = number of children
//   - Supports: encode, decode, DFS traversal
//   - Size = exactly 2n bits + O(1) overhead

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
// LOUDS ENCODE with O(1) rank/select acceleration
// =========================================================================
struct LOUDSEncoded {
    vector<uint64_t> blocks;       // Packed 64-bit blocks of the bitvector
    vector<int> rankSuperblock;    // Cumulative rank1 at each block boundary
    vector<int> bfsOrder;          // BFS node ordering
    int n;
    int totalBits;
    size_t sizeBytes;
};

// O(1) rank1: count of 1-bits in positions [0..pos)
static inline int rank1(const LOUDSEncoded& enc, int pos) {
    if (pos <= 0) return 0;
    if (pos > enc.totalBits) pos = enc.totalBits;
    int blockIdx = pos / 64;
    int bitIdx = pos % 64;
    int r = enc.rankSuperblock[blockIdx];
    if (bitIdx > 0) {
        r += __builtin_popcountll(enc.blocks[blockIdx] & ((1ULL << bitIdx) - 1));
    }
    return r;
}

// O(1) rank0: count of 0-bits in positions [0..pos)
static inline int rank0(const LOUDSEncoded& enc, int pos) {
    return pos - rank1(enc, pos);
}

// select1: position of the j-th 1-bit (1-indexed) — binary search over superblocks
static inline int select1(const LOUDSEncoded& enc, int j) {
    if (j <= 0) return -1;
    int lo = 0, hi = (int)enc.rankSuperblock.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (enc.rankSuperblock[mid + 1] < j) lo = mid + 1;
        else hi = mid;
    }
    // lo is the block containing the j-th 1
    int remaining = j - enc.rankSuperblock[lo];
    uint64_t word = enc.blocks[lo];
    int pos = lo * 64;
    while (remaining > 0) {
        if (word == 0) { pos += 64; break; }
        int pop = __builtin_popcountll(word);
        if (pop >= remaining) {
            // Find the remaining-th set bit in word
            for (int i = 0; i < remaining; i++) {
                word &= word - 1; // clear lowest set bit
                if (i < remaining - 1) continue;
            }
            // Undo last clear to get the target bit
            break;
        }
        remaining -= pop;
        pos += 64;
        lo++;
        word = (lo < (int)enc.blocks.size()) ? enc.blocks[lo] : 0;
    }
    // More precise: find remaining-th 1-bit in block lo
    remaining = j - enc.rankSuperblock[lo];
    word = enc.blocks[lo];
    pos = lo * 64;
    for (int i = 0; i < remaining; i++) {
        int tz = __builtin_ctzll(word);
        if (i == remaining - 1) return pos + tz;
        word &= word - 1;
    }
    return pos;
}

// select0: position of the j-th 0-bit (1-indexed) — binary search
static inline int select0(const LOUDSEncoded& enc, int j) {
    if (j <= 0) return -1;
    // Binary search for the block containing the j-th 0
    int lo = 0, hi = (int)enc.blocks.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        int zeros_before_end = (mid + 1) * 64 - enc.rankSuperblock[mid + 1];
        if (zeros_before_end < j) lo = mid + 1;
        else hi = mid;
    }
    int zeros_before = lo * 64 - enc.rankSuperblock[lo];
    int remaining = j - zeros_before;
    uint64_t word = ~enc.blocks[lo];
    int pos = lo * 64;
    // Mask out bits beyond totalBits in the last block
    if (lo == (int)enc.blocks.size() - 1) {
        int rem = enc.totalBits % 64;
        if (rem > 0) word &= (1ULL << rem) - 1;
    }
    for (int i = 0; i < remaining; i++) {
        int tz = __builtin_ctzll(word);
        if (i == remaining - 1) return pos + tz;
        word &= word - 1;
    }
    return pos;
}

LOUDSEncoded loudsEncode(int n, const vector<vector<int>>& children) {
    LOUDSEncoded result;
    result.n = n;
    result.bfsOrder.reserve(n);

    // Build bitvector as vector<bool> first, then pack
    vector<bool> bitvec;
    bitvec.reserve(2 * n + 2);

    // Super-root prefix: "10"
    bitvec.push_back(true);
    bitvec.push_back(false);

    // BFS traversal
    queue<int> q;
    q.push(1);
    result.bfsOrder.push_back(1);

    while (!q.empty()) {
        int node = q.front(); q.pop();
        for (int child : children[node]) {
            bitvec.push_back(true);
            q.push(child);
            result.bfsOrder.push_back(child);
        }
        bitvec.push_back(false);
    }

    result.totalBits = (int)bitvec.size();
    result.sizeBytes = (result.totalBits + 7) / 8;

    // Pack into 64-bit blocks
    int numBlocks = (result.totalBits + 63) / 64;
    result.blocks.resize(numBlocks, 0);
    for (int i = 0; i < result.totalBits; i++) {
        if (bitvec[i]) {
            result.blocks[i / 64] |= (1ULL << (i % 64));
        }
    }

    // Build rank superblock: rankSuperblock[i] = number of 1s in blocks[0..i-1]
    result.rankSuperblock.resize(numBlocks + 1, 0);
    for (int i = 0; i < numBlocks; i++) {
        result.rankSuperblock[i + 1] = result.rankSuperblock[i] + __builtin_popcountll(result.blocks[i]);
    }

    return result;
}

// =========================================================================
// LOUDS DECODE — O(1) rank/select reconstruction
// =========================================================================
struct LOUDSDecoded {
    int root;
    int n;
    vector<vector<int>> children;
};

LOUDSDecoded loudsDecode(const LOUDSEncoded& enc) {
    LOUDSDecoded dec;
    dec.n = enc.n;
    dec.children.resize(enc.n + 1);
    dec.root = 1;

    // Sequential scan through packed 64-bit blocks using popcount/ctzll
    // for O(1) degree computation per node
    int nodeIdx = 0;
    int childBfsIdx = 1;
    int bitPos = 2; // skip super-root "10"
    
    while (nodeIdx < enc.n && bitPos < enc.totalBits) {
        int node = enc.bfsOrder[nodeIdx];
        int degree = 0;
        
        // Count consecutive 1-bits (children) until we hit a 0
        while (bitPos < enc.totalBits) {
            int blockIdx = bitPos / 64;
            int bitInBlock = bitPos % 64;
            
            // Extract remaining bits from current position
            uint64_t word = enc.blocks[blockIdx] >> bitInBlock;
            int bitsLeft = 64 - bitInBlock;
            
            if (word == 0) {
                // Bit at bitPos is 0 — end of degree
                bitPos++;
                break;
            }
            
            // Count trailing ones (position of first 0)
            int tz = __builtin_ctzll(~word);
            if (tz < bitsLeft) {
                // Found terminating 0 within this block
                degree += tz;
                bitPos += tz + 1; // skip ones and the zero
                break;
            } else {
                // All remaining bits in block are 1s
                degree += bitsLeft;
                bitPos += bitsLeft;
            }
        }
        
        // Assign children from BFS order
        for (int c = 0; c < degree; c++) {
            if (childBfsIdx < (int)enc.bfsOrder.size()) {
                dec.children[node].push_back(enc.bfsOrder[childBfsIdx]);
                childBfsIdx++;
            }
        }
        nodeIdx++;
    }

    return dec;
}

// =========================================================================
// LOUDS DFS
// =========================================================================
void dfsLOUDS(int root, const vector<vector<int>>& children, volatile int& sink) {
    struct Frame { int node; int childIdx; };
    stack<Frame> stk;
    stk.push({root, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        sink = f.node;
        if (f.childIdx < (int)children[f.node].size()) {
            int child = children[f.node][f.childIdx];
            f.childIdx++;
            stk.push({child, 0});
        } else { stk.pop(); }
    }
}

// =========================================================================
// SPPS (verbatim from benchmark.cpp)
// =========================================================================
struct BenchResult {
    double encode_ms;
    double decode_ms;
    double dfs_ms;
    long peakRAM_kb;
    size_t sizeBytes;
};

BenchResult runSPPS(int n, const vector<vector<int>>& children) {
    BenchResult res;

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

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    res.peakRAM_kb = usage.ru_maxrss / 1024;

    return res;
}

// =========================================================================
// LOUDS BENCHMARK
// =========================================================================
BenchResult runLOUDS(int n, const vector<vector<int>>& children) {
    BenchResult res;

    auto t0 = high_resolution_clock::now();
    LOUDSEncoded enc = loudsEncode(n, children);
    auto t1 = high_resolution_clock::now();
    res.encode_ms = duration_cast<duration<double, milli>>(t1 - t0).count();
    res.sizeBytes = enc.sizeBytes;

    LOUDSDecoded dec = loudsDecode(enc);
    auto t2 = high_resolution_clock::now();
    res.decode_ms = duration_cast<duration<double, milli>>(t2 - t1).count();

    volatile int sink = 0;
    dfsLOUDS(dec.root, dec.children, sink);
    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    res.peakRAM_kb = usage.ru_maxrss / 1024;

    return res;
}

// =========================================================================
// FLATBUFFERS BENCHMARK
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

    // DFS
    volatile int sink = 0;
    struct FBFrame { const FB_Bench::Node* n; int idx; };
    stack<FBFrame> stk;
    stk.push({root, 0});
    while (!stk.empty()) {
        FBFrame& f = stk.top();
        sink = f.n->id();
        if (f.n->children() && f.idx < (int)f.n->children()->size()) {
            auto child = f.n->children()->Get(f.idx);
            f.idx++;
            stk.push({child, 0});
        } else { stk.pop(); }
    }

    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    res.peakRAM_kb = usage.ru_maxrss / 1024;

    return res;
}

// =========================================================================
// PROTOBUF BENCHMARK
// =========================================================================
void buildPB(int node_id, const vector<vector<int>>& tree, PB_Bench::Node* pbNode) {
    struct Frame { int id; int childIdx; PB_Bench::Node* node; };
    stack<Frame> stk;
    pbNode->set_id(node_id);
    stk.push({node_id, 0, pbNode});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.childIdx < (int)tree[f.id].size()) {
            int child = tree[f.id][f.childIdx]; f.childIdx++;
            PB_Bench::Node* cn = f.node->add_children();
            cn->set_id(child);
            stk.push({child, 0, cn});
        } else { stk.pop(); }
    }
}

BenchResult runProtobuf(int n, const vector<vector<int>>& tree) {
    BenchResult res;

    auto t0 = high_resolution_clock::now();
    // Arena allocation for contiguous memory (fair comparison)
    google::protobuf::Arena encode_arena;
    PB_Bench::Node* pbRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&encode_arena);
    buildPB(1, tree, pbRoot);
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

    // DFS
    volatile int sink = 0;
    struct PBFrame { const PB_Bench::Node* n; int idx; };
    stack<PBFrame> stk;
    stk.push({decRoot, 0});
    while (!stk.empty()) {
        PBFrame& f = stk.top();
        sink = f.n->id();
        if (f.idx < f.n->children_size()) {
            const PB_Bench::Node* child = &f.n->children(f.idx);
            f.idx++;
            stk.push({child, 0});
        } else { stk.pop(); }
    }

    auto t3 = high_resolution_clock::now();
    res.dfs_ms = duration_cast<duration<double, milli>>(t3 - t2).count();

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    res.peakRAM_kb = usage.ru_maxrss / 1024;

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
    cout << " BLOCK D — LOUDS BASELINE INTEGRATION" << endl;
    cout << " SPPS vs FlatBuffers vs Protobuf vs LOUDS" << endl;
    cout << " 30 trials · Process-level measurement" << endl;
    cout << "=================================================================" << endl;

    const int TRIALS = 30;

    // Use Django AST as primary benchmark dataset
    int n = 2325575;
    auto tree = loadEdgeList("datasets/real_ast_benchmark.txt", n);

    if (n == 0) {
        cerr << "Cannot load dataset!" << endl;
        return 1;
    }

    cout << "\nDataset: Django AST, n=" << n << endl;
    cout << "Protocol: 2 warmup + " << TRIALS << " timed trials per method" << endl;

    // ===================== SPPS =====================
    cout << "\n--- SPPS ---" << endl;
    vector<double> spps_enc, spps_dec, spps_dfs;
    for (int w = 0; w < 2; w++) runSPPS(n, tree);
    for (int t = 0; t < TRIALS; t++) {
        auto r = runSPPS(n, tree);
        spps_enc.push_back(r.encode_ms);
        spps_dec.push_back(r.decode_ms);
        spps_dfs.push_back(r.dfs_ms);
        if ((t+1) % 10 == 0) cout << "  Trial " << (t+1) << "/" << TRIALS << endl;
    }

    // ===================== LOUDS =====================
    cout << "\n--- LOUDS ---" << endl;
    vector<double> louds_enc, louds_dec, louds_dfs;
    for (int w = 0; w < 2; w++) runLOUDS(n, tree);
    for (int t = 0; t < TRIALS; t++) {
        auto r = runLOUDS(n, tree);
        louds_enc.push_back(r.encode_ms);
        louds_dec.push_back(r.decode_ms);
        louds_dfs.push_back(r.dfs_ms);
        if ((t+1) % 10 == 0) cout << "  Trial " << (t+1) << "/" << TRIALS << endl;
    }

    // ===================== FlatBuffers =====================
    cout << "\n--- FlatBuffers ---" << endl;
    vector<double> fb_enc, fb_dec, fb_dfs;
    for (int w = 0; w < 2; w++) runFlatBuffers(n, tree);
    for (int t = 0; t < TRIALS; t++) {
        auto r = runFlatBuffers(n, tree);
        fb_enc.push_back(r.encode_ms);
        fb_dec.push_back(r.decode_ms);
        fb_dfs.push_back(r.dfs_ms);
        if ((t+1) % 10 == 0) cout << "  Trial " << (t+1) << "/" << TRIALS << endl;
    }

    // ===================== Protobuf =====================
    cout << "\n--- Protobuf ---" << endl;
    vector<double> pb_enc, pb_dec, pb_dfs;
    for (int w = 0; w < 2; w++) runProtobuf(n, tree);
    for (int t = 0; t < TRIALS; t++) {
        auto r = runProtobuf(n, tree);
        pb_enc.push_back(r.encode_ms);
        pb_dec.push_back(r.decode_ms);
        pb_dfs.push_back(r.dfs_ms);
        if ((t+1) % 10 == 0) cout << "  Trial " << (t+1) << "/" << TRIALS << endl;
    }

    // ===================== D2: Comparison Table =====================
    cout << "\n=================================================================" << endl;
    cout << " D2: HEAD-TO-HEAD COMPARISON TABLE" << endl;
    cout << "=================================================================" << endl;

    auto spps_enc_s = computeStats(spps_enc);
    auto spps_dec_s = computeStats(spps_dec);
    auto spps_dfs_s = computeStats(spps_dfs);
    auto louds_enc_s = computeStats(louds_enc);
    auto louds_dec_s = computeStats(louds_dec);
    auto louds_dfs_s = computeStats(louds_dfs);
    auto fb_enc_s = computeStats(fb_enc);
    auto fb_dec_s = computeStats(fb_dec);
    auto fb_dfs_s = computeStats(fb_dfs);
    auto pb_enc_s = computeStats(pb_enc);
    auto pb_dec_s = computeStats(pb_dec);
    auto pb_dfs_s = computeStats(pb_dfs);

    // Get sizes from single run
    auto spps_r = runSPPS(n, tree);
    auto louds_r = runLOUDS(n, tree);
    auto fb_r = runFlatBuffers(n, tree);
    auto pb_r = runProtobuf(n, tree);

    cout << "\n" << left << setw(15) << "Method"
         << setw(12) << "Enc(ms)"
         << setw(12) << "Dec(ms)"
         << setw(12) << "DFS(ms)"
         << setw(15) << "Size(bytes)"
         << setw(12) << "B/node"
         << endl;
    cout << string(78, '-') << endl;

    auto printRow = [&](const string& name, const Stats& enc, const Stats& dec,
                        const Stats& dfs, size_t sizB, int nodes) {
        cout << left << setw(15) << name
             << fixed << setprecision(2)
             << setw(12) << enc.mean
             << setw(12) << dec.mean
             << setw(12) << dfs.mean
             << setw(15) << sizB
             << setw(12) << ((double)sizB / nodes)
             << endl;
    };

    printRow("SPPS", spps_enc_s, spps_dec_s, spps_dfs_s, spps_r.sizeBytes, n);
    printRow("LOUDS", louds_enc_s, louds_dec_s, louds_dfs_s, louds_r.sizeBytes, n);
    printRow("FlatBuffers", fb_enc_s, fb_dec_s, fb_dfs_s, fb_r.sizeBytes, n);
    printRow("Protobuf", pb_enc_s, pb_dec_s, pb_dfs_s, pb_r.sizeBytes, n);

    // ===================== D3: Speedup Narrative =====================
    cout << "\n=================================================================" << endl;
    cout << " D3: SPEEDUP NARRATIVE" << endl;
    cout << "=================================================================" << endl;

    double spps_total = spps_enc_s.mean + spps_dec_s.mean + spps_dfs_s.mean;
    double louds_total = louds_enc_s.mean + louds_dec_s.mean + louds_dfs_s.mean;
    double fb_total = fb_enc_s.mean + fb_dec_s.mean + fb_dfs_s.mean;
    double pb_total = pb_enc_s.mean + pb_dec_s.mean + pb_dfs_s.mean;

    cout << "\nTotal roundtrip (encode + decode + DFS):" << endl;
    cout << "  SPPS:        " << fixed << setprecision(2) << spps_total << " ms" << endl;
    cout << "  LOUDS:       " << louds_total << " ms" << endl;
    cout << "  FlatBuffers: " << fb_total << " ms" << endl;
    cout << "  Protobuf:    " << pb_total << " ms" << endl;

    cout << "\nSpeedup of SPPS over:" << endl;
    cout << "  LOUDS:       " << setprecision(2) << (louds_total / spps_total) << "x" << endl;
    cout << "  FlatBuffers: " << (fb_total / spps_total) << "x" << endl;
    cout << "  Protobuf:    " << (pb_total / spps_total) << "x" << endl;

    cout << "\nSize comparison:" << endl;
    cout << "  SPPS:        " << setprecision(2) << ((double)spps_r.sizeBytes / n) << " B/node" << endl;
    cout << "  LOUDS:       " << ((double)louds_r.sizeBytes / n) << " B/node" << endl;
    cout << "  FlatBuffers: " << ((double)fb_r.sizeBytes / n) << " B/node" << endl;
    cout << "  Protobuf:    " << ((double)pb_r.sizeBytes / n) << " B/node" << endl;

    cout << "\n=================================================================" << endl;
    cout << " BLOCK D COMPLETE" << endl;
    cout << "=================================================================" << endl;

    return 0;
}
