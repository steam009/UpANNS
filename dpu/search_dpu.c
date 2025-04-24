#include <perfcounter.h>
#include <stdio.h>
#include <mram.h>
#include <alloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <defs.h>
#include <sem.h>
#include <barrier.h>
#include <mram_unaligned.h>
#include "../include/common.h"
#include "../include/search_type.h"

#define DATASIZE 1
#define READSIZE 32 // update_LUT中每次从MRAM读取codebook的大小
#define CACHE_LEN 4
#define CACHE_PS_LEN 1024
#define CACHE_PS_LEN_ET 64 //2048/NR_TASKLETS  // 128
#define CACHE_LUT_LEN_ET 256 //(8*1024)/NR_TASKLETS
// #define ETD 16 // EACH TASKLET DEAL WITH IDS AND CODES
#define ONCEREAD ETD*NR_TASKLETS
#define EACHTSKREAD MS*KSUB/8/NR_TASKLETS
// __host uint64_t ins_main;
// __host uint64_t ins_insert;
// __host uint64_t ins_update;
// __host uint64_t ins_calculate;
// __host uint64_t ins_total_topk;
// __host uint64_t ins_memory_access;

int first_run = 1;

BARRIER_INIT(barrier_LUT, NR_TASKLETS);
BARRIER_INIT(barrier_LUT_FINISH, NR_TASKLETS);
BARRIER_INIT(barrier_CACHE_FINISH, NR_TASKLETS);
BARRIER_INIT(barrier_write, NR_TASKLETS);
// BARRIER_INIT(barrier_test, NR_TASKLETS);
SEMAPHORE_INIT(result_sem,1);
SEMAPHORE_INIT(write_sem,1);
// SEMAPHORE_INIT(read_sem,1);

__mram_noinit int64_t ids[1366000];//1640000
__mram_noinit uint16_t codes[1366000*CODE_SIZE];
__mram_noinit int8_t codebook_M[MS*KSUB*DSUB];
__mram_noinit Idx_piar I[TOPK*MAX_Q_O];//返回结果的索引
// __mram_noinit int32_t dis[TOPK*MAX_Q_O];//返回结果的距离
__mram_noinit int8_t q_c[MAX_PROBE_NUM * DIMM]; //69KB
// __mram_noinit Code_pair hbm_cached_ps[MAX_DPU_ID*CACHE_PS_LEN];

__host int32_t qCentroid[MAX_PROBE_NUM]; //556B
__host int32_t q_o[MAX_Q_O]; //468 B
// __dma_aligned int64_t cur_I[TOPK];//返回当前query结果的索引 808B
__dma_aligned Heap_q result;

__host int32_t offset[MAX_DPU_ID+1];//176B
__host int32_t centroid_id[MAX_DPU_ID];//172B

__dma_aligned uint16_t LUT[KSUB*MS + 2]; //16KB KSUB*MS
__host uint16_t* cache_LUT = &LUT[KSUB*MS];//12KB  最大18KB
// __dma_aligned int8_t codebook_W[CODEBOOK_SIZE];//8KB
// __dma_aligned bool LUT_calculated[KSUB*MS];//1KB
__dma_aligned int64_t cur_ids[ONCEREAD];//2KB
__dma_aligned uint16_t cur_codes[ONCEREAD * CODE_SIZE];//8KB  能不能试试分成两次读看看 cur_codes 大小一定要大于READSIZE * NR_TASKLETS
__dma_aligned int8_t cur_q_c[DIMM];//512B
// __dma_aligned int8_t rasidual[NR_TASKLETS];//512B

__dma_aligned int8_t* codebook_LUT = (int8_t*)cur_codes;
// __dma_aligned Code_pair* cached_ps = (Code_pair*)cur_codes;//[ONCEREAD * CODE_SIZE / 2];

// Heapify function for maximum heap
void minHeapify(Heap_q* heap, int i) {
    int largest = i;
    int left = 2 * i;
    int right = 2 * i + 1;

    if (left <= heap->size && heap->data[left].fir < heap->data[largest].fir) {
        largest = left;
    }

    if (right <= heap->size && heap->data[right].fir < heap->data[largest].fir) {
        largest = right;
    }

    if (largest != i) {
        Idx_piar temp = heap->data[i];
        heap->data[i] = heap->data[largest];
        heap->data[largest] = temp;
        minHeapify(heap, largest);
    }
}

// Function to build a maximum heap from a minimum heap
void convertMaxHeapToMinHeap(Heap_q* heap) {
    for (int i = heap->size / 2; i > 0; i--) {
        minHeapify(heap, i);
    }
}

// Function to pop the maximum element from the heap
Idx_piar pop2(Heap_q* heap) {
    Idx_piar max_r = heap->data[1];
    heap->data[1] = heap->data[heap->size--];
    minHeapify(heap, 1);

    return max_r;
}

Idx_piar pop(Heap_q* sto){
    Idx_piar min_r = sto->data[1];
    Idx_piar tem = sto->data[sto->size--];
    int i,next;
    for(i=1; (i<<1) <= sto->size; i=next){
        next = i<<1;
        if(next != sto->size && sto->data[next].fir < sto->data[next+1].fir){
            next++;
        }
        if(tem.fir < sto->data[next].fir){
            sto->data[i].fir = sto->data[next].fir;
            sto->data[i].sed = sto->data[next].sed;
        }
        else{
            break;
        }
    }
    sto->data[i].fir = tem.fir;
    sto->data[i].sed = tem.sed;

    return min_r;
}

void push(Heap_q* sto, int32_t fir, int64_t sed){
    
    if(sto->size == sto->cap){
        if(sto->data[1].fir < fir) return;
        else pop(sto);
    }
    int i;
    for(i = ++sto->size; sto->data[i>>1].fir < fir && i>0; i = i>>1){
        sto->data[i].fir = sto->data[i>>1].fir;
        sto->data[i].sed = sto->data[i>>1].sed;
    }
    sto->data[i].fir = fir;
    sto->data[i].sed = sed;
}

void updated_LUT(int i, int c_id)
{
    c_id = 0;
    int t_id = me();
    if(t_id==0)
        mram_read(&q_c[i*DIMM],cur_q_c,ALIGN(MIN(2048,DIMM*sizeof(int8_t)),8));
    barrier_wait(&barrier_LUT);
    // mram_read(&q[i*DIMM],cur_q,ALIGN(MIN(2048,DIMM*sizeof(int32_t)),8));

    // mram_read(&q_c[i*DIMM],cur_q_c,ALIGN(MIN(2048,DIMM*sizeof(int32_t)),8));

    // int times = (MS*KSUB*DSUB) / CODEBOOK_SIZE;//4
    // int mini_MS = CODEBOOK_SIZE / (KSUB*DSUB);//4
    
    int begin_MS = t_id * (MS / NR_TASKLETS + ((MS % NR_TASKLETS)!=0));// 2
    int end_MS = (t_id+1) * (MS / NR_TASKLETS + ((MS % NR_TASKLETS)!=0));
    int q_idx = 0;
    int nr_of_read = READSIZE / (DSUB);
    // mram_read(&codebook_M[t_id*(2048/DATASIZE)],codebook_W+t_id*(2048/DATASIZE),2048);//这里tasklet一定要是16

    // uint32_t begin_t = perfcounter_get();
    // for(int k=begin_MS; k<end_MS; k++)//计算LUT 每个线程处理2个k 256*2 = 512
    // {
    //     for(int m=0; m<KSUB; m++)
    //     {
    //         LUT_calculated[k*KSUB + m] = 0;
    //     }
    // }
    // uint32_t end_t = perfcounter_get();
    // ins_LUT[t_id] += (end_t - begin_t);


    int begin_adr = begin_MS*(2048/DATASIZE);
    uint8_t read_count = 0;//记录读MRAM的次数
    int next_adr = 0;//记录下一个地址的起始位置
    int begin_cb_adr = begin_MS * READSIZE;
    for(int k=begin_MS; k<end_MS && k < MS; k++)//计算LUT 每个线程处理4个k
    {
        read_count = 0;
        for(int m=0; m<KSUB; m+=nr_of_read)
        {
            mram_read(&codebook_M[begin_adr+next_adr+read_count*READSIZE],codebook_LUT+begin_cb_adr,READSIZE);//这里tasklet一定要是16
            for(int j=0; j<nr_of_read; j++){
                q_idx = k * DSUB;
                uint16_t temp_sum = 0;
                for(int n=0; n<DSUB; n++)
                {
                    int8_t temp_res = cur_q_c[q_idx] - codebook_LUT[begin_cb_adr + j * DSUB +n];
                    temp_sum += (temp_res * temp_res);
                    q_idx = q_idx + 1;
                }
                LUT[k*KSUB + (m+j)] = temp_sum;
            }
            read_count++;
        }
        next_adr = next_adr + KSUB*DSUB;
    }
    barrier_wait(&barrier_LUT_FINISH);
    // // 遍历所有可能的二进制索引
    // mram_read(&hbm_cached_ps[c_id * CACHE_PS_LEN + t_id * CACHE_PS_LEN_ET], cached_ps + t_id * CACHE_PS_LEN_ET,ALIGN(MIN(2048,CACHE_PS_LEN_ET * sizeof(Code_pair)),8));
    // int LUT_begin_idx = 0;
    // uint16_t sum = 0;
    // for(int k= t_id * CACHE_PS_LEN_ET ; k < (t_id+1) * CACHE_PS_LEN_ET; k+= CACHE_LEN){
    //     for(int j = 0; j < (1<<CACHE_LEN) ; j++)
    //     {
    //         sum = 0;
    //         for(int m = 0; m<CACHE_LEN; m++)
    //         {
    //             if( j & (1 << m)){
    //                 // if(cached_ps[k+m].fir*KSUB + cached_ps[k+m].sed >= 8192)
    //                 // {
    //                 //     printf("some error in LUT, fir: %d, sed: %d\n", cached_ps[k+m].fir, cached_ps[k+m].sed);
    //                 //     continue;
    //                 // }
    //                 sum += LUT[cached_ps[k+m].fir*KSUB + cached_ps[k+m].sed];
    //             }
    //         }
    //         cache_LUT[t_id * CACHE_LUT_LEN_ET + LUT_begin_idx * (1<<CACHE_LEN) + j] = sum;
    //     }
    //     LUT_begin_idx++;
    // }
    // barrier_wait(&barrier_CACHE_FINISH);
    // sem_take(&write_sem);
    // if(first_run)
    // {
    //     for(int m=0 ; m < 32; m++)
    //     {
    //         for(int n=0; n < KSUB ; n++)
    //         {
    //             printf("LUT[%d]: %d ",m*KSUB+n,LUT[m*KSUB+n]);
    //         }
    //     }
    //     first_run = 0;
    // }
    // // if(first_run)
    // // {
    // //     for(int m=0; m<1024; m++)
    // //     {
    // //         printf("cache_ps[%d].fir: %d cache_ps[%d].sed: %d ",m,cached_ps[m].fir,m, cached_ps[m].sed);
    // //     }
    // //     printf("\n");
    // //     for(int m=0; m<1024; m++)
    // //     {
    // //         printf("LUT[%d]: %d ",cached_ps[m].fir*KSUB + cached_ps[m].sed, LUT[cached_ps[m].fir*KSUB + cached_ps[m].sed]);
    // //     }
    // //     printf("\n");
    // //     for(int m=0 ; m < 4096; m++)
    // //     {
    // //         printf("cache_LUT[%d]: %d ",m,cache_LUT[m]);
    // //     }
    // //     first_run = 0;
    // // }
    // sem_give(&write_sem);
    // barrier_wait(&barrier_test);
}

int main(){
    // Profiling
    // perfcounter_config(COUNT_CYCLES, true);
    // printf("current thread:%d ", me());

    int t_id = me();
    if(t_id==0){
        mem_reset();
        result.size = 0;
        result.cap = TOPK;
    }
    // 计算lookup table
    int i=0;
    Heap_q temp_result;
    for(i=0; i< MAX_Q_O - 1 ; i++)
    {
        if(q_o[i+1] == -1){
            // printf("i: %d tasklet id: %d \n",i,t_id);
            break;
        }
        uint16_t ita = t_id;
        int len = 0;
        uint32_t copied_ids = 0; 
        temp_result.size = 0;
        temp_result.cap = TOPK;
        for(int j=q_o[i]; j< q_o[i+1]; j++)
        {
            // if(t_id==0)
            // {
            //     printf("q_c[%d]: %d \n",j, q_c[j]);
            // }
            int32_t c_id_temp = qCentroid[j];
            int32_t c_id = 0;
            while(true)
            {
                if(centroid_id[c_id]==c_id_temp)
                {
                    break;
                }
                else
                {
                    c_id++;
                }
            }
            uint32_t ids_size = offset[c_id +1] - offset[c_id];
            // uint32_t begin = perfcounter_get();
            updated_LUT(j, c_id);
            // uint32_t end = perfcounter_get();
            // if(t_id==0)
            //     ins_update += (end - begin);
            copied_ids = 0; 
            while(copied_ids < ids_size)
            {
                // uint32_t begin3 = perfcounter_get();
                len = MIN(ONCEREAD, (ids_size-copied_ids));
                mram_read(&ids[offset[c_id]+copied_ids+t_id * ETD],cur_ids + t_id * ETD, ALIGN(MIN(2048,ETD*8),8));
                mram_read(&codes[(offset[c_id]+copied_ids+t_id * ETD)*CODE_SIZE], cur_codes + t_id * ETD * CODE_SIZE, ALIGN(MIN(2048,(ETD)*CODE_SIZE)*2,8));
                copied_ids += ONCEREAD;
                int dist_size = MIN(len, (t_id+1) * ETD);
                // uint32_t end3 = perfcounter_get();
                // if(t_id==0)
                //     ins_memory_access += (end3 - begin3);
                // uint32_t begin2 = perfcounter_get();
                for(ita = t_id * ETD; ita < dist_size; ita++)
                {
                    int32_t dis_te = 0;
                    // uint16_t LUT_len = (cur_codes[ita*CODE_SIZE] & 15) + 1;
                    // LUT_len ++ ;
                    // int no_cache_len = (((cur_codes[ita*CODE_SIZE]) & 0xf ) + 1) * 2;
                    uint16_t* c_codes = &cur_codes[ita*CODE_SIZE];
                    for(uint8_t k = 0; k < 16; k++)
                    {
                        // if(cur_codes[ita*CODE_SIZE + LUT_idx + 3]+k*KSUB > 8192)
                        // {
                        //     printf("too large in LUT, k: %d, ,ita: %d  LUT_idx: %d , code: %d\n", k, ita, (int)LUT_idx, (int)cur_codes[ita*CODE_SIZE + LUT_idx + 3]);
                        //     // continue;
                        // }
                        dis_te  += LUT[c_codes[k]];
                        // if(t_id==0)
                        // {
                        //     printf("cur_code: %d LUT[%d]: %d, cur_dis:%d  ", cur_codes[ita*CODE_SIZE + LUT_idx + 3], cur_codes[ita*CODE_SIZE + LUT_idx + 3]+k*KSUB,LUT[cur_codes[ita*CODE_SIZE + LUT_idx + 3]+k*KSUB],dis_te );
                        // }
                        // if(cur_ids[ita]==145976)
                        // {
                        //     printf("LUT[%d]: %d ", cur_codes[ita*CODE_SIZE + k]+k*KSUB, LUT[cur_codes[ita*CODE_SIZE + k]+k*KSUB]);
                        // }
                    }
                    // sem_take(&result_sem); // 加锁大概多 20 ms
                    // begin = perfcounter_get();
                    // printf("ita: %d tasklet id: %d ",ita,t_id);
                    // if(cur_ids[ita]==145976)
                    // {
                    //     printf("\n ita: %d tasklet id: %d cur_ids[ita]: %lld dis_te: %u \n",ita,t_id,cur_ids[ita],dis_te);
                    // }
                    // if(cur_ids[ita]==982142 || cur_ids[ita]==653124)
                    // {
                    //     printf("\n i: %d j: %d ita: %d tasklet id: %d cur_ids[ita]: %lld dis_te: %u \n",i,j,ita,t_id,cur_ids[ita],dis_te);
                    // }
                    // if(j==1)
                    // {
                    //     printf("\n j:%d ita: %d tasklet id: %d cur_ids[ita]: %lld dis_te: %u \n",j,ita,t_id,cur_ids[ita],dis_te);
                    // }
                    push(&temp_result, dis_te , cur_ids[ita]);
                    // end = perfcounter_get();
                    // if(t_id==0)
                    //     ins_insert += (end - begin);
                    // sem_give(&result_sem);
                }
                // uint32_t end2 = perfcounter_get();
                // if(t_id==0)
                //     ins_calculate += (end2 - begin2);
            }
        }
        // for(int m = 0; m< TOPK ; m++)
        // {
        //     if(cur_dis[m]==0)
        //     {
        //         printf("i:%d q_o[i+1]:%d cur_I[%d]: %lld \n ",i,q_o[i+1], m,cur_I[m]);
        //     }
        // }
        // uint32_t begin = perfcounter_get();
        int temp_size = temp_result.size;
        convertMaxHeapToMinHeap(&temp_result);
        for(int j=temp_size;j>0;--j)
        {
            Idx_piar max_r = pop2(&temp_result);
            if(result.size >= TOPK && result.data[1].fir < max_r.fir)
                break;
            sem_take(&result_sem);
            push(&result, max_r.fir, max_r.sed);
            sem_give(&result_sem);
        }
        // uint32_t end = perfcounter_get();
        // if(t_id==0)
        //     ins_total_topk += (end - begin);
        //写回当前query 的结果回结果数组
        // printf("i: %d tasklet id: %d cur_I[0]: %lld cur_dis[0]: %u \n",i,t_id,cur_I[0],cur_dis[0]);
        barrier_wait(&barrier_write);
        // printf("i: %d tasklet id: %d cur_I[0]: %lld cur_dis[0]: %u \n",i,t_id,cur_I[0],cur_dis[0]);
        if(ita==len){
            // sem_take(&write_sem);
            // printf("i: %d ita: %d tasklet id: %d len:%d\n",i,ita,t_id,len);
            // for(int j=TOPK-1;j>=0;--j)//30ms
            // {
            //     Idx_piar min_r = pop(&result);
            //     cur_I[j] = min_r.sed;
            //     cur_dis[j] = min_r.fir;
            // }
            if(result.size != TOPK)
                result.data[++result.size].fir = -1;
            mram_write(&result.data[1], &I[i*TOPK], ALIGN(TOPK*sizeof(Idx_piar),8));
            result.size = 0;
            // copied_ids = 0;
            // sem_give(&write_sem);
        }
        // barrier_wait(&barrier_write);
    }
    // if(t_id==0)
    //     ins_main = perfcounter_get();
    // if(q_o[1]!=-1)
    // {
    //     printf("tasklet id: %d finish\n",t_id);
    // }
    // if(t_id==0)
    // {
    //     printf("ins_main: %lu, ins_update: %lu, ins_insert:%lu, ins_calculate:%lu, ins_total_topk:%lu, ins_memory_access:%u\n", ins_main,ins_update, ins_insert, ins_calculate,ins_total_topk,ins_memory_access);
    // }
    return 0;
}

