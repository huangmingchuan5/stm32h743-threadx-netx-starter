/**
 * @file    threadx_kernel.c
 * @brief   ThreadX 内核引导与 CPU 利用率统计
 *
 * @note    tx_application_define 创建启动 / 统计 / 空闲三个内核任务，
 *          启动任务完成 OS 适配（统计参数、HAL tick、BSP）后把业务任务
 *          交给 AppTaskCreate（在 threadx_task_manager.c 中实现）。
 *          统计任务每秒打印一次 CPU 占用条到 Manage_Terminal
 */
#include "threadx_kernel.h"
#include "tx_api.h"
#include "tx_port.h"
#include "tx_timer.h"
#include "stm32h7xx_hal.h"
#include "bsp.h"
#include <stdbool.h>
#include "threadx_printf.h"

/*
*********************************************************************************************************
*                                 任务优先级，数值越小优先级越高
*********************************************************************************************************
*/
#define APP_CFG_TASK_START_PRIO 2u
#define APP_CFG_TASK_STAT_PRIO  30u
#define APP_CFG_TASK_IDLE_PRIO  31u

/*
*********************************************************************************************************
*                                    任务栈大小，单位字节
*********************************************************************************************************
*/
#define APP_CFG_TASK_START_STK_SIZE 2048u
#define APP_CFG_TASK_IDLE_STK_SIZE  1024u
#define APP_CFG_TASK_STAT_STK_SIZE  2048u

/*
*********************************************************************************************************
*                                        任务控制块地址与任务栈
*********************************************************************************************************
*/
static TX_THREAD AppTaskStartTCB;
static uint64_t AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE / 8];

static TX_THREAD AppTaskIdleTCB;
static uint64_t AppTaskIdleStk[APP_CFG_TASK_IDLE_STK_SIZE / 8];

static TX_THREAD AppTaskStatTCB;
static uint64_t AppTaskStatStk[APP_CFG_TASK_STAT_STK_SIZE / 8];

/*
*********************************************************************************************************
*                                         变量
*********************************************************************************************************
*/
/* 统计任务使用 */
__IO uint8_t OSStatRdy; /* 统计任务就绪标志 */
__IO uint32_t OSIdleCtr; /* 空闲任务计数 */
__IO
float OSCPUUsage; /* CPU百分比 */
__IO
float OSCPUUsage_Max; /* CPU百分比 */
uint32_t OSIdleCtrMax; /* 1秒内最大的空闲计数 */
uint32_t OSIdleCtrRun; /* 1秒内空闲任务当前计数 */

/*
*********************************************************************************************************
*                                      函数声明
*********************************************************************************************************
*/
static void AppTaskStart(ULONG thread_input);

static void AppTaskIDLE(ULONG thread_input);

static void AppTaskStat(ULONG thread_input);

static void OSStatInit(void);

void my_stack_error_handler(TX_THREAD *thread_ptr);

static void DispTaskInfo(void);

/**
 * @brief ThreadX核心线程创建，创建初始化线程和统计线程
 * @param first_unused_memory 未使用的地址空间
 */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    /*
       主函数进入kernal后开始创建任务
       此函数仅用于实现启动任务，统计任务，其它任务在函数
       AppTaskCreate里面创建。
    */

    /*
       如果实现任务CPU利用率统计的话，此函数仅用于实现启动任务，统计任务和空闲任务，其它任务在函数
       AppTaskCreate里面创建。
    */
    /**************创建启动任务*********************/
    tx_thread_create(&AppTaskStartTCB, /* 任务控制块地址 */
                     "App Task Start", /* 任务名 */
                     AppTaskStart, /* 启动任务函数地址 */
                     0, /* 传递给任务的参数 */
                     &AppTaskStartStk[0], /* 堆栈基地址 */
                     APP_CFG_TASK_START_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_START_PRIO, /* 任务优先级*/
                     APP_CFG_TASK_START_PRIO, /* 任务抢占阀值 */
                     TX_NO_TIME_SLICE, /* 不开启时间片 */
                     TX_AUTO_START); /* 创建后立即启动 */

    /**************创建统计任务*********************/
    tx_thread_create(&AppTaskStatTCB, /* 任务控制块地址 */
                     "App Task STAT", /* 任务名 */
                     AppTaskStat, /* 启动任务函数地址 */
                     0, /* 传递给任务的参数 */
                     &AppTaskStatStk[0], /* 堆栈基地址 */
                     APP_CFG_TASK_STAT_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_STAT_PRIO, /* 任务优先级*/
                     APP_CFG_TASK_STAT_PRIO, /* 任务抢占阀值 */
                     TX_NO_TIME_SLICE, /* 不开启时间片 */
                     TX_AUTO_START); /* 创建后立即启动 */

    /**************创建空闲任务*********************/
    tx_thread_create(&AppTaskIdleTCB, /* 任务控制块地址 */
                     "App Task IDLE", /* 任务名 */
                     AppTaskIDLE, /* 启动任务函数地址 */
                     0, /* 传递给任务的参数 */
                     &AppTaskIdleStk[0], /* 堆栈基地址 */
                     APP_CFG_TASK_IDLE_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_IDLE_PRIO, /* 任务优先级*/
                     APP_CFG_TASK_IDLE_PRIO, /* 任务抢占阀值 */
                     TX_NO_TIME_SLICE, /* 不开启时间片 */
                     TX_AUTO_START); /* 创建后立即启动 */
}

extern void threadx_printf_mutex_init(void);

extern void AppTaskCreate(void);

/**
 * @brief 初始化线程，会在内部初始化外设、初始化统计参数和创建线程
 * @param thread_input 线程传递参数
 */
static void AppTaskStart(ULONG thread_input)
{
    (void)thread_input;

#ifndef TX_NO_TIMER
    tx_thread_suspend(&_tx_timer_thread);
#endif

    /* 优先执行任务统计 */
    OSStatInit();

#ifndef TX_NO_TIMER
    tx_thread_resume(&_tx_timer_thread);
#endif

    /* 内核开启后，恢复HAL里的时间基准 */
    HAL_ResumeTick();

    /* printf 互斥锁初始化 */
    threadx_printf_mutex_init();

    /* 外设初始化 */
    bsp_Init();

    /* 创建任务 */
    AppTaskCreate();

    /* 任务栈溢出处理 */
    tx_thread_stack_error_notify(my_stack_error_handler);

    /* 启动任务引导结束，启动任务进入终止态，在AppTaskStat中删除启动任务 */
    tx_thread_terminate(&AppTaskStartTCB);
}

/**
 * @brief 统计参数的初始化
 */
void OSStatInit(void)
{
    OSStatRdy = false;

    tx_thread_sleep(5u); /* 时钟同步 */

    //__disable_irq();
    OSIdleCtr = 0uL; /* 清空闲计数 */
    //__enable_irq();

    tx_thread_sleep(100); /* 统计100ms内，最大空闲计数 */

    //__disable_irq();
    OSIdleCtrMax = OSIdleCtr; /* 保存最大空闲计数 */
    OSStatRdy = true;
    //__enable_irq();
}

/**
 * @brief 统计线程
 * @param thread_input 线程参数
 */
static void AppTaskStat(ULONG thread_input)
{
    (void)thread_input;

    uint8_t time_1s = 0;

    while (OSStatRdy == false) {
        tx_thread_sleep(200); /* 等待统计任务就绪 */
    }

    OSIdleCtrMax = (OSIdleCtrMax + 99) / 100uL;
    if (OSIdleCtrMax == 0uL) {
        OSCPUUsage = 0u;
    }

    //__disable_irq();
    OSIdleCtr = OSIdleCtrMax * 100uL; /* 设置初始CPU利用率 0% */
    //__enable_irq();

    /* AppTaskStart启动任务运行结束后删除任务 */
    while (AppTaskStartTCB.tx_thread_state != TX_TERMINATED)
        tx_thread_sleep(10);

    tx_thread_delete(&AppTaskStartTCB);

    for (;;) {
        // __disable_irq();
        OSIdleCtrRun = OSIdleCtr; /* 获得100ms内空闲计数 */
        OSIdleCtr = 0uL; /* 复位空闲计数 */
        //	__enable_irq();            /* 计算100ms内的CPU利用率 */
        OSCPUUsage = (100.0 - (float)OSIdleCtrRun / OSIdleCtrMax);
        if (OSCPUUsage_Max < OSCPUUsage) {
            OSCPUUsage_Max = OSCPUUsage;
        }
        if (++time_1s == 10) {
            time_1s = 0;
            DispTaskInfo();
        }
        tx_thread_sleep(100); /* 每100ms统计一次 */
    }
}

/**
 * @brief 空闲线程
 * @param thread_input 线程参数
 */
static void AppTaskIDLE(ULONG thread_input)
{
    TX_INTERRUPT_SAVE_AREA

    (void)
        thread_input;

    while (1) {
        TX_DISABLE
        OSIdleCtr++;
        TX_RESTORE
    }
}

/**
 * @brief 线程栈溢出处理
 * @param thread_ptr 线程句柄
 */
void my_stack_error_handler(TX_THREAD *thread_ptr)
{
    TX_THREAD *p_tcb; /* 定义一个任务控制块指针 */

    threadx_printf(Manage_Terminal,
                   "===============================================================\r\n");
    threadx_printf(Manage_Terminal, "OverRun\r\n");
    threadx_printf(Manage_Terminal,
                   "===============================================================\r\n");
    threadx_printf(Manage_Terminal,
                   "   Prio     StackSize   CurStack    MaxStack   Taskname                                    \r\n");

    p_tcb = &AppTaskStartTCB;

    /* 遍历任务控制列表TCB list)，打印所有的任务的优先级和名称 */
    do {
        if (p_tcb != (TX_THREAD *)thread_ptr) {
            p_tcb = p_tcb->tx_thread_created_next;
        } else {
            threadx_printf(Manage_Terminal,
                           "   %2d        %5d      %5d       %5d      %s\r\n",
                           p_tcb->tx_thread_priority,
                           p_tcb->tx_thread_stack_size,
                           (int)p_tcb->tx_thread_stack_end - (int)p_tcb->
                           tx_thread_stack_ptr,
                           (int)p_tcb->tx_thread_stack_end -
                           (int)p_tcb->tx_thread_stack_highest_ptr,
                           p_tcb->tx_thread_name);

            while (1);
        }
    } while (1);
}

#define PBWIDTH 40
static const char bar_tmp[PBWIDTH + 1] =
    "||||||||||||||||||||||||||||||||||||||||";
static const char bar_space_tmp[PBWIDTH + 1] = "";
static const char label[4] = {'-', '\\', '|', '/'};

/**
 * @brief SEGGER RTT viewer输出threadx线程信息以及CPU占用率
 */
static void DispTaskInfo(void)
{
    TX_THREAD *p_tcb; /* 定义一个任务控制块指针 */
    p_tcb = &AppTaskStatTCB;

    threadx_printf(Manage_Terminal, "\r\n\r\n");
    threadx_printf_clear(Manage_Terminal);

    /* 打印标题 */
    int8_t bar_size = (OSCPUUsage / 100.0) * 40;
    static uint8_t cnt = 0;
    if (bar_size < 1)
        bar_size = 1;
    else if (bar_size > PBWIDTH)
        bar_size = PBWIDTH;
    int8_t bar_space_size = 40 - bar_size;

    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_TEXT_BRIGHT_CYAN);
    threadx_printf(Manage_Terminal, "  CPU ");
    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_RESET);
    threadx_printf(Manage_Terminal, "[");
    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_TEXT_BRIGHT_GREEN);
    threadx_printf(Manage_Terminal, "%.*s%*s", bar_size, bar_tmp,
                   bar_space_size, bar_space_tmp);
    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_RESET);
    threadx_printf(Manage_Terminal,
                   RTT_CTRL_TEXT_BRIGHT_BLACK"%6.2lf%%"RTT_CTRL_RESET,
                   OSCPUUsage);
    threadx_printf(Manage_Terminal, "]");
    threadx_printf(Manage_Terminal,
                   RTT_CTRL_TEXT_BRIGHT_BLACK" peak:%.2lf%%"RTT_CTRL_RESET,
                   OSCPUUsage_Max);
    threadx_printf(Manage_Terminal, "\r\n");

    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_TEXT_BRIGHT_CYAN);
    threadx_printf(Manage_Terminal, "  CPU Clock Speed ");
    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_RESET);
    double freq = HAL_RCC_GetSysClockFreq() / 1000000.0;
    threadx_printf(Manage_Terminal, "%.2lfMHz %c", freq, label[(cnt++) % 4]);

    threadx_printf(Manage_Terminal, "\r\n\r\n");

    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_TEXT_BLACK);
    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_BG_BRIGHT_GREEN);
    threadx_printf(Manage_Terminal,
                   "   Prio     StackSize   CurStack    MaxStack   Taskname\t\t\t\t\t\t\t\r\n");
    threadx_printf_cmd(Manage_Terminal, RTT_CTRL_RESET);

    /* 遍历任务控制列表TCB list)，打印所有的任务的优先级和名称 */
    while (p_tcb != (TX_THREAD *)0) {
        threadx_printf(Manage_Terminal,
                       "   %2d        %5d      %5d       %5d      %s\r\n",
                       p_tcb->tx_thread_priority, p_tcb->tx_thread_stack_size,
                       (int)p_tcb->tx_thread_stack_end - (int)p_tcb->
                       tx_thread_stack_ptr,
                       (int)p_tcb->tx_thread_stack_end - (int)p_tcb->
                       tx_thread_stack_highest_ptr,
                       p_tcb->tx_thread_name);

        p_tcb = p_tcb->tx_thread_created_next;

        if (p_tcb == &AppTaskStatTCB)
            break;
    }
}