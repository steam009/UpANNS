# UpANNS
# [SC 25] UpANNS: Enhancing Billion-Scale ANNS Efficiency with Real-World PIM Architecture
## Prerequisites

### Software
- Faiss 1.8.0
- UPMEM SDK version 2023.2.0

### Hardware
- UPMEM PIM x 7
- Memory: 128 GB

## Directory Structure
```
./
├─dpu # dpu source code
└─include # head file
```

## Branch explanation
- UpANNS-{dataset name}: Contains the optimized UpANNS implementation for PIM.
- PIM-naive-{dataset name}: Contains the baseline naive implementation of ANNS on PIM.
- UpANNS-{dataset name}-cache-ablation: Contains the ablation study evaluating the contribution of the occurrence-aware encoding mechanism.

## Dataset Download
- [**DEEP**](https://research.yandex.com/blog/benchmarks-for-billion-scale-similarity-search)
- [**SIFT**](http://corpus-texmex.irisa.fr/)
- [**SPACEV**](https://github.com/microsoft/SPTAG/tree/main/datasets/SPACEV1B)

## Reproduce
We have provided shell scripts to automate the experiments. Simply, you can use `sh run.sh` to run the UpANNS.

### Main Environment Parameter description in `run.sh`
- `NR_DPUS`: numbr of DPU to run the UpANNS
- `NR_TASKLETS`: number of threads to run the UpANNS
- `MS`: the length of vector after encoded
- `KSUB`: the number of rows in codebook
- `DSUB`: each `DSUB` dimensions are encoded into one dimension
- `TOPK`: the size of search result.
- `DIMM`: the original dimensions.
- `ETD`:  the number of vectors that read once by each thread
- `BS`: the batch size

