#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUF_SIZE 1024
#define MAX_KEYS 1024

// 定义与 PsPIN 一致的协议结构
typedef struct {
    uint32_t op;    // 1=GET, 2=SET
    uint32_t key;
    uint32_t value;
} kvs_req_t;

// 模拟内存存储
uint32_t kv_store[MAX_KEYS];

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    uint8_t buffer[BUF_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // 1. 创建 UDP Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    // 2. 绑定端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("CPU KVS Server running on port %d...\n", PORT);

    // 3. 循环处理请求
    while (1) {
        // 接收数据包 (System Call: recvfrom)
        int n = recvfrom(sockfd, (char *)buffer, BUF_SIZE, MSG_WAITALL, 
                        (struct sockaddr *)&client_addr, &addr_len);
        
        if (n < sizeof(kvs_req_t)) continue;

        kvs_req_t *req = (kvs_req_t *)buffer;

        // 核心逻辑 (与 PsPIN Handler 相同)
        if (req->key < MAX_KEYS) {
            if (req->op == 2) { 
                // SET
                kv_store[req->key] = req->value;
                // CPU 实现通常不需要对 SET 回包，或者回一个简单的 ACK
                // 这里为了简单，我们只处理 GET 的回包，或者保持静默
            } else if (req->op == 1) {
                // GET
                req->value = kv_store[req->key];
                
                // 发送回包 (System Call: sendto)
                // 这里发生了从 用户态 -> 内核态 的拷贝
                sendto(sockfd, (const char *)req, sizeof(kvs_req_t), MSG_CONFIRM, 
                      (const struct sockaddr *)&client_addr, addr_len);
            }
        }
    }
    return 0;
}
