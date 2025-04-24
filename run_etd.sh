export NR_DPUS=831
export NR_TASKLETS=16
export MS=32
export KSUB=256
export DSUB=4
export MAX_DPU_STORE_SIZE=1640000
export CODE_SIZE=32
export TOPK=100
export MAX_Q_O=127
export MAX_PROBE_NUM=200
export DIMM=128
export MAX_DPU_ID=186
export CODEBOOK_SIZE=8192
# export DEBUG=1

export ETD=16
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_etd${ETD}.txt

export ETD=8
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_etd${ETD}.txt

export ETD=4
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_etd${ETD}.txt


