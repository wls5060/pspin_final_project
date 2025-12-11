#include <stdint.h>
#include "gdriver.h"
#include "packets.h"
#include "../handlers/map_reduce.h"

// 统计预期的总和，用于验证
uint64_t expected_total_sum = 0;

// --- 包填充函数 (Data Generator) ---
uint32_t mapreduce_packet_filler(uint32_t msg_idx, uint32_t pkt_idx, 
                                 uint8_t *pkt_buf, uint32_t max_len, 
                                 uint32_t *l1_len) {
    
    pkt_hdr_t *hdr = (pkt_hdr_t*)pkt_buf;
    uint32_t header_len = sizeof(pkt_hdr_t); // IP+UDP 头

    // 1. 设置 IP 头长度 (PsPIN 仿真需要)
    hdr->ip_hdr.version = 4;
    hdr->ip_hdr.ihl = 5;
    hdr->ip_hdr.source_id = 0x0A000001;
    hdr->ip_hdr.dest_id   = 0x0A000002;
    hdr->udp_hdr.src_port = 1234;
    hdr->udp_hdr.dst_port = 5678;

    // 2. 填充 Payload (全是整数)
    uint32_t *data = (uint32_t *)(pkt_buf + header_len);
    
    // 计算 Payload 能放多少个整数
    // 假设我们限制每个包总长 1024 字节
    uint32_t payload_bytes = max_len - header_len;
    uint32_t num_ints = payload_bytes / sizeof(uint32_t);
    printf("Filling packet %u of message %u with %u integers\n", pkt_idx, msg_idx, num_ints);
    // 填充数据：全部填 1，方便验证
    for (uint32_t i = 0; i < num_ints; i++) {
        data[i] = 1;
        expected_total_sum += 1; // 记录预期值
    }

    uint32_t total_len = header_len + (num_ints * sizeof(uint32_t));
    
    hdr->ip_hdr.length = total_len;
    hdr->udp_hdr.length = sizeof(udp_hdr_t) + (num_ints * sizeof(uint32_t));

    // l1_len 告诉 PsPIN 这个包有多长
    *l1_len = total_len;

    return max_len;
}

int main(int argc, char **argv) {
    int ectx_num;
    
    // 初始化 gdriver
    if (gdriver_init(argc, argv, NULL, &ectx_num) != GDRIVER_OK) {
        return -1;
    }

    printf("Starting MapReduce (Aggregate) Demo...\n");
    
    // 重置预期值
    expected_total_sum = 0;

    // 添加 Context
    gdriver_add_ectx(
        "build/map_reduce",   // 对应 handler 文件名
        "map_phase_hh",       // HH (Header Handler)
        "map_phase_ph",        // PH (Payload Handler)
        "reduce_phase_th",     // TH (Task/Completion Handler)
        mapreduce_packet_filler, // 发包回调
        NULL, 0, NULL, 0
    );

    // 运行仿真
    gdriver_run();
    
    printf("Simulation finished.\n");
    printf("Host Expected Sum: %lu\n", expected_total_sum);
    printf("Please check NIC memory dump or logs for actual result.\n");

    gdriver_fini();
    
    return 0;
}
