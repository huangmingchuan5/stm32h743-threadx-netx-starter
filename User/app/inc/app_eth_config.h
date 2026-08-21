/**
 * @file    app_eth_config.h
 * @brief   网络配置中心：cfg/status 数据持有、参数校验与异步 apply
 *
 * @par Public API
 *      业务代码可直接调用：
 *        - app_eth_config_init           由 thread_eth_entry 调一次
 *        - app_eth_config_get_default    取内置默认 cfg
 *        - app_eth_config_get_current    取当前正在用的 cfg 快照
 *        - app_eth_status_get            取当前运行时状态快照
 *        - app_eth_config_apply          应用新 cfg（异步）
 *
 * @par Internal API
 *      下划线前缀（_app_eth_*）仅供 app_eth_dhcp 模块使用，业务代码请勿
 *      调用——这些函数与 DHCP 线程上的状态机隐式耦合，绕过约定会破坏
 *      cfg/status 一致性。
 */
#ifndef STM32F4_TX_NETX_TEMPLATE_APP_ETH_CONFIG_H
#define STM32F4_TX_NETX_TEMPLATE_APP_ETH_CONFIG_H

#include <stdint.h>
#include "tx_api.h"
#include "app_eth_link_check.h"

#define APP_ETH_CONFIG_MAGIC        0xE7C0FFEEUL
#define APP_ETH_CONFIG_VERSION      1UL

#define APP_ETH_DHCP_TIMEOUT_DEFAULT_MS  30000UL

/*
 * 网络配置结构体。
 * 字段宽度全部定长、显式 padding、CRC 在末尾，后续可直接 memcpy 到 flash sector。
 * 不要轻易改字段顺序，新增字段优先用 _rsv2 预留区。
 *
 * 总大小：64 字节，自然 8B 对齐。
 *
 * IP / mask / gateway / dns 用主机序 uint32_t，和 NetX 的 IP_ADDRESS() / ULONG
 * 表达方式一致（注意：不是网络序）。
 */
struct app_eth_config {
    /* 校验/版本 —— flash 持久化用 */
    uint32_t magic;
    uint32_t version;

    /* IP 配置（静态 IP / DHCP fallback IP 共用） */
    uint32_t ip;
    uint32_t mask;
    uint32_t gateway;
    uint32_t dns1;
    uint32_t dns2;

    /* DHCP */
    uint32_t dhcp_timeout_ms; /* 0 表示用 DEFAULT */
    uint8_t dhcp_enable; /* 0/1 */
    uint8_t _rsv0[3];

    /* 链路层 */
    uint8_t mac[6]; /* 全 0 表示沿用 BSP 默认 */
    uint8_t _rsv1[2];

    /* 预留扩展位（后续新字段从这里割） + 末尾 CRC */
    uint8_t _rsv2[16];
    uint32_t crc32;
};

/* 加字段时一旦越过 64B 边界立刻编译失败，提醒重新评估持久化布局 */
_Static_assert(sizeof(struct app_eth_config) == 64,
               "app_eth_config size drift, review flash layout");

/*
 * 当前 IP 来源，反映"为什么用现在这套 IP/mask/gw"。
 * status 而非 config —— 不需要持久化。
 */
enum app_eth_ip_source {
    APP_ETH_IP_NONE = 0, /* 还没生效 */
    APP_ETH_IP_STATIC, /* 用户配置就是静态 */
    APP_ETH_IP_DHCP, /* DHCP 拿到的 */
    APP_ETH_IP_DHCP_FALLBACK, /* 配置是 DHCP，但超时回退到了静态 */
};

struct app_eth_status {
    enum app_eth_link_state link;
    enum app_eth_ip_source ip_source;
    uint32_t ip;
    uint32_t mask;
    uint32_t gateway;
    uint32_t dns1;
    uint32_t dns2;
};

/* ============================= Public API ============================= */

/**
 * @brief  把内置默认 cfg 填到 *cfg
 * @param  cfg 待填充；NX_NULL 直接返回
 * @note   默认 DHCP 启用，静态回退 192.168.2.233/24 gw=.1，
 *         DNS 8.8.8.8/8.8.4.4，MAC 根据芯片唯一 ID 生成
 */
void app_eth_config_get_default(struct app_eth_config *cfg);

/**
 * @brief  取当前正在用的 cfg 快照
 * @param  cfg 输出；NX_NULL 或未初始化直接返回
 * @note   读时持锁，可在任意线程上下文使用
 */
void app_eth_config_get_current(struct app_eth_config *cfg);

/**
 * @brief  取当前运行时状态快照（IP 来源、ip/mask/gw/dns、link）
 * @param  out 输出；NX_NULL 或未初始化直接返回
 * @note   link 字段实时从 link_check 模块取
 */
void app_eth_status_get(struct app_eth_status *out);

/**
 * @brief  应用新配置（异步）
 * @param  cfg 新配置
 * @return NX_SUCCESS / NX_NOT_SUCCESSFUL（cfg 非法或未初始化）
 * @note   仅做参数校验并把 cfg 提交到 pending，立即返回；DHCP 线程在下一次
 *         循环中真正应用。
 *         若 MAC 与当前不同，本函数会调 NVIC_SystemReset 不返回
 */
UINT app_eth_config_apply(const struct app_eth_config *cfg);

/**
 * @brief  初始化 cfg/status/同步原语，把 cfg 填成默认值
 * @note   由 thread_eth_entry 在 nx_ip_create 之前调用一次，重复调用是 no-op
 */
void app_eth_config_init(void);

/* ============================ Internal API ============================
 * 下划线前缀函数仅供 app_eth_dhcp 模块调用，业务代码请勿直接使用。
 * 详见本文件头部说明。
 */

/**
 * @brief  取当前 cfg 副本（DHCP 线程内部用）
 */
void _app_eth_config_snapshot(struct app_eth_config *out);

/**
 * @brief  阻塞等待 pending cfg（DHCP 线程内部用）
 * @return NX_SUCCESS 时通过 out 拿到新 cfg；超时或被吃掉返回错误码
 */
UINT _app_eth_config_wait_pending(struct app_eth_config *out, ULONG wait_ticks);

/**
 * @brief  汇报当前 IP 来源与地址（DHCP 线程内部用）
 */
void _app_eth_status_set(enum app_eth_ip_source src,
                         uint32_t ip, uint32_t mask, uint32_t gateway,
                         uint32_t dns1, uint32_t dns2);

#endif /* STM32F4_TX_NETX_TEMPLATE_APP_ETH_CONFIG_H */
