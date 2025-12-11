
// 操作码
#define OP_ADD  1  // 发送数据，网卡请累加
#define OP_RESULT 2  // 请求当前的累加结果

// 头部格式
typedef struct {
    uint32_t op;         // 操作类型
    uint32_t num_items;  // 后面跟着多少个整数
    uint32_t seq;        // 序列号 (可选，用于调试)
} reduce_hdr_t;

