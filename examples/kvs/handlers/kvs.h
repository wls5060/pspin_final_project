// 定义操作码
#define OP_GET 1
#define OP_SET 2

// 自定义应用层头部
typedef struct {
    uint32_t op;    // 1=GET, 2=SET
    uint32_t key;
    uint32_t value;
} kvs_header;