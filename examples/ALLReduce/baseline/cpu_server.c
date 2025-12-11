#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8888
#define PACKET_SIZE 1024 

// 定义协议 (必须与 Bench 端一致)
#define TYPE_DATA  0
#define TYPE_PROBE 1

typedef struct {
    uint32_t type; 
    uint32_t seq;
    float payload[]; // 变长数组，存放 float 梯度
} ar_header_t;

// --- 全局模型参数 (模拟 Parameter Server) ---
// 假设这是模型的一层参数，驻留在内存中
// 我们使用 volatile 防止编译器优化掉计算过程
#define MODEL_PARAM_SIZE 16384 // 足够大以避免所有数据都在 L1 Cache
volatile float global_model[MODEL_PARAM_SIZE] = {0.0f};

volatile uint64_t total_packets = 0;

// 中断处理：打印统计信息
void handle_sigint(int sig) {
    printf("\n\n=== CPU AllReduce Server Stopped ===\n");
    printf("Total Packets Processed: %lu\n", total_packets);
    printf("Sample Model Value[0]  : %f\n", global_model[0]);
    exit(0);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    uint8_t buffer[PACKET_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // 注册 Ctrl+C
    signal(SIGINT, handle_sigint);

    // 1. 创建 Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 2. 绑定
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("CPU AllReduce Server (Float Vector Add) running on port %d...\n", PORT);

    // 3. 主循环
    while (1) {
        int n = recvfrom(sockfd, buffer, PACKET_SIZE, 0, 
                        (struct sockaddr *)&client_addr, &addr_len);
        
        if (n > sizeof(ar_header_t)) {
            ar_header_t *hdr = (ar_header_t *)buffer;
            
            // --- 核心负载：浮点向量加法 (Float Vector Addition) ---
            
            // 1. 计算 Payload 中有多少个 float
            int data_len = n - sizeof(ar_header_t);
            int num_floats = data_len / sizeof(float);
            
            // 2. 模拟更新模型参数
            // 真实的 AllReduce 会根据包里的 offset 更新模型的特定部分
            // 这里我们简单循环更新 global_model 的前 N 个值
            // 这模拟了 CPU 从内存读取参数 -> 加法 -> 写回内存的过程
            for (int i = 0; i < num_floats; i++) {
                // 使用取模防止数组越界，模拟对大模型的更新
                int model_idx = i % MODEL_PARAM_SIZE;
                global_model[model_idx] += hdr->payload[i];
            }
            
            total_packets++;

            // --- 延迟探测回包 ---
            if (hdr->type == TYPE_PROBE) {
                // 原样发回头部作为 ACK
                // 注意：只发回头部，因为 AllReduce 通常只返回确认或最终结果，不回传大梯度
                sendto(sockfd, buffer, sizeof(ar_header_t), 0, 
                      (struct sockaddr *)&client_addr, addr_len);
            }
        }
    }
    return 0;
}
