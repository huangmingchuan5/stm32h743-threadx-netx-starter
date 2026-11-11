# 第三方组件声明

本仓库是一个学习/起步用的示例工程，整合了以下开源与第三方组件。各组件的版权归原作者
所有，本仓库不修改其授权条款；原始版权声明均保留在各文件头部。

| 组件 | 版本 | 目录 / 来源 | 版权方 | 许可 |
|---|---|---|---|---|
| Eclipse ThreadX | 6.x（git 子模块） | `Middlewares/threadx/` | Microsoft / Eclipse ThreadX Contributors | MIT（见 `Middlewares/threadx/LICENSE.txt`） |
| Eclipse NetX Duo | 6.x（git 子模块） | `Middlewares/netxduo/` | Microsoft / Eclipse ThreadX Contributors | MIT（见 `Middlewares/netxduo/LICENSE.txt`） |
| STM32H7xx HAL Driver | STM32Cube H7 固件包 | `Drivers/STM32H7xx_HAL_Driver/` | STMicroelectronics | BSD-3-Clause（见各文件头 `@attention`） |
| CMSIS Core / Device | 5.x | `Drivers/CMSIS/` | Arm Limited / STMicroelectronics | Apache-2.0 / BSD-3-Clause |
| SEGGER RTT | 7.x | `Middlewares/SEGGER_RTT/` | SEGGER Microcontroller GmbH | SEGGER 自有 BSD 式许可（见 `SEGGER_RTT.c` 文件头，保留原声明） |
| ST 以太网驱动 + LAN8742 PHY 驱动 | STM32Cube 扩展包 | `User/netxduo/`、`User/dev/lan8742/` | STMicroelectronics | BSD-3-Clause |
| 安富莱 V7 板级支持 / 例程 | 教程配套 | `User/`（BSP、`tx_initialize_low_level.S` 等） | 安富莱电子 www.armfly.com | 原例程声明 `Copyright (C), 2020-2030, 安富莱电子`，仅供配合教程学习使用 |

## 本仓库对第三方代码所做的改动

1. **构建系统重写**：以 CMake + Ninja 的组织方式替代原例程的 IDE 工程，组件按
   `OBJECT` 库分层（ThreadX / NetX Duo / HAL / BSP / SEGGER RTT）。
2. **链接脚本**：`STM32H743XX_FLASH.ld` 基于 STM32CubeMX 的 H743 脚本改写，新增
   ThreadX 所需的 `_vectors` / `__RAM_segment_used_end__` 符号与多段 RAM 布局。
3. **单镜像化**：去掉原工程的 boot/app 双镜像与固件容器，改为单镜像直接烧录。
4. **未改动**：ThreadX / NetX Duo 的 `common/`、`ports/cortex_m7/gnu/`，以及 HAL、
   CMSIS 本体保持上游原样（仅做目录裁剪与头文件路径适配）。

## 说明

- 原例程《安富莱 ThreadX 内核教程（V7/V6 板）》及配套 BSP 的著作权归安富莱电子所有，
  本仓库不主张任何权利，也不提供任何商业授权。
- 若用于产品化，请自行替换为 ST 官方最新 HAL/CMSIS、Eclipse ThreadX 官方发行版，
  并复核各组件的许可义务。
