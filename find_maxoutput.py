import re

def main():
    # 指定文件路径
    file_path = "res_deep1b4096nprobs64top1_propose.txt"  # 替换为你的文件路径

    # 初始化存储数据的列表
    ins_data = []
    current_ins = {}

    # 打开文件并按行读取内容
    with open(file_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()

            # 检查是否是DPU块的起始行
            if line.startswith("=== DPU#"):
                # 如果之前有未保存的数据，先保存
                if current_ins:
                    ins_data.append(current_ins)
                    current_ins = {}
                continue

            # 使用正则表达式提取ins_xx的值
            parts = re.findall(r'ins_([a-z_]+):\s*(\d+)', line)
            for key, value in parts:
                current_ins['ins_' + key] = int(value)

        # 保存最后一个DPU块的数据
        if current_ins:
            ins_data.append(current_ins)

    # 提取所有ins_main的最大值及其索引
    max_main = -1
    max_index = -1
    for index, data in enumerate(ins_data):
        main = data.get('ins_main', 0)
        if main > max_main:
            max_main = main
            max_index = index

    max_index = 202
    # 输出结果
    if max_index != -1:
        print(f"ins_main 最大的索引是: {max_index}")
        print(f"该索引下的其他 ins 值为:")
        for key, value in ins_data[max_index].items():
            print(f"{key}: {value}")
    else:
        print("没有找到有效的 ins_xx 数据。")

if __name__ == "__main__":
    main()