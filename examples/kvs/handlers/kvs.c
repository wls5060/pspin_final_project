// Copyright 2020 ETH Zurich
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HOST
#include <handler.h>
#else
#include <handler_profiler.h>
#endif

#include <packets.h>

#include <spin_conf.h>
#include "kvs.h"
#define NUM_CLUSTERS 4
#define STRIDE 1
#define OFFSET 0
#define NUM_INT_OP 0

#define ZEROS 2048

#define MAX_KEYS 1024
__attribute__((section(".l2_data"))) volatile uint32_t kv_store[MAX_KEYS];
//uint32_t kv_store[MAX_KEYS];
static inline void swap_headers(pkt_hdr_t *hdr) {

    uint32_t temp_ip = hdr->ip_hdr.source_id;
    hdr->ip_hdr.source_id = hdr->ip_hdr.dest_id;
    hdr->ip_hdr.dest_id = temp_ip;

    uint16_t temp_port = hdr->udp_hdr.src_port;
    hdr->udp_hdr.src_port = hdr->udp_hdr.dst_port;
    hdr->udp_hdr.dst_port = temp_port;
}

__handler__ void kvs_header_handler(handler_args_t *args) {
    task_t* task = args->task;
    
    pkt_hdr_t *hdr = (pkt_hdr_t*)task->pkt_mem;

    uint8_t *payload_ptr = (uint8_t*)task->pkt_mem + sizeof(pkt_hdr_t);
    
    kvs_header *req = (kvs_header *)payload_ptr;
    if (req->key >= MAX_KEYS) {
        return; 
    }

    if (req->op == 2) { //set
        kv_store[req->key] = req->value;
        
    } else if (req->op == 1) {//get
        uint32_t val = kv_store[req->key];
        
        req->value = val;
        swap_headers(hdr);
        
    }
}


void init_handlers(handler_fn *hh, handler_fn *ph, handler_fn *th, void **handler_mem_ptr)
{
    volatile handler_fn handlers[] = {kvs_header_handler, NULL, NULL};
    *hh = handlers[0];
    *ph = handlers[1];
    *th = handlers[2];
}
