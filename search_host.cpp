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
#include <dpu>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <sstream>
#include <utility>
#include <functional>
#include <omp.h>

#ifndef DPU_BINARY
#    define DPU_BINARY "./dpu/search_dpu" // Relative path regarding the PyTorch code
#endif
#define THREADSHOLD 1
#define CACHE_LEN 4
#define CACHE_PS_LEN 256

/**
 * @struct dpu_runtime
 * @brief DPU execution times
 */
typedef struct dpu_runtime_totals {
    double execution_time_prepare;
    double execution_time_populate_copy_in;
    double execution_time_copy_in;
    double execution_time_copy_out;
    double execution_time_aggregate_result;
    double execution_time_launch;
} dpu_runtime_totals;

/**
 * @struct dpu_timespec
 * @brief ....
 */
typedef struct dpu_timespec {
    long tv_nsec;
    long tv_sec;
} dpu_timespec;

/**
 * @struct dpu_runtime_interval
 * @brief DPU execution interval
 */
typedef struct dpu_runtime_interval {
    dpu_timespec start;
    dpu_timespec stop;
} dpu_runtime_interval;

/**
 * @struct dpu_runtime_config
 * @brief ...
 */
typedef enum dpu_runtime_config {
    RT_ALL = 0,
    RT_LAUNCH = 1
} dpu_runtime_config;

/**
 * @struct dpu_runtime_group
 * @brief ...
 */
typedef struct dpu_runtime_group {
    unsigned int in_use;
    unsigned int length;
    dpu_runtime_interval *intervals;
} dpu_runtime_group;

// Custom hash function for std::pair<uint8_t, uint8_t>
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto hash1 = std::hash<T1>{}(p.first);
        auto hash2 = std::hash<T2>{}(p.second);
        return hash1 ^ (hash2 << 1); // Combine the two hashes
    }
};

float* bvecs_read(const char* fname, int32_t* d_out, int32_t* n_out) {
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

double elapsed() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

// 比较函数，用于降序排序
bool compareab(const std::pair<int, int>& a, const std::pair<int, int>& b) {
    return a.first > b.first;
}
bool comparefloatab(const std::pair<float, int>& a, const std::pair<float, int>& b) {
    return a.first > b.first;
}
bool comparefloatba(const std::pair<float, int>& a, const std::pair<float, int>& b) {
    return a.first < b.first;
}
bool compareba(const std::pair<int32_t, int64_t>& a, const std::pair<int32_t, int64_t>& b) {
    return a.first < b.first;
}
bool comparebaINT(const std::pair<int, int>& a, const std::pair<int, int>& b) {
    return a.first < b.first;
}
bool compare(const std::vector<int>& a, const std::vector<int>& b) {
    return a > b;
}

int get_dpu_index(std::vector<int32_t> dpu_offset, int32_t nr_of_dpus, int32_t nr_centroids)
{
    int dpu_index = 0;
    while(dpu_offset[dpu_index]>=nr_centroids && dpu_index < nr_of_dpus)
        dpu_index++;
    return dpu_index;
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
    const char *index_key = "IVF4096,PQ32";
    // const char *index_key = "IMI2x8,PQ32";
    // const char *index_key = "IMI2x8,PQ8+16";
    // const char *index_key = "OPQ16_64,IMI2x8,PQ8+16";

    faiss::IndexIVFPQ* index;

    printf("begin to load model: MAX STORE: %d \n", MAX_DPU_STORE_SIZE);
    // index = dynamic_cast<faiss::IndexIVFPQ*>(faiss::read_index("kmeans16.index"));
    index = dynamic_cast<faiss::IndexIVFPQ*>(faiss::read_index("sift1B_4096PQ16.index"));

    uint32_t nr_of_dpus;
    // auto system = dpu::DpuSet::allocate(NR_DPUS);
    // system.load("./dpu/search_dpu");
    nr_of_dpus = NR_DPUS;

    int32_t d = 128;

    // printf("[%.3f s] Loading train set\n", elapsed() - t0);

    // int32_t nt;
    // float* xt = fvecs_read("./sift1M/sift_learn.fvecs", &d, &nt);

    // printf("[%.3f s] Preparing index \"%s\" d=%ld\n",
    //         elapsed() - t0,
    //         index_key,
    //         d);
    // index = static_cast<faiss::IndexIVFPQ *>(faiss::index_factory(d, index_key));

    // printf("[%.3f s] Training on %ld vectors\n", elapsed() - t0, nt);

    // index->train(nt, xt);
    // delete[] xt;

    // printf("[%.3f s] Loading database\n", elapsed() - t0);

    // int32_t nb, d2;
    // float* xb = fvecs_read("./sift1M/sift_base.fvecs", &d2, &nb);
    // assert(d == d2 || !"dataset does not have same dimension as train set");

    // printf("[%.3f s] Indexing database, size %ld*%ld\n",
    //         elapsed() - t0,
    //         nb,
    //         d);

    // index->add(nb, xb);

    // faiss::write_index(index, "kmeans.index");


    // delete[] xb;

    faiss::ArrayInvertedLists *invlists = static_cast<faiss::ArrayInvertedLists*>(index->invlists);
    // Create vectors and copy data from invlists
    std::vector<std::vector<uint8_t>> codes;
    codes.resize(invlists->codes.size());
    for (size_t i = 0; i < invlists->codes.size(); i++) {
        const auto& code_vec = invlists->codes[i];
        codes[i].resize(code_vec.size());
        std::copy(code_vec.begin(), code_vec.end(), codes[i].begin());
    }
    
    std::vector<std::vector<faiss::idx_t>> ids;
    ids.resize(invlists->ids.size());
    for (size_t i = 0; i < invlists->ids.size(); i++) {
        const auto& id_vec = invlists->ids[i];
        ids[i].resize(id_vec.size());
        std::copy(id_vec.begin(), id_vec.end(), ids[i].begin());
    }
    
    int32_t nlist = index->nlist;
    int32_t code_size = index->invlists->code_size;

    std::vector<std::vector<uint8_t>> source;
    source.resize(invlists->codes.size());
    for (size_t i = 0; i < invlists->codes.size(); i++) {
        const auto& code_vec = invlists->codes[i];
        source[i].resize(code_vec.size());
        std::copy(code_vec.begin(), code_vec.end(), source[i].begin());
    }

    // faiss::ArrayInvertedLists *invlists = static_cast<faiss::ArrayInvertedLists*>(index->invlists);
    // std::vector<std::vector<uint8_t>> codes = invlists->codes; // size nlist * n
    // std::vector<std::vector<faiss::idx_t>> ids = invlists->ids;// size nlist * n
    // int32_t nlist = index->nlist;
    // int32_t code_size = index->invlists->code_size;

    // std::vector<std::vector<uint8_t>> source = invlists->codes; 
    std::vector<std::vector<uint16_t>> new_codes(source.size());

    for (size_t i = 0; i < source.size(); ++i) {
        new_codes[i].resize(source[i].size());
        for (size_t j = 0; j < ids[i].size(); ++j) {
            for(size_t k=0; k< code_size; k++)
            {
                new_codes[i][j*code_size + k] = static_cast<uint16_t>(source[i][j*code_size + k]) + k * KSUB; // 转换每个元素
            }
        }
    }
    std::vector<std::vector<uint8_t>>().swap(source);


    //codebook : size M * ksub * dsub
    // Layout : (M, ksub, dsub)
    int32_t ksub = index->pq.ksub;
    int32_t dsub = index->pq.dsub;
    int32_t M = index->pq.M; 

    // std::vector<std::vector<std::vector<Code_pair>>> hbm_cached_ps(nlist);
    // std::vector<std::unordered_map<uint16_t, int>> original_item_to_cacheline(nlist);
    // std::vector<std::vector<int>> cluster_starting_addr(nlist, std::vector<int>(1, 0));
    // std::vector<std::vector<std::vector<bool> >> is_cached(nlist, std::vector<std::vector<bool> >(code_size, std::vector<bool>(KSUB, 0)));
    // for(int cur_c_id=0; cur_c_id< nlist; cur_c_id++){
    //     std::string cache_name =  "./sift1B16-cache/centroid16_"+ std::to_string(cur_c_id) + "__decay_0_sampling_50_alpha"+ "_50_length_"+std::to_string(CACHE_LEN)+"_hbm_1.0.hbm.cluster";
    //     std::ifstream hbm_graphfile(cache_name);
    //     std::string line;
    //     int nr_of_line=CACHE_PS_LEN;
    //     int i=0;
    //     for(i=0; i< nr_of_line; i++)
    //     {
    //         std::getline(hbm_graphfile, line);
    //         if (hbm_graphfile.eof())
    //         {
    //             if(i<nr_of_line)
    //                 printf("need to pad c_id: %d, cur line num: %d\n", cur_c_id, i);
    //             break;
    //         }
    //         std::istringstream iss_pair(line);
    //         uint16_t cache_num;
    //         std::vector<uint16_t> cache_values;
    //         std::vector<Code_pair> tmp;
    //         std::unordered_map<uint8_t,uint8_t> col_value;
    //         while (iss_pair >> cache_num) {
    //             uint8_t first = static_cast<uint8_t>((cache_num >> 8) & 0xFF);
    //             if(col_value.find(first) != col_value.end())
    //             {
    //                 break;
    //             }
    //             cache_values.push_back(cache_num);
    //             col_value[first] = 1;
    //         }
    //         if(cache_values.size()!=CACHE_LEN)
    //         {
    //             i--;
    //             continue;
    //         }
    //         for(uint16_t cache_num : cache_values)
    //         {
    //             uint8_t first = static_cast<uint8_t>((cache_num >> 8) & 0xFF);
    //             uint8_t second = static_cast<uint8_t>(cache_num & 0xFF);
    //             if(first > 32 || second > 256)
    //             {
    //                 printf("some error ,first too large. c_id : %d , line: %d \n", cur_c_id, i);
    //             }
    //             Code_pair cur_tmp;
    //             cur_tmp.fir = first;
    //             cur_tmp.sed = second;
    //             tmp.push_back(cur_tmp);
    //             original_item_to_cacheline[cur_c_id][cache_num] = i;
    //             is_cached[cur_c_id][first][second] = 1;
    //         }
    //         hbm_cached_ps[cur_c_id].push_back(tmp);
    //         if(tmp.size()!=CACHE_LEN)
    //         {
    //             printf("some error, tmp.size() != 4, in file %d\n", cur_c_id);
    //         }
    //         cluster_starting_addr[cur_c_id].push_back(cluster_starting_addr[cur_c_id].back() + (1 << tmp.size()));
    //     }
    //     std::vector<Code_pair> tmp0;
    //     Code_pair cur_tmp0;
    //     cur_tmp0.fir = 0;
    //     cur_tmp0.sed = 0;
    //     for(int m = 0; m < CACHE_LEN; m++)
    //     {
    //         tmp0.push_back(cur_tmp0);
    //     }
    //     while(i<nr_of_line)
    //     {
    //         hbm_cached_ps[cur_c_id].push_back(tmp0);
    //         cluster_starting_addr[cur_c_id].push_back(cluster_starting_addr[cur_c_id].back() + (1 << tmp0.size()));
    //         i++;
    //     }
    //     hbm_graphfile.close();
    //     // if(cur_c_id==2059)
    //     // {
    //     //     for(int m=0;m<256;m++)
    //     //     {
    //     //         for(int n=0;n<4;n++){
    //     //             printf("hbm_cached_ps[%d].fir: %d hbm_cached_ps[%d].sed: %d ",m*4+n,hbm_cached_ps[cur_c_id][m][n].fir,m*4+n, hbm_cached_ps[cur_c_id][m][n].sed);
    //     //         }
    //     //     }
    //     // }
    // }

    // std::vector<std::vector<uint16_t>> new_codes(nlist);
    // // std::unordered_map<int, std::vector<std::pair<uint8_t,uint8_t>>> hbm_access_set;
    // // std::vector<uint8_t> tmp_codes;
    int MAX_NUM = omp_get_max_threads();
    // uint32_t cache_hit = 0;
    // uint32_t cache_hit_total = 0;
    // #pragma omp parallel for num_threads(MAX_NUM) reduction(+:cache_hit) reduction(+:cache_hit_total)
    // for(int i=0; i< nlist; i++)
    // {
    //     if(i==2059)
    //     {
    //         printf("hhh");
    //     }
    //     for(int j = 0; j < ids[i].size(); j++)
    //     {
    //         uint8_t new_len = 0;
    //         uint8_t no_cache_len = 0;
    //         std::vector<uint8_t> tmp_codes;
    //         std::vector<uint16_t> new_tmp_codes;
    //         uint16_t base_adr = MS*KSUB;
    //         tmp_codes.push_back(0);
    //         std::unordered_map<int, std::vector<std::pair<uint8_t,uint8_t>>> hbm_access_set;
    //         for(int k = 0; k< code_size; k++)
    //         {
    //             if(is_cached[i][k][codes[i][j*code_size + k]])
    //             {
    //                 Code_pair cur_tmp;
    //                 cur_tmp.fir = static_cast<uint8_t>(k);
    //                 cur_tmp.sed = static_cast<uint8_t>(codes[i][j*code_size + k]);
    //                 uint8_t high_byte = k;
    //                 uint8_t low_byte = codes[i][j*code_size + k];
    //                 // 组合两个 uint8_t 成 uint16_t
    //                 uint16_t combined_value = ((uint16_t)high_byte << 8) | (uint16_t)low_byte;
    //                 int line_idx = original_item_to_cacheline[i][combined_value];
    //                 for(int m=0; m < hbm_cached_ps[i][line_idx].size();m++)
    //                 {
    //                     if(hbm_cached_ps[i][line_idx][m].fir == cur_tmp.fir && hbm_cached_ps[i][line_idx][m].sed == cur_tmp.sed)
    //                     {
    //                         std::pair<uint8_t,uint8_t> std_tmp(static_cast<uint8_t>(k),static_cast<uint8_t>(m));
    //                         hbm_access_set[line_idx].push_back(std_tmp);
    //                         break;
    //                     }
    //                 }
    //             }
    //             else
    //             {
    //                 new_len++;
    //                 no_cache_len++;
    //                 tmp_codes.push_back(k);
    //                 tmp_codes.push_back(codes[i][j*code_size + k]);
    //             }
    //         }
    //         bool morethan1 = false;
    //         for (const auto& kv : hbm_access_set) {
    //             int key = kv.first;
    //             const std::vector<std::pair<uint8_t,uint8_t>>& value = kv.second;
    //             if(value.size()==1)
    //             {
    //                 int index_t = 0;
    //                 int tmp_idx = 0;
    //                 for(int m=0; m<value[0].first;m++)
    //                 {
    //                     if(codes[i][j*code_size + m]!=tmp_codes[tmp_idx+2])
    //                     {
    //                         index_t+=2;
    //                     }
    //                     else
    //                     {
    //                         tmp_idx+=2;
    //                     }
    //                 }
    //                 if(tmp_codes.size() > value[0].first * 2 - index_t + 1){
    //                     tmp_codes.insert(tmp_codes.begin() + value[0].first*2 - index_t + 1, value[0].first);//需要验证！！！
    //                     tmp_codes.insert(tmp_codes.begin() + value[0].first*2 - index_t + 2, codes[i][j*code_size + value[0].first]);
    //                 }
    //                 else
    //                 {
    //                     tmp_codes.push_back(value[0].first);
    //                     tmp_codes.push_back(codes[i][j*code_size + value[0].first]);
    //                 }
    //                 new_len++;
    //                 no_cache_len++;
    //                 continue;
    //             }
    //             morethan1 = true;
    //             uint16_t this_starting_addr = cluster_starting_addr[i][key];
    //             for (std::pair<uint8_t,uint8_t> n : value) {
    //                 this_starting_addr += static_cast<uint16_t>(1 << n.second); // 使用 std::pow 计算幂
    //             }
    //             // 将 this_starting_addr 拆分为两个 uint8_t 并存入 tmp_codes
    //             if(this_starting_addr >= 4096)
    //             {
    //                 printf("some error!!!\n");
    //             }
    //             uint8_t high_byte = static_cast<uint8_t>((this_starting_addr >> 8) & 0xFF); // 高 8 位
    //             uint8_t low_byte = static_cast<uint8_t>(this_starting_addr & 0xFF);          // 低 8 位
    //             tmp_codes.push_back(high_byte);
    //             tmp_codes.push_back(low_byte);
    //             new_len++;
    //         }
    //         if(morethan1)
    //         {
    //             cache_hit_total++;
    //         }
    //         tmp_codes[0] = new_len * 2;
    //         // tmp_codes[0] |= (no_cache_len-1);
    //         new_tmp_codes.push_back((uint16_t)(new_len - 1));
    //         if(new_len > 16)
    //         {
    //             printf("some error, new len larger than MS \n");
    //         }
    //         for(int k = 0; k < no_cache_len; k++)
    //         {
    //             uint16_t adr = tmp_codes[k*2+1] * KSUB + tmp_codes[k*2+2];
    //             new_tmp_codes.push_back(adr);
    //         }
    //         for(int k= no_cache_len; k < new_len; k++)
    //         {
    //             uint8_t high_byte = tmp_codes[k*2+1];
    //             uint8_t low_byte = tmp_codes[k*2+2];
    //             // 组合两个 uint8_t 成 uint16_t
    //             uint16_t combined_value = ((uint16_t)high_byte << 8) | (uint16_t)low_byte;
    //             new_tmp_codes.push_back(combined_value + base_adr);
    //         }
    //         for(int k = 0; k < CODE_SIZE; k++)
    //         {
    //             if(k < new_len + 1)
    //                 new_codes[i].push_back(new_tmp_codes[k]);
    //             else
    //                 new_codes[i].push_back(base_adr);//其实可以压缩，但是没想好！！！
    //         }
    //         cache_hit++;
    //         // if(new_len <= code_size){
    //         //     tmp_codes[0] = new_len;
    //         //     tmp_codes[0] |= 0x80;
    //         //     for(int k = 0; k < CODE_SIZE; k++)
    //         //     {
    //         //         if(k < new_len + 1)
    //         //             new_codes[i].push_back(tmp_codes[k]);
    //         //         else
    //         //             new_codes[i].push_back(0);//其实可以压缩，但是没想好！！！
    //         //     }
    //         //     cache_hit++;
    //         // }
    //         // else // 没有cache ， 存回原来的code
    //         // {
    //         //     for(int k = 0; k < code_size; k++)
    //         //     {
    //         //         if(k==0)
    //         //         {
    //         //             new_codes[i].push_back((codes[i][j*code_size + k] & 0x7f));
    //         //         }
    //         //         else
    //         //         {
    //         //             new_codes[i].push_back(codes[i][j*code_size + k]);
    //         //         }
    //         //     }
    //         //     for(int k=code_size; k<CODE_SIZE;k++)
    //         //     {
    //         //         new_codes[i].push_back(0);
    //         //     }
    //         // }
    //     }
    // }
    // std::vector<std::vector<uint8_t>>().swap(codes);
    // std::vector<std::unordered_map<uint16_t, int>>().swap(original_item_to_cacheline);
    // std::vector<std::vector<int>>().swap(cluster_starting_addr);
    // std::vector<std::vector<std::vector<bool> >>().swap(is_cached);

    std::vector<int8_t> codebook (M*ksub*dsub);
    float max_codebook = -100000;
    float min_codebook = 100000;
     for(int32_t i = 0;i < M*ksub*dsub;i++)// 这里好像有问题
    {
        float temp = (index->pq.centroids[i]);
        max_codebook = MAX(temp, max_codebook);
        min_codebook = MIN(temp, min_codebook);
        codebook[i] = (int8_t)temp;//应该用量化的方法，后面改
    }

    printf("max codebook: %f, min codebook: %f\n", max_codebook, min_codebook);

    //传输encoded point 到相应的DPU
    int32_t nr_centroids = nlist / nr_of_dpus; // number of controids that one DPU need to store.
    std::vector<int32_t> Cp(nlist,0); // number of points in each centroid.
    int32_t avg_cp = 0;
    for(int32_t i = 0;i<nlist;i++)
    {
        Cp[i] = ids[i].size();
        avg_cp += Cp[i];
    }
    avg_cp = avg_cp / nlist;//average points in each centroid.

    int32_t nq;
    float* xq;

    {
        printf("[%.3f s] Loading queries\n", elapsed() - t0);

        int32_t d2;
        xq = bvecs_read("./sift1B/bigann_query.bvecs", &d2, &nq);
        // xq = fvecs_read("./sift1M/sift_query.fvecs", &d2, &nq);
        assert(d == d2 || !"query does not have same dimension as train set");
    }

    int32_t ks;         // nb of results per query in the GT
    faiss::idx_t* gt; // nq * k matrix of ground-truth nearest-neighbors

    printf("[%.3f s] Loading ground truth for %ld queries\n",
            elapsed() - t0,
            nq);

    // load ground-truth and convert int to long
    int32_t nq2;
    int* gt_int = ivecs_read("./sift1B/idx_1000M.ivecs", &ks, &nq2);
    // int* gt_int = ivecs_read("./sift1M/sift_groundtruth.ivecs", &ks, &nq2);
    assert(nq2 == nq || !"incorrect nb of ground truth entries");

    gt = new faiss::idx_t[ks * nq];
    for (int i = 0; i < ks * nq; i++) {
        gt[i] = gt_int[i];
    }
    delete[] gt_int;

    int32_t nq_test = nq / 10 ;
    nq_test = BS;
    int32_t nq_freq = nq_test;// 前9/10的数据用来统计frequency
    float* xq_freq = new float[d*nq_freq];
    // nq_test = 1;
    for(int i=9000*d;i<d*(9000+nq_freq);i++)
    {
        xq_freq[i-9000*d] = xq[i];
    }
    ks = TOPK;
    faiss::ParameterSpace params;
    std::string set_nprobs = "nprobe=" + std::to_string(NPROBS);
    printf("%s \n",set_nprobs.c_str());
    params.set_index_parameters(index, set_nprobs.c_str());
    int32_t nprobe = NPROBS;
    faiss::idx_t* idx = new faiss::idx_t[nq_freq * nprobe];
    float* coarse_dis = new float[nq_freq * nprobe];
    index->quantizer->search(nq_freq,xq_freq,nprobe,coarse_dis,idx,nullptr);
    delete[] xq_freq;
    //統計idx的頻率
    std::vector<int32_t> Fq(nlist,0); // access frequency for each centroid.
    for(int32_t i = 0; i < nq_freq*nprobe;i++)
    {
        Fq[idx[i]]++;
    }
    delete[] idx;
    delete[] coarse_dis;
    int32_t avg_fq = 0;
    for(int32_t i=0; i<nlist;i++)
    {
        avg_fq += Fq[i];
    }
    avg_fq = avg_fq / nlist; // Average access frequency for each centroid.

    // int32_t avg_process = avg_cp * avg_fq * nr_centroids;
    int32_t avg_process = 0;
    double avg_ps = 0;
    int32_t LUT_process = d * 256 * 8; // update_LUT的计算量，因为有乘法和没有乘法速度差了5x，所以*5
    int32_t avg_Centroids = (nq_freq * nprobe) / (nr_of_dpus); // 平均每个DPU需要计算的 centroid 的个数

    std::vector<int32_t> C_process(nlist,0);
    for(int i=0;i<nlist;i++)
    {
        C_process[i] = Fq[i] * Cp[i] ;
        avg_ps += C_process[i];
    }
    avg_process = std::ceil(static_cast<double>(avg_ps) / nr_of_dpus);
    printf("[%.6f s] avg_process: %d\n",
            elapsed() - t0,
            avg_process);

    //获取 C_process 降序排序的索引
    std::vector<std::pair<int, int>> C_process_idx;
    for (int32_t i = 0; i < nlist; ++i) {
        C_process_idx.push_back(std::make_pair(C_process[i], i));
    }
    std::sort(C_process_idx.begin(), C_process_idx.end(), compareab);

    //获取centroid 之间的距离
    std::vector<std::vector<float> > C_dis(nlist,std::vector<float>(nlist,0));
    float* centroid_i = new float[d];
    float* centroid_j = new float[d];
    for(int i=0 ; i<nlist; i++)
    {
        index->quantizer->reconstruct(i,centroid_i);
        for(int j=0; j<nlist; j++)
        {
            index->quantizer->reconstruct(j,centroid_j);
            float c_distance = 0;
            for(int k = 0 ; k<d ; k++)
            {
                float j_i = centroid_i[k] - centroid_j[k]; 
                c_distance += (j_i * j_i);
            }
            C_dis[i][j] = c_distance;
        }
    }
    // 创建一个新的二维向量，用于存储排序后的索引
    std::vector<std::vector<int>> C_dis_sorted_indices(nlist);

    // 对每个 C_dis[i] 进行升序排序，并记录排序后的索引
    for (int i = 0; i < nlist; ++i) {
        std::vector<std::pair<float, int>> temp;
        for (int j = 0; j < nlist; ++j) {
            temp.push_back(std::make_pair(C_dis[i][j], j));
        }
        std::sort(temp.begin(), temp.end(), comparefloatba);
        for (const auto& pair : temp) {
            C_dis_sorted_indices[i].push_back(pair.second);
        }
    }
    std::vector<std::vector<float>>().swap(C_dis);

    //记录centroid是否已经分配
    std::vector<int8_t> visit(nlist,0);
    //记录每个 DPU 存的 centroid 的编号
    std::vector<std::vector<int32_t> > dpu_id(nr_of_dpus);
    std::vector<std::vector<Code_pair>> dpu_hbm_cached_ps(nr_of_dpus);
    std::vector<int32_t> dpu_size(nr_of_dpus,0); // 记录每个DPU含有encoded point 个数
    std::vector<std::vector<int32_t> > C_dpu(nlist);//记录每个centroid分配到的DPU id
    std::vector<std::vector<int64_t> > dpu_store_ids(nr_of_dpus);//每个DPU 存的 ids
    std::vector<std::vector<uint16_t> > dpu_store_code(nr_of_dpus);//每个DPU 存的 encoded points
    std::vector<std::vector<int32_t> > dpu_store_offset(nr_of_dpus);// 每个DPU 存的 centroid 集合的 offset
    std::vector<int32_t> each_dpu_process(nr_of_dpus,0); // 每个DPU处理的大小
    int32_t max_dpu_store_size = 0; //DPU存储的点的最大数
    int32_t max_dpu_id = 0; //DPU存的 centroid 的最大数
    // int32_t thedshold_Q = nprobe / 2; //这里也可设置一个参数调
    for(int i=0;i<nr_of_dpus;i++)
    {
        dpu_store_offset[i].push_back(0);
    }
    float adaptive_threadshold = THREADSHOLD;
    float adaptive_store_threadshold = THREADSHOLD;
    //将每个centroid 分配到DPU上
    for(int i=0;i<nr_of_dpus;i++)
    {
        if(each_dpu_process[i] >= avg_process || dpu_store_offset[i][dpu_store_offset[i].size()-1] >= MAX_DPU_STORE_SIZE)
            continue;
        int begin = 0;
        while(begin<Cp.size() && (visit[C_process_idx[begin].second]!=0 || C_process_idx[begin].first == 0))
        {
            visit[C_process_idx[begin].second] = 1;
            begin++;
        }
        if(begin == nlist)
            continue;
        visit[C_process_idx[begin].second] = 1;
        int32_t C_id = C_process_idx[begin].second;
        int dis_id = 0;
        int dpu_offsets = dpu_store_offset[i][dpu_store_offset[i].size()-1];
        int j = i;
        bool unsuitable_dpu = false;
        if(C_process[C_id] > avg_process * 1)
        {
            int cnr_of_dpu = (C_process[C_id] + avg_process - 1) / avg_process; // 该centroid 应该分配到多个DPU
            int per_process = C_process[C_id] / cnr_of_dpu;
            float first_round_threadshold = THREADSHOLD - 0.05;
            for(j=i ; cnr_of_dpu > 0; j = (j+1) % nr_of_dpus)
            {
                if(j==i)
                {
                    first_round_threadshold += 0.05;
                    printf("update first round threadshold : %f \n", first_round_threadshold);
                    if(first_round_threadshold > 1.2)
                    {
                        printf("some error: first_round_threadshold larger than 1.2\n");
                    }
                }
                dpu_offsets = dpu_store_offset[j][dpu_store_offset[j].size()-1];
                if(each_dpu_process[j] + per_process > avg_process * first_round_threadshold || dpu_offsets + Cp[C_id] > MAX_DPU_STORE_SIZE)
                    continue;
                each_dpu_process[j] += per_process;
                cnr_of_dpu--;
                dpu_id[j].push_back(C_id);
                max_dpu_id = std::max(max_dpu_id,(int32_t)dpu_id[j].size());
                // for(int m=0; m<hbm_cached_ps[C_id].size();m++)
                // {
                //     for(int n=0; n<CACHE_LEN; n++)
                //     {
                //         dpu_hbm_cached_ps[j].push_back(hbm_cached_ps[C_id][m][n]);
                //     }
                // }
                dpu_size[j] += Cp[C_id];
                C_dpu[C_id].push_back(j);
                for(int32_t k = 0; k < Cp[C_id];k++)
                {
                    dpu_store_ids[j].push_back(ids[C_id][k]);
                    for(int32_t w = k*CODE_SIZE; w < (k+1)*CODE_SIZE;w++)
                        dpu_store_code[j].push_back(new_codes[C_id][w]);
                    dpu_offsets++;
                }
                max_dpu_store_size = std::max(max_dpu_store_size,dpu_offsets);
                dpu_store_offset[j].push_back(dpu_offsets);
                if(cnr_of_dpu == 0)
                    break;
            }
            std::vector<uint16_t>().swap(new_codes[C_id]);
            std::vector<faiss::idx_t>().swap(ids[C_id]);
        }
        else{
            for(j=i ; j < nr_of_dpus; j++)
            {
                if(each_dpu_process[j] + C_process[C_id] <= avg_process * 1 && dpu_store_offset[j][dpu_store_offset[j].size()-1] + Cp[C_id] <= MAX_DPU_STORE_SIZE)
                    break;
                else
                    continue;
            }
            while(j == nr_of_dpus)// 后面没有合适的DPU可以放这个元素了，增大THREADSHOLD从头来
            {
                for(j=0 ; j < nr_of_dpus; j++)
                {
                    if(each_dpu_process[j] + C_process[C_id] > avg_process * adaptive_threadshold)
                    {
                        continue;
                    }
                    else if(dpu_store_offset[j][dpu_store_offset[j].size()-1] + Cp[C_id] > MAX_DPU_STORE_SIZE)
                    {
                        unsuitable_dpu = true;
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                if(j == nr_of_dpus)
                {
                    if(unsuitable_dpu)
                        break;
                    adaptive_threadshold += 0.02;
                    if(adaptive_threadshold > 1.2)
                    {
                        printf("hhh\n");
                    }
                    printf("update adptivate threadshold: %.2f\n", adaptive_threadshold);
                }
            }
            if(unsuitable_dpu && j == nr_of_dpus)
            {
                visit[C_id] = 2;
                // i--;
                continue;
            }
            // if(j != i)
            //     i--;
            each_dpu_process[j] += C_process[C_id];
            dpu_id[j].push_back(C_id);
            max_dpu_id = std::max(max_dpu_id,(int32_t)dpu_id[j].size());
            // for(int m=0; m<hbm_cached_ps[C_id].size();m++)
            // {
            //     for(int n=0; n<CACHE_LEN; n++)
            //     {
            //         dpu_hbm_cached_ps[j].push_back(hbm_cached_ps[C_id][m][n]);
            //     }
            // }
            dpu_size[j] += Cp[C_id];
            C_dpu[C_id].push_back(j);
            dpu_offsets = dpu_store_offset[j][dpu_store_offset[j].size()-1];
            for(int32_t k = 0; k < Cp[C_id];k++)
            {
                dpu_store_ids[j].push_back(ids[C_id][k]);
                for(int32_t w = k*CODE_SIZE; w < (k+1)*CODE_SIZE;w++)
                    dpu_store_code[j].push_back(new_codes[C_id][w]);
                dpu_offsets++;
            }
            max_dpu_store_size = std::max(max_dpu_store_size,dpu_offsets);
            dpu_store_offset[j].push_back(dpu_offsets);
            std::vector<uint16_t>().swap(new_codes[C_id]);
            std::vector<faiss::idx_t>().swap(ids[C_id]);
        }

        while(each_dpu_process[j] < avg_process)
        {
            if(visit[C_dis_sorted_indices[C_id][dis_id]]!=0 || C_process[C_dis_sorted_indices[C_id][dis_id]] == 0)
            {
                dis_id++;
                if(dis_id >= nlist)
                    break;
                continue;
            }
            else if(each_dpu_process[j] + C_process[C_dis_sorted_indices[C_id][dis_id]] > avg_process || dpu_offsets + Cp[C_dis_sorted_indices[C_id][dis_id]] > MAX_DPU_STORE_SIZE)
            {
                dis_id++;
                if(dis_id >= nlist)
                    break;
                continue;
            }
            int next_id = C_dis_sorted_indices[C_id][dis_id];
            visit[next_id] = 1;
            dpu_id[j].push_back(next_id);
            max_dpu_id = std::max(max_dpu_id,(int32_t)dpu_id[j].size());
            // for(int m=0; m<hbm_cached_ps[next_id].size();m++)
            // {
            //     for(int n=0; n<CACHE_LEN; n++)
            //     {
            //         dpu_hbm_cached_ps[j].push_back(hbm_cached_ps[next_id][m][n]);
            //     }
            // }
            each_dpu_process[j] += C_process[next_id];
            dpu_size[j] += Cp[next_id];
            C_dpu[next_id].push_back(j);
            dpu_offsets = dpu_store_offset[j][dpu_store_offset[j].size()-1];
            for(int32_t k = 0; k < Cp[next_id];k++)
            {
                dpu_store_ids[j].push_back(ids[next_id][k]);
                for(int32_t w = k*CODE_SIZE; w < (k+1)*CODE_SIZE;w++)
                    dpu_store_code[j].push_back(new_codes[next_id][w]);
                dpu_offsets++;
            }
            max_dpu_store_size = std::max(max_dpu_store_size,dpu_offsets);
            dpu_store_offset[j].push_back(dpu_offsets);
            std::vector<uint16_t>().swap(new_codes[next_id]);
            std::vector<faiss::idx_t>().swap(ids[next_id]);
        }
    }
    for(int begin=0; begin<nlist; begin++)// 以DPU_STORE_SIZE为重点,找到最合适的DPU
    {
        if(visit[begin]==1)
            continue;
        visit[begin] = 1;
        int32_t C_id = begin;
        std::vector<std::pair<int, int>> Dpu_proc_idx;
        for(int i=0; i< nr_of_dpus; i++)
        {
            if(dpu_store_offset[i][dpu_store_offset[i].size()-1]+Cp[C_id] < MAX_DPU_STORE_SIZE)
            {
                Dpu_proc_idx.push_back(std::make_pair(each_dpu_process[i], i));
            }
        }
        std::sort(Dpu_proc_idx.begin(), Dpu_proc_idx.end(), comparebaINT);
        int D_id = Dpu_proc_idx[0].second;
        int dpu_offsets = dpu_store_offset[D_id][dpu_store_offset[D_id].size()-1];
        each_dpu_process[D_id] += C_process[C_id];
        dpu_id[D_id].push_back(C_id);
        max_dpu_id = std::max(max_dpu_id,(int32_t)dpu_id[D_id].size());
        // for(int m=0; m<hbm_cached_ps[C_id].size();m++)
        // {
        //     for(int n=0; n<CACHE_LEN; n++)
        //     {
        //         dpu_hbm_cached_ps[D_id].push_back(hbm_cached_ps[C_id][m][n]);
        //     }
        // }
        dpu_size[D_id] += Cp[C_id];
        C_dpu[C_id].push_back(D_id);
        for(int32_t k = 0; k < Cp[C_id];k++)
        {
            dpu_store_ids[D_id].push_back(ids[C_id][k]);
            for(int32_t w = k*CODE_SIZE; w < (k+1)*CODE_SIZE;w++)
                dpu_store_code[D_id].push_back(new_codes[C_id][w]);
            dpu_offsets++;
        }
        max_dpu_store_size = std::max(max_dpu_store_size,dpu_offsets);
        dpu_store_offset[D_id].push_back(dpu_offsets);
        std::vector<uint16_t>().swap(new_codes[C_id]);
        std::vector<faiss::idx_t>().swap(ids[C_id]);
    }
    // //从后往前又倒一次
    // for(int i=nr_of_dpus-1;i>=0;i--)
    // {
    //     if(dpu_id[i].size() >= (nlist / nr_of_dpus)*3)// 超参数
    //         continue;
    //     int begin = 0;
    //     while(begin<nlist && (visit[C_process_idx[begin].second]||dpu_store_offset[i][dpu_store_offset[i].size()-1]+Cp[C_process_idx[begin].second] > MAX_DPU_STORE_SIZE * 1.1))
    //         begin++;
    //     if(begin == nlist)
    //         break;
    //     visit[C_process_idx[begin].second] = true;
    //     int32_t C_id = C_process_idx[begin].second;
    //     int dis_id = 0;
    //     int dpu_offsets = dpu_store_offset[i][dpu_store_offset[i].size()-1];
    //     each_dpu_process[i] += C_process[C_id];
    //     dpu_id[i].push_back(C_id);
    //     max_dpu_id = std::max(max_dpu_id,(int32_t)dpu_id[i].size());
    //     dpu_size[i] += Cp[C_id];
    //     C_dpu[C_id].push_back(i);
    //     for(int32_t k = 0; k < Cp[C_id];k++)
    //     {
    //         dpu_store_ids[i].push_back(ids[C_id][k]);
    //         for(int32_t w = k*code_size; w < (k+1)*code_size;w++)
    //             dpu_store_code[i].push_back(codes[C_id][w]);
    //         dpu_offsets++;
    //     }
    //     max_dpu_store_size = std::max(max_dpu_store_size,dpu_offsets);
    //     dpu_store_offset[i].push_back(dpu_offsets);

    //     while(each_dpu_process[i] < avg_process && dpu_id[i].size() < (nlist / nr_of_dpus)*2)
    //     {
    //         if(visit[C_dis_sorted_indices[C_id][dis_id]])
    //         {
    //             dis_id++;
    //             if(dis_id >= nlist)
    //                 break;
    //             continue;
    //         }
    //         else if(each_dpu_process[i] + C_process[C_dis_sorted_indices[C_id][dis_id]] > avg_process || dpu_offsets + Cp[dis_id] > MAX_DPU_STORE_SIZE)//you wu
    //         {
    //             dis_id++;
    //             if(dis_id >= nlist)
    //                 break;
    //             continue;
    //         }
    //         int next_id = C_dis_sorted_indices[C_id][dis_id];
    //         visit[next_id] = true;
    //         dpu_id[i].push_back(next_id);
    //         max_dpu_id = std::max(max_dpu_id,(int32_t)dpu_id[i].size());
    //         each_dpu_process[i] += C_process[next_id];
    //         dpu_size[i] += Cp[next_id];
    //         C_dpu[next_id].push_back(i);
    //         for(int32_t k = 0; k < Cp[next_id];k++)
    //         {
    //             dpu_store_ids[i].push_back(ids[next_id][k]);
    //             for(int32_t w = k*code_size; w < (k+1)*code_size;w++)
    //                 dpu_store_code[i].push_back(codes[next_id][w]);
    //             dpu_offsets++;
    //         }
    //         max_dpu_store_size = std::max(max_dpu_store_size,dpu_offsets);
    //         dpu_store_offset[i].push_back(dpu_offsets);
    //     }
    // }

    std::vector<std::vector<faiss::idx_t>>().swap(ids);
    std::vector<std::vector<uint16_t>>().swap(new_codes);

    for(int i=0 ;i< nlist;i++)
    {
        if(visit[i]==false)
            printf("some error!!! centroid %d not allocate. \n",i);
    }
    int max_dproc = 0;
    int min_dproc = 99999999;
    int max_did = 0;
    int min_did = 0;
    for(int i=0 ; i < nr_of_dpus; i++)
    {
        if(each_dpu_process[i]>max_dproc)
        {
            max_dproc = each_dpu_process[i];
            max_did = i;
        }
        if(each_dpu_process[i] < min_dproc)
        {
            min_dproc = each_dpu_process[i];
            min_did = i;
        }
        printf("dpu %d : %d\n",i,each_dpu_process[i]);
        printf("centroid dpu %d : %d\n",i,(int32_t)dpu_id[i].size());
        printf("centroid size dpu %d : %d\n",i,dpu_size[i]);
    }
    printf("max dpu_proc %d : %d \n", max_did, max_dproc);
    printf("min dpu_proc %d : %d \n", min_did, min_dproc);
    // printf("cache code rate: %f\n", (float)((float)cache_hit / 1000000000));
    // printf("cache code rate total: %f\n", (float)((float)cache_hit_total / 1000000000));

    // std::vector<int32_t> N_div(nlist,0); // Number of dpu one centroid need to be divided.
    // std::vector<int32_t> AP(nlist,0); // Average points number for each centroid.
    // int32_t max_N_div = 0;
    // for(int32_t i=0; i<nlist;i++)
    // {
    //     N_div[i] = (int32_t)(Fq[i] * Cp[i]) / (avg_fq * avg_cp);
    //     if(N_div[i] <= 0)
    //     {
    //         N_div = 1;
    //     }
    //     max_N_div = max(max_N_div,N_div);
    //     AP[i] = (int32_t)Cp[i] / N_div[i];//这里不一定是整除
    // }

    // //记录每个 DPU 存的 centroid 的编号
    // std::vector<std::vector<int32_t> > dpu_id(nr_of_dpus,std::vector<int32_t>(nr_centroids,-1));
    // std::vector<int32_t> dpu_size(nr_of_dpus,0); // 记录每个DPU含有encoded point 个数
    // std::vector<int32_t> dpu_offset(nr_of_dpus,0); // 记录每个DPU 的偏移，便于分配操作
    // std::vector<std::vector<int32_t> > C_dpu(nlist);//记录每个centroid分配到的DPU id
    // std::vector<std::vector<int64_t> > dpu_store_ids(nr_of_dpus);//每个DPU 存的 ids
    // std::vector<std::vector<uint8_t> > dpu_store_code(nr_of_dpus);//每个DPU 存的 encoded points
    // std::vector<std::vector<int32_t> > dpu_store_offset(nr_of_dpus,(nr_centroids,0));// 每个DPU 存的 centroid 集合的 offset
    // int32_t max_dpu_store_size = 0;


    // //获取 N_div 降序排序的索引
    // std::vector<std::pair<int, int>> index_N_div;
    // for (int32_t i = 0; i < N_div.size(); ++i) {
    //     index_N_div.push_back(std::make_pair(N_div[i], i));
    // }
    // std::sort(index_N_div.begin(), index_N_div.end(), compareab);

    // //将每个centroid 分配到对应的DPU
    // for (int32_t i = 0; i < N_div.size(); ++i) {
    //     int dpu_index = get_dpu_index(dpu_offset,nr_of_dpus,nr_centroids);
    //     for (int32_t j = 0; j < index_N_div[i].first; j++)
    //     {
    //         dpu_id[dpu_index][dpu_offset[dpu_index]] = index_N_div[i].second;
    //         for(int32_t k = j*AP[index_N_div[i].second]; k < max((j+1)*AP[index_N_div[i].second],Cp[index_N_div[i].second]);k++)
    //         {
    //             dpu_store_ids[dpu_index].push_back(ids[index_N_div[i].second][k]);
    //             for(int32_t w = k*code_size; w < (k+1)*code_size;w++)
    //                 dpu_store_code[dpu_index].push_back(codes[index_N_div[i].second][w]);
    //             dpu_store_offset[dpu_index][dpu_offset[dpu_index]]++;
    //             dpu_size[dpu_index] ++;
    //         }
    //         dpu_offset[dpu_index]++;
    //         C_dpu[index_N_div[i].second].push_back(dpu_index);
    //         max_dpu_store_size = max(max_dpu_store_size, dpu_size[dpu_index]);
    //         if(dpu_index < nr_of_dpus -1 )
    //         {
    //             dpu_index ++;
    //         }
    //         else
    //         {
    //             dpu_index = get_dpu_index(dpu_offset,nr_of_dpus,nr_centroids);
    //         }
    //     }
    // }

    printf("[%.6f s] max_dpu_id: %d , max_dpu_store_size: %d\n",
        elapsed() - t0,
        max_dpu_id,max_dpu_store_size);

    int32_t dpus_id;

    auto system = dpu::DpuSet::allocate(NR_DPUS);
    system.load("./dpu/search_dpu");
    nr_of_dpus = system.dpus().size();
    printf("Allocated %d DPU(s)\n", nr_of_dpus);

    //广播codebook到所有的DPU
    system.copy("codebook_M",codebook);

    //传输idx 和 codes 到 DPU 中
    size_t max_size = 0;
    for (const auto& vec : dpu_store_ids) {
        max_size = std::max(max_size, vec.size());
    }
    for (auto& vec : dpu_store_ids) {
        vec.resize(max_size, 0);
    }
    max_size = 0;
    for (const auto& vec : dpu_store_code) {
        max_size = std::max(max_size, vec.size());
    }
    for (auto& vec : dpu_store_code) {
        vec.resize(max_size, 0);
    }
    system.copy("ids",dpu_store_ids); // 注意要传输一样的大小
    system.copy("codes",dpu_store_code);

    //传输每个DPU 存的 centroid 集合的 offset
    // printf("here");
    max_size = 0;
    for (const auto& vec : dpu_store_offset) {
        max_size = std::max(max_size, vec.size());
    }
    for (auto& vec : dpu_store_offset) {
        vec.resize(max_size, 0);
    }
    system.copy("offset",dpu_store_offset);

    //传输每个DPU 存的 centroid id
    max_size = 0;
    for (const auto& vec : dpu_id) {
        max_size = std::max(max_size, vec.size());
    }
    for (auto& vec : dpu_id) {
        vec.resize(max_size, 0);
    }
    system.copy("centroid_id",dpu_id);

    // //传输每个DPU 存的 cache
    // max_size = 0;
    // for (const auto& vec : dpu_hbm_cached_ps) {
    //     max_size = std::max(max_size, vec.size());
    // }
    // Code_pair flat;
    // flat.fir = 0;
    // flat.sed = 0;
    // for (auto& vec : dpu_hbm_cached_ps) {
    //     vec.resize(max_size, flat);
    // }
    // system.copy("hbm_cached_ps",dpu_hbm_cached_ps);

    system.copy("codebook_M",codebook);

    //---------------------------------------offline/initial query----------------------------------------

    float* xq_tmp = new float[d*nq_test];
    nq_freq = 9000;
    int32_t begin_idx = 0;
    for(int i=(nq_freq+begin_idx)*d;i<(nq_freq+begin_idx)*d+d*nq_test;i++)
    {
        xq_tmp[i-(nq_freq+begin_idx)*d] = xq[i];
    }
    printf("[%.6f s] Perform a search on %ld queries\n",
            elapsed() - t0,
            nq_test);

    // output buffers
    faiss::idx_t* I = new faiss::idx_t[nq_test * ks];

    //-----------------begin search-----------------
    // nprobe = 1;
    faiss::idx_t* idx_q = new faiss::idx_t[nq_test * nprobe];
    float* coarse_dis_q = new float[nq_test * nprobe];
    index->quantizer->search(nq_test,xq_tmp,nprobe,coarse_dis_q,idx_q,nullptr);

    // q_c layer: [nq_test, nprobe, d]
    int32_t* q_c = new int32_t[d*nq_test*nprobe];
    float* centroid_q = new float[d];

    //使用vector没有预分配空间，可能有点慢，后面需要优化
    std::vector<std::vector<int8_t> > dpu_q_c(nr_of_dpus); // 记录每个DPU分到的q-c
    std::vector<std::vector<int32_t> > dpu_qCentroid(nr_of_dpus); // 记录每个DPU分到的q-c 的centroid id
    std::vector<std::vector<int32_t> > dpu_q_o(nr_of_dpus); // 记录每个DPU 分到的query的offset，只返回offset个top k
    std::vector<int32_t> dpu_probe_num(nr_of_dpus,0); // 每个DPU 要处理的 centroid 的个数
    std::vector<std::vector<bool> > q_dpu(nq_test,std::vector<bool>(nr_of_dpus,false)); // 每个query 对应的dpu
    std::vector<std::vector<int> > I_dpu_offset(nq_test,std::vector<int>(nr_of_dpus, -1)); // 记录读取DPU结果的位置，避免后续重复读
    std::vector<int> dpu_res_offset(nr_of_dpus,0); // 记录DPU结果的偏移
    std::vector<int32_t> C_dpu_idx(nlist, 0);
    for(int w = 0; w < nr_of_dpus; w++)
    {
        dpu_q_o[w].push_back(0);
    }

    double pret0 = elapsed();
    for(int i=0;i<nq_test;i++)
    {
        for(int j = 0 ; j< nprobe;j++)
        {
            index->quantizer->reconstruct(idx_q[i*nprobe+j],centroid_q);
            dpu_qCentroid[C_dpu[idx_q[i*nprobe+j]][C_dpu_idx[idx_q[i*nprobe+j]]]].push_back(idx_q[i*nprobe+j]);
            dpu_probe_num[C_dpu[idx_q[i*nprobe+j]][C_dpu_idx[idx_q[i*nprobe+j]]]]++;
            q_dpu[i][C_dpu[idx_q[i*nprobe+j]][C_dpu_idx[idx_q[i*nprobe+j]]]] = true;
            for(int k=0;k<d;k++)
            {
                int8_t pow_q_c = (int8_t)((xq_tmp[i*d+k] - centroid_q[k]));
                dpu_q_c[C_dpu[idx_q[i*nprobe+j]][C_dpu_idx[idx_q[i*nprobe+j]]]].push_back(pow_q_c);
            }
            C_dpu_idx[idx_q[i*nprobe+j]] = (C_dpu_idx[idx_q[i*nprobe+j]] +1 ) % C_dpu[idx_q[i*nprobe+j]].size();
        }
        for(int w = 0; w < nr_of_dpus; w++)
        {
            if(dpu_q_o[w].size()>0 && dpu_q_o[w][dpu_q_o[w].size()-1] == dpu_probe_num[w])
                continue;
            I_dpu_offset[i][w]=dpu_res_offset[w];
            dpu_res_offset[w] += ks;
            dpu_q_o[w].push_back(dpu_probe_num[w]);
        }
    }
    double pret1 = elapsed();
    int32_t max_probe_num = 0;
    int32_t max_q_o = 0;
    int32_t avg_dpu_q = 0;
    for(int i=0; i < nr_of_dpus; i++)
    {
        dpu_q_o[i].push_back(-1); // 见到-1 就知道结束了
        max_probe_num = std::max(max_probe_num, dpu_probe_num[i]);
        max_q_o = std::max(max_q_o, (int32_t)dpu_q_o[i].size());
        // printf("dpu %d q_o size: %d \n",i, dpu_q_o[i].size());
        avg_dpu_q += (dpu_q_o[i].size()-2);
    }
    avg_dpu_q = avg_dpu_q / nr_of_dpus;
    printf("[%.6f s] Pre pocess the query, Time: %.6f ms\n",
            elapsed() - t0,
            (pret1-pret0)*1000);
    printf("[%.6f s] max_probe_num: %d, max_q_o: %d, avg dpu query: %d\n",
            elapsed() - t0,
            max_probe_num,
            max_q_o,
            avg_dpu_q);

    // for(int w = 0; w < nr_of_dpus; w++)
    // {
    //     printf("query dpu_q_o %d : %d\n",w,(int32_t)dpu_q_o[w].size()-2);
    //     printf("query dpu_probe_num %d : %d\n",w,dpu_probe_num[w]);
    //     int temp_process = 0;
    //     for(int i=0; i< dpu_qCentroid[w].size(); i++)
    //     {
    //         temp_process += Cp[dpu_qCentroid[w][i]];
    //     }
    //     printf("query dpu_process %d : %d\n",w,temp_process);
    // }

    //将 q_c 传到 DPU
    max_size = 0;
    for (const auto& vec : dpu_q_c) {
        max_size = std::max(max_size, vec.size());
    }
    for (auto& vec : dpu_q_c) {
        vec.resize(max_size, 0);
    }

    //将 q_c 对应的centroid 传到 DPU
    max_size = 0;
    for (const auto& vec : dpu_qCentroid) {
        max_size = std::max(max_size, vec.size());
    }
    for (auto& vec : dpu_qCentroid) {
        vec.resize(max_size, 0);
    }
    // system.copy("qCentroid",dpu_qCentroid);

    //将 q_o 传到 DPU
    max_size = 0;
    for (const auto& vec : dpu_q_o) {
        max_size = std::max(max_size, vec.size());
    }
    for (auto& vec : dpu_q_o) {
        vec.resize(max_size, -1);
    }

    double t1 = elapsed();
    system.copy("q_c",dpu_q_c);//69KB
    system.copy("qCentroid",dpu_qCentroid);//556B
    system.copy("q_o",dpu_q_o);//468B

    //广播其他相关的变量到DPU   应该作为全局变量#define
    // struct SearchPara searchpara;
    // searchpara.max_dpu_id = max_dpu_id;
    // searchpara.max_q_o = max_q_o;
    // // searchpara.ksub = ksub;
    // // searchpara.dsub = dsub;
    // DPU_ASSERT(dpu_broadcast_to(*dpu_set, "para", 0, &searchpara, ALIGN(sizeof(struct SearchPara),8), DPU_XFER_DEFAULT));

    double t2 = elapsed();
    printf("[%.6f s] send the query to DPU and begin search, Time: %.6f ms\n",
            elapsed() - t0,
            (t2-t1)*1000);
    
    t2 = elapsed();

    system.exec();

    double t3 = elapsed();
    printf("[%.6f s] finish search, Time: %.6f ms\n",
            elapsed() - t0,
            (t3-t2)*1000);

    std::vector<std::vector<Idx_piar> > I_dpu(nr_of_dpus,std::vector<Idx_piar>((max_q_o-2)*ks)); // dpu 返回的 index 结果
    // std::vector<std::vector<int32_t> > dis_dpu(nr_of_dpus,std::vector<int32_t>((max_q_o-2)*ks)); // dpu 返回的 distance 结果
    std::vector<std::vector<std::pair<int32_t, int64_t>>> I_temp(nq_test); //真实结果 I

    t3 = elapsed();

    //将 index 结果返回
    system.copy(I_dpu,"I");//91KB

    //将 dis 结果返回
    // system.copy(dis_dpu,"dis");//45KB

    double t4 = elapsed();
    printf("[%.6f s] DPU-CPU, Time: %.6f ms\n",
            elapsed() - t0,
            (t4-t3)*1000);

    system.dpus()[430]->log(std::cout);
    // system.log(std::cout);

    // {
    // std::ofstream outfile("I_dpu_offsetVector.txt");
    // if (!outfile.is_open()) {
    //     std::cerr << "无法打开文件" << std::endl;
    //     return 1;
    // }
    
    // // 写入数据到文件
    // for (const auto& row : I_dpu_offset) {
    //     for (const auto& elem : row) {
    //         outfile << elem << " ";
    //     }
    //     outfile << "\n"; // 换行符用来分隔每一行
    // }
    
    // // 关闭文件
    // outfile.close();
    // }
    // {
    // std::ofstream outfile("I_dpuVector.txt");
    // if (!outfile.is_open()) {
    //     std::cerr << "无法打开文件" << std::endl;
    //     return 1;
    // }
    
    // // 写入数据到文件
    // for (const auto& row : I_dpu) {
    //     for (const auto& elem : row) {
    //         outfile << elem.fir << " " << elem.sed << " ";
    //     }
    //     outfile << "\n"; // 换行符用来分隔每一行
    // }
    
    // // 关闭文件
    // outfile.close();
    // }
    // {
    // std::ofstream outfile("q_dpuVector.txt");
    // if (!outfile.is_open()) {
    //     std::cerr << "无法打开文件" << std::endl;
    //     return 1;
    // }
    
    // // 写入数据到文件
    // for (const auto& row : q_dpu) {
    //     for (const auto& elem : row) {
    //         outfile << elem << " ";
    //     }
    //     outfile << "\n"; // 换行符用来分隔每一行
    // }
    
    // // 关闭文件
    // outfile.close();
    // }

    t4 = elapsed();
    #pragma omp parallel for num_threads(MAX_NUM)
    for(int i=0;i<nq_test;i++)
    {
        for(int j=0; j<nr_of_dpus; j++)
        {
            if(q_dpu[i][j])
            {
                int res_begin = I_dpu_offset[i][j];
                for(int k=0; k<ks; k++)
                {
                    // if(i==13 && j==140)
                    // {
                    //     printf("hhh\n");
                    // }
                    // if(I_dpu[j][I_dpu_offset[j]].fir == 229097)
                    // {
                    //     printf("hhh");
                    // }
                    if(I_dpu[j][res_begin+k].fir == -1)
                    {
                        break;
                    }
                    if(I_dpu[j][res_begin+k].fir<=1 || std::isnan(I_dpu[j][res_begin+k].fir))
                    {
                        I_dpu[j][res_begin+k].fir=INT32_MAX;
                    }
                    I_temp[i].push_back(std::make_pair(I_dpu[j][res_begin+k].fir, I_dpu[j][res_begin+k].sed));
                }
            }
        }
    }
    #pragma omp parallel for num_threads(MAX_NUM) schedule(guided)
    for(int i=0;i<nq_test;i++)
    {
        std::sort(I_temp[i].begin(), I_temp[i].end(), compareba);
        #pragma omp parallel for
        for(int j=0; j<ks; j++)
        {
            I[i*ks + j] = I_temp[i][j].second;
            // printf("I[%d]: %ld, D[%d]: %u ",i*ks+j,I_temp[i][j].second,i*ks+j,I_temp[i][j].first);
        }
    }

    double t5 = elapsed();
    printf("[%.6f s] Post-process, Time: %.6f ms\n",
            elapsed() - t0,
            (t5-t4)*1000);

    printf("[%.6f s] Compute recalls\n", elapsed() - t0);

    // evaluate result by hand.
    int n_1 = 0, n_10 = 0, n_100 = 0;
    for (int i = nq_freq+begin_idx; i < nq; i++) {
        int gt_nn = gt[i * 1000];
        for (int j = 0; j < ks; j++) {
            if (I[(i-nq_freq - begin_idx) * ks + j] == gt_nn) {
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
