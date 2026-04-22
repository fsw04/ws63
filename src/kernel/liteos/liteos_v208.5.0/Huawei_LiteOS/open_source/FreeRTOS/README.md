# README -FreeRTOS 适配说明

## 1. 简介 (Introduction)

本目录包含了从 **FreeRTOS RTOS** 中提取的核心组件及接口代码。为了在 **Huawei LiteOS** 上实现FreeRTOS 兼容接口，我们保留了部分不依赖内核底层的通用工具代码。

## 2. 来源与版本 (Source & Version)

- **开源项目**：FreeRTOS
- **官方仓库**：https://github.com/FreeRTOS/FreeRTOS
- **版本/分支**：v11.1.0

- **文件精简**：仅保留了必要的头文件及源文件。

## 4. 目录结构 (Directory Structure)

```
opensource/
├─FreeRTOS
│  └─FreeRTOS-Kernel-11.1.0
│      └─include
│  └─ README.md      #本文件
```

## 5. 许可协议 (License)

本目录下的 FreeRTOS 源码遵循 **Apache License 2.0** 协议。

- **协议类型**：本目录下的代码源自 [FreeRTOS 项目](https://github.com/FreeRTOS/FreeRTOS)，遵循 **Apache License 2.0** 协议。
- **修改说明**：本目录下的部分文件经过了裁剪与修改以适配 LiteOS。根据协议第 4.b 条规定，所有被修改的文件均已在文件头部记录了修改日志（Change Logs）。
- **详细协议**：详细协议内容请参阅 [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0)。