// profiler.cpp — Profiling binary for perf stat / AMD uProf
// Runs SPPS, LOUDS, FlatBuffers, Protobuf sequentially
// Each method separated by a sleep() marker for profiler segmentation
// Uses all 3 datasets: Django AST (2.3M), sqlite3 AST (503K), XMark XML (500K)

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <fstream>
#include <stack>
#include <queue>
#include <unistd.h>
#include <sys/resource.h>

#include "tree.pb.h"
#include "tree_generated.h"

using namespace std;
using namespace std::chrono;

// =========================================================================
// TREE LOADING
// =========================================================================
vector<vector<int>> loadEdgeList(const string& f, int& n) {
    ifstream file(f);
    if (!file.is_open()) { cerr << "Cannot open " << f << endl; n = 0; return {}; }
    file >> n;
    vector<vector<int>> c(n + 1);
    int u, v;
    while (file >> u >> v) c[u].push_back(v);
    return c;
}

// =========================================================================
// SPPS ENCODE + DECODE + DFS
// =========================================================================
void runSPPS(int n, const vector<vector<int>>& children) {
    cout << "  [SPPS] n=" << n << flush;
    // Encode
    vector<int> ChildRank(n + 2, 0);
    vector<int> parent(n + 2, 0);
    vector<long long> neighborSum(n + 2, 0);
    int r = 1;
    for (int u = 1; u <= n; ++u)
        for (int k = 0; k < (int)children[u].size(); ++k) {
            int v = children[u][k];
            ChildRank[v] = k; parent[v] = u;
            neighborSum[u] += v; neighborSum[v] += u;
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
    cout << " enc" << flush;

    // Decode
    vector<int> D_dec(n + 2, 1), out_deg(n + 2, 0);
    for (long long V : S) { long long P = abs(V) / N; D_dec[P]++; out_deg[P]++; }
    vector<int> BP(n + 2, 0);
    int off = 0;
    for (int v = 1; v <= n + 1; ++v) { BP[v] = off; off += out_deg[v]; }
    vector<int> M(off + 1, 0);
    ptr = 1;
    while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++;
    leaf = ptr;
    for (long long V : S) {
        long long absV = abs(V);
        long long P = absV / N, k = absV % N;
        M[BP[P] + k] = leaf;
        D_dec[P]--;
        if (D_dec[P] == 1 && P < ptr) leaf = (int)P;
        else { ptr++; while (ptr <= n + 1 && D_dec[ptr] != 1) ptr++; leaf = ptr; }
    }
    int u_dec = -1, v_dec = -1;
    for (int i = 1; i <= n + 1; i++)
        if (D_dec[i] == 1) { if (u_dec == -1) u_dec = i; else v_dec = i; }
    int root = (u_dec == n + 1) ? v_dec : u_dec;
    cout << " dec" << flush;

    // DFS
    volatile int sink = 0;
    struct Frame { int node; int ci; };
    stack<Frame> stk;
    stk.push({root, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        sink = f.node;
        if (f.ci < out_deg[f.node]) {
            int child = M[BP[f.node] + f.ci]; f.ci++;
            stk.push({child, 0});
        } else stk.pop();
    }
    cout << " dfs" << endl;
}

// =========================================================================
// LOUDS
// =========================================================================
void runLOUDS(int n, const vector<vector<int>>& children) {
    cout << "  [LOUDS] n=" << n << flush;
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
    cout << " enc" << flush;

    // Decode
    vector<vector<int>> dc(n + 1);
    int bp = 2, ni = 0, ci = 1;
    while (ni < n && bp < (int)bv.size()) {
        int nd = bfs[ni];
        while (bp < (int)bv.size() && bv[bp]) {
            if (ci < (int)bfs.size()) dc[nd].push_back(bfs[ci++]);
            bp++;
        }
        bp++; ni++;
    }
    cout << " dec" << flush;

    // DFS
    volatile int sink = 0;
    struct Frame { int node; int ci; };
    stack<Frame> stk;
    stk.push({1, 0});
    while (!stk.empty()) {
        Frame& f = stk.top();
        sink = f.node;
        if (f.ci < (int)dc[f.node].size()) {
            int child = dc[f.node][f.ci++];
            stk.push({child, 0});
        } else stk.pop();
    }
    cout << " dfs" << endl;
}

// =========================================================================
// FLATBUFFERS
// =========================================================================
void runFB(int n, const vector<vector<int>>& tree) {
    cout << "  [FB] n=" << n << flush;
    flatbuffers::FlatBufferBuilder builder(1024*1024);
    struct Frame { int id; int ci; vector<flatbuffers::Offset<FB_Bench::Node>> co; };
    stack<Frame> stk;
    stk.push({1, 0, {}});
    flatbuffers::Offset<FB_Bench::Node> result;
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.ci < (int)tree[f.id].size()) {
            int child = tree[f.id][f.ci]; f.ci++;
            stk.push({child, 0, {}});
        } else {
            auto off = FB_Bench::CreateNode(builder, f.id, builder.CreateVector(f.co));
            stk.pop();
            if (!stk.empty()) stk.top().co.push_back(off);
            else result = off;
        }
    }
    builder.Finish(result);
    cout << " enc" << flush;

    auto root = FB_Bench::GetNode(builder.GetBufferPointer());
    cout << " dec" << flush;

    volatile int sink = 0;
    struct FBF { const FB_Bench::Node* n; int idx; };
    stack<FBF> stk2;
    stk2.push({root, 0});
    while (!stk2.empty()) {
        FBF& f = stk2.top();
        sink = f.n->id();
        if (f.n->children() && f.idx < (int)f.n->children()->size()) {
            auto child = f.n->children()->Get(f.idx); f.idx++;
            stk2.push({child, 0});
        } else stk2.pop();
    }
    cout << " dfs" << endl;
}

// =========================================================================
// PROTOBUF
// =========================================================================
void runPB(int n, const vector<vector<int>>& tree) {
    cout << "  [PB] n=" << n << flush;
    PB_Bench::Node pbRoot;
    struct Frame { int id; int ci; PB_Bench::Node* node; };
    stack<Frame> stk;
    pbRoot.set_id(1);
    stk.push({1, 0, &pbRoot});
    while (!stk.empty()) {
        Frame& f = stk.top();
        if (f.ci < (int)tree[f.id].size()) {
            int child = tree[f.id][f.ci]; f.ci++;
            PB_Bench::Node* cn = f.node->add_children();
            cn->set_id(child);
            stk.push({child, 0, cn});
        } else stk.pop();
    }
    string data; pbRoot.SerializeToString(&data);
    cout << " enc" << flush;

    PB_Bench::Node decRoot;
    decRoot.ParseFromString(data);
    cout << " dec" << flush;

    volatile int sink = 0;
    struct PBF { const PB_Bench::Node* n; int idx; };
    stack<PBF> stk2;
    stk2.push({&decRoot, 0});
    while (!stk2.empty()) {
        PBF& f = stk2.top();
        sink = f.n->id();
        if (f.idx < f.n->children_size()) {
            const PB_Bench::Node* child = &f.n->children(f.idx); f.idx++;
            stk2.push({child, 0});
        } else stk2.pop();
    }
    cout << " dfs" << endl;
}

// =========================================================================
// MAIN — Profile all methods × all datasets
// =========================================================================
int main() {
    cout << "========== PERF STAT / AMD uPROF PROFILING RUN ==========" << endl;

    struct DS { string name; string file; int n; };
    vector<DS> datasets;
    { int n = 0; loadEdgeList("datasets/real_ast_benchmark.txt", n);
      if (n > 0) datasets.push_back({"Django AST", "datasets/real_ast_benchmark.txt", n}); }
    { int n = 0; loadEdgeList("datasets/sqlite3_ast_edges.txt", n);
      if (n > 0) datasets.push_back({"sqlite3 AST", "datasets/sqlite3_ast_edges.txt", n}); }
    { int n = 0; loadEdgeList("datasets/xmark_edges.txt", n);
      if (n > 0) datasets.push_back({"XMark XML", "datasets/xmark_edges.txt", n}); }

    cout << "Datasets: " << datasets.size() << endl;
    for (auto& d : datasets) cout << "  " << d.name << ": n=" << d.n << endl;

    for (auto& ds : datasets) {
        int n = ds.n;
        auto tree = loadEdgeList(ds.file, n);

        cout << "\n--- " << ds.name << " (n=" << n << ") ---" << endl;

        // Run each method 3 times for stable counters
        for (int rep = 0; rep < 3; rep++) {
            runSPPS(n, tree);
            usleep(100000); // 100ms marker between methods
            runLOUDS(n, tree);
            usleep(100000);
            runFB(n, tree);
            usleep(100000);
            runPB(n, tree);
            usleep(200000); // 200ms marker between reps
        }
    }

    // Also get peak memory
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    cout << "\nPeak RSS: " << (usage.ru_maxrss / 1024 / 1024) << " MB" << endl;
#else
    cout << "\nPeak RSS: " << (usage.ru_maxrss / 1024) << " MB" << endl;
#endif
    cout << "========== PROFILING COMPLETE ==========" << endl;
    return 0;
}
