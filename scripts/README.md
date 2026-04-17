# Scripts

| Script | Purpose | Requires |
|--------|---------|----------|
| `install_deps.sh` | Install all Ubuntu 22.04 dependencies | `sudo` |
| `run_all_blocks.sh` | Build and run all experiment blocks | Deps installed |
| `run_perf_profile.sh` | Microarchitectural profiling via `perf stat` | `sudo` or `paranoid=-1` |
| `run_uprof_profile.sh` | AMD uProf pipeline decomposition | AMD uProf installed |

## Quick Start

```bash
# First time setup
sudo ./install_deps.sh

# Run all experiments
./run_all_blocks.sh

# Profiling (requires root or perf_event_paranoid=-1)
sudo ./run_perf_profile.sh
```
