# WS63 RPT 分包重组修改指南

## 背景

BS21 单包发送 vitals JSON 时，WS63 仍然只收到 248 字节：

```text
[SLE] report notify conn=0 len=248 json:{"deviceId":"watch_01",...,"fasting
[REPORT] report incomplete json device=0 len=248
```

说明当前 SLE 链路不能保证单包承载 276 字节完整 JSON。BS21 将改为应用层分包发送：

```text
RPT:<msgId>:<seq>:<total>:<chunk>
```

WS63 必须先重组 `RPT:` 分片，收齐后再把完整 JSON 交给原来的 `vitals_report_handle()` JSON 处理逻辑。

## 分包协议

### 格式

```text
RPT:<msgId>:<seq>:<total>:<chunk>
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `msgId` | BS21 每发送一份 report 递增一次 |
| `seq` | 当前分片序号，从 0 开始 |
| `total` | 总分片数量 |
| `chunk` | 原始 JSON 的一段文本 |

示例：

```text
RPT:1:0:2:{"deviceId":"watch_01","type":"vitals",...
RPT:1:1:2:...,"fastingBloodGlucose":"5.2 mmol/L"}}
```

## WS63 当前入口

文件：

```text
D:\HiSpark\fbb_ws63\src\application\samples\peripheral\st7796s_ft6336u\sle_watch_server.c
```

当前 SLE notify/indicate 收到数据后会调用：

```c
vitals_report_handle(device_index, data, len);
```

文件：

```text
D:\HiSpark\fbb_ws63\src\application\samples\peripheral\st7796s_ft6336u\vitals_report.c
```

当前 `vitals_report_handle()` 只识别 `{...}` JSON。修改后应先识别 `RPT:`，分片收齐后再走原 JSON 处理。

## 推荐修改方案

### 1. 增加重组状态

文件：

```text
vitals_report.c
```

在宏定义附近增加：

```c
#define VITALS_RPT_PREFIX "RPT:"
#define VITALS_RPT_MAX_FRAGMENTS 8
#define VITALS_RPT_REASSEMBLE_LEN VITALS_REPORT_MAX_LEN
```

增加重组结构：

```c
typedef struct {
    uint8_t active;
    uint16_t msg_id;
    uint8_t total;
    uint8_t received_mask;
    uint16_t len;
    char payload[VITALS_RPT_REASSEMBLE_LEN];
} vitals_rpt_reasm_t;

static vitals_rpt_reasm_t g_rpt_reasm[WATCH_MODEL_MAX_DEVICES];
```

说明：

- `received_mask` 用位标记已收到的分片，`total` 最大建议不超过 8。
- 当前 JSON 长度约 276，BS21 每片 180 字节时通常只需要 2 片。
- 如果以后 report 超过 8 片，应改成 `uint32_t received_mask` 或数组。

### 2. 拆分原 JSON 处理逻辑

把当前 `vitals_report_handle()` 中处理 `{...}` 的逻辑移动到内部函数：

```c
static void vitals_report_handle_json(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    /* 原 vitals_report_handle() 里从复制 payload 到 report ready/incomplete 的逻辑 */
}
```

然后新的 `vitals_report_handle()` 变成：

```c
void vitals_report_handle(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        return;
    }

    if ((len >= 4) && (memcmp(data, VITALS_RPT_PREFIX, 4) == 0)) {
        vitals_report_handle_fragment(device_index, data, len);
        return;
    }

    vitals_report_handle_json(device_index, data, len);
}
```

### 3. 增加分片解析工具

建议增加一个简单的无动态内存解析函数：

```c
static bool parse_u16_field(const char **cursor, uint16_t *out)
{
    uint32_t value = 0;
    const char *p;

    if ((cursor == NULL) || (*cursor == NULL) || (out == NULL)) {
        return false;
    }

    p = *cursor;
    if ((*p < '0') || (*p > '9')) {
        return false;
    }

    while ((*p >= '0') && (*p <= '9')) {
        value = value * 10 + (uint32_t)(*p - '0');
        if (value > 65535) {
            return false;
        }
        p++;
    }

    if (*p != ':') {
        return false;
    }
    p++;

    *out = (uint16_t)value;
    *cursor = p;
    return true;
}
```

### 4. 增加分片处理函数

```c
static void vitals_report_handle_fragment(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    char packet[VITALS_REPORT_MAX_LEN];
    const char *p;
    const char *chunk;
    uint16_t msg_id;
    uint16_t seq;
    uint16_t total;
    uint16_t chunk_len;
    uint8_t expected_mask;
    vitals_rpt_reasm_t *reasm;

    if ((data == NULL) || (len == 0) || (len >= sizeof(packet))) {
        return;
    }

    if (device_index >= WATCH_MODEL_MAX_DEVICES) {
        device_index = 0;
    }

    if (memcpy_s(packet, sizeof(packet), data, len) != EOK) {
        return;
    }
    packet[len] = '\0';

    p = packet + 4; /* skip RPT: */
    if (!parse_u16_field(&p, &msg_id) ||
        !parse_u16_field(&p, &seq) ||
        !parse_u16_field(&p, &total)) {
        osal_printk("[REPORT] bad fragment header len=%u\r\n", (unsigned int)len);
        return;
    }

    if ((total == 0) || (total > VITALS_RPT_MAX_FRAGMENTS) || (seq >= total)) {
        osal_printk("[REPORT] bad fragment index msg=%u seq=%u total=%u\r\n",
                    (unsigned int)msg_id, (unsigned int)seq, (unsigned int)total);
        return;
    }

    chunk = p;
    chunk_len = (uint16_t)strlen(chunk);
    reasm = &g_rpt_reasm[device_index];

    if ((reasm->active == 0) || (reasm->msg_id != msg_id)) {
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        reasm->active = 1;
        reasm->msg_id = msg_id;
        reasm->total = (uint8_t)total;
    }

    if (reasm->total != total) {
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        return;
    }

    if ((reasm->len + chunk_len) >= sizeof(reasm->payload)) {
        osal_printk("[REPORT] fragment overflow msg=%u len=%u chunk=%u\r\n",
                    (unsigned int)msg_id, (unsigned int)reasm->len, (unsigned int)chunk_len);
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        return;
    }

    /*
     * BS21 sends fragments in order. If out-of-order support is needed later,
     * store each seq in its own slot instead of appending.
     */
    if (seq != (uint16_t)__builtin_popcount((unsigned int)reasm->received_mask)) {
        osal_printk("[REPORT] fragment out of order msg=%u seq=%u mask=0x%x\r\n",
                    (unsigned int)msg_id, (unsigned int)seq, (unsigned int)reasm->received_mask);
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        return;
    }

    if (memcpy_s(&reasm->payload[reasm->len], sizeof(reasm->payload) - reasm->len, chunk, chunk_len) != EOK) {
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
        return;
    }
    reasm->len += chunk_len;
    reasm->payload[reasm->len] = '\0';
    reasm->received_mask |= (uint8_t)(1U << seq);

    osal_printk("[REPORT] rpt fragment device=%u msg=%u seq=%u total=%u chunk=%u len=%u mask=0x%x\r\n",
                (unsigned int)device_index, (unsigned int)msg_id,
                (unsigned int)seq, (unsigned int)total,
                (unsigned int)chunk_len, (unsigned int)reasm->len,
                (unsigned int)reasm->received_mask);

    expected_mask = (uint8_t)((1U << total) - 1U);
    if (reasm->received_mask == expected_mask) {
        uint16_t full_len = reasm->len;
        char full_json[VITALS_REPORT_MAX_LEN];

        if (memcpy_s(full_json, sizeof(full_json), reasm->payload, full_len + 1) == EOK) {
            osal_printk("[REPORT] rpt complete device=%u msg=%u len=%u\r\n",
                        (unsigned int)device_index, (unsigned int)msg_id, (unsigned int)full_len);
            vitals_report_handle_json(device_index, (const uint8_t *)full_json, full_len);
        }
        (void)memset_s(reasm, sizeof(*reasm), 0, sizeof(*reasm));
    }
}
```

如果编译器不支持 `__builtin_popcount`，替换为简单函数：

```c
static uint8_t count_bits_u8(uint8_t value)
{
    uint8_t count = 0;
    while (value != 0) {
        count += value & 1U;
        value >>= 1;
    }
    return count;
}
```

然后把：

```c
__builtin_popcount((unsigned int)reasm->received_mask)
```

替换为：

```c
count_bits_u8(reasm->received_mask)
```

## 推荐的更稳实现

上面的实现假设 BS21 分片按顺序到达。SLE notify 通常会按发送顺序到达，但如果要更稳，建议把重组结构改成每个分片独立保存：

```c
uint8_t chunk_len[VITALS_RPT_MAX_FRAGMENTS];
char chunks[VITALS_RPT_MAX_FRAGMENTS][220];
```

收齐后按 `seq = 0..total-1` 拼接，这样可以支持乱序。

当前阶段为了尽快验证 248 截断问题，顺序追加方案足够。

## sle_watch_server.c 是否要改

如果 `sle_watch_server.c` 已经无条件调用：

```c
vitals_report_handle(device_index, data, len);
```

则不用改。

如果只在 `payload[0] == '{'` 时才调用 `vitals_report_handle()`，必须改成无条件调用，否则 `RPT:` 分片不会进入重组逻辑。

推荐：

```c
device_index = sle_watch_find_device_by_conn_id(conn_id);
vitals_report_handle(device_index >= 0 ? (uint8_t)device_index : VITALS_REPORT_DEVICE_UNKNOWN, data, len);
```

## 验收日志

BS21 应看到：

```text
[sle 1vn server] report fragment msg=1 seq=0/2 chunk=200 packet=214 ret=0x0
[sle 1vn server] report fragment msg=1 seq=1/2 chunk=76 packet=90 ret=0x0
[sensor task] report uploaded len:276
```

WS63 应看到：

```text
[REPORT] rpt fragment device=0 msg=1 seq=0 total=2 chunk=200 len=200 mask=0x1
[REPORT] rpt fragment device=0 msg=1 seq=1 total=2 chunk=76 len=276 mask=0x3
[REPORT] rpt complete device=0 msg=1 len=276
[REPORT] report ready device=0 len=276
```

## 注意事项

1. `RPT:` 分片本身不是 JSON，不能直接判断 `type == vitals`。
2. 只有重组完成后的完整 JSON 才能进入原 `vitals_report_is_complete()`。
3. BS21 每片之间建议延迟 20 ms，避免连续 notify 过快。
4. 如果 report 后续超过 8 片，WS63 要扩大 `VITALS_RPT_MAX_FRAGMENTS` 或改 `received_mask` 类型。
5. 如果姓名是中文，分片可能切在 UTF-8 多字节字符中间。这不影响最终拼接后的 JSON，但 WS63 不要对单片 `chunk` 做 UTF-8 文本展示或 JSON 解析。
