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
// Handler that implements reduce in scratchpad for int32

// 4 uint32_t -> locks
// 4 uint32_t -> L1 addresses
// 1 uint32_t -> msg_count (now 0x0200)

typedef struct {
    uint32_t msg_id;
    uint32_t elem_count;
} intsum_hdr_t;

#define MAX_MSG 1024

typedef struct {
    uint32_t total_elems;
    uint32_t processed;
    int64_t  sum;
} intsum_state_t;



__handler__ void int_sum_hh(handler_args_t *args) {
    task_t* task = args->task;
    uint8_t *pkt_pld_ptr;
    uint32_t pkt_pld_len;
    GET_IP_UDP_PLD(task->pkt_mem, pkt_pld_ptr, pkt_pld_len);
    intsum_hdr_t *hdr = (intsum_hdr_t *)pkt_pld_ptr;
    uint32_t total = hdr->elem_count;
    intsum_state_t *st = (intsum_state_t *)task->handler_mem;

    if (st->processed == 0) {
        st->total_elems = total;
        st->sum         = 0;
    }

}

__handler__ void int_sum_ph(handler_args_t *args)
{
    task_t* task = args->task;
    uint8_t *pkt_pld_ptr;
    uint32_t pkt_pld_len;
    GET_IP_UDP_PLD(task->pkt_mem, pkt_pld_ptr, pkt_pld_len);

    intsum_state_t *st = (intsum_state_t *)task->handler_mem;
    

    int32_t *data = (int32_t *)pkt_pld_ptr;
    uint32_t n    = pkt_pld_len / sizeof(int32_t);

    int64_t local_sum = st->sum;


    for (uint32_t i = 0; i < n; i++) {
        amo_add(&(local_sum), data[i]);
    }
    amo_add(&(st->processed), n);
}
__handler__ void int_sum_th(handler_args_t *args)
{
    task_t* task = args->task;
    uint8_t *pkt_pld_ptr;
    uint32_t pkt_pld_len;
    GET_IP_UDP_PLD(task->pkt_mem, pkt_pld_ptr, pkt_pld_len);
    intsum_hdr_t *hdr = (intsum_hdr_t *)pkt_pld_ptr;
    uint32_t id = hdr->msg_id;

    intsum_state_t *st = (intsum_state_t *)task->handler_mem;

    if (st->processed == st->total_elems) {
        typedef struct {
            uint32_t msg_id;
            int64_t  sum;
        } intsum_result_t;

        intsum_result_t res;
        res.msg_id = id;
        res.sum    = st->sum;
        uint64_t host_address = task->host_mem_high;
        host_address = (host_address << 32) | (task->host_mem_low);
        spin_host_write(host_address, (uint64_t)&res, false);
    }

}


void init_handlers(handler_fn *hh, handler_fn *ph, handler_fn *th, void **handler_mem_ptr)
{
    volatile handler_fn handlers[] = {int_sum_hh, int_sum_ph, int_sum_th};
    *hh = handlers[0];
    *ph = handlers[1];
    *th = handlers[2];
}
