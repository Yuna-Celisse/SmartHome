# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 TI MSPM0G3507 (Cortex-M0+ @ 32MHz) 的智能电风扇固件，运行在 LP-MSPM0G3507 LaunchPad + **BOOSTXL-SENSORS** BoosterPack 平台。集成光照控灯、语音开关、陀螺仪报警、温度调速、**华为云 IoTDA MQTT 远程控制**五大功能。UART0 以 FireWater 协议实时上报传感器数据至 VOFA+。

**外设分配：**

| UART | 硬件 | 引脚 | 用途 |
|------|------|------|------|
| UART0 | UART0 | PA10(TX)/PA11(RX) | FireWater 传感器数据上报 (VOFA+ CSV, ~1.25Hz)；RX 转发 AT 指令至 UART1 |
| UART1 | UART1 | PB6(TX)/PB7(RX) | ESP8266 WiFi 模块 — AT 指令/MQTT 双向通信 |
| UART3 | UART3 | PB12(TX)/PB13(RX) | 语音模块，5 字节协议 (AA 55 [type] [cmd] FB) |

**其他关键引脚：**

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA0 | LED / 灯光输出 | 低有效，光照控灯 + 报警 5Hz 闪烁 |
| PA1 | 风扇 PWM | TIMA0 CCP1, 25kHz, TB6612 驱动 |
| PB0/PB1 | 风扇方向 | TB6612 AIN1/AIN2 |
| PB8 | 蜂鸣器 | 高有效，震动报警 10s 后自动关闭 |

### BOOSTXL-SENSORS 板载传感器

| 设备 | I2C 地址 | 型号 | 说明 |
|------|---------|------|------|
| BMI160 | 0x69 | Bosch 6 轴 IMU | ✓ 3 轴陀螺仪 (GYR_NORMAL 模式, ±2000°/s, 16.384 LSB/°/s) |
| BME280 | 0x77 | Bosch 温湿度/气压 | ✓ 环境温度 (自动探测 0x77/0x76, 影子锁存+重试) |
| OPT3001 | 0x47 | TI 环境光 | ✓ 照度 (ADDR=SCL, 自动量程+连续模式, 800ms) |

**传感器使能引脚**（由 `Board_Sensor_Enable()` 配置）：

| 引脚 | 功能 | 有效电平 |
|------|------|---------|
| PB24 | 保留 (BASSENSORS HDC2010 兼容) | LOW |
| PA22 | 保留 (BASSENSORS DRV5055 兼容) | LOW |
| PA24 | OPT3001 电源 | HIGH |

> **注意**：BOOSTXL-SENSORS 与 BOOSTXL-BASSENSORS 是不同的板子。HDC2010/TMP116/DRV5055 驱动已移除，仅保留 BOOSTXL-SENSORS 板载的 3 个传感器 + 语音模块。

## 构建命令

```bash
# 克隆（含 SDK 子模块）
git clone --recurse-submodules <repo-url>

# TI CCS / ticlang 命令行构建
cd ticlang && make

# Keil MDK：打开 keil/smart_home.uvprojx → F7 构建 → F8 烧录
```

构建入口为 `ticlang/makefile`（TI Arm Clang 3.2.0+，`-mcpu=cortex-m0plus -O2`）或 Keil MDK 工程文件。两种构建都依赖 SysConfig 从 `smart_home.syscfg` 生成 `ti_msp_dl_config.c/h`。

### CCS 调试

CCS Theia 的调试配置由 GUI 生成，**不在仓库中维护** `.ccxml`、`.gel`、`.launch` 或 `.theia/launch.json`。

1. CCS Theia → Project → Import CCS Project → 选择 `ticlang/` 目录
2. 右键工程 → New → Target Configuration File → XDS110 + MSPM0G3507 → 保存为 `MSPM0G3507.ccxml`
3. CCS 自动根据 `ticlang/smart_home.projectspec` 解析构建选项和调试参数
4. F5 启动调试

> 手工创建的 `.ccxml` 无法被 CCS Theia 识别，目标配置文件必须通过 CCS GUI 生成。

## 架构层次

```
smart_home.syscfg ──SysConfig──▶ ti_msp_dl_config.c/h   (外设初始化，自动生成，勿手动改)
app/board_init.c/h               (板级抽象：I2C/ADC/UART/LED/PWM/Buzzer，DL API 集中层)
app/sensors/sensor_*.c/h         (传感器驱动：BMI160/BME280/OPT3001)
app/voice_protocol.c/h           (语音模块 5 字节协议解析与命令分发)
app/fan_control.c/h              (TB6612 风扇 PWM 恒温控制算法)
app/esp8266_at.c/h               (ESP8266 AT 指令驱动：环形缓冲区 + 行解析器 + MQTT 负载提取)
app/iotda_mqtt.c/h               (华为云 IoTDA MQTT 状态机：连接/订阅/属性上报/命令分发)
app/cloud_config.h               (WiFi SSID/密码、MQTT Broker/设备凭证)
app/json_helper.c/h              (手写 JSON 构造器/解析器，无外部库依赖)
app/str_utils.c/h                (ftoa_fixed/str_find/str_parse_uint32，替代 printf/strstr)
app/system_state.h               (跨模块共享状态：枚举、全局变量、阈值常量)
app/main.c                       (主循环调度：传感器采集 + 云端遥测 + 报警 + FireWater 上报)
```

- **`ti_msp_dl_config.c/h`**：SysConfig 生成，包含 `SYSCFG_DL_init()`（GPIO、SYSCTL）。**注意**：生成的代码**不含** I2C 和 UART 初始化——它们由 `board_init` 手动完成。`CPUCLK_FREQ=32000000`。**禁止手动编辑**。
- **`board_init`**：
  - `Board_I2C_Init()`：手动初始化 I2C1 (PB2 SCL / PB3 SDA, 100kHz)，含 pinmux、reset、clock、controller enable。必须在 `Board_Sensor_Enable()` 之后调用（上拉电阻需要先上电）。
  - `Board_Sensor_Enable()`：驱动 BOOSTXL-SENSORS 传感器使能引脚。
  - I2C 读写 (`Board_I2C_ReadReg/WriteReg/Write`)：轮询模式，使用 `DL_I2C_CONTROLLER_STATUS_IDLE` 和 `BUSY_BUS` 标志，含 ~10ms 超时保护。**16 位寄存器写（3 字节）存在兼容性问题**——TMP007 配置寄存器无法通过标准 I2C 写操作正确写入。
  - UART 收发 (`Board_UART_Write/WriteString/Read/RXAvailable`)
  - LED 控制宏 (`LED_ON/OFF/TOGGLE`)
  - `Board_Buzzer_Init()`：PB8 数字输出 + 使能，高有效
  - `Board_Fan_Init()`：TIMA0 PWM (PA1, 25kHz) + TB6612 方向引脚 (PB0/PB1)
  - `Board_Fan_SetSpeed()`：设置 PWM 占空比 (0–100%)
- **`sensors/`**：
  | 驱动 | 状态 | 说明 |
  |------|------|------|
  | `sensor_bmi160` | ✓ 工作 | I2C 0x69, Chip ID 0xD1, 3 轴陀螺仪 GYR_NORMAL 模式, 灵敏度 16.384 LSB/°/s, 原始 DL I2C 读写 + 6 字节单次读取 + 50µs 间隔 |
  | `sensor_bme280` | ✓ 工作 | I2C 0x77 (TI 板默认, 自动探测 0x76), Chip ID 0x60, 全部使用原始 DL I2C API 读写, 8 字节影子锁存突发读取 + MSB 验证 + 最多 5 次重试 |
  | `sensor_opt3001` | ✓ 工作 | I2C 0x44 (ADDR=GND), 照度 lux, 原始 DL I2C API, auto-range + continuous + 800ms + latch |
- **`voice_protocol`**：5 字节协议解析（`AA 55 [type] [cmd] FB`），`Voice_Process_Byte()` 逐字节组包，`Voice_Protocol_Init()` 上电广播。
- **`main.c`**：
  - 初始化顺序：`SYSCFG_DL_init()` → `Board_Sensor_Enable()` → `Board_I2C_Init()` → `Board_Sensor_Init()` → UART0/1/3 Init → `Voice_Protocol_Init()` → `FanControl_Init()` → `Board_Buzzer_Init()` → `OPT3001_Init()` → `ESP_Init()` → `IoTDA_Init()`
  - IoTDA 连接阶段：阻塞式状态机（ATE0 → WIFI → MQTT → OPERATIONAL），连接成功前关闭 UART0→UART1 转发以避免 AT 指令干扰
  - 主循环 @~1.25Hz：传感器采集 → IoTDA 属性上报 → MQTT 命令处理 → 光照控灯 → 温度调速 → 陀螺仪报警 → FireWater CSV 上报
  - UART0 TX 专用于 FireWater 数据输出（无启动横幅、无温度上报、无回显、无桥接转发）
  - 三路 ISR：UART0 仅接收转发到 UART1，UART1 接收送 ESP8266 环形缓冲区，UART3 仅协议解析
  - `float_to_str()` 手工格式化浮点数，避免引入 printf 库

### VOFA+ FireWater 输出协议

UART0 (COM10, 115200 8N1) 以 ~1.25Hz 频率输出光照度数据：

```
lux\r\n
```

- **协议**：FireWater（VOFA+ 原生支持，文本 CSV 格式）
- **通道**：1 通道 — lux
- **帧尾**：`\r\n`（CRLF）
- **示例**：`123.4\r\n`
- **VOFA+ 配置**：协议选 FireWater，通道数 1

### 华为云 IoTDA MQTT 协议栈

ESP8266 通过 AT 固件（v2.2.0+）内置的 MQTT 客户端连接华为云 IoTDA，协议层次：

```
app/iotda_mqtt.c  ──状态机──▶  AT+MQTTUSERCFG/CONN/SUB/PUB
app/esp8266_at.c  ──AT驱动──▶  UART1 ←→ ESP8266 ←→ WiFi ←→ IoTDA Broker
app/json_helper.c ──JSON────▶  属性上报/命令解析（3个Service）
app/cloud_config.h──配置────▶  WiFi凭证 + MQTT Broker + 设备ID/密码
```

**IoTDA 状态机**（`IoTDA_Step()` 每次主循环调用一次）：

| 状态 | 操作 | 说明 |
|------|------|------|
| `IOTDA_AT_E0` | `ATE0` | 关闭回显 |
| `IOTDA_AT_CWMODE` | `AT+CWMODE=1` | WiFi Station 模式 |
| `IOTDA_WIFI_CONN` | `AT+CWJAP` | 连接 WiFi AP |
| `IOTDA_MQTT_USERCFG` | `AT+MQTTUSERCFG` | 配置 MQTT 客户端 ID/用户名/密码 |
| `IOTDA_MQTT_CONN` | `AT+MQTTCONN` | 连接 IoTDA Broker (TCP 1883) |
| `IOTDA_MQTT_SUB` | `AT+MQTTSUB` ×3 | 订阅 commands/# / properties/report/response / messages/down |
| `IOTDA_OPERATIONAL` | `AT+MQTTPUBRAW` | 运行态：属性上报 + 命令处理 |
| `IOTDA_RECONNECT` | 1s LED 闪烁 | 重连等待，超时后回 `IOTDA_AT_E0` |

**属性上报**（`IoTDA_ReportProperties()`，~10s 间隔）：

3 个 IoTDA Service，每个独立 MQTT topic 上报：
- **LightControl** (service_id=`light`): `lux`, `light_mode`, `light_on`
- **FanControl** (service_id=`fan`): `temperature`, `fan_level`, `fan_mode`
- **AlarmStatus** (service_id=`alarm`): `alarm_state`, `gyro_x`, `gyro_y`, `gyro_z`

**命令分发**（`IoTDA_ProcessCommand()`）：

| 命令 | service_id | 操作 |
|------|-----------|------|
| `setFan` | `fan` | 设置风扇档位 (0–4)，切换至 MANUAL 模式 |
| `setFanMode` | `fan` | 切换 AUTO/MANUAL 风扇模式 |
| `setLight` | `light` | 开关灯，切换至 MANUAL 模式 |
| `resetAlarm` | `alarm` | 清除报警状态 |

命令订阅 topic 使用 `commands/#` 通配符匹配任意 requestId。仅对 `commands/request` 下发 ACK 响应，`messages/down` 异步消息不响应。

**ESP8266 AT 驱动关键设计**：

- **256 字节环形缓冲区**：UART1 ISR 写入，`ESP_PollRx()` 主循环消费
- **行解析器**：逐字节扫描 `\n`，按 `+MQTTSUBRECV`/`OK`/`ERROR`/`FAIL` 分类
- **双槽 MQTT 负载缓冲**：两个 `+MQTTSUBRECV` 先后到达时分别存入 slot 0/1，防止覆盖
- **`ESP_DelayMs()`**：阻塞延迟期间持续排空环形缓冲区，防止 UART1 溢出丢包
- **`ESP_FlushRx()`**：先排空缓冲区处理未消费数据，再清零解析器状态

### TB6612 风扇 PWM 恒温控制

- **硬件**：TIMA0 CCP1 (PA1) 25kHz PWM + PB0/PB1 方向控制
- **恒温映射**：温度 <25°C OFF (0%) → 25–28°C LOW (25%) → 28–32°C MED (50%) → 32–35°C HIGH (75%) → >35°C MAX (100%)
- **语音/云端超控**：`FanControl_SetSpeed()` 直接设置占空比，切换至 MANUAL 模式

### 蜂鸣器报警

- **引脚**：PB8 数字输出，高有效
- **触发条件**：BMI160 任意轴角速率 > 150°/s
- **持续时间**：10s 后自动关闭，5s 冷却期防重复触发
- **LED 同步**：报警期间 PA0 以 5Hz 频率闪烁

## I2C 已知问题

1. **`Board_I2C_WriteReg` 寄存器写损坏**：产生损坏的寄存器值（BME280 ctrl_meas：写入 0x23 → 读回 0x35）。**所有传感器驱动已改用原始 DL I2C API**（`DL_I2C_fillControllerTXFIFO` + `DL_I2C_startControllerTransfer`）直接填充 TX FIFO 并启动传输来绕过此问题。
2. **`Board_I2C_ReadReg` 多字节读不可靠**：连续读 2+ 字节返回数据不稳定，约 50% 概率出现字节移位/镜像。BMI160 改用单字节读 + 50µs 间隔。BME280 通过 8 字节影子锁存突发读取 + MSB 范围验证 + 最多 5 次重试来保证数据一致性。
3. **所有新传感器驱动必须使用原始 DL I2C API**，禁止调用 `Board_I2C_Write`/`WriteReg`/`ReadReg`。

## 第三方代码边界

以下为生成代码或第三方代码，**默认不修改、不格式化**：

| 路径 | 来源 | 说明 |
|------|------|------|
| `ti_msp_dl_config.c/h` | SysConfig 生成 | 外设初始化 |
| `mspm0-sdk/` | git submodule | TI DriverLib、CMSIS、启动文件 |
| `keil/startup_mspm0g350x_uvision.s` | TI SDK | 启动汇编 |
| `ticlang/device_linker.cmd` | TI SDK 模板 | 链接脚本 |

自研代码仅限 `app/` 目录。

## 编码规范速查

- **命名**：文件/函数 `snake_case`，局部变量/参数 `lowerCamelCase`，全局变量 `g_` + `lowerCamelCase`，宏/枚举值 `UPPER_SNAKE_CASE`，typedef 类型 `PascalCase`
- **格式**：4 空格缩进（禁止 TAB），K&R 大括号，行宽 ≤81 列
- **注释**：每个函数需 Doxygen 风格注释（`@brief/@param/@return`），文件头使用标准模板。注释用英文，说明设计意图而非复述代码
- **控制流**：`if/for/while` 必须显式加大括号，`switch` 的 `case` 缩进一级
- **安全**：检查所有可能失败的 API 返回值（HAL、I2C、内存分配等）；校验外部输入；资源释放用 `cleanup:` 收口；`assert` 仅用于内部不变量
- **可移植性**：长度/索引用 `size_t`，指针运算用 `uintptr_t`，不通过 `uint32_t` 传递指针
- 详细规则参见 `.claude/skills/embedded-c-coding-standard-zh-v2/`

## Git 提交规范

```
<type>(<scope>): <中文描述>
```

- **type**：`feat` / `fix` / `docs` / `chore` / `build` / `refactor` / `test`
- **scope**：模块名（`board`、`main`、`sensors`、`.gitignore`、`keil` 等）
- 提交信息结尾附加 `Co-Authored-By: Claude <noreply@anthropic.com>`
- 审查时优先用 `git diff` 确定增量范围，聚焦自研 `.c/.h`，排除 SDK 和生成文件
