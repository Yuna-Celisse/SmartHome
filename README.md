# SmartHome — MSPM0G3507 多传感器固件

[![ARM GCC Build](https://github.com/Yuna-Celisse/SmartHome/actions/workflows/ticlang-build.yml/badge.svg)](https://github.com/Yuna-Celisse/SmartHome/actions/workflows/ticlang-build.yml)

基于 TI **MSPM0G3507** (Cortex-M0+ @ 32MHz) 的嵌入式多传感器采集系统，运行在 LP-MSPM0G3507 LaunchPad + **BOOSTXL-SENSORS** BoosterPack。

UART0 以 **FireWater 协议** 实时输出 OPT3001 光照度数据至 VOFA+ 上位机，同时板载 BME280 环境温度和 BMI160 陀螺仪驱动可用。

## 硬件

| 传感器 | 型号 | 接口 | I2C 地址 | 测量量 |
|--------|------|------|----------|--------|
| 陀螺仪 | BMI160 | I2C1 | 0x69 | 3 轴角速率 (±2000°/s) |
| 温湿度 | BME280 | I2C1 | 0x77 | 环境温度 (-40~85°C) |
| 环境光 | OPT3001 | I2C1 | 0x44 | 照度 (0.01~83000 lux) |
| 语音模块 | — | UART3 | — | 5 字节协议 (AA 55 [type] [cmd] FB) |

### 引脚分配

| 引脚 | 功能 |
|------|------|
| PA0 | LED（低有效） |
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

UART0 (115200 8N1) 以 ~1.25Hz 频率输出 CSV 格式光照度数据：

```
123.4\r\n
```

VOFA+ 配置：协议 FireWater，通道数 1（lux）。

## 软件架构

```
SmartHome/
├── app/
│   ├── main.c                    # ~1.25Hz 光照度 FireWater 上报
│   ├── board_init.c/h            # I2C/UART/LED 底层驱动
│   ├── voice_protocol.c/h        # 语音模块 5 字节协议
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

> 所有传感器驱动统一使用原始 DL I2C API（`DL_I2C_fillControllerTXFIFO` +
> `startControllerTransfer`），绕过 `Board_I2C_WriteReg` 的寄存器写损坏问题。

## CI

每次 push/PR 自动在 ubuntu-latest 上以 ARM GCC 构建，上传 hex 产物。

## 许可

MIT
