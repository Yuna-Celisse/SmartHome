# SmartHome — MSPM0G3507 智能电风扇固件

[![ARM GCC Build](https://github.com/Yuna-Celisse/SmartHome/actions/workflows/ticlang-build.yml/badge.svg)](https://github.com/Yuna-Celisse/SmartHome/actions/workflows/ticlang-build.yml)

基于 TI **MSPM0G3507** (Cortex-M0+ @ 32MHz) 的智能电风扇控制系统，运行在 LP-MSPM0G3507 LaunchPad + **BOOSTXL-SENSORS** BoosterPack。

集成四大功能：**光照自动开灯**、**语音开关灯**、**陀螺仪震荡报警**、**温度传感器调速**。UART0 以 **FireWater 协议** 实时输出 8 通道传感器数据至 VOFA+ 上位机。

## 产品功能

| 功能 | 传感器 | 描述 |
|------|--------|------|
| 🌞 **光照自动开灯** | OPT3001 | lux < 50 自动开灯，> 100 自动关灯（滞回防抖） |
| 🎤 **语音开关灯** | 语音模块 (UART3) | 语音命令 0x01/0x02 手动控制灯光，覆盖自动模式 |
| 🚨 **陀螺仪震荡报警** | BMI160 | 任意轴角速率 > 150°/s 触发 LED 5Hz 闪烁报警（5s 冷却） |
| 🌡️ **温度传感器调速** | BME280 | 温度 25–35°C 自动映射 5 档风扇 PWM（0%–100%） |

## 硬件

| 传感器 | 型号 | 接口 | I2C 地址 | 测量量 |
|--------|------|------|----------|--------|
| 陀螺仪 | BMI160 | I2C1 | 0x69 | 3 轴角速率 (±2000°/s) |
| 温湿度 | BME280 | I2C1 | 0x77 | 环境温度 (-40~85°C) |
| 环境光 | OPT3001 | I2C1 | 0x47 | 照度 (0.01~83000 lux) |
| 语音模块 | — | UART3 | — | 5 字节协议 (AA 55 [type] [cmd] FB) |

### 引脚分配

| 引脚 | 功能 |
|------|------|
| PA0 | LED / 灯光输出（低有效） |
| PA1 | 风扇 PWM 输出（TIMA0 CCP1, 25kHz） |
| PA10 | UART0 TX → FireWater 数据输出 |
| PA11 | UART0 RX → AT 指令转发至 ESP8266 |
| PB2 | I2C1 SCL |
| PB3 | I2C1 SDA |
| PB6 | UART1 TX → ESP8266 |
| PB7 | UART1 RX → ESP8266 |
| PB12 | UART3 TX → 语音模块 |
| PB13 | UART3 RX → 语音模块 |
| PA22 | DRV5055 电源使能（低有效） |
| PA24 | OPT3001 电源使能（高有效） |
| PB24 | HDC2010 电源使能（低有效，保留） |

## 语音协议

5 字节固定帧：`AA 55 [type] [cmd] FB`

| 命令 | 字节 | 功能 |
|------|------|------|
| `LIGHT_ON` | 0x01 | 手动开灯（切换至 MANUAL 模式） |
| `LIGHT_OFF` | 0x02 | 手动关灯（切换至 MANUAL 模式） |
| `ALARM` | 0x26 | 触发震荡报警（5s LED 闪烁） |
| `INIT` | 0x67 | MCU 上电广播 (type=0xFF) |

## 构建

### 方式一：Keil MDK

```
打开 keil/smart_home.uvprojx → F7 构建 → F8 烧录
```

需要 Keil MDK 5.40 + ARM Compiler 6.22。MSPM0G3507 (Cortex-M0+) 在 Community Edition 免费许可范围内。

### 方式二：TI CCS / 命令行

```bash
cd ticlang && make
```

需要 TI Arm Clang 3.2.0+ + SysConfig 1.24.0 + MSPM0 SDK。

### 方式三：ARM GCC（CI 使用）

```bash
cd ticlang && make -f Makefile.gcc
```

需要 `arm-none-eabi-gcc`。无需 SysConfig（生成文件已提交仓库）。

## FireWater 输出协议

UART0 (115200 8N1) 以 ~1Hz 频率输出光照度数据：

```
123.4\r\n
```

VOFA+ 配置：协议 FireWater，通道数 1（lux）。

## 软件架构

```
SmartHome/
├── app/
│   ├── main.c                    # Tick 调度器：光照/温度/陀螺仪/报警 + lux FireWater
│   ├── system_state.h            # 跨模块共享状态（枚举、全局变量、阈值）
│   ├── board_init.c/h            # I2C/UART/LED/ADC/TIMA0-PWM 底层驱动
│   ├── voice_protocol.c/h        # 语音模块 5 字节协议解析与命令分发
│   └── sensors/
│       ├── sensor_bmi160.c/h     # BMI160 陀螺仪（raw DL I2C）
│       ├── sensor_bme280.c/h     # BME280 温度（raw DL I2C）
│       └── sensor_opt3001.c/h    # OPT3001 照度（raw DL I2C）
├── ti_msp_dl_config.c/h          # SysConfig 生成（已提交）
├── smart_home.syscfg             # SysConfig 工程
├── keil/                         # Keil MDK 工程
├── ticlang/                      # TI Clang + GCC makefile
├── mspm0-sdk/                    # TI SDK (git submodule)
└── .github/workflows/            # CI: ARM GCC Build
```

### 主循环调度

基于 10ms tick 的协作式调度器，各任务独立间隔：

| 任务 | 间隔 | 操作 |
|------|------|------|
| BMI160 陀螺仪 | 100ms | 读取三轴角速率，超阈值触发报警 |
| OPT3001 光照 | 800ms | 读取 lux，AUTO 模式下滞回控灯 |
| BME280 温度 | 1000ms | 读取温度，5 档映射风扇 PWM |
| FireWater 输出 | 1000ms | 单通道 lux → VOFA+ |
| LED 状态机 | 10ms | 报警 5Hz 闪烁 / 正常跟随灯光状态 |

### 温度→风扇调速映射

| 温度范围 | 风扇档位 | PWM 占空比 |
|---------|---------|-----------|
| < 25°C | OFF (0) | 0% |
| 25–28°C | LOW (1) | 25% |
| 28–32°C | MED (2) | 50% |
| 32–35°C | HIGH (3) | 75% |
| > 35°C | MAX (4) | 100% |

> 所有传感器驱动统一使用原始 DL I2C API（`DL_I2C_fillControllerTXFIFO` +
> `startControllerTransfer`），绕过 `Board_I2C_WriteReg` 的寄存器写损坏问题。

## CI

每次 push/PR 自动在 ubuntu-latest 上以 ARM GCC 构建，上传 hex 产物。

## 许可

MIT
