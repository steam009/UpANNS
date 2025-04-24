g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=128 -DTOPK=100

./demo_sift1B > res_cpusift8192nprob128top100.txt

g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=128 -DTOPK=10

./demo_sift1B > res_cpusift8192nprob128top10.txt

g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=128 -DTOPK=5

./demo_sift1B > res_cpusift8192nprob128top5.txt

g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=128 -DTOPK=1

./demo_sift1B > res_cpusift8192nprob128top1.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=64 -DTOPK=100

# ./demo_SPACE1B > res_cpuSPACEnprob64top100.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=64 -DTOPK=10

# ./demo_SPACE1B > res_cpuSPACEnprob64top10.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=64 -DTOPK=5

# ./demo_SPACE1B > res_cpuSPACEnprob64top5.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=64 -DTOPK=1

# ./demo_SPACE1B > res_cpuSPACEnprob64top1.txt
