# =============================================================================
#  板级支持包 BSP —— 系统初始化 / 时钟 / 缓存 / MPU / RTT 初始化
#
#  只编译本工程用到的两个 BSP 源文件：
#    bsp.c             System_Init()：MPU / Cache / HAL / 480MHz 时钟 / RTT 初始化
#    bsp_RTT_viewer.c  RTT_viewer_init()：SEGGER RTT 通道与终端初始化
#  外设驱动（ETH 等）随业务源码一起编进可执行目标，见顶层 CMakeLists.txt。
# =============================================================================

set(BSP_DIR "${CMAKE_SOURCE_DIR}/User/bsp")

set(BSP_SOURCES
    ${BSP_DIR}/src/bsp.c
    ${BSP_DIR}/src/bsp_RTT_viewer.c
)

add_library(bsp OBJECT ${BSP_SOURCES})
target_link_libraries(bsp PRIVATE netx_app_config)
