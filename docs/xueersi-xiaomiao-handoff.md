# 学而思“小喵”移植交接（xiaozhi-esp32）

更新时间：2026-09-15

## 1. 交接结论

`xueersi-xiaomiao` 板型的软件适配已经完成到“可构建、可发布、待实机验收”状态：

- 经典 ESP32-WROVER/B，4 MB Flash、带 PSRAM。
- ICS43434 麦克风与 MAX98357A 功放共用 GPIO25/GPIO26 的 I2S 时钟。
- 麦克风输入和扬声器输出均固定为 **24 kHz、32-bit、mono left slot**。
- 原机 ST7735 屏幕已接入小智 LVGL UI。
- 六个实体按键已接入 PTT、对话、音量和主题操作。
- Wi-Fi 配网、小智协议栈、音频收发及显示代码均已纳入该板型。
- 本地构建、板型回归测试和 GitHub Linux CI 均通过。

当前设备因实验室人员较多已经关机，**24 kHz UI 版本尚未刷入并进行实机验收**。因此不能把屏幕方向/颜色、实体按键、24 kHz 端到端语音、打断和断线重连标为已验证。

私有仓库：<https://github.com/TMMXA/xueersi-xiaomiao-xiaozhi>

分支：`main`

本次写交接前的实现基线：`133e9f1ca2ce249e24f0a75d9bc21a3315fa0148`

上游仓库：<https://github.com/78/xiaozhi-esp32>

## 2. 硬件事实与证据边界

### 2.1 音频接线

| 信号 | GPIO | 方向 |
| --- | ---: | --- |
| WS / LRCLK（共用） | 25 | ESP32 -> MIC / AMP |
| BCLK / SCK（共用） | 26 | ESP32 -> MIC / AMP |
| ICS43434 SD | 32 | MIC -> ESP32 |
| MAX98357A DIN | 33 | ESP32 -> AMP |

GPIO25/GPIO26 是 RX/TX 共用时钟，输入和输出不能配置成不同采样率。当前代码用编译期断言锁定两路相等且均为 24 kHz。

已完成的硬件证据：

- 早期 MicroPython 测试在 16 kHz 下分别验证了 ICS43434 录音和 MAX98357A 放音。
- 电池独立供电时扬声器能够播放测试音。
- 第一阶段无屏小智固件已实机启动到 Wi-Fi 配网热点。

尚未完成的硬件证据：

- 24 kHz 同时收发和小智服务器端到端对话。
- 当前带屏固件的启动、显示、按键、录放音及网络稳定性。

音频模块 VCC 最终焊接到主板的具体电源节点没有可靠记录。若排查噪声、功耗或低电量问题，应先实测 VCC-GND 并追线，不要猜测或改线。

### 2.2 屏幕

原生面板为 ST7735 128x160，当前按 160x128 横屏使用：

| 信号 | GPIO |
| --- | ---: |
| SPI host | SPI2 |
| SCLK | 18 |
| MOSI | 23 |
| MISO | 19 |
| CS | 5 |
| DC | 4 |
| RESET | 无 |

当前配置为 SPI mode 0、40 MHz、RGB、`swap_xy=true`、`mirror_x=true`、不反色。最终方向、RGB/BGR 和反色参数仍要以实机画面为准。

GPIO19 保留在 SPI 总线定义中。原机 TF 卡与屏幕共用 SPI2，TF 卡 CS 为 GPIO22；当前固件未挂载 TF 卡。

### 2.3 按键

按键均为低电平有效：

| 按键 | GPIO | 当前功能 |
| --- | ---: | --- |
| A | 34 | 按下开始 PTT，松开停止收听 |
| B | 12 | 单击切换对话；启动阶段或长按进入配网 |
| 上 | 2 | 音量 +10；长按最大音量 |
| 下 | 13 | 音量 -10；长按静音 |
| 左 | 27 | 浅色主题 |
| 右 | 35 | 深色主题 |

GPIO34/35 没有内部上拉，代码对 A/右键禁用软件上拉，依赖小喵 PCB 上已有的空闲高电平偏置。所有会改变 `Application` 状态的按键回调均通过调度执行；启动状态还加了保护，避免 UI 对象尚未创建时修改主题或音量。

### 2.4 明确不做的硬件范围

- 不继续返修 MPU6050；该补焊尝试未获得可靠识别结果。
- 不重新设计或改变已验证的音频接线，除非出现可复现的硬件故障。
- 不在当前 PTT 基线前加入本地唤醒词或 AEC。
- 不把 TF 卡挂载作为语音链路验收的前置条件。

## 3. 代码落点

主要文件：

- `main/boards/xueersi-xiaomiao/config.h`：24 kHz 音频、LCD 和按键 GPIO。
- `main/boards/xueersi-xiaomiao/xueersi_xiaomiao.cc`：板型、I2S codec、LCD 初始化及按键行为。
- `main/boards/xueersi-xiaomiao/config.json`：经典 ESP32 + PSRAM 的构建元数据。
- `main/boards/xueersi-xiaomiao/README.md`：板型说明和构建入口。
- `main/CMakeLists.txt`：板型注册及适合 160x128 屏幕的 14 px 内置字体。
- `main/idf_component.yml`：`waveshare/esp_lcd_st7735` 依赖。
- `main/display/lcd_display.cc`：按可映射 PSRAM 容量选择缓存，而不是按物理 8 MB 误选过大的缓存档位。
- `scripts/tests/test_xueersi_xiaomiao.py`：引脚、采样率、板型唯一性和运行时约束回归测试。
- `.github/workflows/build.yml`：只测试并构建 `xueersi-xiaomiao` 的私有仓库 CI。

关键实现提交：

| 提交 | 内容 |
| --- | --- |
| `de17a8b` | 第一阶段无屏板型，可作为已知能启动配网的回退点 |
| `601d460` | 加入 ST7735 UI 与六按键控制 |
| `f44eac7` | 用测试锁定小喵硬件配置 |
| `9993d6b` | 启动阶段按键保护 |
| `133e9f1` | CI 收敛为只构建小喵固件 |

## 4. 构建与验证记录

### 4.1 环境与命令

已使用 ESP-IDF v6.1 构建。Windows 环境的导入脚本为：

```powershell
. 'C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1'
```

从仓库根目录构建：

```powershell
python .\scripts\build.py xueersi-xiaomiao `
  --name xueersi-xiaomiao `
  --wake-word disabled
```

构建脚本会生成：

```text
build/merged-binary.bin
```

该文件应从 Flash 偏移 `0x0` 写入。

### 4.2 已确认结果

本地 ESP-IDF 6.1 完整构建与合并通过。最新应用镜像统计：

```text
xiaozhi.bin        0x20e440 bytes
最小 app 分区      0x2f0000 bytes
剩余               0xe1bc0 bytes（30%）
```

板型相关定向测试最终为 16 项全部通过。Windows 上全量主机测试曾运行 73 项，其中 68 项通过，5 项仅因 `WinError 32` 临时目录文件句柄清理失败；没有断言失败。随后 Linux CI 全量运行 85 项测试并全部通过，因此该 Windows 现象不构成功能失败证据。

GitHub Actions 运行：

- Run：<https://github.com/TMMXA/xueersi-xiaomiao-xiaozhi/actions/runs/34948258918>
- 结果：成功。
- 已完成：85 项主机测试、ESP-IDF 6.1 构建、合并固件上传。
- CI artifact：`xiaozhi_xueersi-xiaomiao_133e9f1ca2ce249e24f0a75d9bc21a3315fa0148`
- CI 合并镜像大小：`0x341e39`，写入偏移 `0x0`。

下载该 CI 固件：

```powershell
gh run download 34948258918 `
  --repo TMMXA/xueersi-xiaomiao-xiaozhi `
  --name xiaozhi_xueersi-xiaomiao_133e9f1ca2ce249e24f0a75d9bc21a3315fa0148
```

### 4.3 当前机器上的交付固件

以下文件位于仓库外，不会随 Git clone 自动出现：

```text
F:\杂活\小喵\xiaozhi-xueersi-xiaomiao-24khz-ui-20260915.bin
SHA256  0B17A6FC967E4D56E4B2478A0F5C0816A397518A1BF727D1EC2BD236C7CCB474

F:\杂活\小喵\xiaozhi-xueersi-xiaomiao-24khz-ui-20260915.zip
SHA256  1C5D18805783E019C1425338ADF35DB62B4849803968CC51AD68934F1ABAFEF1
```

ZIP 内含 `merged-binary.bin`。在另一台机器继续时，优先从 GitHub Actions 下载 CI artifact，或从源码重新构建；不要假定本地交付文件与 CI 构建逐字节相同，因为构建元数据可能造成少数字节差异。

## 5. 下次上电的准确验收流程

设备当前处于关机状态。下次实验室条件合适时按以下顺序进行：

1. 上电并连接 USB，确认实际串口号，不要沿用旧的 COM 号。
2. 先做只读连接检查：

   ```powershell
   python -m esptool --chip esp32 --port COMx chip-id
   ```

3. 写入合并镜像。将 `COMx` 和固件路径替换为实际值：

   ```powershell
   python -m esptool --chip esp32 --port COMx --baud 460800 `
     --before default-reset --after hard-reset write-flash `
     --flash-mode dio --flash-size 4MB --flash-freq 40m `
     0x0 'F:\杂活\小喵\xiaozhi-xueersi-xiaomiao-24khz-ui-20260915.bin'
   ```

4. 以 115200 波特率打开串口日志。若板子不能自动进入下载模式，再按板子实际 BOOT/RESET 操作，不要先擦除 Flash。
5. 检查冷启动：无重启循环、PSRAM 初始化正常、UI 能进入。
6. 检查 LCD：横屏 160x128、无偏移/裁切、颜色正确、无镜像、状态和聊天文字可读。
7. 检查六键：
   - B 键进入/切换配网或对话。
   - 上/下键改变音量，长按最大/静音。
   - 左/右键切换浅色/深色主题。
   - A 键按下开始 PTT、松开结束。
8. 完成 Wi-Fi 配网，确认能连接小智服务器。
9. 做 PTT 端到端测试：观察录音开始/结束日志，确认服务器收到语音，并确认返回语音从喇叭播放。
10. 连续做至少三轮对话，再测试播放时打断、Wi-Fi 临时断开后的重连。
11. 保存串口日志，并在本文件末尾记录每一项的通过/失败和复现条件。

如果显示异常，只调整 `config.h` 中方向、镜像、反色或 RGB/BGR 参数；一次只改一个维度并拍照记录。若音频异常，先从串口确认 I2S 初始化、采样率和收发状态，再检查峰值/RMS，暂不改接线。

## 6. 未完成项与通过标准

| 项目 | 当前状态 | 完成标准 |
| --- | --- | --- |
| 24 kHz RX/TX 初始化 | 编译和配置测试通过 | 实机无 I2S 错误且可连续工作 |
| ST7735 UI | 编译通过 | 方向、颜色、刷新和文字均正确 |
| 六按键 | 代码和回归测试通过 | 每个短按/长按行为与表格一致 |
| Wi-Fi / 服务器 | 无屏旧版到过配网热点 | 当前版完成配网并稳定在线 |
| PTT 上行 | 未实测 | A 键控制录音且服务端正确识别 |
| 语音下行 | 未实测 | 回复连续、无明显速率或声道问题 |
| 打断 / 重连 | 未实测 | 多轮、打断和断网恢复均正常 |
| 唤醒词 / AEC | 明确关闭 | PTT 基线通过后再单独评估 |
| TF 卡 / MPU6050 | 不在当前范围 | 不阻塞移植验收 |

只有上表中从 24 kHz RX/TX 到打断/重连的实机项目全部通过，才可以把“小喵上的完整小智移植”标记为完成。

## 7. 回退与继续开发

- 若当前 UI 固件无法启动，可先回退到 `de17a8b` 重建无屏版本，确认基础启动/配网，再二分定位 UI 相关问题。
- 不要用 `git reset --hard` 覆盖未保存的现场日志或本地修改；为实验修复新建分支并单独提交。
- 继续同步上游时，优先检查 `AudioCodec`、`LcdDisplay`、板型注册和构建脚本的接口变化，再运行全量主机测试与单板构建。
- 保留 UART 日志、I2S 初始化日志、音频峰值/RMS、Wi-Fi 状态和协议连接状态，避免只凭听感定位问题。

## 8. 实机验收记录（待填写）

```text
日期：
测试固件 commit / SHA256：
串口：

启动：             PASS / FAIL / 未测
PSRAM：            PASS / FAIL / 未测
LCD 方向与颜色：    PASS / FAIL / 未测
A/B/方向键：        PASS / FAIL / 未测
Wi-Fi 配网：        PASS / FAIL / 未测
服务器连接：        PASS / FAIL / 未测
PTT 上行：          PASS / FAIL / 未测
语音下行：          PASS / FAIL / 未测
连续三轮：          PASS / FAIL / 未测
播放打断：          PASS / FAIL / 未测
断网重连：          PASS / FAIL / 未测

关键日志 / 复现步骤：
```
