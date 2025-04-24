#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/time.h>

#include <faiss/AutoTune.h>
#include <faiss/index_factory.h>
#include <faiss/IndexIVFPQ.h>
#include <faiss/index_io.h>
#include <faiss/invlists/OnDiskInvertedLists.h>
#include "include/common.h"
#include "include/search_type.h"
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>

double elapsed() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

float* fvecs_read(const char* fname, int32_t* d_out, int32_t* n_out) {
    FILE* f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "could not open %s\n", fname);
        perror("");
        abort();
    }
    int d;
    fread(&d, 1, sizeof(int), f);
    assert((d > 0 && d < 1000000) || !"unreasonable dimension");
    fseek(f, 0, SEEK_SET);
    struct stat st;
    fstat(fileno(f), &st);
    int32_t sz = st.st_size;
    assert(sz % ((d + 1) * 4) == 0 || !"weird file size");
    int32_t n = sz / ((d + 1) * 4);

    *d_out = d;
    *n_out = n;
    float* x = new float[n * (d + 1)];
    int32_t nr = fread(x, sizeof(float), n * (d + 1), f);
    assert(nr == n * (d + 1) || !"could not read whole file");

    // shift array to remove row headers
    for (int32_t i = 0; i < n; i++)
        memmove(x + i * d, x + 1 + i * (d + 1), d * sizeof(*x));

    fclose(f);
    return x;
}

// not very clean, but works as long as sizeof(int) == sizeof(float)
int* ivecs_read(const char* fname, int32_t* d_out, int32_t* n_out) {
    return (int*)fvecs_read(fname, d_out, n_out);
}

bool compareba(const std::pair<int32_t, int64_t>& a, const std::pair<int32_t, int64_t>& b) {
    return a.first > b.first;
}

int main()
{
    int nq_test = 1000;
    int nr_of_dpus = 511;
    int ks = 100; // TOPK

    // output buffers
    faiss::idx_t* I = new faiss::idx_t[nq_test * ks];

    faiss::idx_t* gt; // nq * k matrix of ground-truth nearest-neighbors
    // load ground-truth and convert int to long
    int32_t nq2;
    int32_t nq = 10000;
    int* gt_int = ivecs_read("./sift1M/sift_groundtruth.ivecs", &ks, &nq2);
    assert(nq2 == nq || !"incorrect nb of ground truth entries");

    gt = new faiss::idx_t[ks * nq];
    for (int i = 0; i < ks * nq; i++) {
        gt[i] = gt_int[i];
    }
    delete[] gt_int;

    //read from file
    std::vector<std::vector<int> > q_dpu; // 每个query 对应的dpu (nq_test,std::vector<bool>(nr_of_dpus,false))
    std::vector<std::vector<Idx_piar> > I_dpu; // dpu 返回的 index 结果  (nr_of_dpus,std::vector<Idx_pair>((max_q_o-2)*ks))
    std::vector<std::vector<int> > I_dpu_offset; // 记录读取DPU结果的位置，避免后续重复读 (nq_test,std::vector<int>(nr_of_dpus))
    // 读取q_dpu
    std::ifstream inFile("q_dpuVector.txt");

    // 检查文件是否成功打开
    if (!inFile) {
        std::cerr << "无法打开文件进行读操作。" << std::endl;
        return 1;
    }

    // 从文件中读取数据并存入二维 vector
    std::string line;
    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        std::vector<int> row;
        int num;
        while (iss >> num) {
            row.push_back(num);
        }
        q_dpu.push_back(row);
    }
    // 关闭文件流
    inFile.close();

    // 读取I_dpu
    std::ifstream inFile2("I_dpuVector.txt");

    // 检查文件是否成功打开
    if (!inFile2) {
        std::cerr << "无法打开文件进行读操作。" << std::endl;
        return 1;
    }

    // 从文件中读取数据并存入二维 vector
    while (std::getline(inFile2, line)) {
        std::istringstream iss(line);
        std::vector<Idx_piar> row;
        int fir,sed;
        while (iss >> fir && iss >> sed) {
            Idx_piar num; 
            num.fir = fir;
            num.sed = sed;
            row.push_back(num);
        }
        I_dpu.push_back(row);
    }

    // 关闭文件流
    inFile2.close();

    // 读取I_dpu_offset
    std::ifstream inFile3("I_dpu_offsetVector.txt");

    // 检查文件是否成功打开
    if (!inFile3) {
        std::cerr << "无法打开文件进行读操作。" << std::endl;
        return 1;
    }

    // 从文件中读取数据并存入二维 vector
    while (std::getline(inFile3, line)) {
        std::istringstream iss(line);
        std::vector<int> row;
        int num;
        while (iss >> num) {
            row.push_back(num);
        }
        I_dpu_offset.push_back(row);
    }

    // 关闭文件流
    inFile2.close();



    // record the result
    std::vector<std::vector<std::pair<int32_t, int64_t>>> I_temp(nq_test); //真实结果 I


    double t4 = elapsed();
    for(int i=0;i<nq_test;i++)
    {
        for(int j=0; j<nr_of_dpus; j++)
        {
            if(q_dpu[i][j])
            {
                int res_begin = I_dpu_offset[i][j];
                for(int k=0; k<ks; k++)
                {
                    if(I_dpu[j][res_begin+k].fir == -1)
                    {
                        break;
                    }
                    if(I_dpu[j][res_begin+k].fir<=1 || std::isnan(I_dpu[j][res_begin+k].fir))
                    {
                        I_dpu[j][res_begin+k].fir=INT32_MIN;
                    }
                    I_temp[i].push_back(std::make_pair(I_dpu[j][res_begin+k].fir, I_dpu[j][res_begin+k].sed));
                }
            }
        }
    }
    for(int i=0;i<nq_test;i++)
    {
        std::sort(I_temp[i].begin(), I_temp[i].end(), compareba);
        for(int j=0; j<ks; j++)
        {
            I[i*ks + j] = I_temp[i][j].second;
            // printf("I[%d]: %ld, D[%d]: %u ",i*ks+j,I_temp[i][j].second,i*ks+j,I_temp[i][j].first);
        }
    }

    double t5 = elapsed();
    printf("[%.6f s] Post-process, Time: %.6f ms\n",
            elapsed() - t4,
            (t5-t4)*1000);

    printf("[%.6f s] Compute recalls\n", elapsed() - t4);

    // evaluate result by hand.
    int n_1 = 0, n_10 = 0, n_100 = 0;
    for (int i = 9000; i < nq; i++) {
        int gt_nn = gt[i * ks];
        for (int j = 0; j < ks; j++) {
            if (I[(i-9000) * ks + j] == gt_nn) {
                if (j < 1)
                    n_1++;
                if (j < 10)
                    n_10++;
                if (j < 100)
                    n_100++;
            }
        }
    }
    printf("R@1 = %.4f\n", n_1 / float(nq_test));
    printf("R@10 = %.4f\n", n_10 / float(nq_test));
    printf("R@100 = %.4f\n", n_100 / float(nq_test));
    return 0;
}