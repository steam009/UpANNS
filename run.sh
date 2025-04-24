export NR_DPUS=896
export NR_TASKLETS=16
export MS=20
export KSUB=256
export DSUB=5
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=20
export TOPK=10
export MAX_Q_O=1008
export MAX_PROBE_NUM=2099
export DIMM=104
export MAX_DPU_ID=10
export CODEBOOK_SIZE=8192
export ETD=16
export NPROBS=16
# export DEBUG=1

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make


