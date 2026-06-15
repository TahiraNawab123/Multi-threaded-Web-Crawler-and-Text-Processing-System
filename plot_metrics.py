#!/usr/bin/env python3
import argparse, csv, os, subprocess, sys, time
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

CRAWLER  = "./crawler"
URL_FILE = "URLs.txt"
CSV_FILE = "metrics.csv"
OUT_DIR  = "graphs"

THREAD_COUNTS  = [2, 4, 6, 8, 10, 12, 15, 20]
CHUNK_SIZES    = [50, 100, 200, 500, 1000, 2000]
FIXED_T        = 16
DP_RATIOS = [
    (2,           FIXED_T - 2),
    (FIXED_T//4,  FIXED_T - FIXED_T//4),
    (FIXED_T//2,  FIXED_T - FIXED_T//2),
    (3*FIXED_T//4, FIXED_T - 3*FIXED_T//4),
    (FIXED_T - 2, 2),
]
DEFAULT_CHUNK = 200
DEFAULT_T     = 8

def run_crawler(url_file, T, chunk, D=None, label=""):
    cmd = [CRAWLER, url_file, str(T), str(chunk)]
    if D is not None:
        cmd.append(str(D))
    print(f"  Running: {' '.join(cmd)}  {label}")
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        if result.returncode != 0:
            print(f"  [ERROR] exit {result.returncode}: {result.stderr[:300]}")
            return False
        return True
    except subprocess.TimeoutExpired:
        print("  [TIMEOUT] skipped"); return False

def collect_experiments():
    if os.path.exists(CSV_FILE): os.remove(CSV_FILE)
    print("\n=== Experiment 1: Varying total threads ===")
    for T in THREAD_COUNTS:
        run_crawler(URL_FILE, T, DEFAULT_CHUNK, label=f"(T={T})"); time.sleep(1)
    print("\n=== Experiment 2: Varying chunk size ===")
    for cs in CHUNK_SIZES:
        run_crawler(URL_FILE, DEFAULT_T, cs, label=f"(chunk={cs})"); time.sleep(1)
    print(f"\n=== Experiment 3: D:P ratio (T={FIXED_T}) ===")
    for D, P in DP_RATIOS:
        run_crawler(URL_FILE, FIXED_T, DEFAULT_CHUNK, D=D, label=f"(D={D}, P={P})"); time.sleep(1)
    print("\nAll experiments done.\n")

def load_csv():
    rows = []
    if not os.path.exists(CSV_FILE):
        print(f"[ERROR] {CSV_FILE} not found."); sys.exit(1)
    with open(CSV_FILE, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({k: float(v) if k not in ("T","D","P","chunk_size") else int(v)
                         for k, v in row.items()})
    return rows

STYLE = dict(marker="o", linewidth=2, markersize=7)

def save(fig, name):
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, name)
    fig.savefig(path, dpi=150, bbox_inches="tight")
    print(f"  Saved: {path}"); plt.close(fig)

def plot_threads_vs_time(rows):
    seen = {}
    for r in rows:
        if r["chunk_size"] == DEFAULT_CHUNK:
            seen.setdefault(r["T"], []).append(r["total_time"])
    if not seen: print("  [SKIP] No data for graph 1"); return
    Ts = sorted(seen)
    times = [sum(seen[t])/len(seen[t]) for t in Ts]
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(Ts, times, color="#2563EB", **STYLE)
    ax.set_xlabel("Total Threads (T)", fontsize=12)
    ax.set_ylabel("Execution Time (s)", fontsize=12)
    ax.set_title("Total Threads vs Execution Time", fontsize=14, fontweight="bold")
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax.grid(True, linestyle="--", alpha=0.6); fig.tight_layout()
    save(fig, "graph1_threads_vs_time.png")

def plot_chunk_vs_throughput(rows):
    seen = {}
    for r in rows:
        if r["T"] == DEFAULT_T:
            seen.setdefault(r["chunk_size"], []).append(r["throughput"])
    if not seen: print("  [SKIP] No data for graph 2"); return
    cs_vals = sorted(seen)
    tps = [sum(seen[c])/len(seen[c]) for c in cs_vals]
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(cs_vals, tps, color="#16A34A", **STYLE)
    ax.set_xlabel("Chunk Size (words)", fontsize=12)
    ax.set_ylabel("Throughput (words/s)", fontsize=12)
    ax.set_title("Chunk Size vs Throughput", fontsize=14, fontweight="bold")
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax.grid(True, linestyle="--", alpha=0.6); fig.tight_layout()
    save(fig, "graph2_chunksize_vs_throughput.png")

def plot_dp_ratio_vs_time(rows):
    seen = {}
    for r in rows:
        if r["T"] == FIXED_T and r["chunk_size"] == DEFAULT_CHUNK:
            seen.setdefault(r["D"], []).append(r["total_time"])
    if not seen: print("  [SKIP] No data for graph 3"); return
    labels, times = [], []
    for D, P in sorted(DP_RATIOS, key=lambda x: x[0]):
        if D in seen:
            labels.append(f"D={D}\nP={P}"); times.append(sum(seen[D])/len(seen[D]))
    if not times: print("  [SKIP] No matching D:P rows"); return
    fig, ax = plt.subplots(figsize=(9, 5))
    bars = ax.bar(labels, times, color="#DC2626", width=0.5, edgecolor="black")
    ax.bar_label(bars, fmt="%.2fs", padding=3, fontsize=9)
    ax.set_xlabel("D : P Ratio", fontsize=12)
    ax.set_ylabel("Execution Time (s)", fontsize=12)
    ax.set_title(f"D:P Ratio vs Execution Time  (T={FIXED_T}, chunk={DEFAULT_CHUNK})",
                 fontsize=13, fontweight="bold")
    ax.grid(True, axis="y", linestyle="--", alpha=0.6); fig.tight_layout()
    save(fig, "graph3_dp_ratio_vs_time.png")

def plot_stage_breakdown(rows):
    seen = {}
    for r in rows:
        if r["chunk_size"] == DEFAULT_CHUNK:
            T = r["T"]
            if T not in seen: seen[T] = {"dl": [], "par": []}
            seen[T]["dl"].append(r["dl_avg"]); seen[T]["par"].append(r["par_avg"])
    if not seen: return
    Ts = sorted(seen)
    dl_t  = [sum(seen[t]["dl"]) /len(seen[t]["dl"])  for t in Ts]
    par_t = [sum(seen[t]["par"])/len(seen[t]["par"]) for t in Ts]
    x, w = list(range(len(Ts))), 0.35
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.bar([i-w/2 for i in x], dl_t,  w, label="Download (avg/thread)",
           color="#F59E0B", edgecolor="black")
    ax.bar([i+w/2 for i in x], par_t, w, label="Parse (avg/thread)",
           color="#6366F1", edgecolor="black")
    ax.set_xticks(x); ax.set_xticklabels([str(t) for t in Ts])
    ax.set_xlabel("Total Threads (T)", fontsize=12)
    ax.set_ylabel("Avg Stage Time (s)", fontsize=12)
    ax.set_title("Download vs Parse Stage Time per Thread", fontsize=13, fontweight="bold")
    ax.legend(); ax.grid(True, axis="y", linestyle="--", alpha=0.6); fig.tight_layout()
    save(fig, "graph4_stage_breakdown.png")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    if args.run:
        if not os.path.exists(CRAWLER):
            print("[ERROR] ./crawler not found. Run 'make' first."); sys.exit(1)
        collect_experiments()
    rows = load_csv()
    print(f"Loaded {len(rows)} rows from {CSV_FILE}\n")
    print("Generating graphs...")
    plot_threads_vs_time(rows)
    plot_chunk_vs_throughput(rows)
    plot_dp_ratio_vs_time(rows)
    plot_stage_breakdown(rows)
    print(f"\nDone! Graphs saved in ./{OUT_DIR}/")
