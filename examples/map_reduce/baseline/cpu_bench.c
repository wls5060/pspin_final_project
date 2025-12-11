#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <getopt.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8888
#define PACKET_SIZE 1024 // 保持 1024 字节

// 获取时间戳
static inline long long current_timestamp_ns() {
    struct timespec te;
    clock_gettime(CLOCK_MONOTONIC, &te);
    return (long long)te.tv_sec * 1000000000LL + te.tv_nsec;
}

// 忙等待 (用于控制发包速率，对应 PsPIN 的 -d 参数)
static inline void spin_wait_ns(long long delay_ns) {
    if (delay_ns <= 0) return;
    long long target = current_timestamp_ns() + delay_ns;
    while (current_timestamp_ns() < target) { __asm__ volatile ("nop"); }
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    // 默认参数
    int num_packets = 1000000; // 发送 100万个包
    long long delay_ns = 0;    // 默认全速发送

    int opt;
    while ((opt = getopt(argc, argv, "n:d:")) != -1) {
        switch (opt) {
            case 'n': num_packets = atoi(optarg); break;
            case 'd': delay_ns = atoll(optarg); break;
        }
    }

    // 准备数据：填满 1，方便 Server 验证
    uint8_t send_buf[PACKET_SIZE];
    uint32_t *data = (uint32_t *)send_buf;
    int num_ints = PACKET_SIZE / sizeof(uint32_t);
    for(int i=0; i<num_ints; i++) data[i] = 1;

    // 创建 Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    printf("Starting CPU MapReduce Benchmark...\n");
    printf("Packets: %d, Size: %d B, Delay: %lld ns\n", num_packets, PACKET_SIZE, delay_ns);

    long long start_time = current_timestamp_ns();

    // 发送循环
    for (int i = 0; i < num_packets; i++) {
        sendto(sockfd, send_buf, PACKET_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        
        if (delay_ns > 0) spin_wait_ns(delay_ns);
    }

    long long end_time = current_timestamp_ns();

    // 统计结果
    double total_time_sec = (double)(end_time - start_time) / 1000000000.0;
    double total_bits = (double)num_packets * PACKET_SIZE * 8.0;
    double throughput_gbps = total_bits / total_time_sec / 1000000000.0;
    double pps = num_packets / total_time_sec;

    printf("\n====== CPU Baseline Results (MapReduce) ======\n");
    printf("Total Packets : %d\n", num_packets);
    printf("Total Time    : %.4f s\n", total_time_sec);
    printf("--------------------------------------------\n");
    printf("Throughput    : %.3f Gbit/s\n", throughput_gbps);
    printf("Packet Rate   : %.2f Mpps\n", pps / 1000000.0);
    printf("============================================\n");

    close(sockfd);
    return 0;
}
