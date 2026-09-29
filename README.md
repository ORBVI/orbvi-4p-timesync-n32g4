# ORBVI 4P N32G4 模拟授时固件

用于 ORBVI 4P 的 PPS 与串口时间报文联调，在 N32G4 上生成模拟时间信号。

## 功能

- PPS 输出：生成 1 Hz 秒脉冲。
- 串口时间报文：以 115200 波特率输出 NMEA GPRMC 报文。
- 演示时钟：复位后从 `2025-01-01 00:00:00 UTC` 开始逐秒递增。
- 时间设置：接收串口设时指令，在下一次 PPS 上升沿生效。

## 使用文档

板端 PPS 与串口时间报文的接收、验证流程，请按 [ORBVI 4P 使用指南](https://huanshizhineng.feishu.cn/wiki/GoeUwlTiRiaY5skses2crmPFnCg?fromScene=spaceOverview) 中的时钟同步章节操作。

## 源码目录

- [`Core/`](Core/)：授时逻辑与硬件配置。
- [`firmware/`](firmware/)：启动代码和外设驱动。
- [`tools/`](tools/)：板端联调工具。
- [`tests/`](tests/)：协议与固件校验测试。
