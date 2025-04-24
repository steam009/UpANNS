#include <stdint.h>
#include <stdbool.h>

#define MAX_ENC_BUFFER_SIZE MEGABYTE(MAX_ENC_BUFFER_MB)
#define MAX_CAPACITY MEGABYTE(14) //Must be a multiply of 2
#define DPUS_PER_RANK 64
#define AVAILABLE_RANKS 20
#define MAX_NR_BUFFERS 65

struct SearchPara {
    size_t max_dpu_id;
    size_t max_q_o;
    // size_t ksub;
    // size_t dsub;
}__attribute__((packed));

typedef struct{
    int32_t fir; // store the distance
    int32_t sed; // store the idx
}Idx_piar;

typedef struct{
    uint8_t fir; // store the distance
    uint8_t sed; // store the idx
}Code_pair;

typedef struct{
    int32_t cap; // TOPK
    int32_t size; // current size
    Idx_piar data[TOPK+1];
}Heap_q;