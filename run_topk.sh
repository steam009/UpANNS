export NR_DPUS=896
export NR_TASKLETS=16
export MS=16
export KSUB=256
export DSUB=8
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=20
export TOPK=100
export MAX_Q_O=262
export MAX_PROBE_NUM=413
export DIMM=128
export MAX_DPU_ID=20
export CODEBOOK_SIZE=8192
export ETD=32
export NPROBS=64
# export DEBUG=1

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_siftnprobs64topk${TOPK}.txt

export TOPK=10
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_siftnprobs64topk${TOPK}.txt

export TOPK=5
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_siftnprobs64topk${TOPK}.txt

export TOPK=1
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_siftnprobs64topk${TOPK}.txt

