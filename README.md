# stm32h743xi_threadx_netx

STM32H743XIH6 + ThreadX + NetX Duo 的以太网脚手架工程，包含独立 Boot/App、组合固件、
DHCP/静态 IP 管理、链路状态机，以及 TCP Client/Server Echo 示例。

## 1. 拉取代码

ThreadX 和 NetX Duo 以 git submodule 方式引入（见 `.gitmodules`）。主仓库 clone 完成后，
需要单独初始化这一层 submodule，否则会因为缺少 RTOS 和协议栈源码而无法编译。

本工程只依赖 `.gitmodules` 中列出的 ThreadX / NetX Duo，不需要递归拉取它们内部的其他
submodule，因此不要使用 `--recursive`。

```bash
# 第一步：拉取主仓库
git clone <本仓库地址>
cd STM32H743-ThreadX-NetXDuo_Demo

# 第二步：只初始化当前仓库声明的 submodule
git submodule update --init
```

确认下面两个目录中已经有源码：

```bash
ls Middlewares/threadx/common/src
ls Middlewares/netxduo/common/src
```

如果目录为空，检查网络和 Git 凭据。submodule 指向的是公开上游
（`github.com/eclipse-threadx/threadx` 与 `github.com/eclipse-threadx/netxduo`），
不需要内网权限。不要在缺少 submodule 的情况下继续编译。

## 2. 说明

这是一套已经接通 STM32H7、ThreadX 和 NetX Duo 的网络基础工程，不是某个具体产品的成品应用。
它把 Boot/App 分区、H7 Cache 一致性、ETH DMA、PHY、DHCP、链路管理和 TCP 示例等基础设施放在
一起，适合继续增加设备协议、采集上报、远程控制等网络业务。

**已经就绪：**

- 128 KiB Boot + 1920 KiB App 的双镜像布局，Boot 校验 App 向量表后跳转
- 自动生成 `boot`、`app` 和 Boot+App 组合固件
- STM32H743 Ethernet MAC + LAN8720A PHY 的 RMII 接入
- H7 I-Cache/D-Cache 和 MPU 配置，ETH 描述符、NetX packet pool 放在非缓存区
- NetX Duo IPv4、ARP、ICMP、UDP、TCP 和 DHCP Client
- DHCP 超时回退静态 IP，运行时区分 DHCP / STATIC / DHCP_FALLBACK 来源
- link UP/DOWN 电平状态机、变化订阅 API 和 RX 描述符 buffer 兜底补给
- 基于芯片 96 位 UID 生成稳定的本地管理单播 MAC
- TCP Server Echo：监听 7777，最多 10 个并发连接
- TCP Client Echo：连接固定目标，收到什么就原样回发，断线自动重连
- SEGGER RTT 四终端日志分流、CPU 利用率和线程栈监控

**暂未实现：**

- 网络配置持久化；当前复位后重新使用内置默认配置
- TLS、证书和业务层鉴权
- 固件签名、版本管理、升级回滚；Boot 当前只校验 App 向量表是否合法
- 具体产品协议；Echo 只是验证 TCP 收发的示例
- CMake 烧录 target；工程生成标准 ELF/HEX/BIN，由调试器或烧录工具加载

## 3. 硬件与软件依赖

| 项 | 当前配置 |
|---|---|
| MCU | STM32H743XIH6，Cortex-M7，2 MiB Flash、1 MiB RAM |
| 时钟 | 25 MHz HSE，SYSCLK 480 MHz，HCLK 240 MHz，VOS0 + overdrive |
| PHY | LAN8720A 10/100M PHY，RMII，MDIO 地址启动时自动扫描 0~31 |
| RTOS | ThreadX，git submodule，用户裁剪配置在 `User/threadx/tx_user.h` |
| 网络栈 | NetX Duo，git submodule，用户裁剪配置在 `User/netxduo/nx_user.h` |
| 工具链 | CMake ≥ 3.22、Ninja、GNU Arm Embedded Toolchain |
| 编译参数 | `-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard` |
| 调试输出 | SEGGER J-Link + RTT Viewer |

Ethernet 使用 RMII，当前板级引脚如下：

引脚按野火_H743X_H750X_底板_V1.0_原理图第 9 页（U16 = LAN8720A）确定：

| 信号 | 引脚 | LAN8720A 引脚 |
|---|---|---|
| PHY nRST | 与 MCU NRST 共用 | 15 |
| ETH_MDC | PC1 | 13 |
| ETH_REF_CLK | PA1 | 14（NINT/REFCLKO，PHY 输出 50 MHz） |
| ETH_MDIO | PA2 | 12 |
| ETH_CRS_DV | PA7 | 11 |
| ETH_RXD0 / ETH_RXD1 | PC4 / PC5 | 8 / 7 |
| ETH_TX_EN | PB11 | 16 |
| ETH_TXD0 / ETH_TXD1 | PG13 / PG12 | 17 / 18 |

PHY 的 nRST 直接接 MCU 的 NRST，上电时两者一起复位，软件无法单独复位 PHY，因此代码在
使能 GPIO 后先等 50 ms 再访问 SMI。之后驱动通过 MDIO 扫描地址并读取 PHY ID；地址和协商出的
速率/双工状态会打印到 `Netx_Terminal`。

> `RXER/PHYAD0`（pin 10）在板上未连接，靠芯片内部下拉，所以 PHY 地址固定为 0。
> `RXD0/RXD1/CRS_DV` 同时是 MODE0/MODE1/MODE2 的锁存脚，模式由 LAN8720A 内部上拉决定
> （all capable auto-negotiation），板上没有额外上下拉电阻。

## 4. 目录结构

```text
User/
├── main.c                       # App 入口：系统初始化后进入 ThreadX
├── boot/
│   └── src/boot_main.c          # Boot：校验 App MSP/Reset_Handler 并跳转
├── app/
│   ├── inc/  src/
│   ├── threadx_kernel.*         # ThreadX 引导、CPU/栈监控
│   ├── threadx_task_manager.*   # 业务任务集中创建入口
│   ├── threadx_printf.*         # RTT 多终端线程安全输出
│   ├── app_eth.*                # NetX IP 实例和网络模块总入口
│   ├── app_eth_config.*         # 网络 config/status 和异步 apply
│   ├── app_eth_dhcp.*           # DHCP 控制、静态 IP 和 fallback
│   ├── app_eth_link_check.*     # link 状态机、订阅 API、RX buffer 兜底
│   ├── app_tcp_server_echo.*    # 多连接 TCP Server Echo
│   └── app_tcp_client_echo.*    # 自动重连 TCP Client Echo
├── bsp/
│   ├── bsp.*                    # 时钟、MPU、Cache 和板级初始化
│   ├── bsp_eth.*                # RMII GPIO、PHY 复位、ETH HAL、MAC 写入
│   └── bsp_RTT_viewer.*         # SEGGER RTT 初始化
├── core/                        # CMSIS/HAL 系统、中断、syscalls
├── dev/lan8742/                 # ST LAN8742 组件驱动
├── netxduo/
│   ├── nx_stm32_eth_driver.*    # NetX ↔ STM32 HAL ETH 适配层
│   ├── nx_stm32_eth_config.h    # NetX ETH 驱动配置
│   ├── nx_user.h                # NetX Duo 用户裁剪配置
│   └── lan8720/                 # 当前 LAN8742 PHY 适配层
└── threadx/
    ├── tx_user.h                # ThreadX 用户裁剪配置
    └── tx_initialize_low_level.S

cmake/
├── gcc-arm-none-eabi.cmake      # GNU Arm 工具链
├── STM32H743xI_flash.ld         # Boot/App 共用、地址参数化链接脚本
├── firmware_container.*         # 组合 Boot/App 固件
├── user_boot.cmake              # Boot 源文件和分区
├── user_app.cmake               # App 源文件和分区
├── threadx.cmake / netxduo.cmake
└── startup_stm32h743xx.s

Drivers/                         # STM32H7 HAL、CMSIS、CMSIS-DSP/NN
Middlewares/                     # ThreadX、NetX Duo、SEGGER RTT
```

## 5. 编译 / 烧录 / 调试

### 5.1 CMake（推荐）

工程使用 Ninja 和 `CMakePresets.json`，提供 `Debug` / `Release` 两套配置。

```bash
# 首次配置，或 CMake 文件发生变化后重新配置
cmake --preset Debug

# 构建 Boot、App 和组合固件
cmake --build --preset Debug
```

产物位于 `cmake/build/Debug/`：

| 产物 | 内容 | 加载地址 |
|---|---|---|
| `boot.elf/.hex/.bin` | 独立 Boot | `0x08000000` |
| `app.elf/.hex/.bin` | 独立 App | `0x08020000` |
| `stm32h743xi_threadx_netx.elf/.hex/.bin` | Boot + App 组合固件 | `0x08000000` |
| `boot.map` / `app.map` | 各镜像链接映射 | — |
| `compile_commands.json` | IDE/clangd 索引数据库 | — |

只想快速编译某个镜像时，可以指定 target：

```bash
cmake --build cmake/build/Debug --target app
cmake --build cmake/build/Debug --target boot
```

### 5.2 Flash 分区与烧录

| 区域 | 起始地址 | 长度 | 说明 |
|---|---:|---:|---|
| Boot | `0x08000000` | `0x00020000`（128 KiB） | 复位入口、App 合法性检查和跳转 |
| App | `0x08020000` | `0x001E0000`（1920 KiB） | ThreadX + NetX Duo 应用 |

STM32H743XIH6 有 2 MiB Flash，App 分区因此从 896 KiB 扩到 1920 KiB 吃满整片，结束于 `0x08200000`。

推荐直接烧录组合固件：

- 烧录 `.hex` 时地址已经包含在文件中，无需手动指定基地址。
- 烧录组合 `.bin` 时，基地址必须设置为 `0x08000000`。
- 单独烧录 `app.bin` 时，基地址必须设置为 `0x08020000`，不能按默认 Flash 起点写入。
- 单独烧录 `boot.bin` 时，基地址为 `0x08000000`。

仓库当前没有 `flash`/`reset` CMake target。可使用 J-Link Commander、J-Flash、
STM32CubeProgrammer 或 IDE 下载生成的 ELF/HEX/BIN。调试 App 时优先加载 `app.elf` 的符号，
并确认调试器知道 App 向量表位于 `0x08020000`。

Boot 只检查两项：

1. App 初始 MSP 是否位于 DTCM `0x20000000~0x20020000`。
2. App Reset Handler 是否带 Thumb 位，且地址落在 App Flash 分区。

当前没有 CRC、签名和版本检查，因此向量表合法不等于固件内容完整。

### 5.3 Keil / MDK-ARM

仓库不附带 `.uvprojx`。如需建立 Keil 工程，应以 CMake 文件中的源文件和宏为权威来源，并分别建立
Boot、App 两个 target：

1. MCU 选择 `STM32H743XIH6`，C 标准使用 C11。
2. Boot IROM 起点/长度设置为 `0x08000000 / 0x00020000`。
3. App IROM 起点/长度设置为 `0x08020000 / 0x001E0000`。
4. App 定义 `USER_VECT_TAB_ADDRESS` 和 `VECT_TAB_OFFSET=0x00020000U`。
5. ThreadX 使用 `ports/cortex_m7/ac6`，不要混用 GNU 移植层。
6. NetX Duo 加入 `common/src` 和 DHCP addon，并使用 `User/netxduo/nx_user.h`。
7. Scatter file 需要复现链接脚本中的 DTCM、非缓存 AXI SRAM 和 ETH 描述符布局。

尤其不能把 GNU ld 的 `STM32H743xI_flash.ld` 直接当作 Keil scatter file 使用。新增或删除源文件时，
CMake 与 Keil 工程分组也必须同步维护。

### 5.4 RTT 调试输出

运行日志通过 SEGGER RTT 的 4 个终端分流：

| 终端 | 编号 | 内容 |
|---|---:|---|
| `Manage_Terminal` | 1 | CPU 利用率、峰值、系统时钟和线程栈状态 |
| `Net_Info_Terminal` | 2 | link、IP 来源、IP/MAC/DNS、协议计数和 packet pool |
| `Netx_Terminal` | 3 | PHY、DHCP、link、TCP Client/Server Echo 事件 |
| `Other_Terminal` | 4 | 标准 `printf` / `_write` 输出 |

业务线程中使用：

```c
#include "threadx_printf.h"

threadx_printf(Netx_Terminal,
               "[my_app] status=%u\r\n",
               (unsigned)status);
```

`threadx_printf` 单次格式化缓冲区为 200 字节，超长内容会截断；其内部使用继承优先级的 mutex，
只能在线程上下文和初始化完成后使用。

## 6. 关键设计要点

### 6.1 Boot → App 启动链路

完整启动关系如下：

```text
MCU Reset
  → Boot Reset_Handler
    → boot_main.c: main()
      → 读取 0x08020000 处的 MSP / Reset_Handler
      → 关闭 SysTick、NVIC 中断和挂起状态
      → VTOR = 0x08020000，恢复特权 MSP 模式
      → 跳转 App Reset_Handler
        → User/main.c: main()
          → MPU_Config + I/D Cache + HAL + 480 MHz 时钟 + RTT
          → tx_kernel_enter()
```

Boot 和 App 使用同一份参数化链接脚本，但由 CMake 分别注入 Flash 起点、长度和向量偏移。不要在源文件
中另写一套地址常量；调整分区时应同时修改 `cmake/user.cmake`、Boot 校验参数和烧录流程。

### 6.2 H7 Cache 与网络内存布局

STM32H7 开启 D-Cache 后，CPU Cache 与 ETH DMA 不能直接共享普通缓存内存，否则会出现“CPU 看到了、
DMA 没看到”或相反的隐蔽故障。本工程通过 MPU 和链接脚本划分两块非缓存网络内存：

| 区域 | 地址/长度 | MPU 属性 | 放置内容 |
|---|---|---|---|
| `RAM_D1_NOCACHE` | `0x24000000` / 64 KiB | region 1，非缓存 | NetX packet pool（`.NetXPoolSection`） |
| `RAM_D2` | `0x30000000` / 288 KiB | **仅前 32 KiB** 为 region 0 非缓存 | ETH RX/TX DMA 描述符（在 `0x30000000` 起） |
| `RAM_D1` | `0x24010000` / 448 KiB | 默认可缓存 | 预留，当前未使用 |
| `RAM_D3` | `0x38000000` / 64 KiB | 默认可缓存 | 预留，当前未使用 |
| DTCM | `0x20000000` / 128 KiB | 默认可缓存 | 普通 `.data/.bss`、线程栈、ThreadX 对象 |

`RAM_D2` 在链接脚本里按芯片真实容量声明为 288 KiB，但 MPU region 0 只覆盖前 32 KiB
（MPU 长度只能是 2 的幂，288 KiB 无法用一个 region 表达）。ETH 描述符放在最前面，所以是安全的；
**但往 `0x30008000` 之后放 DMA buffer 时它是可缓存的**，必须自己维护 Cache clean/invalidate，
或者同时新增 MPU region。

`MPU_Config()` 必须在启用 D-Cache 前执行。不要随意删掉 MPU region、section 属性或 32/128 字节对齐；
新增 DMA buffer 时也应放入合适的非缓存区域，或者显式维护 Cache clean/invalidate。

### 6.3 线程优先级规划

ThreadX 数值越小优先级越高。当前代码的分配如下：

| 线程 | 优先级 | 职责 |
|---|---:|---|
| NetX IP 线程 | **2** | 协议栈核心、收发、ACK、TCP 定时器 |
| App Task Start | 2 | 一次性启动引导，完成后终止并删除 |
| TCP Server Echo setup/worker ×10 | 3 | listen、accept 和并发 Echo |
| TCP Client Echo | 5 | 连接固定服务端、接收并原样回发 |
| NetX DHCP 内部线程 | 6 | DHCP 状态机（`NX_DHCP_THREAD_PRIORITY`） |
| App ETH | 10 | 创建网络模块、刷新状态表 |
| App ETH Link Check | 10 | PHY 轮询、link 状态、RX 描述符兜底 |
| App ETH DHCP 控制线程 | 10 | 应用配置、DHCP/fallback 管理 |
| App Task STAT | 30 | CPU 和线程栈统计 |
| App Task IDLE | 31 | 空闲计数 |

NetX IP 线程必须保持最高优先级之一，避免业务线程长期阻塞 ACK、重传和 RX 处理。新增实时网络业务建议
放在 4~9；后台维护任务放在 11 及以上。不要创建优先级高于 2、且长期占用 CPU 的业务线程。

### 6.4 link 状态与订阅 API

`app_eth_link_check` 使用电平事件维护真实 link 状态，不依赖
`ip.nx_ip_driver_link_up`。电平事件会在状态持续期间保持置位，因此多个等待者不会互相抢事件。

```c
#include "app_eth_link_check.h"

enum app_eth_link_state state = app_eth_link_get_state();

app_eth_link_wait_up(TX_WAIT_FOREVER);
app_eth_link_wait_down(timeout_ticks);

struct link_sub sub;
enum app_eth_link_state new_state;

app_eth_link_sub_init(&sub);
app_eth_link_wait_change(&sub, &new_state, TX_WAIT_FOREVER);
```

link UP 时，检查线程还会扫描 HAL ETH 的 RX 描述符，为之前因 packet pool 紧张而没有挂上 buffer 的
描述符重新补给，避免所有 RX 描述符耗尽后 DMA 无法继续收包。

### 6.5 DHCP、静态 IP 和运行时状态

默认配置定义在 `app_eth_config_get_default()`：

| 配置项 | 默认值 |
|---|---|
| DHCP | 启用 |
| DHCP 超时 | 30 秒 |
| 静态/回退 IP | `192.168.2.233/24` |
| 网关 | `192.168.2.1` |
| DNS | `8.8.8.8`、`8.8.4.4` |
| MAC | 由 STM32H743 96 位 UID 生成 |

静态字段有两种用途：

- `dhcp_enable = 1`：DHCP 超时后使用这组地址作为 fallback。
- `dhcp_enable = 0`：直接把这组地址作为正式静态配置。

读取和应用配置：

```c
#include "app_eth_config.h"

struct app_eth_config cfg;

app_eth_config_get_current(&cfg);
cfg.dhcp_enable = 0;
cfg.ip = IP_ADDRESS(192, 168, 2, 100);
cfg.mask = 0xFFFFFF00UL;
cfg.gateway = IP_ADDRESS(192, 168, 2, 1);

UINT status = app_eth_config_apply(&cfg);
```

`app_eth_config_apply()` 只做校验、写 pending 和发送事件；真正的 IP/DHCP 切换由 DHCP 控制线程完成。
要读取当前实际生效的网络状态，而不是“配置里写了什么”，使用：

```c
struct app_eth_status status;

app_eth_status_get(&status);
/* status.ip / mask / gateway / dns1 / dns2 / link / ip_source */
```

`ip_source` 可能是 `APP_ETH_IP_STATIC`、`APP_ETH_IP_DHCP` 或
`APP_ETH_IP_DHCP_FALLBACK`。

默认 MAC 使用 96 位 UID 经 64 位散列生成，并强制设置为本地管理的单播地址。相同芯片每次启动得到
相同 MAC，不同芯片通常得到不同 MAC。该方案适合开发和内部网络；商业量产若要求地址分配可审计、
绝对避免冲突，应使用正式 MAC 地址段并在生产阶段烧录。

配置尚未持久化。MAC 通过 `app_eth_config_apply()` 改变时会触发软复位，但复位后仍会回到 UID 生成的
默认 MAC，直到实现 Flash/EEPROM 持久化。

### 6.6 TCP Server Echo 与 Client Echo

#### TCP Server Echo

`app_tcp_server_echo.c` 监听端口 7777，最多 10 个并发连接。每个已连接 socket 由 worker 处理，
收到的 `NX_PACKET` 直接原样发送，发送成功后包的所有权交还 NetX，发送失败才由应用释放。

Server 使用动态 listen/unlisten 接力：

- 有空闲 socket 时，通过 relisten 补上监听槽位。
- 10 个 socket 全部占用时取消监听，让超额连接立即收到 RST。
- worker 归还 socket 后重新开启监听。

这比把额外 SYN 长时间留在 listen queue 中更容易让客户端及时感知“服务已满”。

#### TCP Client Echo

Client 的目标集中定义在 `app_tcp_client_echo.h`：

```c
#define APP_TCP_CLIENT_ECHO_SERVER_IP_A 192U
#define APP_TCP_CLIENT_ECHO_SERVER_IP_B 168U
#define APP_TCP_CLIENT_ECHO_SERVER_IP_C 2U
#define APP_TCP_CLIENT_ECHO_SERVER_IP_D 2U
#define APP_TCP_CLIENT_ECHO_SERVER_PORT 7777U
```

它等待本机获得 IP 后连接 `192.168.2.2:7777`，连接失败或断开时自动重试。连接成功后不主动生成
业务数据，只等待对端发送，并把收到的 packet 原样回发。

> 不要让两个“无条件 Echo”端点互相连接后再注入数据。两边都会继续回发同一份数据，形成无限
> Echo 循环并持续占用网络与 CPU。测试 Client Echo 时，对端应当是“发送一次、验证响应”的测试程序，
> 而不是另一个收到数据就永久回发的 Echo 服务。

### 6.7 packet pool 和 ETH 描述符

`app_eth.c` 创建 30 个、每个有效载荷 1536 字节的 packet。计算时还包含 `NX_PACKET` 头和对齐，
最终整个 pool 放入 64 KiB 非缓存 AXI SRAM。ETH RX/TX 描述符各 4 个，放在 `0x30000000` 的
非缓存 RAM_D2。

packet 会同时被 TCP 未确认发送队列、RX 描述符、DHCP 和业务 socket 占用。不要因为空闲时看起来
富余就随意减小 30；增加并发、扩大 TCP window 或增加协议模块后，应观察 `Net_Info_Terminal` 的
`free`、`empty_req` 和 `suspended` 再评估容量。

## 7. 在此基础上增加业务

### 7.1 删除或替换 Echo 示例

产品工程不需要 Echo 时：

1. 删除 `app_tcp_server_echo.*` 和/或 `app_tcp_client_echo.*`。
2. 从 `app_eth.c` 删除对应 include 和 `*_create()` 调用。
3. 从 `cmake/user_app.cmake` 删除对应 `.c`。

如果要实现多连接 TCP Server，建议先阅读 Server Echo 顶部关于 listen/relisten/unlisten 的说明；
如果要实现主动连接业务，可在 Client Echo 的自动重连框架上替换收发循环。

### 7.2 挂接新线程

当前业务启动链路：

```text
tx_application_define()
  → AppTaskStart
    → AppTaskCreate()
      → thread_eth_create()
        → thread_eth_entry()
          → app_eth_config_init()
          → nx_ip_create() + ARP/ICMP/UDP/TCP
          → app_eth_link_check_create()
          → app_eth_dhcp_create()
          → app_tcp_server_echo_create()
          → app_tcp_client_echo_create()
```

一般网络业务可在 `thread_eth_entry()` 中调用自己的 `xxx_create()`；业务线程内部先等待 link 或 IP：

```c
app_eth_link_wait_up(TX_WAIT_FOREVER);

ULONG actual_status;
nx_ip_status_check(&ip,
                   NX_IP_ADDRESS_RESOLVED,
                   &actual_status,
                   NX_WAIT_FOREVER);
```

仅依赖二层 link 的模块等待 link UP 即可；需要 TCP/UDP 地址通信的模块应等待
`NX_IP_ADDRESS_RESOLVED`。

### 7.3 API 使用边界

业务代码可以直接使用：

- `app_eth_link_get_state/wait_up/wait_down/sub_init/wait_change`
- `app_eth_config_get_default/get_current/apply`
- `app_eth_status_get`
- `threadx_printf/threadx_printf_clear/threadx_printf_cmd`
- NetX Duo 公共 `nx_*` API；IP 实例为全局 `extern NX_IP ip`

不要调用 `_app_eth_config_snapshot()`、`_app_eth_config_wait_pending()`、`_app_eth_status_set()`。
下划线前缀接口是 DHCP 模块内部契约，绕过它们会破坏 config/status 一致性。

### 7.4 配置持久化 TODO

`struct app_eth_config` 已按 64 字节定长 blob 设计，包含 `magic`、`version`、预留字段和末尾
`crc32`。实现持久化时建议：

1. 启动时从内部 Flash、外部 Flash 或 EEPROM 读取 64 字节配置。
2. 校验 `magic/version/crc32` 和网络字段；失败时调用 `app_eth_config_get_default()`。
3. `app_eth_config_apply()` 校验成功后，把新配置写入存储介质。
4. MAC 改变时先完成持久化，再执行软复位，使 ETH 初始化读到新 MAC。

新增字段优先从 `_rsv2[16]` 中分配，不要轻易改变已有字段顺序或总大小。

## 8. 已知行为 / 限制

- **App 必须烧到 `0x08020000`**：单独下载 `app.bin` 到 `0x08000000` 会覆盖 Boot，且 App
  的向量偏移与链接地址都不匹配。
- **Boot 仅校验向量表**：没有 CRC、签名、版本、升级状态和回滚策略。
- **网络配置不持久化**：复位恢复 DHCP + `192.168.2.233` fallback 等默认值。
- **Client Echo 目标固定**：默认 `192.168.2.2:7777`，换网络前修改
  `app_tcp_client_echo.h`。
- **两个 Echo 端点会形成回声循环**：对端也无条件 Echo 时，不要注入业务数据。
- **DHCP 模式在新的 link UP 后会重试**：即使此前已经 fallback，只要配置中的
  `dhcp_enable` 仍为 1，拔插网线后的 link UP 会重新启动 DISCOVER；纯静态模式不会。
- **Cache 属性不可随意改变**：packet pool 和描述符离开非缓存区后，必须补充严格的 Cache 维护。
- **packet pool 是共享资源**：高并发 TCP、DHCP 和 RX 描述符会共同消耗 30 个 packet。
- **IPv6 已关闭**：`User/netxduo/nx_user.h` 定义了 `NX_DISABLE_IPV6`。
- **没有 TLS 和业务鉴权**：Echo 数据为明文，只用于局域网联调和示例。
- **标准 `printf` 依赖 ThreadX printf mutex**：App 内核初始化前不要走 `_write`；Boot 使用 RTT 的
  weak `__io_putchar` 路径，不依赖该 mutex。

## 9. PHY 与板级 Ethernet

### 9.1 当前 PHY 结构

当前使用 ST LAN8742 组件驱动来驱动板上的 LAN8720A：

- `User/dev/lan8742/`：ST 提供的 LAN8742 寄存器和状态机实现。
- `User/netxduo/lan8720/nx_stm32_phy_driver.c`：NetX ETH 驱动要求的 PHY 适配接口。
- `User/bsp/src/bsp_eth.c`：RMII GPIO、PHY 就绪延时、ETH HAL 和中断。

目录名保留为 `lan8720`，实际组件和日志仍以 LAN8742 命名，这是历史原因，不影响功能：
**LAN8720A 与 LAN8742A 的厂商寄存器布局一致**，PHY 地址都镜像在 0x12 寄存器（Special Modes
Register）的低 5 位，所以同一套代码对两者都成立。PHY 初始化会扫描 0~31 地址，并用该地址字段
确认设备，不在上层硬编码固定 MDIO 地址。

板上 `RXER/PHYAD0` 未连接（内部下拉），PHY 地址固定为 0。上板后可看 `Netx_Terminal` 的
`[phy] init ok: addr=0, ID1=0x0007, ID2=0xC0F1` —— LAN8720A 的 ID 是 `0x0007/0xC0F1`，
LAN8742A 则是 `0x0007/0xC130`，用 ID2 可以区分实际焊的是哪一颗。

### 9.2 PHY 适配接口

NetX ETH 驱动通过以下接口访问 PHY：

```c
int32_t nx_eth_phy_init(void);
int32_t nx_eth_phy_get_link_state(void);
int32_t nx_eth_phy_set_link_state(int32_t linkstate);
nx_eth_phy_handle_t nx_eth_phy_get_handle(void);
```

`nx_eth_phy_get_link_state()` 返回 `ETH_PHY_STATUS_LINK_DOWN`、
`ETH_PHY_STATUS_100MBITS_FULLDUPLEX` 等状态，NetX 驱动据此更新 MAC 速率和双工模式。

### 9.3 更换 PHY

1. 新增或替换 `User/netxduo/<phy>/` 适配目录，实现上述 4 个接口。
2. 使用 `HAL_ETH_ReadPHYRegister()` / `HAL_ETH_WritePHYRegister()` 完成 MDIO 访问。
3. 在 `cmake/user_app.cmake` 中替换 PHY include 路径和源文件。
4. 若不再使用 LAN8742 组件，从构建列表中移除 `User/dev/lan8742/lan8742.c`。
5. 核对 RMII/MII 模式、PHY nRST、REF_CLK、MDIO 地址和协商速率。
6. 使用 Keil 时同步修改工程分组和 include path。

更换为千兆 PHY 不代表 STM32H743 内置 MAC 自动支持千兆；必须以 MCU Ethernet MAC、板级接口和
PHY 工作模式的共同能力为准。

## 10. User/ 代码来源

| 目录 | 来源 | 说明 |
|---|---|---|
| `User/app/` | 本工程自研 | ThreadX 业务线程、网络配置、link 状态机和 Echo 示例 |
| `User/boot/` | 本工程自研 | App 向量校验和跳转 |
| `User/bsp/` | 本工程自研 / CubeMX 基础上整理 | 时钟、MPU、Cache、ETH 和 RTT 初始化 |
| `User/core/` | STM32CubeMX / CMSIS 模板 | 中断、HAL MSP、system、syscalls |
| `User/netxduo/nx_stm32_eth_driver.*` | Microsoft Azure RTOS / ST 模板改写 | NetX Duo 与 STM32 HAL ETH 适配 |
| `User/netxduo/lan8720/` | ST PHY 模板适配 | LAN8742 与 NetX ETH 驱动之间的接口层 |
| `User/dev/lan8742/` | ST 官方组件 | LAN8742 寄存器和 PHY 控制 |
| `User/SEGGER_RTT/` | SEGGER 配置 | RTT 用户配置 |

以下用户配置/移植文件可对照 submodule 中的官方模板：

| 本工程文件 | 官方参考 |
|---|---|
| `User/threadx/tx_user.h` | `Middlewares/threadx/common/inc/tx_user_sample.h` |
| `User/threadx/tx_initialize_low_level.S` | ThreadX `ports/cortex_m7/gnu` 示例 |
| `User/netxduo/nx_user.h` | `Middlewares/netxduo/common/inc/nx_user_sample.h` |
| `User/SEGGER_RTT/config/SEGGER_RTT_Conf.h` | `Middlewares/SEGGER_RTT/Config/SEGGER_RTT_Conf.h` |

ThreadX、NetX Duo、STM32 HAL/CMSIS、LAN8742 和 SEGGER RTT 分别遵循各自目录中的许可文件。
