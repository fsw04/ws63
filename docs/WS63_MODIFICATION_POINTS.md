# WS63 修改点：接收 BS21 RPT 分包报告

## 背景

BS21 完整 vitals JSON 长度约 276 字节，但 WS63 当前 SLE notify 回调只收到 248 字节。继续依赖单包 MTU/data length 协商不稳定，因此 BS21 改为应用层分包发送：

```text
RPT:<msgId>:<seq>:<total>:<chunk>
```

WS63 必须先把 `RPT:` 分片重组成完整 JSON，然后再走原来的 `vitals_report_handle()` 判断、缓存和 MQTT 上传逻辑。

## 涉及文件

WS63 工程路径：

```text
D:\HiSpark\fbb_ws63\src\application\samples\peripheral\st7796s_ft6336u
```

主要修改：

```text
vitals_report.c
```

确认即可、通常不用改：

```text
sle_watch_server.c
```

`sle_watch_server.c` 现在已经在收到 notification/write 后调用：

```c
vitals_report_handle(device_index, data, len);
```

只要这个调用不是限定 `payload[0] == '{'` 后才执行，就能把 `RPT:` 分片送进重组逻辑。

## 修改点 1：增加分包重组状态

在 `vitals_report.c` 宏定义区域增加：

```c
#define VITALS_RPT_PREFIX "RPT:"
#define VITALS_RPT_PREFIX_LEN 4
#define VITALS_RPT_MAX_FRAGMENTS 8
#define VITALS_RPT_CHUNK_MAX_LEN 220
```

增加重组结构：

```c
typedef struct {
    uint8_t active;
    uint16_t msg_id;
    uint8_t total;
    uint8_t received_mask;
    uint16_t chunk_len[VITALS_RPT_MAX_FRAGMENTS];
    char chunks[VITALS_RPT_MAX_FRAGMENTS][VITALS_RPT_CHUNK_MAX_LEN];
} vitals_rpt_reasm_t;

static vitals_rpt_reasm_t g_rpt_reasm[WATCH_MODEL_MAX_DEVICES];
```

说明：

- 当前 BS21 每片 chunk 为 200 字节，`VITALS_RPT_CHUNK_MAX_LEN` 留 220 字节余量。
- 当前 JSON 约 276 字节，一般是 2 片。
- 以后报告变大时，只要总片数不超过 8，仍可重组。

## 修改点 2：拆出原 JSON 处理逻辑

把现有 `vitals_report_handle()` 中处理 `{...}` 的逻辑移动到内部函数：

```c
static void vitals_report_handle_json(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    /* 原 vitals_report_handle() 的 JSON 判断、latest report 更新、report ready 打印逻辑 */
}
```

新的 `vitals_report_handle()` 只做分流：

```c
void vitals_report_handle(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        return;
    }

    if ((len >= VITALS_RPT_PREFIX_LEN) &&
        (memcmp(data, VITALS_RPT_PREFIX, VITALS_RPT_PREFIX_LEN) == 0)) {
        vitals_report_handle_fragment(device_index, data, len);
        return;
    }

    vitals_report_handle_json(device_index, data, len);
}
```

这样单片 `RPT:` 不会被当成 JSON 误判。

## 修改点 3：解析 RPT 头

`RPT:` 头只有前三个冒号字段参与解析，后面的 `chunk` 是 JSON 原始片段，里面可能继续包含 `:`，不能用全包拆分。

建议增加：

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

    *out = (uint16_t)value;
    *cursor = p + 1;
    return true;
}
```

## 修改点 4：重组分片

新增 `vitals_report_handle_fragment()`，核心流程：

1. 复制收到的数据到临时 `packet`，并补 `'\0'`。
2. 跳过 `RPT:`，解析 `msgId/seq/total`。
3. 校验 `total <= VITALS_RPT_MAX_FRAGMENTS`、`seq < total`。
4. 新 `msgId` 到来时清空旧重组状态。
5. 将 chunk 按 `seq` 存到 `chunks[seq]`。
6. `received_mask == ((1U << total) - 1U)` 时，按 `seq=0..total-1` 拼出完整 JSON。
7. 调用 `vitals_report_handle_json(device_index, full_json, full_len)`。

建议日志：

```text
[REPORT] rpt fragment device=0 msg=1 seq=0 total=2 chunk=200 mask=0x1
[REPORT] rpt fragment device=0 msg=1 seq=1 total=2 chunk=76 mask=0x3
[REPORT] rpt complete device=0 msg=1 len=276
[REPORT] report ready device=0 len=276
```

## 修改点 5：网络和 MQTT 逻辑保持原样

WS63 已有逻辑是：

```c
if (!mqtt_task_is_connected()) {
    return vitals_report_cache_pending(payload);
}

if (mqtt_task_enqueue_report(payload) == ERRCODE_SUCC) {
    return ERRCODE_SUCC;
}

return vitals_report_cache_pending(payload);
```

所以重组完成后只要进入原 JSON 处理和提交流程：

- MQTT 已连接：入队上传。
- MQTT 未连接：写入本地 pending 缓存。
- MQTT 后续恢复：`vitals_report_flush_pending()` 再补发。

UI 不需要展示身高、体重、血压、血糖，只保留 “report ready / cached / published” 这类状态即可。

## BS21 配套发送日志

BS21 修改后应看到：

```text
[sle 1vn server] report fragment msg=1 seq=0 total=2 chunk=200 packet=210 ret=0x0
[sle 1vn server] report fragment msg=1 seq=1 total=2 chunk=76 packet=86 ret=0x0
[sle 1vn server] report fragmented len=276 ready=... mtu=... ret=0x0
[sensor task] report uploaded len:276
```

`packet` 长度会随 `msgId/seq/total` 的位数变化，验收时主要看每片 `chunk <= 200`、`ret=0x0`，以及 WS63 最终 `rpt complete len=276`。

WS63 如果仍看到单条 `len=248`，说明 BS21 固件还没有烧入分包版本，或 WS63 仍在旧回调路径打印旧 JSON。
