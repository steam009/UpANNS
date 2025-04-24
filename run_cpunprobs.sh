g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=32 -DTOPK=10

./demo_sift1B > res_cpusift8192top10nprobs32.txt

g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=64 -DTOPK=10

./demo_sift1B > res_cpusift8192top10nprobs64.txt

g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=128 -DTOPK=10

./demo_sift1B > res_cpusift8192top10nprobs128.txt

g++ demo_sift1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_sift1B -DNPROBS=256 -DTOPK=10

./demo_sift1B > res_cpusift8192top10nprobs256.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=32

# ./demo_SPACE1B > res_cpuSPACEtop10nprobs32.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=64

# ./demo_SPACE1B > res_cpuSPACEtop10nprobs64.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=128

# ./demo_SPACE1B > res_cpuSPACEtop10nprobs128.txt

# g++ demo_SPACE1B.cpp -L/home/cst -lfaiss -Ifaiss -O2 -o demo_SPACE1B -DNPROBS=256

# ./demo_SPACE1B > res_cpuSPACEtop10nprobs256.txt
