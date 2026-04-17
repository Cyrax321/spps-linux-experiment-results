# Datasets

| File | Nodes | Source | Description |
|------|-------|--------|-------------|
| `real_ast_benchmark.txt` | 2,325,575 | Django framework | Python AST via `ast.parse()` |
| `sqlite3_ast_edges.txt` | 503,141 | sqlite3.c | Clang AST via `-ast-dump` |
| `xmark_edges.txt` | 500,000 | XMark schema | Synthetic XML benchmark |

## Format

```
n
parent1 child1
parent2 child2
...
```

First line: number of nodes `n`
Subsequent lines: directed edge pairs (parent → child)
Nodes use contiguous integer labels 1..n with root = 1.
