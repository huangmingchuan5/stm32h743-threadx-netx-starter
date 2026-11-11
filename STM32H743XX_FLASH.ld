/*
******************************************************************************
**

**  File        : LinkerScript.ld
**
**  Author		: STM32CubeMX
**
**  Abstract    : Linker script for STM32H743XIHx series
**                2048Kbytes FLASH and 1056Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed “as is,” without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** <h2><center>&copy; COPYRIGHT(c) 2025 STMicroelectronics</center></h2>
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**   1. Redistributions of source code must retain the above copyright notice,
**      this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright notice,
**      this list of conditions and the following disclaimer in the documentation
**      and/or other materials provided with the distribution.
**   3. Neither the name of STMicroelectronics nor the names of its contributors
**      may be used to endorse or promote products derived from this software
**      without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Specify the memory areas */
MEMORY
{
DTCMRAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K
RAM (xrw)      : ORIGIN = 0x24000000, LENGTH = 512K
RAM_D2 (xrw)      : ORIGIN = 0x30000000, LENGTH = 288K
RAM_D3 (xrw)      : ORIGIN = 0x38000000, LENGTH = 64K
ITCMRAM (xrw)      : ORIGIN = 0x00000000, LENGTH = 64K
FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 2048K
}

/* ==========================================================================
 * ThreadX 移植改动 #1：主 RAM 改为 AXI SRAM(0x24000000, 512K)
 *   Keil 实际链接用的 output.sct 把全部 RW/ZI 放在 0x24000000，
 *   且安富莱 MPU_Config() 把这块配置为 Write-back Cacheable。
 *   512K 也更适合放 ThreadX 的任务栈/内存池（DTCM 只有 128K）。
 *   _estack 必须与 .data/.bss/heap 同一块 RAM，否则 sysmem.c 的 _sbrk 会算错堆上限。
 * ========================================================================== */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* 0x24080000，AXI SRAM 顶端 */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size  = 0x1000;    /* 4 KB newlib 堆 */
_Min_Stack_Size = 0x1000;    /* 4 KB MSP / ThreadX 系统栈预留 */

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    /* ThreadX 移植改动 #2：GNU 版 tx_initialize_low_level.S 用 _vectors
     * 重设 SCB->VTOR 并读取初始 MSP；CMSIS 启动文件只提供 g_pfnVectors，
     * 因此在链接脚本里补一个指向 .isr_vector 起始地址的符号，
     * 而不是去修改 ThreadX 官方汇编（便于日后升级内核）。 */
    PROVIDE ( _vectors = . );
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab (READONLY) : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM (READONLY) : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array (READONLY) : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array (READONLY) : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array (READONLY) : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    . = ALIGN(4);
  } >RAM AT> FLASH

 /* Initialized TLS data section */
  .tdata : ALIGN(4)
  {
    *(.tdata .tdata.* .gnu.linkonce.td.*)
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
    PROVIDE(__data_end = .);
    PROVIDE(__tdata_end = .);
  } >RAM AT> FLASH

  PROVIDE( __tdata_start = ADDR(.tdata) );
  PROVIDE( __tdata_size = __tdata_end - __tdata_start );

  PROVIDE( __data_start = ADDR(.data) );
  PROVIDE( __data_size = __data_end - __data_start );

  PROVIDE( __tdata_source = LOADADDR(.tdata) );
  PROVIDE( __tdata_source_end = LOADADDR(.tdata) + SIZEOF(.tdata) );
  PROVIDE( __tdata_source_size = __tdata_source_end - __tdata_source );

  PROVIDE( __data_source = LOADADDR(.data) );
  PROVIDE( __data_source_end = __tdata_source_end );
  PROVIDE( __data_source_size = __data_source_end - __data_source );
  /* Uninitialized data section */
  .tbss (NOLOAD) : ALIGN(4)
  {
     /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.tbss .tbss.*)
    . = ALIGN(4);
    PROVIDE( __tbss_end = . );
  } >RAM

  PROVIDE( __tbss_start = ADDR(.tbss) );
  PROVIDE( __tbss_size = __tbss_end - __tbss_start );
  PROVIDE( __tbss_offset = ADDR(.tbss) - ADDR(.tdata) );

  PROVIDE( __tls_base = __tdata_start );
  PROVIDE( __tls_end = __tbss_end );
  PROVIDE( __tls_size = __tls_end - __tls_base );
  PROVIDE( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );
  PROVIDE( __tls_size_align = (__tls_size + __tls_align - 1) & ~(__tls_align - 1) );
  PROVIDE( __arm32_tls_tcb_offset = MAX(8, __tls_align) );
  PROVIDE( __arm64_tls_tcb_offset = MAX(16, __tls_align) );

  .bss (NOLOAD) : ALIGN(4)
  {
    *(.bss)
    *(.bss*)
    *(COMMON)

      . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
      PROVIDE( __bss_end = .);
  } >RAM
  PROVIDE( __non_tls_bss_start = ADDR(.bss) );

  /* ------------------------------------------------------------------
   * DMA 缓冲区段
   * DTCM(0x20000000) 是 CPU 私有 TCM，DMA1/DMA2 通过 AHB/AXI 访问不到，
   * 若把 DMA 目的缓冲区放在 .bss（本脚本指向 DTCMRAM），DMA 写入无效，
   * CPU 读回恒为 0。
   * 这里单独开一段放到 D2 SRAM(0x30000000)，DMA 可正常访问。
   * 用法： __IO uint16_t buf[2] __attribute__((section(".dma_buffer")));
   * ------------------------------------------------------------------ */
  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(4);
    *(.dma_buffer)
    *(.dma_buffer*)
    . = ALIGN(4);
  } >RAM_D2


  PROVIDE( __bss_start = __tbss_start );
  PROVIDE( __bss_size = __bss_end - __bss_start );

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* ------------------------------------------------------------------
   * 可选高速/外设 RAM 段。默认 .data/.bss 全部在 AXI SRAM；
   * 需要把热点数据放 DTCM，或把以太网/ADC 缓冲放 D2/D3 域时，
   * 用 __attribute__((section(".dtcmram"))) 之类显式指定即可。
   * ------------------------------------------------------------------ */
  .dtcmram (NOLOAD) :
  {
    . = ALIGN(4);
    _sdtcmram = .;
    *(.dtcmram)
    *(.dtcmram*)
    . = ALIGN(4);
    _edtcmram = .;
  } >DTCMRAM

  .ram_d3 (NOLOAD) :
  {
    . = ALIGN(4);
    *(.ram_d3)
    *(.ram_d3*)
    . = ALIGN(4);
  } >RAM_D3

  /* ==========================================================================
   * ThreadX 移植改动 #3：__RAM_segment_used_end__
   *   GNU 版 tx_initialize_low_level.S 把该符号 +4 作为
   *   tx_application_define(first_unused_memory) 的起点。
   *   这里在 newlib 堆/栈预留之后再留出 32KB，避免 ThreadX 的动态内存
   *   与 malloc/MSP 栈打架。示例 main.c 用的是静态任务栈，不受影响；
   *   若后续用 tx_byte_pool_create 等动态分配，可按需要调大余量。
   * ========================================================================== */
  PROVIDE ( __RAM_segment_used_end__ =
            ADDR(._user_heap_stack) + SIZEOF(._user_heap_stack) + 0x8000 );
  ASSERT ( __RAM_segment_used_end__ < ORIGIN(RAM) + LENGTH(RAM) - _Min_Stack_Size,
           "AXI SRAM 剩余空间不足：请减小 _Min_Heap_Size 或 ThreadX 保留余量" )

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a:* ( * )
    libm.a:* ( * )
    libgcc.a:* ( * )
  }

}
