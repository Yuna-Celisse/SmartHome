# SmartHome — 多传感器环境监测固件

基于 TI MSPM0G3507 的嵌入式环境传感采集系统，运行在 LP-MSPM0G3507 LaunchPad + BOOSTXL-BASSENSORS BoosterPack 平台。

## 硬件架构

| 组件 | 型号 | 接口 | I2C 地址 | 测量量 |
|------|------|------|----------|--------|
| MCU | MSPM0G3507 (Cortex-M0+ @ 32MHz) | — | — | — |
| 温湿度 | HDC2010 | I2C1 (PB2/PB3) | 0x40 | 温度 ±0.2°C、湿度 ±2%RH |
| 高精度温度 | TMP116 | I2C1 (PB2/PB3) | 0x48 | 温度 ±0.2°C (16-bit) |
| 环境光 | OPT3001 | I2C1 (PB2/PB3) | 0x44 | 照度 0.01~83000 lux |
| 霍尔 | DRV5055 | ADC0 CH2 (PA25) | — | 磁通量密度 (mT) |

### 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| PB2 | I2C1 SCL | 传感器总线时钟 |
| PB3 | I2C1 SDA | 传感器总线数据 |
| PA25 | ADC0 CH2 | DRV5055 模拟输入 |
| PB24 | GPIO 输出 (低有效) | HDC2010 使能 |
| PB15 | GPIO 输出 (低有效) | DRV5055 使能 |

## 软件架构

```
SmartHome/
├── app/
│   ├── main.c                    # 主循环：轮询全部传感器 @1Hz
│   ├── board_init.c/h            # I2C/ADC 底层驱动 + 传感器上电
│   └── sensors/
│       ├── sensor_hdc2010.c/h    # HDC2010 温湿度传感器驱动
│       ├── sensor_tmp116.c/h     # TMP116 高精度温度传感器驱动
│       ├── sensor_opt3001.c/h    # OPT3001 环境光传感器驱动
│       └── sensor_drv5055.c/h    # DRV5055 霍尔传感器驱动
├── ti_msp_dl_config.c/h          # SysConfig 生成的外设初始化
├── smart_home.syscfg             # SysConfig 工程文件
├── keil/                         # Keil MDK 工程
│   ├── smart_home.uvprojx
│   └── smart_home.uvoptx
└── ticlang/                      # TI CCS / Makefile 工程
    ├── makefile
    ├── smart_home.projectspec
    └── device_linker.cmd
```

## 构建与烧录

### 环境准备

```bash
git clone --recurse-submodules <repo-url>
```

SDK 依赖通过 git submodule 管理（`mspm0-sdk`，版本 `mspm0_sdk_2_10_00_04`）。

### 方式一：TI Code Composer Studio (推荐)

1. 打开 TI CCS Theia，Import CCS Project，选择 `ticlang/` 目录。
2. Build → Program Target。

### 方式二：Keil MDK

1. 打开 `keil/smart_home.uvprojx`。
2. 确认 SDK 路径配置（已配置为项目相对路径）。
3. Build (F7) → Download (F8)。

### 方式三：命令行 (ticlang makefile)

```bash
cd ticlang
make
```

## 运行行为

- 上电后 `SYSCFG_DL_init()` 初始化 GPIO、I2C1 (400kHz)、ADC12。
- `Board_Sensor_Init()` 等待传感器上电，逐一初始化各传感器。
- 主循环以 **1Hz** 频率轮询全部四个传感器，读取温湿度、照度、磁通量数据。

## 系统参数

| 参数 | 值 |
|------|-----|
| CPU 主频 | 32 MHz |
| I2C 速率 | 400 kHz (Fast Mode) |
| ADC 采样时间 | 62.5 ns |
| 主循环周期 | 1 s |

## 依赖

- [mspm0-sdk](https://github.com/TexasInstruments/mspm0-sdk) (v2.10.00.04) — TI DriverLib、SysConfig、启动文件
- TI Arm Clang Compiler 3.2.0+ 或 Arm Compiler 6
