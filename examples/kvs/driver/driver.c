#include <stdint.h>
#include "gdriver.h"
#include "packets.h"
#include "../handlers/kvs.h"

uint32_t kvs_packet_filler(uint32_t msg_idx, uint32_t pkt_idx, 
                           uint8_t *pkt_buf, uint32_t max_len, 
                           uint32_t *l1_len) {
    
    // 1. 映射头部结构
    //printf("Filling packet for msg_idx=%u, pkt_idx=%u\n", msg_idx, pkt_idx);
    pkt_hdr_t *hdr = (pkt_hdr_t*)pkt_buf;
    uint32_t header_len = sizeof(pkt_hdr_t); // 28 bytes
    // IP
    hdr->ip_hdr.version = 4;
    hdr->ip_hdr.ihl = 5; // 5 * 4 = 20 bytes
    hdr->ip_hdr.source_id = 0x0A000001; // 10.0.0.1 (Host)
    hdr->ip_hdr.dest_id   = 0x0A000002; // 10.0.0.2 (NIC/Target)
    hdr->ip_hdr.ttl = 64;
    hdr->ip_hdr.protocol = 17; // UDP

    // UDP
    hdr->udp_hdr.src_port = 1234;
    hdr->udp_hdr.dst_port = 5678;
    hdr->udp_hdr.checksum = 0;

    // KVS Payload
    kvs_header *req = (kvs_header *)(pkt_buf + header_len);
    

    if (msg_idx % 2 == 0) {
        // SET Key=10, Val=8888
        req->op = 2;
        req->key = rand()%10;
        req->value = rand()%10000;
    } else {
        // GET Key=10 (Expect 8888)
        req->op = 1;
        req->key = rand()%10;
        req->value = 0;
    }

    // 5. 计算长度
    uint32_t payload_len = sizeof(kvs_header);
    uint32_t total_len = header_len + payload_len;

    // 填写头部长度字段
    hdr->ip_hdr.length = total_len;
    hdr->udp_hdr.length = sizeof(udp_hdr_t) + payload_len;

    *l1_len = total_len;
    return total_len;
}

int main(int argc, char **argv) {
    int ectx_num;
    
    // 初始化 gdriver
    if (gdriver_init(argc, argv, NULL, &ectx_num) != GDRIVER_OK) {
        return -1;
    }

    printf("Starting KVS Demo (Matched packets.h)...\n");

    // 添加 Context
    gdriver_add_ectx(
        "build/kvs",        // Handler 编译后的文件名 (不带 .bin/.o 取决于你的环境)
        "kvs_header_handler", // Handler 函数名
        NULL, NULL,           // 无 PH/TH
        kvs_packet_filler,    // 发包回调
        NULL, 0,              // 无 L2 image
        NULL, 0
    );
    // 运行
    gdriver_run();
    gdriver_fini();
    
    return 0;
}
