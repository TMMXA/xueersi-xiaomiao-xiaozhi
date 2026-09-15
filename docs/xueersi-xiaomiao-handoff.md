# 学而思“小喵”移植交接（xiaozhi-esp32）

更新时间：2026-09-15 23:00（Asia/Hong_Kong）

## 1. 当前结论

`xueersi-xiaomiao` 已完成显示、按键、配网、激活、联网和扬声器下行，但**麦克风上行仍未打通，不能把完整移植标记为成功**。

已由实机确认：

- 经典 ESP32-WROVER/B、4 MB Flash、PSRAM 启动正常。
- ST7735 以 160x128 横屏显示，方向、颜色正常且没有裁切。
- Wi-Fi 配网、设备激活、MQTT 登录成功。
- 扬声器能够正常播放提示音和服务器语音。
- 上/下键调节音量，左/右键切换明暗主题，A/B 键事件均能进入代码。
- 轻量表情资源直接写入 assets 分区后能够正常显示。

当前阻塞问题：

- A 键 PTT 和 B 键自动对话均能进入 `listening`。
- 串口确认 `AudioCodec: Set input enable to true`。
- 小智平台没有产生历史消息，设备没有收到识别文本或回复。
- 服务器会在没有识别结果后关闭音频会话。
- 2026-09-15 晚间已刷入“物理双声道 I2S 帧”实验驱动，但串口仍未出现任何麦克风 `peak/rms` 日志。因此目前更像是 I2S RX 没有返回帧，而不只是音量太小。

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
- `main/boards/xueersi-xiaomiao/xiaomiao_audio_codec.{h,cc}`：2026-09-15 新增的实验音频驱动。
- `main/boards/xueersi-xiaomiao/config.json`：ESP32 + PSRAM 构建元数据。
- `scripts/tests/test_xueersi_xiaomiao.py`：板型约束回归测试。
- `.github/workflows/build.yml`：私人仓库只构建小喵板型。

实验驱动的意图：

- 物理总线改成 32-bit、stereo、both slots，与 MicroPython 已知可用帧格式一致。
- 麦克风只取 WS-low 的左槽，转换为逻辑单声道。
- 扬声器把同一 PCM 复制到左右两个物理槽，避免 MAX98357A 声道选择差异。
- 每秒打印一次 `Microphone level: peak=... rms=...`，用于确认 RX 是否返回数据。

重要：该驱动**已编译并刷入，但还没有证明修复成功**。第一次实测 listening 期间完全没出现电平日志。下一步应先给 `Read()` 的进入和错误返回加限频日志，而不是继续调整增益或服务器参数。

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

### 5.2 首要诊断修改

在 `XiaomiaoAudioCodec::Read()` 中加入限频日志：

1. 第一次进入 `Read()` 时打印一次，证明输入任务已调用。
2. 保存 `i2s_channel_read()` 返回的 `esp_err_t`。
3. 失败时每秒打印一次 `esp_err_to_name(error)`，不能每 10 ms 刷屏。
4. 成功时保留现有 peak/RMS 日志。

判断分支：

- 从未进入 `Read()`：检查 `AS_EVENT_AUDIO_PROCESSOR_RUNNING`、`AudioInputTask` 和 `EnableVoiceProcessing(true)`。
- 进入但持续 `ESP_ERR_TIMEOUT`：检查 RX channel 是否真正产生 BCLK/WS，必要时验证配对 TX/RX 的 enable 顺序。
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
| 麦克风 I2S RX 返回 PCM | FAIL / 未定位 |
| STT 与平台历史 | FAIL |
| 完整多轮对话 | BLOCKED BY MIC |
| 打断、断网重连 | 待测 |
| 固件 OTA | 当前分区不支持 |
| 唤醒词 / AEC | 明确不在经典 ESP32 PTT 基线内 |

只有麦克风 PCM、STT、TTS、多轮、打断和重连全部通过后，才可将完整移植标记为完成。

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
