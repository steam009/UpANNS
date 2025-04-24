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
 #include <iostream>
 #include <fstream>
 #include <cstdint>
 
 #include <sys/stat.h>
 #include <sys/types.h>
 #include <unistd.h>
 
 #include <sys/time.h>
 
 #include <faiss/AutoTune.h>
 #include <faiss/index_factory.h>
 #include <faiss/IndexIVFPQ.h>
 #include <faiss/index_io.h>
 #include <faiss/invlists/OnDiskInvertedLists.h>
 
 /**
  * To run this demo, please download the ANN_SIFT1M dataset from
  *
  *   http://corpus-texmex.irisa.fr/
  *
  * and unzip it to the sudirectory sift1M.
  **/
 
 /*****************************************************
  * I/O functions for fvecs and ivecs
  *****************************************************/
 float* read_fbin(const std::string& filename, size_t& nvecs, size_t& dim, int start_idx = 0, int chunk_size = -1) {
    std::ifstream file(filename, std::ios::binary);
    assert(file.is_open());

    int nvecs_h;
    int dim_h;
    file.read(reinterpret_cast<char*>(&nvecs_h), sizeof(int));
    file.read(reinterpret_cast<char*>(&dim_h), sizeof(int));
    nvecs = nvecs_h;
    dim = dim_h;

    nvecs = (nvecs - start_idx);
    if (chunk_size != -1) {
        nvecs = chunk_size;
    }

    // Allocate memory for the data
    float* data = new float[nvecs * dim];

    file.seekg(8 + start_idx * sizeof(float) * dim, std::ios::beg);
    file.read(reinterpret_cast<char*>(data), nvecs * dim * sizeof(float));

    return data;
}

int* read_ibin(const std::string& filename, size_t& nvecs, size_t& dim, int start_idx = 0, int chunk_size = -1) {
    std::ifstream file(filename, std::ios::binary);
    assert(file.is_open());

    int nvecs_h;
    int dim_h;
    file.read(reinterpret_cast<char*>(&nvecs_h), sizeof(int));
    file.read(reinterpret_cast<char*>(&dim_h), sizeof(int));
    nvecs = nvecs_h;
    dim = dim_h;

    nvecs = (nvecs - start_idx);
    if (chunk_size != -1) {
        nvecs = chunk_size;
    }

    // Allocate memory for the data
    int* data = new int[nvecs * dim];

    file.seekg(8 + start_idx * sizeof(int) * dim, std::ios::beg);
    file.read(reinterpret_cast<char*>(data), nvecs * dim * sizeof(int));

    return data;
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
     const char *index_key = "IVF4096,PQ12";
     // const char *index_key = "IMI2x8,PQ32";
     // const char *index_key = "IMI2x8,PQ8+16";
     // const char *index_key = "OPQ16_64,IMI2x8,PQ8+16";
 
     faiss::IndexIVFPQ* index;
     std::string index_file = "deep1B_"+ std::to_string(NLIST)+ "PQ12.index";
     printf("index file: %s \n", index_file.c_str());
     index = dynamic_cast<faiss::IndexIVFPQ*>(faiss::read_index(index_file.c_str()));
 
     size_t d = 96;
 
    //  {
    //      printf("[%.3f s] Loading train set\n", elapsed() - t0);
 
    //      size_t nt;
    //      float* xt = read_fbin("../deep/learn.350M.fbin", nt, d);
 
    //      printf("[%.3f s] Preparing index \"%s\" d=%ld\n",
    //             elapsed() - t0,
    //             index_key,
    //             d);
    //      index = static_cast<faiss::IndexIVFPQ *>(faiss::index_factory(d, index_key, faiss::METRIC_L2));
 
    //      printf("[%.3f s] Training on %ld vectors\n", elapsed() - t0, nt);
 
    //      index->train(nt, xt);
    //      delete[] xt;
    //  }
 
    //  {
    //      printf("[%.3f s] Loading database\n", elapsed() - t0);
 
    //      size_t nb, d2;
    //      float* xb = read_fbin("../deep/base.1B.fbin", nb, d2);
    //      assert(d == d2 || !"dataset does not have same dimension as train set");
 
    //      printf("[%.3f s] Indexing database, size %ld*%ld\n",
    //             elapsed() - t0,
    //             nb,
    //             d);
    //      nb = 500000000;
 
    //      index->add(nb, xb);
    //      faiss::write_index(index, "deep500M_4096PQ12.index");
 
    //      float* prec = index->precomputed_table.data();/// size nlist * pq.M * pq.ksub
    //      size_t num = index->precomputed_table.size();
    //      printf("show pre table: %d \n", num);
 
 
    //      delete[] xb;
    //  }
 
     size_t nq;
     float* xq;
 
     {
         printf("[%.3f s] Loading queries\n", elapsed() - t0);
 
         size_t d2;
         xq = read_fbin("./deep/query.public.10K.fbin", nq, d2);
         assert(d == d2 || !"query does not have same dimension as train set");
     }
 
     size_t k;         // nb of results per query in the GT
     faiss::idx_t* gt; // nq * k matrix of ground-truth nearest-neighbors
 
     {
         printf("[%.3f s] Loading ground truth for %ld queries\n",
                elapsed() - t0,
                nq);
 
         // load ground-truth and convert int to long
         size_t nq2;
         int* gt_int = read_ibin("./deep/groundtruth.public.10K.ibin", nq2, k);
         assert(nq2 == nq || !"incorrect nb of ground truth entries");
 
         gt = new faiss::idx_t[k * nq];
         for (int i = 0; i < k * nq; i++) {
             gt[i] = gt_int[i];
         }
         delete[] gt_int;
     }
 
     // Result of the auto-tuning
     std::string selected_params;
 
     // { // run auto-tuning
 
     //     printf("[%.3f s] Preparing auto-tune criterion 1-recall at 1 "
     //            "criterion, with k=%ld nq=%ld\n",
     //            elapsed() - t0,
     //            k,
     //            nq);
 
     //     faiss::OneRecallAtRCriterion crit(nq, 1);
     //     crit.set_groundtruth(k, nullptr, gt);
     //     crit.nnn = k; // by default, the criterion will request only 1 NN
 
     //     printf("[%.3f s] Preparing auto-tune parameters\n", elapsed() - t0);
 
     //     faiss::ParameterSpace params;
     //     params.initialize(index);
 
     //     printf("[%.3f s] Auto-tuning over %ld parameters (%ld combinations)\n",
     //            elapsed() - t0,
     //            params.parameter_ranges.size(),
     //            params.n_combinations());
 
     //     faiss::OperatingPoints ops;
     //     params.explore(index, nq, xq, crit, &ops);
 
     //     printf("[%.3f s] Found the following operating points: \n",
     //            elapsed() - t0);
 
     //     ops.display();
 
     //     // keep the first parameter that obtains > 0.5 1-recall@1
     //     for (int i = 0; i < ops.optimal_pts.size(); i++) {
     //         if (ops.optimal_pts[i].perf > 0.45) {
     //             selected_params = ops.optimal_pts[i].key;
     //             break;
     //         }
     //     }
     //     assert(selected_params.size() >= 0 ||
     //            !"could not find good enough op point");
     // }
 
     { // Use the found configuration to perform a search
 
         faiss::ParameterSpace params;
 
         printf("[%.3f s] Setting parameter configuration \"%s\" on index\n",
                elapsed() - t0,
                selected_params.c_str());
 
         // params.set_index_parameters(index, selected_params.c_str());
         std::string set_nprobs = "nprobe=" + std::to_string(NPROBE);
         printf("nprobe: %s \n",set_nprobs.c_str());
         params.set_index_parameters(index, set_nprobs.c_str());
 
         nq = BS;
         float* xq_tmp = new float[96*nq];
         for(int i=0;i<96*(nq);i++)
         {
             xq_tmp[i] = xq[i+96*9000];
         }
         printf("[%.6f s] Perform a search on %ld queries\n",
                elapsed() - t0,
                nq);
        // output buffers
        faiss::idx_t* I = new faiss::idx_t[nq * k];
        float* D = new float[nq * k];
        float avg = 0;
        for (int i=0;i < 10;i++){
            double begin = elapsed();
    
            k = TOPK;
    
            index->search(nq, xq_tmp, k, D, I);
            double end = elapsed();
            avg += (end - begin);
            printf("[%.6f s] itera: %d, time: %.6f\n", elapsed() - t0, i, (end - begin));
        }
        printf("[%.6f s] Compute recalls, time: %.6f\n", elapsed() - t0, avg/10);
 
         // evaluate result by hand.
         int n_1 = 0, n_10 = 0, n_100 = 0;
         for (int i = 0; i < nq; i++) {
             int gt_nn = gt[(i+9000) * 100];
             for (int j = 0; j < k; j++) {
                 if (I[i * k + j] == gt_nn) {
                     if (j < 1)
                         n_1++;
                     if (j < 10)
                         n_10++;
                     if (j < 100)
                         n_100++;
                 }
             }
         }
         printf("R@1 = %.4f\n", n_1 / float(nq));
         printf("R@10 = %.4f\n", n_10 / float(nq));
         printf("R@100 = %.4f\n", n_100 / float(nq));
         
 
         delete[] I;
         delete[] D;
     }
 
     delete[] xq;
     delete[] gt;
     delete index;
     return 0;
 }
 