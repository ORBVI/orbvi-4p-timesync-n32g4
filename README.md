# ORBVI N32G4 模拟授时固件

为 ORBVI 4P、4V Lite 提供 UART + PPS 测试信号。编译宏单选 ZDA 或 RMC，默认 ZDA，两者共用模拟时钟和 PPS 时序。

## 编译

需要 `arm-none-eabi-gcc`、GNU Make 和 Python 3。

```sh
make                         # 默认 ZDA
make TIME_PROTOCOL=ZDA
make TIME_PROTOCOL=RMC
```

| 选择 | 产物目录 | 每秒报文（含 CRLF） |
| --- | --- | --- |
| ZDA | `build/ZDA/` | `$GPZDA,hhmmss.00,dd,mm,yyyy,00,00*XX` |
| RMC | `build/RMC/` | `$GPRMC,hhmmss.00,V,,,,,,,ddmmyy,,,N*XX` |

固件名为 `n32g4fr_timesync.{elf,hex,bin}`。`Core/Inc/time_protocol.h` 定义 `TIME_PROTOCOL_ZDA`、`TIME_PROTOCOL_RMC`；Makefile 按选择传入 `TIME_PROTOCOL`。两种构建分目录保存，无需清理后切换。

## 接线与时间

- PB1/TIM3_CH4：PPS，1 Hz，上升沿对秒，高电平 10 ms。
- PA2/USART2 TX：UART 输出，115200 8N1，接接收端 RX；双方共地。
- PA3/USART2 RX：可选设时输入，独立演示无需连接。

每个 PPS 上升沿后发送该边沿对应的一条整秒报文，包含 XOR 校验和。复位首个 PPS 从 **2025-01-01 00:00:00 UTC** 起算，随后逐秒递增；这是模拟时间，无需电脑取时。

原有设时命令 `@UTC,YYYYMMDDhhmmss` + LF/CRLF 在下一 PPS 生效，支持 2000–2099 年。RMC 保留原有无定位 `V/N` 格式。

## 验证

`make test` 验证双协议格式、日期、设时和模拟 PPS 配对；两种固件分别构建后运行 `check-image`。4P Demo/产测接收端自动兼容 ZDA/RMC，无需按固件选择协议。

仅在需要单独检查某种输出时使用 `tools/board_link_test.py --protocol ZDA|RMC`；具体参数见 `--help`。实线 UART/PPS 尚需上板验证，主机测试不代表硬件通过。

板端操作见 [ORBVI 4P 使用指南](https://huanshizhineng.feishu.cn/wiki/RxH8wzbVgioHgXk5GqEcy67Uned)。
