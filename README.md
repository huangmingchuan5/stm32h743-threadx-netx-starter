# STM32H743 + ThreadX + NetX Duo 例程（CMake 单镜像 · 初学者教程）

> 安富莱 V7 开发板（STM32H743XIHx）上运行的 **ThreadX 实时内核 + NetX Duo TCP/IP 协议栈** 示例。
> 本工程改编自 `Cmake_threadX_Template`，把原工程里复杂的 **boot/app 双镜像 + 固件容器** 方案
> 简化为**单镜像**，并把构建系统重写成模板那套「分层 OBJECT 库」结构，方便初学者理解和扩展。

---

## 1. 这个工程能做什么

- 板上千兆/百兆 PHY（V7 用 **LAN8742A**）通过 **DHCP** 自动获取 IP 地址；
- 跑一个 **TCP echo 服务器 + 客户端** 示例（用网络调试助手连上去就能收发包）；
- 用 ThreadX 创建多个任务，并通过 **SEGGER RTT** 周期性打印：任务栈使用情况、CPU 占用率；
- 所有 `printf` 走 **RTT 虚拟串口**，不接 USB 转串口线也能在上位机看日志。

> 关于「串口权限」：本工程把调试输出统一接到 SEGGER RTT（通过 SWD 把数据传到电脑），
> 你不需要去碰 COM 口权限、不需要装 CH340 驱动，插上 ST-Link / DAPLink 即可。

---

## 2. 相比原工程改了什么（重要）

| 项目 | 原工程 | 本教程工程 |
| --- | --- | --- |
| 镜像结构 | boot + app 双镜像 + 固件容器（`.S`/`.ld`/多个 cmake） | **单镜像**，一个 `*.elf` 直接烧 |
| 构建系统 | CubeMX 生成的 `add_subdirectory` + 多个 `user_*.cmake` | 模板式 `cmake/*.cmake` 分层 |
| ThreadX 链接 | 静态库 `.a` | **OBJECT 库**（见下面「坑」） |
| 调试输出 | SEGGER RTT | 沿用 SEGGER RTT，文档补全 |

### ⚠️ 一个让新手踩坑的移植要点：为什么必须用 OBJECT 库

ThreadX 在 `tx_thread_schedule.S` 里定义了**强符号** `PendSV_Handler`，在
`tx_initialize_low_level.S` 里定义了**强符号** `SysTick_Handler`，
用来**覆盖** CMSIS 启动文件里的 `.weak` 空实现 —— 否则线程切换和 1kHz 系统节拍就丢了。

如果用「静态库 `.a`」，链接器是**按需抽取**目标文件的：启动文件里已经用 `WEAK` 提供了
`PendSV_Handler`，引用被满足了，链接器就不会去静态库里取 ThreadX 的那个目标文件，
结果——**调度心跳静默消失，程序跑不起来**。

用 **OBJECT 库**（`add_library(xxx OBJECT ...)`）则所有 `.o` 无条件参与链接，
强符号自然覆盖弱符号。这就是模板和我这套 CMake 都坚持用 OBJECT 库的原因。

---

## 3. 环境准备

需要以下工具（你机器上已经装好，路径在 `D:/Embedded_Toolchain/` 下）：

| 工具 | 用途 | 本机路径示例 |
| --- | --- | --- |
| arm-none-eabi-gcc | 交叉编译器 | `D:/Embedded_Toolchain/arm-none-eabi-gcc/15.3.rel2/bin` |
| CMake | 构建系统 | `D:/Embedded_Toolchain/cmake/4.4.5/bin`（≥ 3.22） |
| Ninja | 构建后端 | `D:/Embedded_Toolchain/ninja/1.13.2` |
| OpenOCD | 烧录 / 调试 | `D:/Embedded_Toolchain/xpack-openocd/0.12.0-7/...` |
| J-Link RTT Viewer | 看 RTT 日志 | 单独安装（SEGGER 官网免费） |

> 把上面三个 `bin` 目录加到系统 `PATH`，或在本工程目录打开已配置好的终端。
> 验证：`arm-none-eabi-gcc --version`、`cmake --version`、`ninja --version` 都能打印版本号。

**首次拉取源码**：本工程的 ThreadX / NetX Duo 是 Git 子模块，克隆后需初始化：

```bash
git submodule update --init --recursive
```

（`Middlewares/threadx`、`Middlewares/netxduo` 两个目录若为空，构建会找不到源文件。）

硬件：安富莱 V7 开发板、ST-Link 或 DAPLink 调试器、一根网线（接路由器/交换机，用于 DHCP）。

---

## 4. 目录结构

```
STM32H743-ThreadX-NetXDuo_Demo/
├─ CMakeLists.txt            # 顶层构建：单可执行目标 + 后处理(hex/bin/size)
├─ CMakePresets.json         # Debug / Release 预设，cmake --preset 直接用它
├─ startup_stm32h743xx.s     # 中断向量表（.isr_vector）
├─ STM32H743XX_FLASH.ld      # 链接脚本：AXI SRAM 作主 RAM，提供 _vectors / __RAM_segment_used_end__
├─ cmake/
│  ├─ gcc-arm-none-eabi.cmake  # 工具链：CPU/FPU/编译/链接公共选项
│  ├─ threadx.cmake            # ThreadX → OBJECT 库（common/src + gnu port + 底层初始化）
│  ├─ netxduo.cmake            # NetX Duo → OBJECT 库（common/src + gnu port）
│  ├─ stm32h7.cmake            # STM32H7 HAL → OBJECT 库（全量 Src/*.c，排除 *_template.c）
│  ├─ segger_rtt.cmake         # SEGGER RTT → OBJECT 库
│  └─ bsp.cmake                # BSP 初始化 → OBJECT 库（bsp.c / bsp_RTT_viewer.c）
├─ Drivers/                  # CMSIS + STM32H7xx HAL 驱动（对应模板的 Libraries/）
├─ Middlewares/
│  ├─ threadx/               # ★ git 子模块：ThreadX 源码
│  ├─ netxduo/               # ★ git 子模块：NetX Duo 源码
│  └─ SEGGER_RTT/            # RTT 源码
└─ User/
   ├─ main.c                 # 入口：System_Init → 挂起 HAL tick → tx_kernel_enter
   ├─ core/                  # system_stm32h7xx.c / hal_msp / it / timebase_tim / syscalls / sysmem
   ├─ bsp/                   # 板级支持：bsp.c（MPU/缓存/480MHz 时钟/RTT 初始化）、bsp_eth.c
   ├─ app/                   # ThreadX 内核引导 + 多终端 printf + 网络应用
   │   ├─ src/threadx_kernel.c        # 创建 Start/Stat/Idle 任务，统计 CPU 占用
   │   ├─ src/threadx_printf.c        # 线程安全的多终端 printf（走 RTT）
   │   ├─ src/threadx_task_manager.c  # AppTaskCreate：创建网络相关任务
   │   ├─ src/app_eth*.c              # NetX IP / DHCP / Link 检查 / TCP echo
   │   └─ inc/                       # 对应头文件
   ├─ netxduo/               # nx_user.h（协议栈配置）+ STM32 以太网驱动 + PHY 驱动
   ├─ dev/lan8742/           # LAN8742A PHY 驱动
   └─ threadx/               # tx_user.h（内核配置）+ tx_initialize_low_level.S
```

`Drivers/` 在本工程里等同于模板里的 `Libraries/`（CMSIS + HAL），只是名字沿用
STM32CubeMX 的习惯，方便你以后直接把 CubeMX 生成的 HAL 丢进来。

---

## 5. 一键构建（`./run.sh`，与模板一致）

沿用 `Cmake_threadX_Template` 的 `run.sh`，一条命令搞定构建与烧录：

```bash
./run.sh                  # Debug 全量构建（默认）
./run.sh flash            # 构建后用 OpenOCD 烧录（= openocd -f flash）
./run.sh clean            # 清空 build/ 后重新编译
BUILD_TYPE=Release ./run.sh
JOBS=2 ./run.sh           # 编译卡住时降低并行度（见下）
```

`run.sh` 在 `PATH` 里找不到 gcc / cmake / ninja / openocd 时，会自动回退到
`D:/Embedded_Toolchain/...` 下的本机安装路径，无需手动配环境变量。

> ⚠️ **如果编译卡住**（终端长时间不动、某个 `arm-none-eabi-gcc` 进程无进展），
> 这是 mingw 在高并行度下偶发的进程派生死锁，**不是代码问题**。降低并行度即可：
> ```bash
> JOBS=2 ./run.sh
> ```
> 本工程默认 `JOBS=4`；`JOBS=2` 在 Debug / Release 下均已验证可稳定通过。

等价的 CMake Presets 方式（备选）：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

构建成功后，`build/` 下会生成：

| 文件 | 说明 |
| --- | --- |
| `*.elf` | 带调试信息的可执行文件，烧录用它 |
| `*.hex` / `*.bin` | 烧录镜像 |
| `*.map` | 内存映射，排查体积/链接问题用 |
| `compile_commands.json` | clangd 补全用 |
| （终端会打印 Flash / RAM 占用） | |

产物名自动跟随工程目录名：`build/STM32H743-ThreadX-NetXDuo_Demo.elf`。

---

## 6. 烧录与运行

### 方式 A：一条命令（与模板一致，推荐）

```bash
./run.sh flash        # 编译 + 用 OpenOCD 烧录（内部执行 openocd -f flash）
```

或分步：

```bash
./run.sh              # 只编译
openocd -f flash      # 烧录（必须在工程根目录，flash 里用相对路径）
```

`flash` 默认走 **DAPLink / CMSIS-DAP**；若用 ST-Link，把 `flash` 里
`interface/cmsis-dap.cfg` 那行注释掉、改用 `interface/stlink.cfg` 即可。
芯片被锁 / 一上电就进低功耗导致连不上时，用「连接复位」版：

```bash
openocd -f flash-reset
```

### 方式 B：VS Code（F5）

工程已带 `.vscode/launch.json`，支持 **OpenOCD + ST-Link** 和 **OpenOCD + DAPLink** 两种。
按 `F5` 即可：自动烧录 → 停在 `main` → 开始运行。

---

## 7. 查看调试输出（串口 / RTT）

所有日志通过 **SEGGER RTT** 输出，三种查看方式任选其一：

1. **J-Link RTT Viewer**（推荐）
   打开 SEGGER 家的 *RTT Viewer*，选择调试器（ST-Link / J-Link）→ 芯片 `STM32H743`，
   连接后能看到 4 个终端：
   - 终端 1 `Manage_Terminal`：任务状态 / CPU 占用率条形图
   - 终端 2 `Net_Info_Terminal`：以太网状态、统计
   - 终端 3 `Netx_Terminal`：NetX 业务日志（DHCP / echo / link）
   - 终端 0 / 4 `Other_Terminal`：标准 `printf` 输出
2. **telnet**：`telnet 127.0.0.1 19021`（RTT 默认端口），也能看到输出。
3. VS Code 里装 *cortex-debug* 后，RTT 会直接显示在调试控制台。

> 如果连上后**看不到任何输出**：先确认板子已经跑起来（OpenOCD 烧录成功、程序在跑）；
> RTT 需要在程序运行后、且 `RTT_viewer_init()` 已执行才能捕获。板子复位一下再连通常就好了。

---

## 8. 代码导览（从 main 到网络）

1. `User/main.c`
   `System_Init()`（MPU/缓存/480MHz 时钟/RTT 初始化）→ `HAL_SuspendTick()` → `tx_kernel_enter()`。
2. `tx_kernel_enter()` → ThreadX 底层初始化 → 调用 **`tx_application_define()`**
   （在 `User/app/src/threadx_kernel.c`）。
3. `tx_application_define()` 创建三个内核任务：
   - `App Task Start`（优先级 2）：恢复 HAL tick、初始化 printf 互斥锁、`bsp_Init()`、
     调用 `AppTaskCreate()`（在 `threadx_task_manager.c` 里创建网络任务），然后自终止；
   - `App Task STAT`（优先级 30）：每秒打印一次 CPU 占用与任务栈；
   - `App Task IDLE`（优先级 31）：空转累加计数，供 CPU 占用统计。
4. `User/app/src/app_eth.c` 等：初始化 NetX IP 实例、DHCP 拿 IP、启动 TCP echo。
5. `User/app/src/threadx_printf.c`：把 `printf`/`threadx_printf` 重定向到 RTT 多终端，
   并加 `TX_MUTEX` 做互斥，避免多线程交错打印乱码。

**想加自己的任务？** 在 `threadx_task_manager.c` 的 `AppTaskCreate()` 里照葫芦画瓢
`tx_thread_create()` 一个线程即可，参考已有任务的写法。

---

## 9. 常见问题

- **`Middlewares/threadx` / `Middlewares/netxduo` 为空，编译报找不到 `tx_api.h`**
  → 忘了拉子模块，执行 `git submodule update --init --recursive`。
- **链接报 `PendSV_Handler` / `SysTick_Handler` 重复定义或缺失**
  → 确认 ThreadX 是用 OBJECT 库编译的（见第 2 节），不要把 threadx 改成静态库。
- **烧录后板子没反应 / 不打印**
  → 先看 OpenOCD 是否识别到芯片；再用 RTT Viewer 复位重连；确认用的链接脚本
     `STM32H743XX_FLASH.ld` 在根目录、且 `startup_stm32h743xx.s` 也在根目录。
- **网口不亮 / 拿不到 IP**
  → 检查网线、路由器 DHCP 是否开启；PHY 型号（V7 是 LAN8742A）与
     `User/netxduo/nx_stm32_eth_config.h` 里的配置是否对应。
- **`-Wl,--no-warn-rwx-segments` 相关告警**
  → 正常，新版本 ld 对 RWX 段告警，已在工具链里静默处理，不影响运行。
- **编译卡住 / 不动（Windows 上某个 gcc 进程长时间无进展）**
  → mingw 高并行度下偶发进程派生死锁（与代码无关）。降低并行度即可：
    `JOBS=2 ./run.sh`（或 `cmake --build build -- -j 2`）。本工程用 `JOBS=2`
    在 Debug / Release 下均已验证可跑通。

---

## 10. 如何扩展

- **改成静态 IP**：改 `User/app/src/app_eth_config.c` 里的 IP 相关宏，关掉 DHCP 分支；
- **加协议**：NetX Duo 已编译了 TCP/UDP/ICMP/DHCP，要 HTTP/MQTT 等可把对应 addon
  源文件加进 `cmake/netxduo.cmake` 的源文件列表；
- **换板子**：改 `STM32H743XX_FLASH.ld` 的存储器分布、`startup_stm32h743xx.s` 的向量表、
  以及 `stm32h7xx_hal_conf.h` 里的时钟与 HSE 值即可，其余结构不变。

---

## 11. 参考

- 安富莱《STM32H7 ThreadX 教程》第 1~23 章
- Eclipse ThreadX：`github.com/eclipse-threadx/threadx`、`github.com/eclipse-threadx/netxduo`
- SEGGER RTT 文档（SEGGER 官网）
- 本工程改编模板：`Cmake_threadX_Template`（同机 `Desktop/Projects/` 下）
