import re
import matplotlib.pyplot as plt

# 从文件中读取文本内容
with open('res2.txt', 'r') as file:
    data = file.readlines()

# 创建列表来存储查询进程值
query_process_values = []

# 遍历每行文本内容
for line in data:
    # 使用正则表达式提取数据
    match = re.match(r'query dpu_process \d+ : (\d+)', line)
    if match:
        process_value = int(match.group(1))
        query_process_values.append(process_value)

# 对查询进程值进行排序
sorted_query_process_values = sorted(query_process_values)

# 可视化排序后的查询进程值
plt.figure(figsize=(10, 6))
plt.bar(range(1, len(sorted_query_process_values)+1), sorted_query_process_values, color='lightgreen')
plt.xlabel('Sorted Query Process')
plt.ylabel('Query Process Value')
plt.title('Sorted Query Process Distribution')
plt.xticks(range(0, 512+60, 64))
plt.tight_layout()

# 保存图形
plt.savefig('sorted_query_process_distribution.png')
plt.show()
