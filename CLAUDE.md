# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 TI MSPM0G3507 (Cortex-M0+ @ 32MHz) 的多传感器环境监测固件，运行在 LP-MSPM0G3507 LaunchPad + **BOOSTXL-SENSORS** BoosterPack 平台。通过 I2C1/ADC12 采集温度、照度、磁通量数据，三路 UART 覆盖调试、WiFi 和语音模块通信。

**外设分配：**

| UART | 硬件 | 引脚 | 用途 |
|------|------|------|------|
| UART0 | UART0 | PA10(TX)/PA11(RX) | XDS110 调试串口，中断驱动即时回显 |
| UART1 | UART1 | PB6(TX)/PB7(RX) | ESP8266 WiFi 模块，AT 指令双向桥接 |
| UART3 | UART3 | PB12(TX)/PB13(RX) | 语音模块，5 字节协议 (AA 55 [type] [cmd] FB) |

### BOOSTXL-SENSORS 板载传感器

| 设备 | I2C 地址 | 型号 | 说明 |
|------|---------|------|------|
| BMI160 | 0x69 | Bosch 6 轴 IMU | 片上温度传感器 (T = 23 + raw/512 °C) |
| TMP007 | 0x47 | TI 红外热电堆 | 红外物体温度 (14-bit, 0.0078125 °C/LSB 实测) |
| BME280 | 0x77 | Bosch 温湿度/气压 | ✓ 环境温度 (自动探测 0x77/0x76, 影子锁存+重试) |
| OPT3001 | — | TI 环境光 | 未检测到（上电问题） |
| BMM150 | — | Bosch 地磁 | 未检测到 |

**传感器使能引脚**（由 `Board_Sensor_Enable()` 配置）：

| 引脚 | 功能 | 有效电平 |
|------|------|---------|
| PB24 | HDC2010 电源 (BASSENSORS 兼容) | LOW |
| PA22 | DRV5055 电源 (BASSENSORS 兼容) | LOW |
| PA24 | OPT3001 电源 | HIGH |

> **注意**：BOOSTXL-SENSORS 与 BOOSTXL-BASSENSORS 是不同的板子。部分传感器驱动（HDC2010/TMP116）是从 BASSENSORS 模板创建的，在此板上不可用。

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
app/board_init.c/h               (板级抽象：I2C/ADC/UART/LED，DL API 集中层)
app/sensors/sensor_*.c/h         (传感器驱动：BME280/BMI160/TMP007/HDC2010/OPT3001/DRV5055)
app/voice_protocol.c/h           (语音模块 5 字节协议解析与命令分发)
app/main.c                       (中断驱动 UART 桥接 + 1Hz 传感器轮询 + 5s BME280 温度上报)
```

- **`ti_msp_dl_config.c/h`**：SysConfig 生成，包含 `SYSCFG_DL_init()`（GPIO、SYSCTL）。**注意**：生成的代码**不含** I2C 和 UART 初始化——它们由 `board_init` 手动完成。`CPUCLK_FREQ=32000000`。**禁止手动编辑**。
- **`board_init`**：
  - `Board_I2C_Init()`：手动初始化 I2C1 (PB2 SCL / PB3 SDA, 100kHz)，含 pinmux、reset、clock、controller enable。必须在 `Board_Sensor_Enable()` 之后调用（上拉电阻需要先上电）。
  - `Board_Sensor_Enable()`：驱动 BOOSTXL-SENSORS 传感器使能引脚。
  - I2C 读写 (`Board_I2C_ReadReg/WriteReg/Write`)：轮询模式，使用 `DL_I2C_CONTROLLER_STATUS_IDLE` 和 `BUSY_BUS` 标志，含 ~10ms 超时保护。**16 位寄存器写（3 字节）存在兼容性问题**——TMP007 配置寄存器无法通过标准 I2C 写操作正确写入。
  - UART 收发 (`Board_UART_Write/WriteString/Read/RXAvailable`)
  - LED 控制宏 (`LED_ON/OFF/TOGGLE`)
- **`sensors/`**：
  | 驱动 | 状态 | 说明 |
  |------|------|------|
  | `sensor_bme280` | ✓ 工作 | I2C 0x77 (TI 板默认, 自动探测 0x76), Chip ID 0x60, 8 字节影子锁存突发读取 + MSB 验证 + 最多 5 次重试, 寄存器写入使用原始 DL I2C API（Board_I2C_WriteReg 在此板上会产生损坏值） |
  | `sensor_bmi160` | ✓ 工作 | I2C 0x69, Chip ID 0xD1, 温度 T=23+raw/512, MSB/LSB 分开读 |
  | `sensor_tmp007` | ✓ 可读 | I2C 0x47, MFR ID 0x5449, 温度 14-bit 有符号, LSB=0.0078125°C 实测。**连续模式不可用**——16 位 config 寄存器写失败，传感器停留在单次转换模式 |
  | `sensor_hdc2010` | ✗ 不可用 | 针对 BASSENSORS 编写，0x40 在此板上为无效地址，写操作污染 I2C 总线 |
  | `sensor_tmp116` | ✗ 不可用 | 针对 BASSENSORS 编写，在此板上未检测到 |
- **`voice_protocol`**：5 字节协议解析（`AA 55 [type] [cmd] FB`），`Voice_Process_Byte()` 逐字节组包，`Voice_Protocol_Init()` 上电广播。
- **`main.c`**：
  - 初始化顺序：`SYSCFG_DL_init()` → `Board_Sensor_Enable()` → `Board_I2C_Init()` → `Board_Sensor_Init()` → UART0/1/3 Init → `Voice_Protocol_Init()` → 传感器 Init（BMI160 优先，TMP007 次之，BME280，HDC2010 跳过）
  - 主循环 @1Hz：轮询 BME280 + OPT3001 + DRV5055
  - 每 5 次循环（~5s）通过 UART0 上报温度：`BME:XX.X C`
  - `float_to_str()` 手工格式化浮点数，避免引入 printf 库

## I2C 已知问题

1. **16 位寄存器写**：TMP007 等器件的 16 位配置寄存器无法正确写入。标准 I2C 写 `[reg, MSB, LSB]` 因自动递增将 MSB 写入 reg、LSB 写入 reg+1，导致配置失败。SMBus Word Write（LSB 先）同样无效。TMP007 停留在默认单次转换模式，温度读数不更新。
2. **8 位寄存器写损坏**：`Board_I2C_WriteReg` 即使对 8 位寄存器也会产生损坏值（BME280 ctrl_meas：写入 0x23 → 读回 0x35）。BME280 驱动改用原始 DL I2C API 直接填充 TX FIFO 并启动传输来绕过此问题。
3. **多字节读不可靠**：`Board_I2C_ReadReg` 连续读 2+ 字节返回数据不稳定，约 50% 概率出现字节移位/镜像。BMI160 改用单字节读 + 50µs 间隔。BME280 通过 8 字节影子锁存突发读取 + MSB 范围验证 + 最多 5 次重试来保证数据一致性。
4. **0x40 伪设备**：往地址 0x40 写数据会污染 I2C 总线，导致后续通信失败。已禁用 HDC2010_Init。

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
