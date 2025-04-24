from __future__ import print_function
import os
import time
import numpy as np
import struct
import argparse

try:
    import matplotlib
    matplotlib.use('Agg')
    from matplotlib import pyplot
    graphical_output = True
except ImportError:
    graphical_output = False

import faiss

# 解析命令行参数
parser = argparse.ArgumentParser(description='FAISS GPU search parameters')
parser.add_argument('--nprobe', type=int, default=64, help='Number of probes')
parser.add_argument('--k', type=int, default=10, help='Number of nearest neighbors to return')
args = parser.parse_args()

#################################################################
# Small I/O functions
#################################################################

def read_fbin(filename, start_idx=0, chunk_size=None):
    """ Read *.fbin file that contains float32 vectors
    Args:
        :param filename (str): path to *.fbin file
        :param start_idx (int): start reading vectors from this index
        :param chunk_size (int): number of vectors to read. 
                                 If None, read all vectors
    Returns:
        Array of float32 vectors (numpy.ndarray)
    """
    with open(filename, "rb") as f:
        nvecs, dim = np.fromfile(f, count=2, dtype=np.int32)
        nvecs = (nvecs - start_idx) if chunk_size is None else chunk_size
        arr = np.fromfile(f, count=nvecs * dim, dtype=np.float32, 
                          offset=start_idx * 4 * dim)
    return arr.reshape(nvecs, dim)
 
 
def read_ibin(filename, start_idx=0, chunk_size=None):
    """ Read *.ibin file that contains int32 vectors
    Args:
        :param filename (str): path to *.ibin file
        :param start_idx (int): start reading vectors from this index
        :param chunk_size (int): number of vectors to read.
                                 If None, read all vectors
    Returns:
        Array of int32 vectors (numpy.ndarray)
    """
    with open(filename, "rb") as f:
        nvecs, dim = np.fromfile(f, count=2, dtype=np.int32)
        nvecs = (nvecs - start_idx) if chunk_size is None else chunk_size
        arr = np.fromfile(f, count=nvecs * dim, dtype=np.int32, 
                          offset=start_idx * 4 * dim)
    return arr.reshape(nvecs, dim)

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

xq = read_fbin("faiss/deep/query.public.10K.fbin")
gt = read_ibin("faiss/deep/groundtruth.public.10K.ibin")

query_len, d = xq.shape
k = gt.shape[1]

# 获取可用GPU数量
ngpus = faiss.get_num_gpus()
print(f"Number of GPUs: {ngpus}")

# 定义文件路径
index_filename = "deep1B_8192PQ12.index"

# 加载索引
cpu_index = faiss.read_index(index_filename)

gpu_id = 1
print(f"Using GPU: {gpu_id}")

# 创建GPU资源
gpu_resource = faiss.StandardGpuResources()

# co = faiss.GpuClonerOptions()
# co.use_cuvs = True

gpu_index = faiss.index_cpu_to_gpu(gpu_resource, gpu_id, cpu_index)

# 创建分布在所有GPU上的索引
# gpu_index = faiss.index_cpu_to_all_gpus(cpu_index)

# 设置nprobe参数
faiss.GpuParameterSpace().set_index_parameter(
                gpu_index, 'nprobe', args.nprobe)

# 搜索
xq_tmp = xq[9000:10000]
print("xq_tmp shape: ", xq_tmp.shape)
avg_time = 0
for i in range(10):
    start = time.time()
    D, I = gpu_index.search(xq_tmp, args.k)
    end = time.time()
    avg_time += end - start
    print(f"search time {i} : {end - start} seconds")

print(f"Multi GPU IVFPQ search time: {avg_time / 10} seconds")
recall_1 = 0
recall_10 = 0
recall_100 = 0
for i in range(1000):
    gt_temp = gt[9000+i][0]
    for m in range(args.k):
        if gt_temp == I[i][m]:
            if m < 1:
                recall_1 += 1
            if m < 10:
                recall_10 += 1
            if m < 100:
                recall_100 += 1

print(f"recall@1 : {recall_1 / (100)} ")
print(f"recall@10 : {recall_10 / (100)} ")
print(f"recall@100 : {recall_100 / (100)} ")
# print("Indices:\n", I)
# print("Distances:\n", D)