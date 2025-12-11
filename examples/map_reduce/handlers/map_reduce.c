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
#include "map_reduce.h"
#define NUM_CLUSTERS 4
#define STRIDE 1
#define OFFSET 0
#define NUM_INT_OP 0

#define ZEROS 2048

__handler__ void map_phase_hh(handler_args_t *args) {

    task_t* task = args->task;
    uint32_t *scratchpad = (uint32_t*) task->scratchpad;
    for(uint8_t i = 0; i < NUM_CLUSTERS; i++){ 
        *(uint32_t*)task->scratchpad[i] = 0;
    }
}
// --- MapReduce 逻辑 ---
// [Map Phase] & [Partial Reduce]
// Payload Handler: 并行处理数据包
__handler__ void map_phase_ph(handler_args_t *args)
{
    task_t* task = args->task;
    
    uint8_t *pkt_pld_ptr;
    uint32_t pkt_pld_len;
    GET_IP_UDP_PLD(task->pkt_mem, pkt_pld_ptr, pkt_pld_len);

    uint32_t *data_array = (uint32_t*) pkt_pld_ptr;
    uint32_t num_ints = pkt_pld_len / sizeof(uint32_t);

    // === Step 1: Map ===
    // 遍历数据包内的所有整数，计算局部和
    // 这相当于 MapReduce 中的 "Map"：将数据映射为中间结果
    uint32_t local_sum = 0;
    for(uint32_t i = 0; i < num_ints; i++)
    {
        local_sum += data_array[i];
    }
    // printf("Local sum computed: %u\n", local_sum);
    // === Step 2: Local Reduce ===
    // 将当前核心计算出的局部和，原子累加到所属 Cluster 的 Scratchpad 中
    // scratchpad 是 L1 内存，速度极快
    volatile uint32_t *scratchpad = (uint32_t *)task->scratchpad;
    uint32_t my_cluster_id = args->cluster_id;
    
    // 使用原子加法防止并发冲突
    amo_add((uint32_t*)scratchpad[my_cluster_id], local_sum);
    //printf("Scratchpad[%u] after atomic add: %u, local_sum: %u\n", my_cluster_id, *(uint32_t*)scratchpad[my_cluster_id], local_sum);
}

// [ Reduce]
// Completion/Task Handler: 当一个 Message (所有包) 处理完后触发
__handler__ void reduce_phase_th(handler_args_t *args)
{

    task_t* task = args->task;
    uint32_t *scratchpad = (uint32_t*) task->scratchpad;
    
    // === Step 3: Global Reduce ===
    // 汇总所有 Cluster 的结果
    uint32_t final_result = 0; 
    for(uint8_t i = 0; i < NUM_CLUSTERS; i++){ 
        final_result += *(uint32_t*)scratchpad[i];
    }

    // === Step 4: Output ===
    // 将最终的一个数字写回 Host 内存
    // Host CPU 只需要读这一个地址，不需要遍历所有数据包
    uint64_t host_address = task->host_mem_high;
    host_address = (host_address << 32) | (task->host_mem_low);

    spin_host_write(host_address, (uint64_t) final_result, false);
    // printf("Final MapReduce Result: %u\n", final_result);
}


void init_handlers(handler_fn *hh, handler_fn *ph, handler_fn *th, void **handler_mem_ptr)
{
    volatile handler_fn handlers[] = {map_phase_hh, map_phase_ph, reduce_phase_th};
    *hh = handlers[0];
    *ph = handlers[1];
    *th = handlers[2];
}
