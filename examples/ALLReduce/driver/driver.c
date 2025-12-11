#include <stdint.h>
#include "gdriver.h"
#include "packets.h"

// --- 包填充回调 ---
uint32_t allreduce_packet_filler(uint32_t msg_idx, uint32_t pkt_idx, 
                                 uint8_t *pkt_buf, uint32_t max_len, 
                                 uint32_t *l1_len) {
    
    pkt_hdr_t *hdr = (pkt_hdr_t*)pkt_buf;
    uint32_t header_len = sizeof(pkt_hdr_t);

    // IP/UDP Setup
    hdr->ip_hdr.version = 4;
    hdr->ip_hdr.ihl = 5;
    hdr->ip_hdr.source_id = 0x0A000001;
    hdr->ip_hdr.dest_id   = 0x0A000002;
    hdr->udp_hdr.src_port = 1234;
    hdr->udp_hdr.dst_port = 5678;

    // --- 填充浮点 Payload ---
    float *grads = (float *)(pkt_buf + header_len);
    
    // 计算能放多少 float
    uint32_t payload_bytes = 1024 - header_len;
    uint32_t num_floats = payload_bytes / sizeof(float);

    // 填充数据：每个梯度都是 0.5
    for (uint32_t i = 0; i < num_floats; i++) {
        grads[i] = 0.5f; 
    }

    uint32_t total_len = header_len + (num_floats * sizeof(float));
    
    hdr->ip_hdr.length = total_len;
    hdr->udp_hdr.length = sizeof(udp_hdr_t) + (num_floats * sizeof(float));

    *l1_len = total_len;
    return total_len;
}

int main(int argc, char **argv) {
    int ectx_num;
    if (gdriver_init(argc, argv, NULL, &ectx_num) != GDRIVER_OK) return -1;

    printf("Starting Distributed Training (AllReduce) Demo...\n");

    gdriver_add_ectx(
        "build/ALLReduce",   
        NULL,                  
        "allreduce_ph",        
        "allreduce_th",     
        allreduce_packet_filler, 
        NULL, 0, NULL, 0
    );

    gdriver_run();
    gdriver_fini();
    
    return 0;
}
