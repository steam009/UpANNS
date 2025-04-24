export NR_DPUS=896
export NR_TASKLETS=16
export MS=12
export KSUB=256
export DSUB=8
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=12
export TOPK=1
export MAX_Q_O=496
export MAX_PROBE_NUM=792
export DIMM=96
export MAX_DPU_ID=72
export CODEBOOK_SIZE=8192
export ETD=32
export NPROBS=64
export BS=1000
# export DEBUG=1

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_deep1b4096nprobs64top${TOPK}.txt

export TOPK=10

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_deep1b4096nprobs64top${TOPK}.txt

export TOPK=100

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_deep1b4096nprobs64top${TOPK}.txt