# stm32h743-threadx-netx-starter

**STM32H743 + ThreadX + NetX Duo 的极简起步模板**：CMake 分层构建、单镜像直接烧录、SEGGER RTT 零成本调试——一条 `./run.sh flash` 从源码到上板。

![Platform](https://img.shields.io/badge/platform-STM32H743-03234B?style=flat-square)
![Core](https://img.shields.io/badge/core-Cortex--M7%20480MHz-1f6feb?style=flat-square)
![RTOS](https://img.shields.io/badge/RTOS-ThreadX-3C6EBA?style=flat-square)
![TCP/IP](https://img.shields.io/badge/TCP%2FIP-NetX%20Duo-1E7B4D?style=flat-square)
![Build](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-064F8C?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)

> 面向初学者的可运行模板：不玩 boot/app 双镜像、不玩固件容器，一个 `*.elf`、一套清晰的
> CMake 模块、一个能直接看到日志的 RTT 虚拟串口。把"跑起来"这件事的门槛压到最低。

---

## 亮点

- **单镜像，一个 elf 直接烧**：没有 bootloader、没有固件容器、没有二次跳转。
- **CMake + Ninja 分层**：ThreadX / NetX Duo / HAL / BSP / SEGGER RTT 各为独立
  `OBJECT` 库，加一个组件只改一个 `cmake/*.cmake`。
- **一条命令构建 + 烧录**：`./run.sh flash` 自动探测工具链、构建、OpenOCD 烧录、校验、复位。
- **不用接串口线**：所有日志走 **SEGGER RTT**，通过 SWD 传到电脑，4 个虚拟终端分门别类。
- **开箱即用的网络**：DHCP 自动取 IP + TCP echo 服务器/客户端示例。
- **可移植的模板习惯**：工程名、产物名、烧录目标全部自动跟随目录名，复制重命名即新工程。

---

## 硬件与软件

**硬件**：安富莱 V7 开发板（STM32H743XIHx，Cortex-M7 @480MHz，LAN8742A PHY）、
DAPLink 或 ST-Link 调试器、一根网线（接路由器以使用 DHCP）。

**工具链**（本项目在以下版本验证通过）：

| 工具 | 版本 | 用途 |
|---|---|---|
| arm-none-eabi-gcc | 15.3.rel2 | 交叉编译 |
| CMake | 4.4.5（≥ 3.22） | 构建系统 |
| Ninja | 1.13.2 | 构建后端 |
| OpenOCD | xpack 0.12.0-7 | 烧录 / 调试 |
| SEGGER RTT Viewer | 任意 | 查看 RTT 日志（可选） |

`run.sh` 在 `PATH` 找不到工具时，会自动回退到 `D:/Embedded_Toolchain/...` 下的本机安装
路径；换了机器请改脚本顶部的 `TOOLCHAIN_ROOT`。

---

## 快速开始

```bash
# 1) 克隆 + 拉取子模块（ThreadX / NetX Duo 是 git 子模块）
git clone https://github.com/huangmingchuan5/stm32h743-threadx-netx-starter.git
cd stm32h743-threadx-netx-starter
git submodule update --init --recursive

# 2) 构建 + 烧录
./run.sh flash
```

只想编译不烧录：

```bash
./run.sh                  # Debug 构建
BUILD_TYPE=Release ./run.sh
./run.sh clean            # 清空 build/ 后重建
JOBS=2 ./run.sh           # 编译卡住时降低并行度（见「常见问题」）
```

构建产物（文件名自动跟随工程目录名）：

```
build/<项目名>.elf   # 带调试符号，烧录用
build/<项目名>.hex   # OpenOCD / ST-Link
build/<项目名>.bin
build/<项目名>.map   # 段/符号分布
```

---

## 目录结构

```
stm32h743-threadx-netx-starter/
├─ CMakeLists.txt            # 顶层：单可执行目标 + 后处理(hex/bin/size)
├─ CMakePresets.json         # Debug / Release 预设
├─ run.sh / flash / flash-reset   # 一键构建与烧录（OpenOCD 配置）
├─ startup_stm32h743xx.s     # 中断向量表（.isr_vector）
├─ STM32H743XX_FLASH.ld      # 链接脚本：AXI SRAM 主 RAM，提供 _vectors 等符号
├─ cmake/
│  ├─ gcc-arm-none-eabi.cmake  # 工具链：CPU/FPU/编译/链接公共选项
│  ├─ threadx.cmake            # ThreadX → OBJECT 库
│  ├─ netxduo.cmake            # NetX Duo → OBJECT 库（含 DHCP addon）
│  ├─ stm32h7.cmake            # STM32H7 HAL → OBJECT 库
│  ├─ segger_rtt.cmake         # SEGGER RTT → OBJECT 库
│  └─ bsp.cmake                # 板级初始化 → OBJECT 库
├─ Drivers/                  # CMSIS + STM32H7xx HAL
├─ Middlewares/
│  ├─ threadx/               # ★ git 子模块：ThreadX
│  ├─ netxduo/               # ★ git 子模块：NetX Duo
│  └─ SEGGER_RTT/            # RTT 源码
└─ User/
   ├─ main.c                 # 入口：System_Init → tx_kernel_enter
   ├─ core/                  # system / hal_msp / it / timebase / syscalls
   ├─ bsp/                   # MPU/缓存/480MHz 时钟/RTT 初始化、以太网 BSP
   ├─ app/                   # ThreadX 引导 + 多终端 printf + 网络应用
   ├─ netxduo/               # nx_user.h + STM32 以太网驱动 + PHY 驱动
   ├─ dev/lan8742/           # LAN8742A PHY 驱动
   └─ threadx/               # tx_user.h + tx_initialize_low_level.S
```

---

## 它是怎么工作的（关键设计）

### 1. 为什么 ThreadX 必须用 `OBJECT` 库

ThreadX 在 `tx_thread_schedule.S` 里定义了**强符号** `PendSV_Handler`，在
`tx_initialize_low_level.S` 里定义了**强符号** `SysTick_Handler`，用来**覆盖** CMSIS
启动文件里的 `.weak` 空实现——否则线程切换和 1 kHz 系统节拍就丢了。

若用静态库 `.a`，链接器是按需抽取的：启动文件已用 `WEAK` 满足了引用，就不会去静态库里
取 ThreadX 的目标文件，结果是**调度心跳静默消失、程序跑不起来**。

用 `add_library(xxx OBJECT ...)`，所有 `.o` 无条件参与链接，强符号自然覆盖弱符号。这就是
`cmake/*.cmake` 全部采用 `OBJECT` 库的原因。

### 2. NetX Duo 的 GNU 端口是"纯头文件"

`Middlewares/netxduo/ports/cortex_m7/gnu/` 只有 `inc/nx_port.h`，没有要单独编译的
`.S/.c`——上下文切换、PendSV、SysTick 全由 ThreadX 负责。因此 `netxduo.cmake` 只需编译
`common/src/`，再额外补上 `addons/dhcp/nxd_dhcp_client.c`（DHCP 客户端实现在 addons 里）。

### 3. 单镜像 + 自动命名

工程名 = 目录名（`CMakeLists.txt` 用 `get_filename_component(... NAME)` 取），烧录脚本用
OpenOCD 的 `[file tail [pwd]]` 取同名——**复制整个文件夹并重命名，即得到一个全新工程**，
无需改任何路径。

---

## 烧录与调试

```bash
./run.sh flash            # 编译 + 烧录（内部 openocd -f flash）
openocd -f flash          # 仅烧录（必须在工程根目录）
openocd -f flash-reset    # 芯片被锁/低功耗连不上时的"连接复位"版本
```

`flash` 默认走 **DAPLink / CMSIS-DAP**；用 ST-Link 时，把 `flash` 里
`interface/cmsis-dap.cfg` 换成 `interface/stlink.cfg`。

**VS Code**：仓库自带 `.vscode/launch.json`（DAPLink / ST-Link 两套），按 `F5` 即可
烧录并停在 `main`；`Ctrl+Shift+B` 走 `.vscode/tasks.json` 调 `run.sh`。

---

## 观察输出（SEGGER RTT，无需串口线）

打开 **J-Link RTT Viewer**（选 ST-Link / J-Link → 芯片 `STM32H743`），连接后可见 4 个终端：

| 终端 | 内容 |
|---|---|
| `Manage_Terminal` | 任务状态 / CPU 占用率 |
| `Net_Info_Terminal` | 以太网状态与统计 |
| `Netx_Terminal` | DHCP / echo / link 业务日志 |
| `Other_Terminal` | 标准 `printf` 输出 |

也可 `telnet 127.0.0.1 19021` 直接看。若无输出：确认程序已运行、`RTT_viewer_init()`
已执行，复位后重连通常即可。

---

## 扩展

- **加一个总线/外设**：`cmake/stm32h7.cmake` 已全量编译 HAL，直接用即可。
- **加一个网络协议**（HTTP/MQTT/DNS…）：把对应 `Middlewares/netxduo/addons/xxx/*.c`
  加进 `cmake/netxduo.cmake` 的源码列表，并把 `addons/xxx` 加入头文件路径。
- **加自己的任务**：在 `User/app/src/threadx_task_manager.c` 的 `AppTaskCreate()` 里
  照葫芦画瓢 `tx_thread_create()`。
- **改静态 IP**：调整 `User/app/src/app_eth_config.c` 中的 IP 宏并关闭 DHCP 分支。
- **换板子**：改链接脚本的存储器分布、`startup_stm32h743xx.s` 向量表、`stm32h7xx_hal_conf.h`
  的时钟/HSE 即可，其余结构不变。

---

## 常见问题

- **子模块为空，编译找不到 `tx_api.h`** → `git submodule update --init --recursive`。
- **`PendSV_Handler` / `SysTick_Handler` 重复或缺失** → 确认 ThreadX 用 `OBJECT` 库
  （见上文「关键设计」），不要改成静态库。
- **编译卡住（Windows 上某个 gcc 进程长时间无进展）** → mingw 高并行度下偶发的进程派
  生死锁，与代码无关。降并行度：`JOBS=2 ./run.sh`。
- **objcopy 报 `No such file`（路径含中文）** → 已在 CMake 中用 `$<TARGET_FILE_NAME>`
  规避；若自行改动后处理命令，勿把中文绝对路径直接传给 objcopy。
- **网口不亮 / 拿不到 IP** → 检查网线与路由器 DHCP；确认 PHY 型号（V7 为 LAN8742A）与
  `User/netxduo/nx_stm32_eth_config.h` 一致。

---

## 许可与致谢

本项目自身代码以 **MIT** 许可发布，见 [LICENSE](LICENSE)。仓库内保留的第三方组件
（Eclipse ThreadX、NetX Duo、STM32 HAL、CMSIS、SEGGER RTT、ST 以太网驱动、安富莱 BSP）
各自遵循其原许可，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

- Eclipse ThreadX / NetX Duo：<https://github.com/eclipse-threadx>
- 安富莱《STM32H7 ThreadX 教程》系列（BSP 与例程来源）
- SEGGER RTT 文档：<https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/>
