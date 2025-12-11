#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8888
#define BUF_SIZE 1024 // 必须与 PsPIN 实验保持一致

// 全局累加器
volatile uint64_t global_sum = 0;
volatile uint64_t total_packets = 0;

// 捕获 Ctrl+C 打印最终结果
void handle_sigint(int sig) {
    printf("\n\n=== CPU Reducer Stopped ===\n");
    printf("Total Packets Processed: %lu\n", total_packets);
    printf(" Global Sum       : %lu\n", global_sum);
    exit(0);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    uint8_t buffer[BUF_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // 注册中断处理
    signal(SIGINT, handle_sigint);

    // 1. 创建 UDP Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 2. 绑定端口
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("CPU MapReduce Server running on port %d...\n", PORT);
    printf("Press Ctrl+C to stop and see results.\n");

    // 3. 核心循环：接收 -> 遍历 -> 累加
    while (1) {
        // 瓶颈 1: 系统调用与内存拷贝
        int n = recvfrom(sockfd, buffer, BUF_SIZE, 0, 
                        (struct sockaddr *)&client_addr, &addr_len);
        
        if (n > 0) {
            // 瓶颈 2: 内存遍历与计算
            // 假设包里全是 uint32_t 数据 (忽略头部，简单粗暴模拟计算负载)
            uint32_t *data = (uint32_t *)buffer;
            int num_ints = n / sizeof(uint32_t);
            
            uint64_t local_sum = 0;
            
            // Unroll loop hint for compiler optimization (optional)
            for (int i = 0; i < num_ints; i++) {
                local_sum += data[i];
            }
            
            global_sum += local_sum;
            total_packets++;
        }
    }
    return 0;
}
