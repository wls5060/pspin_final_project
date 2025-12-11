#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <getopt.h> // 用于解析命令行参数

#define SERVER_IP "127.0.0.1"
#define PORT 8888
#define DEFAULT_NUM_REQUESTS 100000
#define PACKET_SIZE 1024 

// 定义协议
typedef struct {
    uint32_t op;
    uint32_t key;
    uint32_t value;
} kvs_req_t;

// 获取当前时间 (纳秒)
static inline long long current_timestamp_ns() {
    struct timespec te;
    clock_gettime(CLOCK_MONOTONIC, &te);
    return (long long)te.tv_sec * 1000000000LL + te.tv_nsec;
}

// 忙等待函数 (Busy Wait)
// 只有忙等待才能实现 < 1us (1000ns) 的延迟精度
// 警告：这会占用 100% CPU，但这正是为了模拟精确发包间隔所必需的
static inline void spin_wait_ns(long long delay_ns) {
    if (delay_ns <= 0) return;
    long long start = current_timestamp_ns();
    long long target = start + delay_ns;
    
    while (current_timestamp_ns() < target) {
        // 插入空指令，防止编译器过度优化循环
        __asm__ volatile ("nop");
    }
}

void print_usage(char *prog_name) {
    printf("Usage: %s [-n num_requests] [-d packet_delay_ns]\n", prog_name);
    printf("  -n  Number of requests (default: 100000)\n");
    printf("  -d  Delay between packets in ns (default: 0, PsPIN default is 20)\n");
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    
    // 默认参数
    int num_requests = DEFAULT_NUM_REQUESTS;
    long long packet_delay_ns = 0; // 默认全力发送

    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "n:d:h")) != -1) {
        switch (opt) {
            case 'n':
                num_requests = atoi(optarg);
                break;
            case 'd':
                packet_delay_ns = atoll(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // 缓冲区准备
    uint8_t send_buf[PACKET_SIZE];
    uint8_t recv_buf[PACKET_SIZE];

    // 初始化 Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // 设置超时
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    printf("Starting CPU Benchmark with Delay Control...\n");
    printf("Packet Size : %d Bytes\n", PACKET_SIZE);
    printf("Requests    : %d\n", num_requests);
    printf("Packet Delay: %lld ns\n", packet_delay_ns);

    long long start_time = current_timestamp_ns();
    long long total_latency_ns = 0;
    int success_count = 0;

    for (int i = 0; i < num_requests; i++) {
        // 1. 构造数据
        memset(send_buf, 0, PACKET_SIZE);
        kvs_req_t *req = (kvs_req_t *)send_buf;
        req->op = 1; // GET
        req->key = i % 1024;
        req->value = 0;

        long long req_start = current_timestamp_ns();

        // 2. 发送
        sendto(sockfd, send_buf, PACKET_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

        // 3. 接收
        int n = recvfrom(sockfd, recv_buf, PACKET_SIZE, 0, NULL, NULL);

        long long req_end = current_timestamp_ns();

        if (n > 0) {
            total_latency_ns += (req_end - req_start);
            success_count++;
        }

        // --- 4. 插入 PsPIN 风格的延迟 (Inter-Arrival Time Control) ---
        // 在完成当前包的处理后，等待指定的纳秒数再发下一个包
        if (packet_delay_ns > 0) {
            spin_wait_ns(packet_delay_ns);
        }
    }

    long long end_time = current_timestamp_ns();

    // --- 统计与输出 ---
    double total_time_sec = (double)(end_time - start_time) / 1000000000.0;
    
    // 吞吐量 (Gbit/s)
    double total_bits = (double)success_count * PACKET_SIZE * 8.0;
    double throughput_gbps = total_bits / total_time_sec / 1000000000.0;

    // 平均延迟 (ns)
    double avg_latency_ns = (double)total_latency_ns / success_count;

    printf("\n====== CPU Baseline Results (Delay: %lld ns) ======\n", packet_delay_ns);
    printf("Packets Processed : %d\n", success_count);
    printf("Total Time        : %.4f s\n", total_time_sec);
    printf("---------------------------------------------\n");
    printf("Throughput        : %.3f Gbit/s\n", throughput_gbps);
    printf("Avg Packet Latency: %.3f ns\n", avg_latency_ns);
    printf("=============================================\n");

    close(sockfd);
    return 0;
}
