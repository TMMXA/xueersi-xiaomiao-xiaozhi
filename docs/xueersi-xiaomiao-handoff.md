# 学而思“小喵”移植交接（xiaozhi-esp32）

更新时间：2026-09-16（Asia/Hong_Kong）

## 1. 当前结论

`xueersi-xiaomiao` **麦克风上行与多轮对话已在实机打通**（在 TX 时钟保持运行时）。

2026-09-16 串口日志（编译时间 Sep 15 22:44:45 的 stereo-framed 驱动）确认：

- A 键 PTT 进入 listening 后持续出现 `XiaomiaoAudio: Microphone level: peak=... rms=...`。
- STT 正确回显：`你好呀。` / `今天天气怎么样？` / `呃，以后你可以称呼我为梅老板。` 等。
- TTS 多轮回复正常，含 `% get_weather...` / `% search_music...` / `% play_music...` 工具调用。
- 音量、主题、扬声器下行仍正常。

根因与修复（2026-09-16 已刷机确认）：

1. **根因**：全双工 RX 为 slave，BCLK/WS 由 TX 生成。电源管理 `Set output enable to false` 后，再 PTT 只开 RX，麦克风无数据甚至死机。
2. **修复**：`XiaomiaoAudioCodec::EnableInput()` 在启用 RX 前若 TX 未开则先拉起 TX；`Read()` 增加限频诊断日志。刷入 `0x10000` 后回归「等 TX 关闭 → 再 PTT」通过。
3. 说话时常见 `peak=32768`，是 `abs(INT16_MIN)` 的满量程削波，STT 仍可用；后续可微调 `>>12` 或输入增益，不阻塞验收。

启动日志中的关键硬件提示：

```text
W (2084) i2s_common: the rx channel on I2S0 is switched from master to slave for full-duplex mode
I (2084) XiaomiaoAudio: Stereo-framed duplex I2S created at 24000 Hz
```

全双工时 RX 被切到 slave，时钟完全由 TX 提供——与上述失效模式一致。

私人仓库：<https://github.com/TMMXA/xueersi-xiaomiao-xiaozhi>

分支：`main`

上游仓库：<https://github.com/78/xiaozhi-esp32>

## 2. 硬件与不可猜测项

### 2.1 音频接线

| 信号 | GPIO | 方向 |
| --- | ---: | --- |
| WS / LRCLK（麦克风与功放共用） | 25 | ESP32 -> MIC / AMP |
| BCLK / SCK（麦克风与功放共用） | 26 | ESP32 -> MIC / AMP |
| ICS43434 SD | 32 | MIC -> ESP32 |
| MAX98357A DIN | 33 | ESP32 -> AMP |

GPIO25/GPIO26 共用，所以 RX/TX 必须采用同一采样率。当前目标采样率为 24 kHz。

早期已知可用的 MicroPython 麦克风基线：

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

该基线中：

- 左声道有有效 24-bit 数据，右声道为 0。
- 说话/拍手时峰值明显变化，曾见瞬态峰值约 `3,189,472`（右移 8 位后的 24-bit 数值）。
- 因而 GPIO25、GPIO26、GPIO32 和 ICS43434 至少在 MicroPython 16 kHz、32-bit stereo framing 下工作过。

音频模块 VCC 最终焊到主板哪个电源节点仍无可靠记录。排查供电时必须实测 VCC-GND 并追线，不要猜成 VBUS、BAT+ 或某个主板 VCC。

### 2.2 屏幕

ST7735 原生 128x160，当前以 160x128 横屏使用：

| 信号 | GPIO |
| --- | ---: |
| SPI host | SPI2 |
| SCLK | 18 |
| MOSI | 23 |
| MISO | 19 |
| CS | 5 |
| DC | 4 |
| RESET | 无 |

实机确认当前 SPI mode 0、40 MHz、RGB、`swap_xy=true`、`mirror_x=true`、不反色的画面正常。

原机 TF 卡与屏幕共用 SPI2，TF 卡 CS 为 GPIO22；当前固件未挂载 TF 卡。

### 2.3 按键

按键均为低电平有效：

| 按键 | GPIO | 功能与实测状态 |
| --- | ---: | --- |
| A | 34 | 按下开始 PTT、松开停止；串口事件及状态切换已确认 |
| B | 12 | 单击切换自动对话、长按配网；单击进入 listening 已确认 |
| 上 | 2 | 音量 +10；已确认 |
| 下 | 13 | 音量 -10；已确认 |
| 左 | 27 | 浅色主题；已确认 |
| 右 | 35 | 深色主题；已确认 |

GPIO34/35 无内部上拉，A/右键依赖小喵 PCB 的空闲高电平偏置。

## 3. 当前代码状态

主要落点：

- `main/boards/xueersi-xiaomiao/config.h`：24 kHz 音频、LCD 和按键引脚。
- `main/boards/xueersi-xiaomiao/xueersi_xiaomiao.cc`：板型、LCD、按键与 codec 实例。
- `main/boards/xueersi-xiaomiao/xiaomiao_audio_codec.{h,cc}`：2026-09-15 新增的实验音频驱动；2026-09-16 起启用 TX 时钟优先 + Read 诊断日志。
- `main/boards/xueersi-xiaomiao/config.json`：ESP32 + PSRAM 构建元数据。
- `scripts/tests/test_xueersi_xiaomiao.py`：板型约束回归测试。
- `.github/workflows/build.yml`：私人仓库只构建小喵板型。

实验驱动的意图：

- 物理总线改成 32-bit、stereo、both slots，与 MicroPython 已知可用帧格式一致。
- 麦克风只取 WS-low 的左槽，转换为逻辑单声道。
- 扬声器把同一 PCM 复制到左右两个物理槽，避免 MAX98357A 声道选择差异。
- 每秒打印一次 `Microphone level: peak=... rms=...`，用于确认 RX 是否返回数据。

重要：2026-09-15 stereo 帧驱动曾出现「TX 已开则麦克可用、TX 关掉后再 PTT 失效」。2026-09-16 已构建并刷入 EnableInput-TX 时钟修复（仅应用分区 `0x10000`），实机回归通过。诊断日志分支：

1. 出现 `Read() entered` → 输入任务已调用 codec。
2. 出现 `Enabled TX clock source before RX` → 时钟源强制路径已生效。
3. 出现 `i2s_channel_read failed: ESP_ERR_TIMEOUT` → 时钟仍不跑，再查 TX 是否真正驱动 BCLK（或需要周期性写静音）。
4. 出现 `Microphone level: peak=...` → I2S RX 已通，再查 STT/Opus/UDP。
5. peak/rms 恒 0 但有电平日志 → 查 ICS43434 供电、L/R 槽、GPIO32、位对齐。

## 4. 2026-09-15 故障复现证据

### 4.1 原 mono-left 驱动

A 键两次 PTT 均正常触发：

```text
XueersiXiaomiao: A/PTT pressed: start listening
StateMachine: State: idle -> listening
AudioCodec: Set input enable to true
XueersiXiaomiao: A/PTT released: stop listening
StateMachine: State: listening -> idle
AudioCodec: Set input enable to false
```

但是平台无历史消息，也没有 STT 文本或 TTS 回复；之后收到服务器 goodbye 并关闭音频通道。

B 键自动对话也复现。首次出现一次临时 DNS 失败：

```text
EspUdp: Failed to get host by name
MQTT: Failed to connect UDP audio channel: DNS resolution failed
```

设备随后自动重试成功并进入 listening，仍没有识别结果。因此这次 DNS 抖动不是持续故障的根因。

### 4.2 stereo-framed 实验驱动

2026-09-15 22:46 使用 ESP-IDF v6.1 构建成功：

```text
xiaozhi.bin size     0x20e890（2,156,688 bytes）
app partition        0x2f0000
remaining            0xe1770（30%）
SHA256                58F2BF34B6A150DD928AEB978EADF2EC94204D0CF7BC27B7BAA2A5B9512B7BEB
```

仅将应用写到 `0x10000`，没有覆盖 assets、NVS、Wi-Fi 或激活信息。esptool 写入后 hash verified。

新固件启动后：

- MQTT 联网、激活状态恢复正常。
- `LiteAudioEngine: Initialized, WakeNet: no`，符合经典 ESP32 的 PTT 方案。
- 两次 B 键测试均进入 listening，并执行 `Set input enable to true`。
- 没有出现 `XiaomiaoAudio: Microphone level` 日志。
- 数秒后会话被关闭，仍无平台历史。

当前最有价值的判据是“没有任何一次成功 Read 的电平日志”。但还需加错误码日志，才能确认是 `i2s_channel_read()` 超时、输入任务未调用 Read，还是启用顺序问题。

## 5. 下一台电脑的第一步

### 5.1 克隆与环境

必须克隆到纯英文路径；ESP-IDF v6.1 在当前中文路径下发生过 Kconfig/CMake 路径乱码：

```powershell
git clone https://github.com/TMMXA/xueersi-xiaomiao-xiaozhi.git C:\work\xiaomiao-xiaozhi
cd C:\work\xiaomiao-xiaozhi
. 'C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1'
$env:PYTHONUTF8 = '1'
```

构建：

```powershell
python .\scripts\build.py xueersi-xiaomiao `
  --name xueersi-xiaomiao `
  --wake-word disabled
```

### 5.2 首要诊断与已落地修改

已在代码中完成并刷机验证：

1. `Read()` 第一次进入时打印一次，证明输入任务已调用。
2. 保存 `i2s_channel_read()` 返回的 `esp_err_t`。
3. 失败时每秒打印一次 `esp_err_to_name(error)`。
4. 成功时保留 peak/RMS 日志。
5. `EnableInput(true)` 时若 TX 未开，先使能 TX 再使能 RX。

判断分支：

- 没有 `Read() entered`：检查 `AS_EVENT_AUDIO_PROCESSOR_RUNNING`、`AudioInputTask` 和 `EnableVoiceProcessing(true)`。
- 有 `Read() entered` 但持续 `ESP_ERR_TIMEOUT`：TX 时钟仍未驱动 BCLK/WS；下一步考虑在 RX 活跃期间周期性写入静音，或用示波器确认 GPIO25/26。
- Read 成功但 peak/rms 恒 0：再检查 ICS43434 供电、L/R 槽选择、GPIO32 和位对齐。
- peak/rms 随说话变化但平台仍无历史：再检查 24 kHz -> 16 kHz 重采样、Opus 编码队列和 UDP 发送。

不要一开始改麦克风增益；数字 I2S 数据都没读到时，增益不是根因。

### 5.3 刷机与保护已有资源

确认串口号后，只更新应用分区以保留已验证的轻量表情和设备配置：

```powershell
python -m esptool --chip esp32 -p COMx -b 460800 `
  --before default-reset --after hard-reset write-flash `
  --flash-mode dio --flash-size 4MB --flash-freq 40m `
  0x10000 build\xiaozhi.bin
```

完整 `merged-binary.bin` 从 `0x0` 写入会覆盖 assets；除非明确需要恢复整机镜像，否则不要这样做。

串口为 115200。测试顺序：

1. B 短按进入自动 listening，说一句完整话并等待。
2. 再按住 A 说话、停半秒后松开，验证 manual PTT。
3. 同时观察 peak/RMS、STT 文本、平台历史和 TTS。
4. 上行通后再回归扬声器、主题、音量和表情，避免音频帧修改破坏下行。

## 6. Assets 与 OTA 现状

4 MB 当前分区为 factory-only 应用 + 1 MB assets，没有双 OTA app slot。启动日志会显示：

```text
Ota: Running from factory partition, skipping
```

因此固件 OTA 目前不可用；主题/assets 更新是另一条链路。

用户生成的 2.13 MB `assets.bin` 超过 1 MB 分区，不能写入。后来使用轻量资源：

```text
D:\UserData\Downloads\assets (2).bin
size    702,321 bytes
SHA256  7F197A08EB6BEC8202D25F2FCBACA2348628C8BE49A1950CEC5583841D3FD979
```

该文件直接写入 `0x300000` 后，用户确认表情正常显示。它不在 Git 仓库内，换电脑后需单独复制。

## 7. 完成标准

| 项目 | 当前状态 |
| --- | --- |
| 启动 / PSRAM | PASS |
| LCD 方向、颜色、裁切 | PASS |
| Wi-Fi 配网 / 激活 / MQTT | PASS |
| 上下左右键 | PASS |
| A/B 事件与 listening 状态 | PASS |
| 语音下行 / 扬声器 | PASS |
| 轻量表情 assets | PASS（直接写分区） |
| 麦克风 I2S RX 返回 PCM | PASS |
| STT 与平台历史 | PASS |
| 完整多轮对话 | PASS |
| TX 关闭后再 PTT | PASS（EnableInput 先拉 TX） |
| 打断、断网重连 | 待测 |
| 固件 OTA | 当前分区不支持 |
| 唤醒词 / AEC | 明确不在经典 ESP32 PTT 基线内 |

PTT 基线（麦克风、STT、多轮、TX 关闭后再听）已通过。完整移植仍待测：打断、断网重连；固件 OTA 当前分区不支持。

## 8. 回退与范围

- `de17a8b`：第一阶段无屏、能启动到配网热点的回退点。
- 不返修 MPU6050；它不阻塞语音移植。
- 不把 TF 卡挂载作为当前验收前置条件。
- 不改已知接线，除非获得可重复的电气测量证据。
- 不使用 `git reset --hard` 丢弃现场修改或日志。

## 9. 当前设备最后已知状态

最后一次操作是：

- COM12，经 GD32 USB 串口桥连接。
- 已刷入 SHA256 为 `58F2BF...B7BEB` 的实验 `xiaozhi.bin` 到 `0x10000`。
- esptool 写入校验通过。
- 设备正常重启、联网并恢复激活。
- assets 分区没有被此次应用更新覆盖。

换电脑后不要假设仍为 COM12，应重新枚举串口。
