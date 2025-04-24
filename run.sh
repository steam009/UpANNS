export NR_DPUS=831
export NR_TASKLETS=16
export MS=16
export KSUB=256
export DSUB=8
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=16
export TOPK=10
export MAX_Q_O=1002
export MAX_PROBE_NUM=1250
export DIMM=128
export MAX_DPU_ID=20
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


