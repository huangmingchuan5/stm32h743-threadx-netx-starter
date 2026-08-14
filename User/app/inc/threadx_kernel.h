/**
 * @file    threadx_kernel.h
 * @brief   ThreadX 内核引导模块占位头
 *
 * @note    tx_application_define 是 ThreadX 提供的弱符号入口，由 tx_kernel_enter
 *          直接回调，不在本头文件暴露。my_stack_error_handler 通过
 *          tx_thread_stack_error_notify 注册，也不需要对外可见。
 *          目前本头文件不暴露任何符号
 */
#ifndef __THREADX_KERNEL_H
#define __THREADX_KERNEL_H

#endif //__THREADX_KERNEL_H