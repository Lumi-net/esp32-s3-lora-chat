# Spec: 在线状态与电量监控

## Problem Statement

用户在使用 LoRa 对讲终端时，无法知晓联系人是否在线，也无法看到设备的剩余电量。当前代码库中心跳广播 (`heartbeat_task`) 虽已实现但从未启动，且心跳错误地更新了`last_time`（最后消息时间）而非`last_online`（最后在线时间），导致菜单页的"最后消息时间"被心跳污染。INA219 电池监控的完整代码（I2C 驱动、电压查表、30 秒轮询、状态栏显示）已实现但因硬件测试不便而禁用。GAME/2048 组件已在文档中移除。

## Solution

1. 启动已有的 `heartbeat_task`，使设备每隔 180–240 秒广播一次心跳帧（`target_id=0xFF, data="HB"`），同时修复心跳处理逻辑以更新 `last_online` 而非 `last_time`
2. 菜单页的联系人列表已准备好展示 `last_online`（`online_labels[]`），心跳接入后自动生效
3. INA219 电池监控代码保留在源码中、保持禁用，但作为完整功能写入文档
4. 移除 AGENTS.md 中已删除的 GAME/2048 组件描述（已完成）

## User Stories

1. 作为用户，我希望设备能定期广播心跳信号，以便其他设备能感知我上线
2. 作为用户，我希望在菜单页看到每个联系人最后的在线时间，从而判断对方是否可能在线
3. 作为用户，我希望心跳广播不会污染聊天记录（不写入 Flash），以便聊天历史保持干净
4. 作为用户，我希望心跳广播不会触发 ACK 回复，以避免广播风暴
5. 作为用户，我希望心跳广播不会干扰正常消息收发，以便聊天体验不受影响
6. 作为用户，我希望心跳去重机制能正常工作，以便同一心跳不会导致多次状态更新
7. 作为用户，我希望自己的心跳不会更新自己的在线状态，以便自己的信息不受回环影响
8. 作为用户，我希望在状态栏看到电池百分比，以便及时充电
9. 作为用户，我希望电池检测只在系统空闲时进行（不在 LoRa 发射或 WiFi 连接时），以便电压读数不受瞬时大电流干扰
10. 作为用户，我希望电池百分比显示有防抖（变化 ≥1% 才更新 UI），以便显示不会频繁闪烁
11. 作为开发者，我希望电池监控代码保留在仓库中但默认禁用，以便硬件就绪后可快速启用
12. 作为开发者，我希望 `battery_voltage_to_percent` 函数可以独立单元测试，以便验证查表/插值逻辑的正确性
13. 作为开发者，我希望心跳帧的构建与解析可以通过已有的 `buildLoRaFrame`/`parseLoRaFrame` 测试，以便复用现有测试接缝

## Implementation Decisions

### 1. 心跳任务启动

- **位置**：`app_main_task` 的 `is_initialized` 分支内，与 `scanKeyTask`、`uart_receive` 并列启动
- **栈大小**：4096
- **优先级**：5
- **CPU 核心**：core 0（与键盘扫描、UART 接收同核）
- **函数**：`heartbeat_task` 已在 `uart.c` 中完整实现

### 2. 心跳帧构建（已有代码，无需修改）

- 帧格式：标准 `buildLoRaFrame(self_id, 0xFF, "HB")`
- `target_id=0xFF`（广播），`data_str="HB"`，`data_len=2`
- 广播帧不触发 ACK（`uart_parse_byte` 判断 `target_id == self_id` 不成立）

### 3. 冗余 `lora_wake()/lora_sleep()` 移除

`heartbeat_task` 当前代码在 `send_lora_packet()` 外层额外调用了 `lora_wake()` 和 `lora_sleep()`，但 `send_lora_packet()` 内部已包含这两个调用。需删除心跳任务中的冗余调用。

### 4. `last_online` 更新逻辑（修复现有代码）

当前 `app_main_task` 中心跳处理代码：
```c
// 旧代码（更新 last_time）
chat_list[event.frame.self_id].last_time = cur_month * 1000000 +
    cur_day * 10000 + cur_hour * 100 + cur_minute;
```
改为：
```c
// 新代码（更新 last_online）
if (event.frame.self_id != self_id) {
    chat_list[event.frame.self_id].last_online = cur_month * 1000000 +
        cur_day * 10000 + cur_hour * 100 + cur_minute;
}
```
- `last_online` 与 `last_time` 使用相同的 packed 编码：`month*1000000 + day*10000 + hour*100 + minute`
- 增加 `self_id` 防护，避免回环更新自己的状态
- `last_time` 保持原样，仅被真实消息更新

### 5. 心跳去重

现有 `seen_peer[256]` + `last_seen_seq[256]` 去重机制工作在非心跳消息分支（`else`），心跳分支不经过去重。这是本意的——每个心跳都应更新在线时间戳，序列号仅用于防止 Flash 重复写入。

### 6. 心跳不写入 Flash

心跳帧的处理位于 `event.frame.target_id == 0xFF` 分支，该分支不调用 `chat_storage_append()`。保持此行为不变。

### 7. INA219 电池监控（禁用状态）

完整实现已在 `components/I2C/i2c.c` 中，包括：

- **电源门控**：GPIO35 控制 INA219 供电，读取前上电、读取后断电
- **I2C 总线管理**：使用 ESP-IDF v5.5 新 API（`i2c_master_bus_handle_t`），读完后释放总线并将 SDA/SCL 拉低防倒灌
- **电压查表**：`battery_table[]` 包含 13 个电压→百分比映射点，线性插值
- **校准计算**：`calculate_calibration()` 根据分流电阻 (0.1Ω) 和最大预期电流 (2.0A) 计算 Calibration 寄存器
- **防抖**：`app_main_task` 中轮询间隔 30 秒，仅剩余电量变化 ≥1% 才更新 UI

**保持禁用的原因**：`i2c_master_init()` 调用在 `peripheral_init_task` 中注释掉，30 秒轮询代码在 `app_main_task` 中注释掉。启用只需取消两处注释并确保硬件 INA219 模块连接。

### 8. 菜单页自动展示在线状态

- 菜单页 `menu_refresh()` 已为每个联系人显示 `online_labels[i]`，数据源为 `chat_list[].last_online`
- `last_online == 0` 时显示 "Maybe Offline"，否则显示格式化的 `MM/DD HH:MM`
- 心跳接入后，此字段自动生效，无需额外 UI 改造

### 9. GAME/2048 移除

- AGENTS.md 中的 `| GAME/ | 2048 game |` 行已删除
- 组件计数从 13 更新为 12

## Testing Decisions

### 测试原则

只测试外部行为，不测试实现细节。对于嵌入式固件，优先测试纯函数，硬件集成部分依赖日志和运行时观察。

### 被测模块

| 模块 | 测试内容 | 接缝类型 |
|------|---------|---------|
| `UART/` -> `buildLoRaFrame` | 验证 `target_id=0xFF`、`data_str="HB"` 的帧结构正确性 | 纯函数（已有） |
| `UART/` -> `parseLoRaFrame` | 验证解析心跳帧能正确填充 `LoRaFrameData` | 纯函数（已有） |
| `I2C/` -> `battery_voltage_to_percent` | 验证电压→百分比查表与插值：边界值 (4.20V→100%, 3.30V→0%)、中间值插值精度 | 纯函数（已有） |
| `main.c` -> `app_main_task` | 集成测试：注入 `EVENT_UART` (HB)，验证 `chat_list[id].last_online` 被更新且 `last_time` 不变 | 集成观察点 |

### 已有先例

- `buildLoRaFrame`/`parseLoRaFrame` 已用于消息帧的构建与解析测试
- `calculateCRC8` 已作为纯函数测试

## Out of Scope

- 自定义心跳间隔（当前固定 180–240 秒随机，不计划做成 NVS 可配）
- 主动心跳查询（"ping" 按钮手动查询某联系人是否在线）
- 在线状态的 UI 动态刷新（当前只在菜单刷新时重绘，无自动定时刷新）
- INA219 的硬件调试与启用（spec 不涉及焊接/连线/调试）
- WiFi 探针/网络在线状态（仅在 LoRa 本地网络范围内）
- 群组聊天或私聊状态分离（在线状态按设备 ID 显示，不区分群组）

## Further Notes

- 心跳对功耗的影响：180–240 秒发一次 LoRa 广播，每次发送约 200ms 唤醒 + 50ms 传输 ≈ 250ms 活动，等效 duty cycle 约 0.05%，对电池续航影响可忽略
- `last_online` 与 `last_time` 使用相同的 packed 编码格式（`month*1000000 + day*10000 + hour*100 + minute`），保持一致性
- 若后续硬件 INA219 就绪，启用步骤为：(1) 取消 `peripheral_init_task` 中 `i2c_master_init()` 的注释 (2) 取消 `app_main_task` 中 30 秒电池轮询代码的注释
