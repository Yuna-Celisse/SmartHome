# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 TI MSPM0G3507 (Cortex-M0+ @ 32MHz) 的多传感器环境监测固件，运行在 LP-MSPM0G3507 LaunchPad + BOOSTXL-BASSENSORS BoosterPack 平台。通过 I2C1/ADC12 采集温湿度、照度、磁通量数据，三路 UART 覆盖调试、WiFi 和语音模块通信。

**外设分配：**

| UART | 硬件 | 引脚 | 用途 |
|------|------|------|------|
| UART0 | UART0 | PA10(TX)/PA11(RX) | XDS110 调试串口，中断驱动即时回显 |
| UART1 | UART1 | PB6(TX)/PB7(RX) | ESP8266 WiFi 模块，AT 指令双向桥接 |
| UART3 | UART3 | PB12(TX)/PB13(RX) | 语音模块，5 字节协议 (AA 55 [type] [cmd] FB) |

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
app/sensors/sensor_*.c/h         (传感器驱动：HDC2010/TMP116/OPT3001/DRV5055)
app/voice_protocol.c/h           (语音模块 5 字节协议解析与命令分发)
app/main.c                       (中断驱动 UART 桥接 + 1Hz 传感器轮询)
```

- **`ti_msp_dl_config.c/h`**：SysConfig 生成，包含 `SYSCFG_DL_init()`（GPIO、I2C1@400kHz、ADC12、时钟树）及外设宏（`CPUCLK_FREQ`、`GPIO_LED_*`、`I2C1`、`ADC0` 等）。UART 模块以代码生成关闭（`false`）注册，仅用于 pinmux 锁定。**禁止手动编辑**。
- **`board_init`**：封装 I2C（`Board_I2C_Write/Read/WriteReg/ReadReg`）、ADC（`Board_ADC_Read`）、三路 UART 收发（`Board_UART_Write/WriteString/Read/RXAvailable`，首个参数 `UART_Regs *uart` 选择实例）、LED 控制宏（`LED_ON/OFF/TOGGLE`）。内部 `uart_periph_init()` 消除三路 UART 初始化代码重复。所有 DL API 调用集中在此层。
- **`sensors/`**：每个传感器一个 `.c/.h` 对，依赖 `board_init.h` 进行 I2C/ADC 访问，`Init()` 返回 `int`（0 = 成功、-1 = 失败），其余接口返回 `float` 物理量或状态码。
- **`voice_protocol`**：5 字节协议解析（`AA 55 [type] [cmd] FB`），`Voice_Process_Byte()` 逐字节组包，`Voice_Protocol_Init()` 上电广播。ALARM 命令 (0x26) 触发 `LED_TOGGLE()` 并回复应答包。
- **`main.c`**：`SYSCFG_DL_init()` → `Board_Sensor_Init()` → 三路 UART Init + NVIC → `Voice_Protocol_Init()` → 主循环 `while(1)` @1Hz。三路 UART 全部中断驱动：UART0 本地回显 + 透传 UART1，UART1 响应转发 UART0，UART3 协议解析 + 转发 UART0。主循环仅负责传感器轮询，LED 由语音模块 ALARM 命令驱动。

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
