# 学而思“小喵”ESP32 掌机：小智 AI 移植交接文档

> [!IMPORTANT]
> 2026-09-15 implementation update: after reviewing device support and the current
> XiaoZhi audio path, the user chose **24 kHz for both microphone input and speaker
> output**. This supersedes the 16 kHz first-stage recommendation below while
> preserving the shared GPIO25/GPIO26 clock requirement.

## 2026-09-15 移植进度

当前私有开发分支已完成可编译的“小智核心 + 小喵 UI”适配：

- 新增板型 `xueersi-xiaomiao`，目标芯片为经典 ESP32。
- 麦克风和扬声器统一使用 24 kHz、32-bit、左声道 I2S 标准模式；BCLK/WS
  共用 GPIO26/GPIO25，RX/TX 分别为 GPIO32/GPIO33。
- 第一阶段固件已实机刷写并启动到 Wi-Fi 配网热点；当时使用无屏版本，尚未完成
  麦克风、扬声器和 PTT 的端到端人工测试。
- 第二阶段已接入 ST7735 160x128 横屏 UI 和六个实体按键，并通过 ESP-IDF 6.1
  完整编译。应用固件约 2.06 MiB，应用分区剩余约 30%。设备关机期间未刷写这版，
  因此屏幕方向、颜色、按键和 24 kHz 语音链路仍需下次上电做硬件验收。

当前按键映射：

| 按键 | GPIO | 小智功能 |
| --- | ---: | --- |
| A | 34 | 按住说话，松开停止 |
| B | 12 | 单击切换对话；启动阶段或长按进入配网 |
| 上 | 2 | 音量 +10；长按最大音量 |
| 下 | 13 | 音量 -10；长按静音 |
| 左 | 27 | 浅色主题 |
| 右 | 35 | 深色主题 |

屏幕使用 SPI2：SCLK=18、MOSI=23、MISO=19、CS=5、DC=4、无硬件复位。
GPIO19 被保留在总线配置中，以便后续按需接回共总线的 TF 卡（CS=22）；当前小智
核心功能不依赖 TF 卡。GPIO34/35 没有内部上拉，按键适配显式禁用了这两路的软件
上拉。

构建命令：

```powershell
python .\scripts\build.py xueersi-xiaomiao --wake-word disabled
```

当前阶段继续保持本地唤醒词和 AEC 关闭；它们不是完成 PTT 对话链路的前置条件。


> 目标：把当前已经完成硬件音频改装的学而思“小喵”ESP32 掌机交接给本地 Codex，继续完成 `xiaozhi-esp32` 软件适配。
>
> 当前原则：**硬件音频链路已经验证正常，后续优先做软件适配，不要再改音频接线。**

---

## 1. 当前硬件状态

### 1.1 主控

- 设备：学而思“小喵”ESP32 编程掌机
- 主控：ESP32-WROVER / ESP32-WROVER-B 系列
- Flash：4 MB
- PSRAM：8 MB
- 当前已刷入 MicroPython，可通过串口 REPL / `mpremote` 调试。
- 当前 MicroPython 使用 `ESP32_GENERIC-SPIRAM` 系列固件；若后续精确依赖版本，请先执行 `os.uname()` 确认。

### 1.2 原机功能

当前掌机仍保留原有屏幕、按键、TF 卡等硬件。

后续若要适配完整 UI，请优先参考：

- `references/xueersi_xiaomiao_schematic.pdf`
- https://github.com/ZyoungInc/xueersi-idf
- https://github.com/pysn2012/xueersi-xiaomiao

不要重新猜测屏幕、按键、SD 卡 GPIO。

---

## 2. 已完成的音频硬件改装

加装模块：

- 麦克风：**ICS43434**
- 功放：**MAX98357A**
- 形式：ICS43434 + MAX98357A 二合一音频模块
- 模块资料：
  - `references/ICS43434_MAX98357_module_reference.pdf`
  - https://www.xinlucity.com/?s=resourcedetail/index/id/544.html

模块主要引脚：

- `WS`：ICS43434 帧/声道时钟
- `SCK`：ICS43434 位时钟
- `SD`：ICS43434 音频数据输出
- `LRCLK`：MAX98357A 帧/声道时钟
- `BCLK`：MAX98357A 位时钟
- `DIN`：MAX98357A 音频数据输入
- `VCC`
- `GND`

---

## 3. 当前音频接线（已实测通过）

```text
ESP32 / 小喵                     ICS43434 + MAX98357A

GPIO25  -----------------------> WS
   |
   +---------------------------> LRCLK

GPIO26  -----------------------> SCK
   |
   +---------------------------> BCLK

GPIO32  <----------------------- SD       (MIC -> ESP32)

GPIO33  -----------------------> DIN      (ESP32 -> SPK)

GND     -----------------------> GND
电源    -----------------------> VCC
```

### 3.1 关键约束

**GPIO25 / GPIO26 是麦克风和功放共享的 I2S 时钟。**

因此：

- `WS` 与 `LRCLK` 共用 GPIO25
- `SCK` 与 `BCLK` 共用 GPIO26
- 麦克风 RX 与扬声器 TX 不能使用互不兼容的采样时钟
- 第一版软件统一采用 **16 kHz**
- 不要沿用某些小智参考板的“MIC 16 kHz / SPK 24 kHz 两套独立时钟”配置

当前已验证基线：

```text
sample_rate = 16000 Hz
BCLK/SCK    = GPIO26
WS/LRCLK    = GPIO25
MIC SD      = GPIO32
SPK DIN     = GPIO33
```

---

## 4. 供电状态

音频模块已经焊接完成，掌机也已经合盖。

**已完成电池独立供电测试：拔掉 USB 后，仅使用掌机电池上电，扬声器启动测试音可以正常播放。**

注意：

- 本交接记录里没有最终记录音频模块 `VCC` 具体焊到了主板的哪个电源节点。
- 不要仅根据旧讨论猜测是 `VBUS`、`BAT+` 或主板 `VCC`。
- 如果后续需要分析电源噪声、功耗或低电量行为，请实测模块 `VCC-GND` 电压并追线确认。

---

## 5. 已完成的 MicroPython 音频验证

### 5.1 麦克风

已验证配置：

```python
I2S(
    0,
    sck=Pin(26),
    ws=Pin(25),
    sd=Pin(32),
    mode=I2S.RX,
    bits=32,
    format=I2S.STEREO,
    rate=16000,
)
```

结果：

- ICS43434 能稳定输出数据
- 左声道有有效数据
- 右声道保持 0
- 对着麦克风说话 / 拍手时，左声道峰值明显跳变
- 曾观察到瞬态峰值约 `3,189,472`
- 可以确认 GPIO25、GPIO26、GPIO32 以及 ICS43434 均工作正常

右声道为 0 是当前模块的正常表现，不应直接判定为故障。

### 5.2 扬声器

已验证配置：

```python
I2S(
    0,
    sck=Pin(26),
    ws=Pin(25),
    sd=Pin(33),
    mode=I2S.TX,
    bits=32,
    format=I2S.STEREO,
    rate=16000,
)
```

结果：

- MAX98357A 可正常驱动喇叭
- GPIO33 TX 正常
- GPIO25 / GPIO26 共用时钟在播放侧正常
- 数字音量调节有效

### 5.3 “快速哒哒声”已定位

第一次播放 440 Hz 测试音时曾出现快速“哒哒”声。

原因是测试程序使用：

```text
sample_rate = 16000
buffer      = 256 samples
tone        = 440 Hz
```

此时：

```text
440 * 256 / 16000 = 7.04 cycles
```

buffer 首尾不连续，循环播放形成约：

```text
16000 / 256 = 62.5 Hz
```

的周期性点击。

改用 1000 Hz（256 点刚好 16 个周期），或使用 400 点 buffer 生成 440 Hz（刚好 11 个周期）后，噪声消失。

**因此该“哒哒声”已确认是测试波形拼接问题，不是硬件故障。**

---

## 6. MPU6050 状态：不要作为下一步重点

主板原本有 MPU6050 空焊位，曾尝试补焊 MPU6050。

结果：

- 整机没有因补焊发生短路
- ESP32 和屏幕仍可正常启动
- MPU6050 一直没有被可靠识别
- 已决定停止继续返修

当前原则：

**不要为了小智移植继续折腾 MPU6050。**

如果未来确实需要姿态传感器，优先考虑外接 I2C 模块。

---

## 7. 下一步：移植 xiaozhi-esp32

目标项目：

- https://github.com/78/xiaozhi-esp32

建议创建新的 board：

```text
xueersi-xiaomiao
```

### 7.1 第一阶段目标：只跑通语音

第一版不要同时做完整 UI、唤醒词、AEC。

优先实现：

```text
按键触发 / 开始录音
        ↓
ICS43434
        ↓
ESP32 / xiaozhi
        ↓
服务器
        ↓
返回语音
        ↓
MAX98357A
        ↓
喇叭
```

建议先做 PTT（Push-to-Talk）：

- A 键：开始 / 结束对话，或按住说话
- 暂时不要先做本地唤醒词
- 暂时不要先做 AEC

### 7.2 音频层 GPIO

```c
BCLK / SCK     GPIO26
WS / LRCLK     GPIO25
MIC_DATA_IN    GPIO32
SPK_DATA_OUT   GPIO33
```

### 7.3 第一版采样率

优先：

```text
MIC = 16000 Hz
SPK = 16000 Hz
```

原因：

1. RX/TX 共享 GPIO25 / GPIO26 时钟。
2. 16 kHz 已经通过 MicroPython 实测。
3. 语音识别/唤醒/对话链路优先保证 16 kHz 输入。
4. 基础链路跑通前不要先引入额外重采样。

### 7.4 I2S 实现重点

MicroPython 测试使用 RX/TX 分开验证，不能直接等同于最终 xiaozhi 的全双工实现。

正式 ESP-IDF 适配应：

- 使用同一组 BCLK / WS
- RX 数据引脚：GPIO32
- TX 数据引脚：GPIO33
- 在 ESP-IDF I2S 驱动层配置兼容的双向数据流
- 确保 RX / TX 共享同一时钟参数
- 不要给 RX/TX 配置两个不同 sample rate

如果当前 `xiaozhi-esp32` 音频抽象默认假设独立 MIC/SPK 时钟，需要新增/修改一个适配类，而不是硬套现有 16k/24k board 配置。

---

## 8. 第二阶段：屏幕和按键

音频链路稳定后再加入：

- ST7735 / ST7735R 160×128 屏幕
- 六个实体按键
- Wi-Fi 状态
- 小智状态 / 表情
- 对话提示
- 音量状态

屏幕、按键、TF 卡等原机 GPIO 请从以下资料读取，不要凭印象重新分配：

- `references/xueersi_xiaomiao_schematic.pdf`
- https://github.com/ZyoungInc/xueersi-idf
- https://github.com/pysn2012/xueersi-xiaomiao

---

## 9. 已知注意事项

### 不要做

1. **不要再修改当前音频硬件接线。**
2. **不要继续返修 MPU6050。**
3. **不要把 MIC 配成 16 kHz、SPK 配成 24 kHz，却仍然要求二者共享 GPIO25/26。**
4. **不要把测试时的 62.5 Hz 点击声误判成硬件噪声。**
5. **不要在没确认当前实际音频供电节点前擅自更改模块供电。**
6. **不要一开始就加入唤醒词、AEC、复杂 UI；先跑通基础双向语音。**

### 建议保留的调试能力

开发阶段至少保留：

- UART 日志
- I2S RX 峰值 / RMS 调试
- I2S TX 纯音测试
- Wi-Fi 状态日志
- 小智连接状态
- 音频 RX/TX 启停日志

---

## 10. 参考资料

### 本交接包内

1. `references/xueersi_xiaomiao_schematic.pdf`
   - 小喵掌机原理图
   - 重点：ESP32-WROVER、扩展 GPIO、LCD、按键、供电、I2C/I2S 备注
   - 原理图明确备注：GPIO25/26 作为 I2S 共用时钟，麦克风/功放采样率必须一致

2. `references/ICS43434_MAX98357_module_reference.pdf`
   - 芯路城 ICS43434 + MAX98357A 二合一模块资料
   - 包含芯片型号、模块引脚说明、5V 标称供电、AI 小智参考接线和测试固件链接

### Web / GitHub

- 小智：
  - https://github.com/78/xiaozhi-esp32

- 小喵 ESP-IDF：
  - https://github.com/ZyoungInc/xueersi-idf

- 小喵社区资料 / MicroPython：
  - https://github.com/pysn2012/xueersi-xiaomiao

- ICS43434 + MAX98357 模块资料页：
  - https://www.xinlucity.com/?s=resourcedetail/index/id/544.html

- ICS43434 Datasheet：
  - https://invensense.tdk.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.2.pdf

- MAX98357A Datasheet：
  - https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf

- MicroPython I2S：
  - https://docs.micropython.org/en/latest/library/machine.I2S.html

---

## 11. 给 Codex 的启动提示

第一轮建议先完成：

1. 拉取/阅读当前 `xiaozhi-esp32`。
2. 找出经典 ESP32（非 ESP32-S3）上最接近的 board。
3. 找到项目当前 AudioCodec / I2S 抽象。
4. 判断现有抽象是否支持：
   - BCLK/WS 共享
   - RX=GPIO32
   - TX=GPIO33
   - RX/TX 同为 16 kHz
5. 给出最小改动方案。
6. 创建 `xueersi-xiaomiao` board。
7. 第一阶段只验证：
   - Wi-Fi
   - 小智服务器连接
   - MIC 上传
   - SPK 播放
8. 上述通过后，再接入屏幕与按键 UI。

### 最重要的当前事实

```text
硬件已经验证正常。

MIC:
  ICS43434
  WS   = GPIO25
  SCK  = GPIO26
  SD   = GPIO32

SPK:
  MAX98357A
  LRCLK = GPIO25
  BCLK  = GPIO26
  DIN   = GPIO33

Audio clock:
  GPIO25 / GPIO26 shared

Known-good baseline:
  16 kHz
  32-bit I2S frame
  Stereo framing
  ICS43434 active on one channel

Battery-only power:
  verified working

Do not redesign the hardware unless a reproducible hardware fault is found.
```
