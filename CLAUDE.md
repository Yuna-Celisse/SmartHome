# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 TI MSPM0G3507 (Cortex-M0+ @ 32MHz) 的多传感器环境监测固件，运行在 LP-MSPM0G3507 LaunchPad + BOOSTXL-BASSENSORS BoosterPack 平台。通过 I2C1/ADC12 采集温湿度、照度、磁通量数据，UART0 输出调试信息。

## 构建命令

```bash
# 克隆（含 SDK 子模块）
git clone --recurse-submodules <repo-url>

# TI CCS / ticlang 命令行构建
cd ticlang && make

# Keil MDK：打开 keil/smart_home.uvprojx → F7 构建 → F8 烧录
```

构建入口为 `ticlang/makefile`（TI Arm Clang 3.2.0+，`-mcpu=cortex-m0plus -O2`）或 Keil MDK 工程文件。两种构建都依赖 SysConfig 从 `smart_home.syscfg` 生成 `ti_msp_dl_config.c/h`。

## 架构层次

```
smart_home.syscfg ──SysConfig──▶ ti_msp_dl_config.c/h   (外设初始化，自动生成，勿手动改)
app/board_init.c/h               (板级抽象：I2C 读写、ADC 采集、UART0、LED 控制宏)
app/sensors/sensor_*.c/h         (传感器驱动：HDC2010/TMP116/OPT3001/DRV5055)
app/main.c                       (主循环：1Hz 轮询全部传感器)
```

- **`ti_msp_dl_config.c/h`**：SysConfig 生成，包含 `SYSCFG_DL_init()`（GPIO、I2C1@400kHz、ADC12、时钟树）及外设宏（`CPUCLK_FREQ`、`GPIO_LED_*`、`I2C1`、`ADC0` 等）。**禁止手动编辑**。
- **`board_init`**：封装 I2C 读写（`Board_I2C_Write/Read/WriteReg/ReadReg`）、ADC 单次转换、UART0 轮询收发。所有 DL API 调用集中在此层。
- **`sensors/`**：每个传感器一个 `.c/.h` 对，依赖 `board_init.h` 进行 I2C/ADC 访问，`Init()` 返回 `int`（0 = 成功、-1 = 失败），其余接口返回 `float` 物理量或状态码。
- **`main.c`**：`SYSCFG_DL_init()` → `Board_Sensor_Init()` → 各传感器 `Init()` → 主循环 `while(1)` @1Hz。

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
