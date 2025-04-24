g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=64 -DNLIST=4096 -DTOPK=1 -DBS=1000
./demo_deep1B > res_deep1B4096cpunprobe64_top1.txt

g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=64 -DNLIST=4096 -DTOPK=10 -DBS=1000
./demo_deep1B > res_deep1B4096cpunprobe64_top10.txt

g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=64 -DNLIST=4096 -DTOPK=100 -DBS=1000
./demo_deep1B > res_deep1B4096cpunprobe64_top100.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=128 -DNLIST=4096 -DTOPK=10
# ./demo_deep1B > res_deep1B4096cputop10nprobe128.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=256 -DNLIST=4096 -DTOPK=10
# ./demo_deep1B > res_deep1B4096cputop10nprobe256.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=64 -DNLIST=8192 -DTOPK=10
# ./demo_deep1B > res_deep1B8192cputop10nprobe64.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=128 -DNLIST=8192 -DTOPK=10
# ./demo_deep1B > res_deep1B8192cputop10nprobe128.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=256 -DNLIST=8192 -DTOPK=10
# ./demo_deep1B > res_deep1B8192cputop10nprobe256.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=64 -DNLIST=16384 -DTOPK=10
# ./demo_deep1B > res_deep1B16384cputop10nprobe64.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=128 -DNLIST=16384 -DTOPK=10
# ./demo_deep1B > res_deep1B16384cputop10nprobe128.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=256 -DNLIST=16384 -DTOPK=10
# ./demo_deep1B > res_deep1B16384cputop10nprobe256.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=128 -DNLIST=8192 -DTOPK=1
# ./demo_deep1B > res_deep1B8192cpunprobe128top1.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=128 -DNLIST=8192 -DTOPK=10
# ./demo_deep1B > res_deep1B8192cpunprobe128top10.txt

# g++ -o demo_deep1B demo_deep1B.cpp -L/home/cst -lfaiss -Ifaiss -DNPROBE=128 -DNLIST=8192 -DTOPK=100
# ./demo_deep1B > res_deep1B8192cpunprobe128top100.txt