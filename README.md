# Multi-threaded-Web-Crawler-and-Text-Processing-System
---
## Overview

A 4-stage concurrent pipeline in C that downloads 15 Project Gutenberg plaintext books,
processes them in parallel, and computes text statistics using POSIX threads,
mutexes, and condition variables — with zero busy-waiting.

**Assigned Group: C**
- Sentence Count (`.` `?` `!`)
- Palindrome Detection (unique palindromic words)
- Unique Word Count (distinct words across all URLs)

---
## Pipeline Architecture
```
URL File
   │
   ▼
[Stage 1] URL Reader          — single thread, fills URL Queue (capacity: 5)
   │
   ▼
[Stage 2] D × Downloader Threads   — I/O-bound, libcurl
          Downloads content → splits into N-word chunks → pushes to Chunk Queue
   │
   ▼
[Stage 3] P × Parser Threads       — CPU-bound
          Pulls chunks → computes local stats (sentences, palindromes, unique words)
   │
   ▼
[Stage 4] Aggregator          — single thread, merges LocalStats → GlobalStats
   │
   ▼
stdout + metrics.csv
```

Both queues use **circular buffers + POSIX mutex + condition variables**.

Thread split: `D = T / 3` (min 1), `P = T − D`
Optional 4th argument overrides D for performance experiments.

---
## Dependencies
```bash
sudo apt-get install gcc make libcurl4-openssl-dev python3-matplotlib
```

---
## Build
```bash
make          # builds ./crawler
make clean    # removes objects, binary, and metrics.csv
```

---

## Run

```bash
./crawler <url_file> <total_threads> <chunk_size> [d_override]
```

| Argument | Description |
|---|---|
| `url_file` | Path to plain-text URL file (one URL per line) |
| `total_threads` (T) | Total thread count (≥ 2) |
| `chunk_size` (N) | Words per text chunk |
| `d_override` *(optional)* | Override number of downloader threads |

### Examples

```bash
./crawler urls.txt 6  200        # D=2, P=4, 200-word chunks
./crawler urls.txt 12 500        # D=4, P=8, 500-word chunks
./crawler urls.txt 16 200 4      # T=16, D=4 (override), P=12
```

Each run **appends** one row to `metrics.csv`.
---

## File Structure

```
crawler/
├── crawler.h        – shared types, structs, queue config constants
├── main.c           – pipeline orchestration, thread creation, main()
├── queue.c          – thread-safe bounded circular queue (mutex + condvars)
├── stats.c          – Group C stats: process_chunk(), aggregate()
├── utils.c          – timing helpers, palindrome checker, FNV-1a hash
├── Makefile
├── urls.txt         – 15 Project Gutenberg plaintext URLs (DO NOT MODIFY)
├── plot_metrics.py  – experiment runner + graph generator
└── README.md
```

---

## Statistics (Group C)

| Stat | Implementation |
|---|---|
| **Sentence Count** | Each parser counts sentence-ending punctuation (`.` `?` `!`) per chunk; aggregator sums totals |
| **Palindrome Detection** | Each parser builds a local list of palindromic words; aggregator deduplicates with a linear scan |
| **Unique Word Count** | FNV-1a hash table (1M slots, chained buckets) per parser thread; aggregator merges for global distinct count |

---

## Performance Metrics

Auto-computed and logged to `metrics.csv` on every run:

- Total execution time
- Download stage time (avg per thread)
- Parse stage time (avg per thread)
- Aggregation time
- Total words processed
- Throughput (words/second)

---

## Performance Graphs

```bash
python3 plot_metrics.py          # plot from existing metrics.csv
python3 plot_metrics.py --run    # run all experiments first, then plot
```

Graphs saved to `./graphs/`:

| File | What it shows |
|---|---|
| `graph1_threads_vs_time.png` | Total threads vs execution time |
| `graph2_chunksize_vs_throughput.png` | Chunk size vs throughput |
| `graph3_dp_ratio_vs_time.png` | D:P ratio vs execution time (fixed T ≥ 15) |
| `graph4_stage_breakdown.png` | Download vs parse stage time breakdown |

D:P ratios tested for graph 3 (e.g. T=16):
`D=2/P=14`, `D=4/P=12`, `D=8/P=8`, `D=12/P=4`, `D=14/P=2`

---

## Key Design Decisions

- **No busy-waiting** — all blocking via `pthread_cond_wait`
- **Queue closure protocol** — producer calls `queue_close()`; consumers receive `NULL` on drain and exit cleanly
- **Last-downloader detection** — shared counter (mutex-protected) ensures chunk queue closes only after *all* downloaders finish
- **FNV-1a hash table** — 1M slots with chained buckets for O(1) average unique word tracking per parser thread
- **Palindrome deduplication** — local lists merged with linear scan during aggregation (palindrome set is small in practice)

---

## URLs

15 Project Gutenberg plaintext books:

```
https://www.gutenberg.org/cache/epub/67979/pg67979.txt
https://www.gutenberg.org/cache/epub/21500/pg21500.txt
https://www.gutenberg.org/cache/epub/27949/pg27949.txt
https://www.gutenberg.org/cache/epub/1259/pg1259.txt
https://www.gutenberg.org/cache/epub/45304/pg45304.txt
https://www.gutenberg.org/cache/epub/42486/pg42486.txt
https://www.gutenberg.org/cache/epub/1184/pg1184.txt
https://www.gutenberg.org/cache/epub/1513/pg1513.txt
https://www.gutenberg.org/cache/epub/2554/pg2554.txt
https://www.gutenberg.org/cache/epub/2641/pg2641.txt
https://www.gutenberg.org/cache/epub/18575/pg18575.txt
https://www.gutenberg.org/cache/epub/27279/pg27279.txt
https://www.gutenberg.org/cache/epub/22210/pg22210.txt
https://www.gutenberg.org/cache/epub/25810/pg25810.txt
https://www.gutenberg.org/cache/epub/27600/pg27600.txt
```
