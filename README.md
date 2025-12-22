# KLL-Polymer: Near-optimal Per-key Quantile Estimation Using One KLL Sketch

This repository provides source codes for **KLL-Polymer** and its variant, KLL-Polymer with deterministic compaction (**KLL-Polymer-DC** for short) for per-key quantile estimation in data streams. 

## Dataset settings

We evaluate **KLL-Polymer** and its variant **KLL-Polymer-DC** on six datasets, including two real datasets (CAIDA and MAWI) and four synthetic datasets. 

To run on CAIDA or MAWI dataset, download the datasets from their websites. 

To run synthetic datasets, run `python generate-dataset.py` to generate synthetic datasets, and compile the source codes to run. 

## Baselines

We also provide source codes for baseline solutions, including **HistSketch**, **SQUAD**, **SketchPolymer** and **M4**. 


## RocksDB implementation

We implement KLL-Polymer on top of RocksDB database to reduce tail latency for quantile queries. 

## Run KLL-Polymer and KLL-Polymer-DC on static datasets: 

First enter the `cpu` directory. 
```bash
$ cd cpu
```

Compile the source code using:

```bash
$ make
```

Then use the following command to run: 

```bash
$ ./test [DATASET_PATH] [SAVE_PATH]
```

The ARE and AQE results are printed in the console, and AQE for different quantiles are saved in `SAVE_PATH`. 

## Run KLL-Polymer on RocksDB: 

First enter the `db` directory.

```bash
$ cd db
```

Clone the repository of RocksDB from GitHub: 

```bash
$ git clone https://github.com/facebook/rocksdb.git
```

Compile RocksDB project to obtain `librocksdb.a`. 

```bash
$ cd rocksdb
$ make
```

Then run KLL-Polymer on RocksDB using the following command: 

```bash
$ cd ..
$ make
$ ./kll_bench [DATASET_PATH] [RUN_MODE(heavy/mixed)] [MEMORY]
```

The results are printed in the console. 