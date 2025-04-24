export NR_DPUS=896
export NR_TASKLETS=14
export MS=20
export KSUB=256
export DSUB=5
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=20
export TOPK=10
export MAX_Q_O=400
export MAX_PROBE_NUM=584
export DIMM=104
export MAX_DPU_ID=72
export CODEBOOK_SIZE=8192
export ETD=32
export NPROBS=256
# export DEBUG=1

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b8192top10nprobs${NPROBS}.txt

export NPROBS=128
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b8192top10nprobs${NPROBS}.txt

export NPROBS=64
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b8192top10nprobs${NPROBS}.txt

export NPROBS=32
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b8192top10nprobs${NPROBS}.txt