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
#define PACKET_SIZE 1024 
// 采样率
#define SAMPLE_RATE 1000 

// 协议头简单定义
typedef struct {
    uint32_t type; // 0=DATA, 1=PROBE
    uint32_t seq;
    float payload[]; // 变更为 float
} ar_header_t;

// 时间与忙等待辅助函数
static inline long long current_timestamp_ns() {
    struct timespec te;
    clock_gettime(CLOCK_MONOTONIC, &te);
    return (long long)te.tv_sec * 1000000000LL + te.tv_nsec;
}
static inline void spin_wait_ns(long long delay_ns) {
    if (delay_ns <= 0) return;
    long long target = current_timestamp_ns() + delay_ns;
    while (current_timestamp_ns() < target) { __asm__ volatile ("nop"); }
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in server_addr;
    int num_packets = 500000;
    long long delay_ns = 0;

    int opt;
    while ((opt = getopt(argc, argv, "n:d:")) != -1) {
        switch (opt) {
            case 'n': num_packets = atoi(optarg); break;
            case 'd': delay_ns = atoll(optarg); break;
        }
    }

    uint8_t send_buf[PACKET_SIZE];
    uint8_t recv_buf[PACKET_SIZE];

    // 初始化 Payload 为 0.5
    ar_header_t *hdr = (ar_header_t *)send_buf;
    int data_len = PACKET_SIZE - sizeof(ar_header_t);
    int num_floats = data_len / sizeof(float);
    for(int i=0; i<num_floats; i++) hdr->payload[i] = 0.5f;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) exit(1);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // 设置超时
    struct timeval tv;
    tv.tv_sec = 1; tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    printf("Starting AllReduce Benchmark (Float Vector Add)...\n");
    printf("Delay: %lld ns, Floats per packet: %d\n", delay_ns, num_floats);

    long long start_time = current_timestamp_ns();
    long long total_latency_ns = 0;
    int latency_samples = 0;

    for (int i = 0; i < num_packets; i++) {
        int is_probe = (i % SAMPLE_RATE == 0);
        hdr->type = is_probe ? 1 : 0;
        
        long long req_start = 0;
        if (is_probe) req_start = current_timestamp_ns();

        sendto(sockfd, send_buf, PACKET_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

        if (is_probe) {
            if (recvfrom(sockfd, recv_buf, PACKET_SIZE, 0, NULL, NULL) > 0) {
                total_latency_ns += (current_timestamp_ns() - req_start);
                latency_samples++;
            }
        }
        if (delay_ns > 0) spin_wait_ns(delay_ns);
    }

    double total_time_sec = (double)(current_timestamp_ns() - start_time) / 1000000000.0;
    double throughput_gbps = ((double)num_packets * PACKET_SIZE * 8.0) / total_time_sec / 1e9;
    double avg_latency = latency_samples > 0 ? (double)total_latency_ns / latency_samples : 0;

    printf("\n====== AllReduce CPU Results ======\n");
    printf("Throughput : %.3f Gbit/s\n", throughput_gbps);
    printf("Avg Latency: %.3f ns\n", avg_latency);
    printf("===================================\n");

    close(sockfd);
    return 0;
}
