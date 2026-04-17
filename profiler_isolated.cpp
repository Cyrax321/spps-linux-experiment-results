// profiler_isolated.cpp — Per-method isolated profiling for perf stat / AMD uProf
// Usage: ./profiler_isolated <method> <dataset_file>
// method: spps|louds|fb|pb
// Runs a SINGLE method on a single dataset, 5 times, for clean perf profiling

#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <stack>
#include <queue>
#include <unistd.h>
#include <sys/resource.h>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <random>

#include "tree.pb.h"
#include "tree_generated.h"
#include <google/protobuf/arena.h>

using namespace std;
using namespace std::chrono;

vector<vector<int>> loadEdgeList(const string& f, int& n) {
    ifstream file(f);
    if (!file.is_open()) { cerr << "Cannot open " << f << endl; n = 0; return {}; }
    file >> n;
    vector<vector<int>> c(n + 1);
    int u, v;
    while (file >> u >> v) c[u].push_back(v);
    return c;
}

void runSPPS(int n, const vector<vector<int>>& children) {
    vector<int> ChildRank(n + 2, 0), parent(n + 2, 0);
    vector<long long> neighborSum(n + 2, 0);
    for (int u = 1; u <= n; ++u)
        for (int k = 0; k < (int)children[u].size(); ++k) {
            int v = children[u][k];
            ChildRank[v] = k; parent[v] = u;
            neighborSum[u] += v; neighborSum[v] += u;
        }
    long long N = n + 2; int v_virt = n + 1, r = 1;
    ChildRank[v_virt] = (int)children[r].size(); parent[v_virt] = r;
    neighborSum[r] += v_virt; neighborSum[v_virt] += r;
    vector<int> D(n + 2, 0);
    for (int i = 1; i <= n; i++) D[i] = (int)children[i].size() + 1;
    D[r] = (int)children[r].size() + 1; D[v_virt] = 1;
    vector<long long> S; S.reserve(n);
    int ptr = 1; while (ptr <= n + 1 && D[ptr] != 1) ptr++; int leaf = ptr;
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

    // Decode
    vector<int> D_dec(n + 2, 1), out_deg(n + 2, 0);
    for (long long V : S) { D_dec[abs(V) / N]++; out_deg[abs(V) / N]++; }
    vector<int> BP(n + 2, 0); int off = 0;
    for (int v = 1; v <= n + 1; ++v) { BP[v] = off; off += out_deg[v]; }
    vector<int> M(off + 1, 0);
    ptr = 1; while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++; leaf = ptr;
    for (long long V : S) {
        long long absV = abs(V), P = absV / N, k = absV % N;
        M[BP[P] + k] = leaf; D_dec[P]--;
        if (D_dec[P] == 1 && P < ptr) leaf = (int)P;
        else { ptr++; while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++; leaf = ptr; }
    }
    int u_dec = -1, v_dec = -1;
    for (int i = 1; i <= n + 1; i++) if (D_dec[i] == 1) { if (u_dec == -1) u_dec = i; else v_dec = i; }
    int root = (u_dec == n + 1) ? v_dec : u_dec;

    volatile int sink = 0;
    struct Frame { int node; int ci; };
    stack<Frame> stk; stk.push({root, 0});
    while (!stk.empty()) {
        Frame& f = stk.top(); sink = f.node;
        if (f.ci < out_deg[f.node]) { stk.push({M[BP[f.node] + f.ci], 0}); f.ci++; }
        else stk.pop();
    }
}

void runLOUDS(int n, const vector<vector<int>>& children) {
    // Encode
    vector<bool> bv; bv.reserve(2*n+2);
    bv.push_back(true); bv.push_back(false);
    queue<int> q; q.push(1);
    vector<int> bfs; bfs.reserve(n); bfs.push_back(1);
    while (!q.empty()) {
        int nd = q.front(); q.pop();
        for (int c : children[nd]) { bv.push_back(true); q.push(c); bfs.push_back(c); }
        bv.push_back(false);
    }

    // Build rank/select acceleration (O(1) via 64-bit blocks)
    int totalBits = (int)bv.size();
    int numBlocks = (totalBits + 63) / 64;
    vector<uint64_t> blocks(numBlocks, 0);
    for (int i = 0; i < totalBits; i++) {
        if (bv[i]) blocks[i / 64] |= (1ULL << (i % 64));
    }
    vector<int> rankSB(numBlocks + 1, 0);
    for (int i = 0; i < numBlocks; i++) {
        rankSB[i + 1] = rankSB[i] + __builtin_popcountll(blocks[i]);
    }

    // Decode using sequential scan through packed 64-bit blocks
    vector<vector<int>> dc(n + 1);
    int ci = 1;
    int bitPos = 2; // skip super-root "10"
    int nodeIdx = 0;
    while (nodeIdx < n && bitPos < totalBits) {
        int nd = bfs[nodeIdx];
        int degree = 0;
        while (bitPos < totalBits) {
            int bi = bitPos / 64, bb = bitPos % 64;
            uint64_t word = blocks[bi] >> bb;
            int bl = 64 - bb;
            if (word == 0) { bitPos++; break; }
            int tz = __builtin_ctzll(~word);
            if (tz < bl) { degree += tz; bitPos += tz + 1; break; }
            else { degree += bl; bitPos += bl; }
        }
        for (int c = 0; c < degree; c++) {
            if (ci < (int)bfs.size()) dc[nd].push_back(bfs[ci++]);
        }
        nodeIdx++;
    }

    // DFS
    volatile int sink = 0;
    struct Frame { int node; int ci; };
    stack<Frame> stk; stk.push({1, 0});
    while (!stk.empty()) {
        Frame& f = stk.top(); sink = f.node;
        if (f.ci < (int)dc[f.node].size()) { stk.push({dc[f.node][f.ci], 0}); f.ci++; }
        else stk.pop();
    }
}

void runFB(int n, const vector<vector<int>>& tree) {
    flatbuffers::FlatBufferBuilder builder(1024*1024);
    struct Frame { int id; int ci; vector<flatbuffers::Offset<FB_Bench::Node>> co; };
    stack<Frame> stk; stk.push({1, 0, {}});
    flatbuffers::Offset<FB_Bench::Node> result;
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.ci < (int)tree[f.id].size()) { int child = tree[f.id][f.ci]; f.ci++; stk.push({child, 0, {}}); }
        else { auto off = FB_Bench::CreateNode(builder, f.id, builder.CreateVector(f.co)); stk.pop();
            if (!stk.empty()) stk.top().co.push_back(off); else result = off; }
    }
    builder.Finish(result);
    auto root = FB_Bench::GetNode(builder.GetBufferPointer());
    volatile int sink = 0;
    struct FBF { const FB_Bench::Node* n; int idx; };
    stack<FBF> stk2; stk2.push({root, 0});
    while (!stk2.empty()) {
        FBF& f = stk2.top(); sink = f.n->id();
        if (f.n->children() && f.idx < (int)f.n->children()->size()) { stk2.push({f.n->children()->Get(f.idx), 0}); f.idx++; }
        else stk2.pop();
    }
}

void runPB(int n, const vector<vector<int>>& tree) {
    google::protobuf::Arena encode_arena;
    PB_Bench::Node* pbRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&encode_arena);
    pbRoot->set_id(1);
    struct Frame { int id; int ci; PB_Bench::Node* node; };
    stack<Frame> stk; stk.push({1, 0, pbRoot});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.ci < (int)tree[f.id].size()) { int child = tree[f.id][f.ci]; f.ci++;
            PB_Bench::Node* cn = f.node->add_children(); cn->set_id(child); stk.push({child, 0, cn}); }
        else stk.pop();
    }
    string data; pbRoot->SerializeToString(&data);
    google::protobuf::Arena decode_arena;
    PB_Bench::Node* decRoot = google::protobuf::Arena::Create<PB_Bench::Node>(&decode_arena);
    decRoot->ParseFromString(data);
    volatile int sink = 0;
    struct PBF { const PB_Bench::Node* n; int idx; };
    stack<PBF> stk2; stk2.push({decRoot, 0});
    while (!stk2.empty()) {
        PBF& f = stk2.top(); sink = f.n->id();
        if (f.idx < f.n->children_size()) { stk2.push({&f.n->children(f.idx), 0}); f.idx++; }
        else stk2.pop();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <spps|louds|fb|pb> <dataset.txt>" << endl;
        return 1;
    }

    string method = argv[1];
    string dsfile = argv[2];
    int n = 0;
    auto tree = loadEdgeList(dsfile, n);
    if (n == 0) { cerr << "Failed to load " << dsfile << endl; return 1; }

    cout << "Method: " << method << ", Dataset: " << dsfile << ", n=" << n << endl;

    // Warmup
    if (method == "spps") runSPPS(n, tree);
    else if (method == "louds") runLOUDS(n, tree);
    else if (method == "fb") runFB(n, tree);
    else if (method == "pb") runPB(n, tree);

    // 5 timed iterations
    for (int i = 0; i < 5; i++) {
        auto t0 = high_resolution_clock::now();
        if (method == "spps") runSPPS(n, tree);
        else if (method == "louds") runLOUDS(n, tree);
        else if (method == "fb") runFB(n, tree);
        else if (method == "pb") runPB(n, tree);
        auto t1 = high_resolution_clock::now();
        double ms = duration_cast<duration<double, milli>>(t1 - t0).count();
        cout << "  Iter " << (i+1) << ": " << ms << " ms" << endl;
    }

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    cout << "Peak RSS: " << (usage.ru_maxrss / 1024 / 1024) << " MB" << endl;
#else
    cout << "Peak RSS: " << (usage.ru_maxrss / 1024) << " MB" << endl;
#endif
    cout << "DONE" << endl;
    return 0;
}
