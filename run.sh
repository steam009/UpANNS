export NR_DPUS=896
export NR_TASKLETS=16
export MS=12
export KSUB=256
export DSUB=8
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=12
export TOPK=10
export MAX_Q_O=1008
export MAX_PROBE_NUM=2201
export DIMM=96
export MAX_DPU_ID=72
export CODEBOOK_SIZE=8192
export ETD=32
export NPROBS=256
export BS=1000
# export DEBUG=1

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
# ./build/release/host > res_deep1b4096top10nprobe64_bs${BS}.txt
./build/release/host > res_deep1B4096nprobe${NPROBS}_withoutcache_ablation.txt

# export NPROBS=128
# # export DEBUG=1

# make clean
# rm dpu/search_dpu
# rm dpu/search_dpu.o
# rm dpu/search_dpu.d

# make -C dpu

# make

# ./build/release/host > res_deep1B4096nprobe${NPROBS}_withoutcache_ablation.txt

# export NPROBS=64
# # export DEBUG=1

# make clean
# rm dpu/search_dpu
# rm dpu/search_dpu.o
# rm dpu/search_dpu.d

# make -C dpu

# make

# ./build/release/host > res_deep1B4096nprobe${NPROBS}_withoutcache_ablation.txt

# export NPROBS=32
# # export DEBUG=1

# make clean
# rm dpu/search_dpu
# rm dpu/search_dpu.o
# rm dpu/search_dpu.d

# make -C dpu

# make

# ./build/release/host > res_deep1B4096nprobe${NPROBS}_cache_ablation.txt





