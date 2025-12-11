
import matplotlib.pyplot as plt
import numpy as np

# #Params regarding visual style
# plt.rcParams['font.family'] = 'sans-serif'
# plt.rcParams['font.sans-serif'] = ['Arial'] # 使用通用字体

# ==========================================
# 图 1: KVS Latency (越低越好)
# ==========================================
def plot_latency():
    labels = ['CPU Baseline', 'PsPIN (Ours)']
    latency_values = [68803.803, 3717.390] # Unit: ns
    
    x = np.arange(len(labels))
    width = 0.5

    fig, ax = plt.subplots(figsize=(6, 5))
    rects = ax.bar(x, latency_values, width, color=['#A9A9A9', '#2E86C1']) # 灰色 vs 蓝色

    # 设置标签和标题
    ax.set_ylabel('Latency (ns) - Lower is Better', fontsize=12, fontweight='bold')
    ax.set_title('Workload 1: KVS Lookup Latency', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=12)
    
    # 添加具体数值标签
    ax.bar_label(rects, padding=3, fmt='%.0f ns', fontsize=11)


    plt.tight_layout()
    plt.savefig('kvs_latency.png', dpi=300)
    plt.show()

# ==========================================
# 图 2: Throughput (越高越好)
# ==========================================
def plot_throughput():
    labels = ['MapReduce\n(Int Aggregation)', 'AllReduce\n(Float Training)']
    
    cpu_means = [0.825, 0.837]
    pspin_means = [141.793, 3.825]

    x = np.arange(len(labels))  
    width = 0.35  

    fig, ax = plt.subplots(figsize=(8, 6))
    
    # 绘制柱状图
    rects1 = ax.bar(x - width/2, cpu_means, width, label='CPU Baseline', color='#A9A9A9')
    rects2 = ax.bar(x + width/2, pspin_means, width, label='PsPIN (Ours)', color='#E67E22') # 橙色高亮

    # 设置标签
    ax.set_ylabel('Throughput (Gbit/s) - Higher is Better', fontsize=12, fontweight='bold')
    ax.set_title('Workload 2 & 3: Throughput Comparison', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=12)
    ax.legend()

    # 添加数值标签
    ax.bar_label(rects1, padding=3, fmt='%.2f', fontsize=10)
    ax.bar_label(rects2, padding=3, fmt='%.2f', fontsize=10)

    # 这里的 MapReduce 差距太大，AllReduce 差距较小
    # 为了让 AllReduce 不至于看起来太扁，可以考虑使用对数坐标，
    # 但为了展示 MapReduce 的惊人优势，线性坐标更震撼。
    # 这里我们手动添加倍数标签
    
    # MapReduce Speedup

    plt.tight_layout()
    plt.savefig('throughput_comparison.png', dpi=300)
    plt.show()

if __name__ == "__main__":
    plot_latency()
    plot_throughput()

