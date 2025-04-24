export NR_DPUS=500
export NR_TASKLETS=16
export MS=16
export KSUB=256
export DSUB=8
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=16
export TOPK=10
export MAX_Q_O=332
export MAX_PROBE_NUM=688
export DIMM=128
export MAX_DPU_ID=31
export CODEBOOK_SIZE=8192
export ETD=32
export NPROBS=32
# export DEBUG=1

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make

./build/release/host > res_sift500M4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}dpu${NR_DPUS}.txt

export NR_DPUS=600

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make

./build/release/host > res_sift500M4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}dpu${NR_DPUS}.txt

export NR_DPUS=700

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make

./build/release/host > res_sift500M4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}dpu${NR_DPUS}.txt

export NR_DPUS=800

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make

./build/release/host > res_sift500M4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}dpu${NR_DPUS}.txt

export NR_DPUS=900

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make

./build/release/host > res_sift500M4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}dpu${NR_DPUS}.txt