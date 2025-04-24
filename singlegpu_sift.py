from __future__ import print_function
import os
import time
import numpy as np

try:
    import matplotlib
    matplotlib.use('Agg')
    from matplotlib import pyplot
    graphical_output = True
except ImportError:
    graphical_output = False

import faiss

#################################################################
# Small I/O functions
#################################################################

def bvecs_read(fname):
    with open(fname, "rb") as f:
        # Read dimension `d`
        d = np.fromfile(f, dtype=np.int32, count=1)[0]
        assert 0 < d < 1000000, "Unreasonable dimension"

        # Get file size and calculate number of vectors `n`
        f.seek(0, 2)  # Seek to end of file
        sz = f.tell()  # Get current position (file size in bytes)
        assert sz % (1 * 4 + d) == 0, "Weird file size"
        n = sz // (1 * 4 + d)  # Number of vectors

        f.seek(0, 0)  # Seek back to beginning of file
        data = np.fromfile(f, dtype=np.uint8, count=n * (d + 1 * 4))

    # Remove row headers and convert to float32
    data = data.reshape(n, -1)[:, 4:].astype(np.float32)

    return data.reshape(n, d)

def ivecs_read(fname):
    a = np.fromfile(fname, dtype="int32")
    d = a[0]
    return a.reshape(-1, d + 1)[:, 1:].copy()

def fvecs_read(fname):
    return ivecs_read(fname).view('float32')

t0 = time.time()

print("load data")

xq = bvecs_read("faiss/sift1B/bigann_query.bvecs")

d = xq.shape[1]

print("load GT")

gt = ivecs_read("faiss/sift1B/gnd/idx_1000M.ivecs")
gt = gt.astype('int64')
k = gt.shape[1]

# nlist 和 nprobe 设置
nlist = 4096
nprobe = 32

# 获取可用GPU数量
ngpus = faiss.get_num_gpus()
print(f"Number of GPUs: {ngpus}")

# 定义文件路径
index_filename = "kmeans.index"

# 加载索引
cpu_index = faiss.read_index(index_filename)

gpu_id = 1
print(f"Using GPU: {gpu_id}")

# 创建GPU资源
gpu_resource = faiss.StandardGpuResources()

gpu_index = faiss.index_cpu_to_gpu(gpu_resource, 1, cpu_index)

# 创建分布在所有GPU上的索引
# gpu_index = faiss.index_cpu_to_all_gpus(cpu_index)

# 设置nprobe参数
faiss.GpuParameterSpace().set_index_parameter(
                gpu_index, 'nprobe', 32)

# 搜索
k = 100  # 返回最近邻个数
begin_idx = 1000
xq_tmp = xq[begin_idx:begin_idx+1000]
print("xq_tmp shape: ", xq_tmp.shape)
start = time.time()
D, I = gpu_index.search(xq_tmp, k)
end = time.time()

print(f"Multi GPU IVFPQ search time: {end - start} seconds")
# recall = 0
# for i in range(1000):
#     for j in range(100):
#         gt_temp = gt[9000+i][j]
#         for m in range(1000):
#             if gt_temp == I[i][m]:
#                 recall += 1

# print(f"recall 100@1000: {recall / (1000*100)} ")
recall_1 = 0
recall_10 = 0
recall_100 = 0
for i in range(1000):
    gt_temp = gt[begin_idx+i][0]
    for m in range(k):
        if gt_temp == I[i][m]:
            if m < 1:
                recall_1 += 1
            if m < 10:
                recall_10 += 1
            if m < 100:
                recall_100 += 1

print(f"recall@1 : {recall_1 / (1000)} ")
print(f"recall@10 : {recall_10 / (1000)} ")
print(f"recall@100 : {recall_100 / (1000)} ")
# print("Indices:\n", I)
# print("Distances:\n", D)