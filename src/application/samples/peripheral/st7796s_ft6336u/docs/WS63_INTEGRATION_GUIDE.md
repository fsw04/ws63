# BS21 sle_1vn 与 WS63 st7796s_ft6336u 对接说明

## 目标

BS21 `sle_1vn` 作为测量采集端，与 WS63
`src/application/samples/peripheral/st7796s_ft6336u` 主控端对接。

目标流程：

1. WS63 与 BS21 建立 SLE 长连接。
2. WS63 NFC 身份验证成功后，将姓名和身份证号发送给 BS21。
3. BS21 收到身份后，更新本轮 `vitals` JSON 的 `name` 和 `idCard`。
4. `phone` 不传输，BS21 固定填空字符串。
5. BS21 写入身份后先打印一次 JSON。
6. BS21 开始分段连接底端测量传感器采集数据。
7. 每完成一个分段采集，BS21 更新 `receivedMask` 并打印一次当前 JSON。
8. `receivedMask == requiredMask` 后，BS21 打包完整 JSON 发送给 WS63 主控端 server。

最终 JSON 格式：

```json
{
  "deviceId": "watch_01",
  "type": "vitals",
  "complete": true,
  "receivedMask": 15,
  "requiredMask": 15,
  "data": {
    "name": "姓名",
    "phone": "",
    "idCard": "身份证号",
    "height": "160 cm",
    "weight": "55 kg",
    "bmi": "21.5",
    "bloodPressure": "120/80 mmHg",
    "fastingBloodGlucose": "5.2 mmol/L"
  }
}
```

## 当前 BS21 侧实现

BS21 目录：

```text
D:\HiSpark\fbb_bs21\src\application\samples\sle_1vn
```

关键文件：

```text
sle_1vn.c
sle_1vn_server/sle_1vn_server.c
sle_1vn_server/sle_1vn_server.h
measurement_session.c
measurement_session.h
sensor_task.c
sensor_collect.c
sensor_collect.h
```

当前 BS21 已实现：

1. `sle_1vn_server.c` 建立 SLE server，UUID 与 WS63 使用的协议保持一致：
   - service UUID: `0x060B`
   - property UUID: `0x1122`
   - MTU: `1500`
2. `ssaps_write_request_cbk()` 解析 WS63 发来的身份命令：

```text
ID:姓名,身份证号
```

3. `wearable_on_identity_received()` 保存身份：
   - `name`
   - `id_number`
   - `identity_received`
   - `sequence`
4. `measurement_session_init()` 初始化测量 session，不再写死默认姓名、手机号、身份证。
5. `measurement_session_set_identity()` 写入本轮身份：
   - `name = WS63 下发的姓名`
   - `id_card = WS63 下发的身份证号`
   - `phone = ""`
6. `sensor_task.c` 等待主控端长连接和新身份后才开始采集。
7. 身份写入后打印一次 JSON。
8. 每个采集项完成后打印一次 JSON。
9. 完整后通过 `sle_1vn_server_send_report()` 发送 JSON。

BS21 当前调试日志应出现类似：

```text
[sle 1vn server] write_request len:...
[sle 1vn server] ===== NFC Identity Stored =====
[sensor task] identity ready seq:1 name:... id:...
[sensor task] identity json:{...}
[sensor collect] connect sensor:height target:sensor-height
[sensor task] height json:{...}
[sensor collect] connect sensor:weight target:sensor-weight
[sensor task] weight json:{...}
[sensor collect] connect sensor:blood-pressure target:sensor-bp
[sensor task] blood-pressure json:{...}
[sensor collect] connect sensor:glucose target:sensor-glucose
[sensor task] glucose json:{...}
[sensor task] session complete, upload report
```

如果只看到：

```text
[demo] master server connected, waiting identity/data...
```

说明 BS21 已连接 WS63，但还没有收到 `ID:` 身份写入。

## 当前发现的问题

### 1. WS63 UI 显示身份不等于身份已发到 BS21

WS63 当前流程中，`watch_borrow_on_identity_received()` 会先更新 UI，再调用：

```c
sle_watch_send_identity_to_device(device_index, name, id_number);
```

因此 NFC 读卡成功、UI 显示身份成功，只能证明 WS63 本地身份识别成功，不能证明 BS21 已收到身份。

判断 BS21 是否收到身份，应以 BS21 日志为准：

```text
[sle 1vn server] write_request len:...
[sle 1vn server] ===== NFC Identity Stored =====
```

### 2. WS63 将 conn_id 为 0 当成未连接

WS63 `sle_watch_send_to_device()` 当前逻辑：

```c
conn_id = g_device_conn_hdl[device_index];
if (conn_id == 0) {
    return ERRCODE_SLE_FAIL;
}
```

但 BS21 日志显示连接 ID 可以是：

```text
conn_id:0x00
```

`0` 是有效连接 ID，不能作为未连接判断。这个逻辑会导致 WS63 在连接正常时仍返回发送失败，身份无法下发。

建议 WS63 增加单独的连接有效标志，例如：

```c
static uint8_t g_device_conn_valid[WATCH_MODEL_MAX_DEVICES];
```

连接成功：

```c
g_device_conn_hdl[i] = conn_id;
g_device_conn_valid[i] = 1;
```

断开连接：

```c
g_device_conn_hdl[i] = 0;
g_device_conn_valid[i] = 0;
```

发送前判断：

```c
if (g_device_conn_valid[device_index] == 0) {
    return ERRCODE_SLE_FAIL;
}
conn_id = g_device_conn_hdl[device_index];
```

### 3. 两端发送方向需要统一

BS21 当前是在 server 的写回调中等待身份：

```c
ssaps_write_request_cbk()
```

也就是说，WS63 需要对 BS21 的 SLE property 执行 client write：

```text
WS63 ssapc_write_req/ssapc_write_cmd -> BS21 ssaps_write_request_cbk
```

但 WS63 当前 `sle_watch_send_to_device()` 使用的是：

```c
ssaps_notify_indicate(...)
```

这是 WS63 作为 server 主动 notify 自己的 property。BS21 如果没有作为 client 订阅 WS63 的 notification，就不会进入 `ssaps_write_request_cbk()`。

因此推荐统一为：

```text
身份下发：WS63 client write -> BS21 server write_request
结果上报：BS21 server notify -> WS63 接收 notification 或 write_request 后转 MQTT/server
```

如果保持 WS63 使用 `ssaps_notify_indicate()`，则 BS21 需要补充 SLE client 侧能力，发现 WS63 service/property 并接收 notification。这会比改 WS63 为 client write 更复杂。

## 推荐对接协议

### 身份下发

WS63 -> BS21：

```text
ID:姓名,身份证号
```

示例：

```text
ID:Test User,110101199001011234
```

BS21 收到后：

1. 保存身份。
2. 回复：

```text
CONNECTED:姓名,身份证号
```

或：

```text
IDENTITY_OK
```

当前代码中 BS21 两种回复都存在，WS63 主要解析 `CONNECTED:`。

### 测量结果上报

BS21 -> WS63：

```json
{"deviceId":"watch_01","type":"vitals","complete":true,"receivedMask":15,"requiredMask":15,"data":{"name":"Test User","phone":"","idCard":"110101199001011234","height":"160 cm","weight":"55 kg","bmi":"21.5","bloodPressure":"120/80 mmHg","fastingBloodGlucose":"5.2 mmol/L"}}
```

WS63 收到 JSON 后，可按 `type == "vitals"` 转发给主控端 server 或 MQTT。

## WS63 侧建议修改点

WS63 目录：

```text
D:\HiSpark\fbb_ws63\src\application\samples\peripheral\st7796s_ft6336u
```

### 1. 修正连接 ID 判断

修改文件：

```text
sle_watch_server.c
```

新增连接有效数组：

```c
static uint8_t g_device_conn_valid[WATCH_MODEL_MAX_DEVICES];
```

在 `sle_watch_set_device_online()` 中，连接成功时设置 valid，断开时清除 valid。

在 `sle_watch_send_to_device()` 中，不再使用 `conn_id == 0` 判断未连接。

### 2. 身份发送成功后再确认 UI 状态

当前 `watch_borrow_on_identity_received()` 先更新 UI，再发送身份。

建议至少在发送失败时打印更明显日志，或者把“借出确认/身份确认”放到收到 BS21 `CONNECTED:` 后完成。

推荐最终语义：

```text
NFC 读卡成功：UI 显示已读取身份
SLE 发送成功：日志显示 identity sent
BS21 回复 CONNECTED：UI 显示身份对接成功/borrow confirmed
```

### 3. 改身份发送方向为 client write

推荐 WS63 增加或复用 SSAP client 流程：

1. 连接 BS21 后发现 service `0x060B`。
2. 发现 property `0x1122`。
3. 保存 BS21 property handle。
4. 身份验证成功后调用：

```c
ssapc_write_req(client_id, conn_id, &param);
```

或：

```c
ssapc_write_cmd(client_id, conn_id, &param);
```

其中 `param.data` 为：

```text
ID:姓名,身份证号
```

这样 BS21 才会进入 `ssaps_write_request_cbk()`。

## BS21 侧后续待验证点

### 1. SensorTask 是否成功创建

之前日志出现：

```text
No mem to alloc 0x300c Bytes
```

说明 `SensorTask` 原栈 `0x3000` 分配失败。当前已改为 `0x1000`，再次烧录后应看到：

```text
[sensor task] start
```

如果仍失败，需要继续降低栈或把采集流程合并到主 demo task 中。

### 2. 是否收到身份

收到身份时，BS21 必须出现：

```text
[sle 1vn server] write_request len:...
[sle 1vn server] ===== NFC Identity Stored =====
```

如果没有，继续检查 WS63 是否真的执行了 client write。

### 3. 是否打印身份 JSON

身份写入成功后，应出现：

```text
[sensor task] identity json:{...}
```

其中：

```json
"name": "WS63下发姓名",
"phone": "",
"idCard": "WS63下发身份证号"
```

### 4. 是否分段打印 JSON

每项采集后应出现：

```text
[sensor task] height json:{...}
[sensor task] weight json:{...}
[sensor task] blood-pressure json:{...}
[sensor task] glucose json:{...}
```

`receivedMask` 应逐步变化：

```text
1 -> 3 -> 7 -> 15
```

## 调试判断表

| 现象 | 含义 | 优先检查 |
| --- | --- | --- |
| BS21 打印 `server running, no master connection yet` | SLE 未连接 | WS63 是否连接到 BS21 |
| BS21 打印 `master server connected, waiting identity/data` | 已连接但未收到身份 | WS63 身份是否发出，是否使用 client write |
| BS21 无 `[sensor task] start` | 采集任务没创建 | 栈/内存是否不足 |
| WS63 UI 显示身份但 BS21 无 `write_request` | NFC 成功但身份未到 BS21 | WS63 发送路径或 conn_id 判断 |
| BS21 有 `write_request` 但无 `NFC Identity Stored` | 数据格式不是 `ID:` | 检查 payload |
| BS21 有身份 JSON 但不上报 | 未完整采集或发送失败 | `receivedMask/requiredMask` 和 `sle_1vn_server_send_report()` |

## 推荐修改顺序

1. 先修 WS63 `conn_id == 0` 判断问题。
2. 确认 WS63 身份发送日志是 `identity sent` 而不是 `identity send failed`。
3. 确认 BS21 出现 `write_request len`。
4. 如果仍无 `write_request`，将 WS63 身份发送改成 `ssapc_write_req/ssapc_write_cmd`。
5. 确认 BS21 打印 `identity json`。
6. 确认每段采集打印 JSON。
7. 最后再对接真实底端传感器短连接采集逻辑。
