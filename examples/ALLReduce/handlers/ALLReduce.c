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
#define NUM_CLUSTERS 4
#define STRIDE 1
#define OFFSET 0
#define NUM_INT_OP 0

#define ZEROS 2048

#define MODEL_SIZE_FLOATS 4096 
__attribute__((section(".l2_data"))) volatile float model_gradients[MODEL_SIZE_FLOATS];

// --- 核心 Payload Handler ---
__handler__ void allreduce_ph(handler_args_t *args)
{
    task_t* task = args->task;
    
    // 1. 解析 Payload
    uint8_t *pkt_pld_ptr;
    uint32_t pkt_pld_len;
    GET_IP_UDP_PLD(task->pkt_mem, pkt_pld_ptr, pkt_pld_len);

    // 强转为 float 指针
    float *incoming_grads = (float*) pkt_pld_ptr;
    
    // 计算这个包里有多少个 float
    uint32_t num_floats = pkt_pld_len / sizeof(float);

    // 2. 向量加法 (Vector Addition)
    
    for(uint32_t i = 0; i < num_floats; i++) {
        if (i < MODEL_SIZE_FLOATS) {
            model_gradients[i] += incoming_grads[i];
        }
    }
}

// --- Completion Handler ---
// 模拟一轮训练结束，通知 Host 读取
__handler__ void allreduce_th(handler_args_t *args)
{
    task_t* task = args->task;
    
    // 这里我们不把整个模型写回 Host (太大)，而是写回一个完成信号
    // 或者写回模型的第一个值用于验证
    uint64_t host_address = task->host_mem_high;
    host_address = (host_address << 32) | (task->host_mem_low);

    float first_val = model_gradients[0];
    
    // spin_host_write 传输的是 64位 整数，我们需要把 float 强转位表示传回去
    // 或者简单的传回 1 表示 Done
    for(int i=0; i<MODEL_SIZE_FLOATS; i++) {
        spin_host_write(host_address+(i * sizeof(float)), model_gradients[i], false);
    }
}

void init_handlers(handler_fn *hh, handler_fn *ph, handler_fn *th, void **handler_mem_ptr)
{
    volatile handler_fn handlers[] = {NULL, allreduce_ph, allreduce_th};
    *hh = handlers[0];
    *ph = handlers[1];
    *th = handlers[2];
}
