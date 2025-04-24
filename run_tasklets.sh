export NR_DPUS=896
export NR_TASKLETS=19
export MS=20
export KSUB=256
export DSUB=5
export MAX_DPU_STORE_SIZE=1366000
export CODE_SIZE=20
export TOPK=10
export MAX_Q_O=496
export MAX_PROBE_NUM=792
export DIMM=104
export MAX_DPU_ID=72
export CODEBOOK_SIZE=8192
export ETD=32
export NPROBS=128
# export DEBUG=1

make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=18
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=17
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=16
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=15
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=14
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=13
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=12
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=11
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=10
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=9
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=8
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=7
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=6
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=5
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=4
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=3
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=2
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

export NR_TASKLETS=1
make clean
rm dpu/search_dpu
rm dpu/search_dpu.o
rm dpu/search_dpu.d

make -C dpu

make
./build/release/host > res_SPACEV1b4096top10nprobs${NPROBS}tasklet${NR_TASKLETS}.txt

