/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
#include <faiss/IndexFlat.h>
#include <faiss/index_io.h>
#include <faiss/invlists/OnDiskInvertedLists.h>

#include <stdint.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include "include/common.h"
#include "include/search_type.h"

float* bvecs_read(const char* fname, size_t* d_out, size_t* n_out) {
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
    size_t sz = st.st_size;
    assert(sz % (1* 4 + d) == 0 || !"weird file size");
    size_t n = sz / (1* 4 + d);

    *d_out = d;
    *n_out = n;
    uint8_t* x = new uint8_t[n * (d + 1*4)];
    size_t nr = fread(x, 1, n * (d + 1 * 4), f);
    assert(nr == n * (d + 1 * 4) || !"could not read whole file");

    // shift array to remove row headers
    for (size_t i = 0; i < n; i++)
        memmove(x + i * d, x + 4 + i * (d + 4), d * sizeof(*x));

    float* xt = new float[n * (d)];
    for (size_t i = 0; i < n*d; i++)
        xt[i] = x[i];

    fclose(f);
    return xt;
}

float* fvecs_read(const char* fname, size_t* d_out, size_t* n_out) {
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
    size_t sz = st.st_size;
    assert(sz % ((d + 1) * 4) == 0 || !"weird file size");
    size_t n = sz / ((d + 1) * 4);

    *d_out = d;
    *n_out = n;
    float* x = new float[n * (d + 1)];
    size_t nr = fread(x, sizeof(float), n * (d + 1), f);
    assert(nr == n * (d + 1) || !"could not read whole file");

    // shift array to remove row headers
    for (size_t i = 0; i < n; i++)
        memmove(x + i * d, x + 1 + i * (d + 1), d * sizeof(*x));

    fclose(f);
    return x;
}

bool compareba(const std::pair<int32_t, int64_t>& a, const std::pair<int32_t, int64_t>& b) {
    return a.first < b.first;
}

// not very clean, but works as long as sizeof(int) == sizeof(float)
int* ivecs_read(const char* fname, size_t* d_out, size_t* n_out) {
    return (int*)fvecs_read(fname, d_out, n_out);
}

double elapsed() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

int main() {
    double t0 = elapsed();

    // this is typically the fastest one.
    // const char* index_key = "IVF4096,Flat";

    // these ones have better memory usage
    // const char *index_key = "Flat";
    // const char *index_key = "PQ32";
    // const char *index_key = "PCA80,Flat";
    // const char *index_key = "IVF4096,PQ8+16";
    const char *index_key = "IVF4096,PQ16";
    // const char *index_key = "IMI2x8,PQ32";
    // const char *index_key = "IMI2x8,PQ8+16";
    // const char *index_key = "OPQ16_64,IMI2x8,PQ8+16";

    faiss::IndexIVFPQ* index;

    // index = dynamic_cast<faiss::IndexIVFPQ*>(faiss::read_index("kmeans32IP.index"));
    index = dynamic_cast<faiss::IndexIVFPQ*>(faiss::read_index("kmeans16.index"));
    // index = dynamic_cast<faiss::IndexIVFPQ*>(faiss::read_index("sift1B_4096PQ16.index"));
    int32_t d = 128;

    faiss::ArrayInvertedLists *invlists = static_cast<faiss::ArrayInvertedLists*>(index->invlists);
    std::vector<std::vector<uint8_t>> codes = invlists->codes; // size nlist * n
    std::vector<std::vector<faiss::idx_t>> ids = invlists->ids;// size nlist * n
    int32_t nlist = index->nlist;
    int32_t code_size = index->invlists->code_size;

    //codebook : size M * ksub * dsub
    // Layout : (M, ksub, dsub)
    int32_t ksub = index->pq.ksub;
    int32_t dsub = index->pq.dsub;
    int32_t M = index->pq.M; 
    std::vector<int8_t> codebook (M*ksub*dsub);
    float max_codebook = -100000;
    float min_codebook = 100000;
    for(int32_t i = 0;i < M*ksub*dsub;i++)
    {
        int8_t temp = index->pq.centroids[i];
        max_codebook = MAX(temp, max_codebook);
        min_codebook = MIN(temp, min_codebook);
        codebook[i] = temp;//应该用量化的方法，后面改
    }

    printf("max codebook: %f, min codebook: %f\n", max_codebook, min_codebook);
    size_t nq;
    float* xq;

    {
        printf("[%.3f s] Loading queries\n", elapsed() - t0);

        size_t d2;
        xq = fvecs_read("./sift1M/sift_query.fvecs", &d2, &nq);
        // xq = bvecs_read("./sift1B/bigann_query.bvecs", &d2, &nq);
        assert(d == d2 || !"query does not have same dimension as train set");
    }

    size_t ks;         // nb of results per query in the GT
    faiss::idx_t* gt; // nq * k matrix of ground-truth nearest-neighbors

    printf("[%.3f s] Loading ground truth for %ld queries\n",
            elapsed() - t0,
            nq);

    // load ground-truth and convert int to long
    size_t nq2;
    int* gt_int = ivecs_read("./sift1M/sift_groundtruth.ivecs", &ks, &nq2);
    // int* gt_int = ivecs_read("./sift1B/idx_1000M.ivecs", &ks, &nq2);
    assert(nq2 == nq || !"incorrect nb of ground truth entries");

    gt = new faiss::idx_t[ks * nq];
    for (int i = 0; i < ks * nq; i++) {
        gt[i] = gt_int[i];
    }
    delete[] gt_int;
    size_t nq_test = nq / 10 ;
    size_t nq_freq = nq - nq_test;// 前9/10的数据用来统计frequency
    float* xq_freq = new float[d*nq_freq];
    // nq_test = 30;
    int32_t begin_idx = 0;
    for(int i=0;i<d*nq_freq;i++)
    {
        xq_freq[i] = xq[i];
    }
    faiss::ParameterSpace params;
    params.set_index_parameters(index, "nprobe=32");
    int32_t nprobe = 32;
    float* xq_tmp = new float[d*nq_test];
    for(int i=(nq_freq+begin_idx)*d;i<(nq_freq+begin_idx + nq_test)*d;i++)
    {
        xq_tmp[i-(nq_freq+begin_idx)*d] = xq[i];
    }
    printf("[%.6f s] Perform a search on %ld queries\n",
            elapsed() - t0,
            nq_test);

    // output buffers
    faiss::idx_t* I = new faiss::idx_t[nq_test * ks];

    //-----------------begin search-----------------

    faiss::idx_t* idx_q = new faiss::idx_t[nq_test * nprobe];
    float* coarse_dis_q = new float[nq_test * nprobe];
    int32_t LUT[256*16];
    float LUT_temp[256*16];
    index->quantizer->search(nq_test,xq_tmp,nprobe,coarse_dis_q,idx_q,nullptr);

    // output buffers
    faiss::idx_t* Is = new faiss::idx_t[nq_test * ks];
    float* Ds = new float[nq_test * ks];

    index->search(nq_test, xq_tmp, ks, Ds, Is);


    // q_c layer: [nq_test, nprobe, d]
    int32_t* q_c = 0;
    float* centroid_q = new float[d];
    std::vector<std::vector<std::pair<int32_t, int64_t>>> I_temp(nq_test); //真实结果 I
    for(int i=0;i<nq_test;i++)
    {
        int q_idx = 0;
        for(int j = 0 ; j< nprobe;j++)
        {
            index->quantizer->reconstruct(idx_q[i*nprobe+j],centroid_q);
            q_c = new int32_t[d];
            for(int k=0;k<d;k++)
            {
                q_c[k] = (int32_t)(xq_tmp[i*d+k] - centroid_q[k]);
            }
            for(int k=0; k<16; k++)//计算LUT
            {
                for(int m=0; m<256; m++)
                {
                    q_idx = k * 8;
                    LUT[k*256 + m] = 0;
                    for(int n=0; n<8; n++)
                    {
                        LUT[k*256 + m] += (int32_t)(q_c[q_idx] - codebook[k*256*8 + m *8 +n])*(q_c[q_idx] - codebook[k*256*8 + m *8 +n]);
                        q_idx = q_idx + 1;
                    }
                    // printf("LUT[%d]: %d ",k*256+m, LUT[k*256 + m]);
                }
            }
            // printf("q_c: %d \n", q_c);
            // index->pq.compute_inner_prod_table(&xq_tmp[i*d], LUT_temp);
            for(int k=0; k<codes[idx_q[i*nprobe+j]].size(); k+=16)
            {
                int32_t res = 0;
                for(int m=0; m<16; m++)
                {
                    res += LUT[codes[idx_q[i*nprobe+j]][k+m] + m*256];
                }
                I_temp[i].push_back(std::make_pair(res, ids[idx_q[i*nprobe+j]][k/16]));
            }
        }
        std::sort(I_temp[i].begin(), I_temp[i].end(), compareba);
        for(int j=0; j<ks; j++)
        {
            I[i*ks + j] = I_temp[i][j].second;
            // printf("I[%d]: %ld dis[%d]: %d ",i*ks+j,I[i*ks+j],i*ks+j,I_temp[i][j].first);
        }
        // printf("\n");
    }
    // evaluate result by hand.
    int n_1 = 0, n_10 = 0, n_100 = 0;
    for (int i = nq_freq + begin_idx; i < nq_freq +begin_idx +nq_test; i++) {
        int gt_nn = gt[i * ks];
        for (int j = 0; j < ks; j++) {
            if (I[(i-nq_freq-begin_idx) * ks + j] == gt_nn) {
                if (j < 1)
                    n_1++;
                if (j < 10)
                    n_10++;
                if (j < 100)
                    n_100++;
            }
        }
    }

    printf("Hand make result:\n");
    printf("R@1 = %.4f\n", n_1 / float(nq_test));
    printf("R@10 = %.4f\n", n_10 / float(nq_test));
    printf("R@100 = %.4f\n", n_100 / float(nq_test));

    printf("implement with Faiss API: \n");

    // evaluate result by hand.
    n_1 = 0; n_10 = 0; n_100 = 0;
    for (int i = nq_freq+begin_idx; i < nq_freq+begin_idx +nq_test; i++) {
        int gt_nn = gt[i * ks];
        for (int j = 0; j < ks; j++) {
            if (Is[(i-nq_freq-begin_idx) * ks + j] == gt_nn) {
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

    delete[] I;

    delete[] xq;
    delete[] gt;
    delete index;
    return 0;
}