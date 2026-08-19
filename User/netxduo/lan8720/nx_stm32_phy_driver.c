
/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


#include "nx_stm32_phy_driver.h"
#include "nx_stm32_eth_config.h"
#include "threadx_printf.h"

#include <limits.h>


/* LAN8742 IO functions */
static int32_t lan8742_io_init(void);
static int32_t lan8742_io_deinit (void);

static int32_t lan8742_io_write_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
static int32_t lan8742_io_read_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);

static int32_t lan8742_io_get_tick(void);
static const char *lan8742_link_state_name(int32_t link_state);
static void lan8742_log_registers(const char *reason, int32_t link_state);

/* LAN8742 IO context object */
static lan8742_IOCtx_t  LAN8742_IOCtx = { lan8742_io_init,
                                          lan8742_io_deinit,
                                          lan8742_io_write_reg,
                                          lan8742_io_read_reg,
                                          lan8742_io_get_tick
                                        };
/* LAN8742 main object */
static lan8742_Object_t LAN8742;
static int32_t previous_link_state = INT32_MIN;

/**
  * @brief  Initialize the PHY interface
  * @param  none
  * @retval ETH_PHY_STATUS_OK on success, ETH_PHY_STATUS_ERROR otherwise
  */

int32_t nx_eth_phy_init(void)
{
    int32_t register_status;
    int32_t init_status;
    uint32_t phy_id1 = 0U;
    uint32_t phy_id2 = 0U;

    /* Set PHY IO functions */
    register_status = LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);
    if (register_status != LAN8742_STATUS_OK)
    {
        threadx_printf(Netx_Terminal, "[phy] register bus IO failed: status=%ld\r\n",
                       (long)register_status);
        return ETH_PHY_STATUS_ERROR;
    }

    /* Initialize the LAN8742 ETH PHY */
    init_status = LAN8742_Init(&LAN8742);
    if (init_status != LAN8742_STATUS_OK)
    {
        threadx_printf(Netx_Terminal, "[phy] init failed: status=%ld\r\n", (long)init_status);
        return ETH_PHY_STATUS_ERROR;
    }

    if ((lan8742_io_read_reg(LAN8742.DevAddr, LAN8742_PHYI1R, &phy_id1) != ETH_PHY_STATUS_OK) ||
        (lan8742_io_read_reg(LAN8742.DevAddr, LAN8742_PHYI2R, &phy_id2) != ETH_PHY_STATUS_OK))
    {
        threadx_printf(Netx_Terminal, "[phy] ID read failed: addr=%lu\r\n",
                       (unsigned long)LAN8742.DevAddr);
    }
    else
    {
        threadx_printf(Netx_Terminal, "[phy] init ok: addr=%lu, ID1=0x%04lX, ID2=0x%04lX\r\n",
                       (unsigned long)LAN8742.DevAddr,
                       (unsigned long)phy_id1,
                       (unsigned long)phy_id2);

        if (((phy_id1 == 0xFFFFU) && (phy_id2 == 0xFFFFU)) ||
            ((phy_id1 == 0U) && (phy_id2 == 0U)))
        {
            threadx_printf(Netx_Terminal,
                           "[phy] WARNING: invalid PHY ID, check MDIO/MDC and PHY address\r\n");
        }
    }

    previous_link_state = INT32_MIN;
    return ETH_PHY_STATUS_OK;
}

/**
  * @brief  set the Phy link state.
  * @param  LinkState
  * @retval the link status.
  */

int32_t nx_eth_phy_set_link_state(int32_t LinkState)
{
    int32_t status = LAN8742_SetLinkState(&LAN8742, LinkState);

    threadx_printf(Netx_Terminal, "[phy] set state=%s(%ld), status=%ld\r\n",
                   lan8742_link_state_name(LinkState),
                   (long)LinkState,
                   (long)status);

    return status;
}

/**
  * @brief  get the Phy link state.
  * @param  none
  * @retval the link status.
  */

int32_t nx_eth_phy_get_link_state(void)
{
    int32_t  linkstate = LAN8742_GetLinkState(&LAN8742);

    if (linkstate != previous_link_state)
    {
        lan8742_log_registers("state changed", linkstate);
        previous_link_state = linkstate;
    }

    return linkstate;
}

/**
  * @brief  get the driver object handle
  * @param  none
  * @retval pointer to the LAN8742 main object
  */

nx_eth_phy_handle_t nx_eth_phy_get_handle(void)
{
    return (nx_eth_phy_handle_t)&LAN8742;
}

/**
  * @brief  Initialize the PHY MDIO interface
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */

int32_t lan8742_io_init(void)
{
  /* We assume that MDIO GPIO configuration is already done
     in the ETH_MspInit() else it should be done here
  */

  /* Configure the MDIO Clock */
  HAL_ETH_SetMDIOClockRange(&eth_handle);

  return ETH_PHY_STATUS_OK;
}

/**
  * @brief  De-Initialize the MDIO interface
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */
int32_t lan8742_io_deinit (void)
{
    return ETH_PHY_STATUS_OK;
}

/**
  * @brief  Read a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  pRegVal: pointer to hold the register value
  * @retval 0 if OK -1 if Error
  */
int32_t lan8742_io_read_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
  HAL_StatusTypeDef hal_status = HAL_ETH_ReadPHYRegister(&eth_handle, DevAddr, RegAddr, pRegVal);

  if(hal_status != HAL_OK)
  {
    threadx_printf(Netx_Terminal, "[phy] MDIO read failed: addr=%lu, reg=0x%02lX, HAL=%d\r\n",
                   (unsigned long)DevAddr,
                   (unsigned long)RegAddr,
                   (int)hal_status);
    return ETH_PHY_STATUS_ERROR;
  }

  return ETH_PHY_STATUS_OK;
}

int32_t lan8742_io_write_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
  HAL_StatusTypeDef hal_status = HAL_ETH_WritePHYRegister(&eth_handle, DevAddr, RegAddr, RegVal);

  if(hal_status != HAL_OK)
  {
    threadx_printf(Netx_Terminal,
                   "[phy] MDIO write failed: addr=%lu, reg=0x%02lX, value=0x%04lX, HAL=%d\r\n",
                   (unsigned long)DevAddr,
                   (unsigned long)RegAddr,
                   (unsigned long)RegVal,
                   (int)hal_status);
    return ETH_PHY_STATUS_ERROR;
  }

  return ETH_PHY_STATUS_OK;
}

/**
  * @brief  Get the time in millisecons used for internal PHY driver process.
  * @retval Time value
  */
int32_t lan8742_io_get_tick(void)
{
  return HAL_GetTick();
}

/**
  * @brief  Convert a PHY link state to a readable name.
  * @param  link_state PHY link state
  * @retval Link state name
  */
static const char *lan8742_link_state_name(int32_t link_state)
{
  switch (link_state)
  {
    case LAN8742_STATUS_LINK_DOWN:
      return "DOWN";
    case LAN8742_STATUS_100MBITS_FULLDUPLEX:
      return "100M_FULL";
    case LAN8742_STATUS_100MBITS_HALFDUPLEX:
      return "100M_HALF";
    case LAN8742_STATUS_10MBITS_FULLDUPLEX:
      return "10M_FULL";
    case LAN8742_STATUS_10MBITS_HALFDUPLEX:
      return "10M_HALF";
    case LAN8742_STATUS_AUTONEGO_NOTDONE:
      return "AUTONEGO";
    case LAN8742_STATUS_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

/**
  * @brief  Print the PHY registers used to determine link state.
  * @param  reason Reason for printing the register snapshot
  * @param  link_state Current PHY link state
  * @retval None
  */
static void lan8742_log_registers(const char *reason, int32_t link_state)
{
  uint32_t bcr = 0U;
  uint32_t bsr = 0U;
  uint32_t physcsr = 0U;
  uint32_t smr = 0U;

  (void)lan8742_io_read_reg(LAN8742.DevAddr, LAN8742_BCR, &bcr);
  (void)lan8742_io_read_reg(LAN8742.DevAddr, LAN8742_BSR, &bsr);
  (void)lan8742_io_read_reg(LAN8742.DevAddr, LAN8742_PHYSCSR, &physcsr);
  (void)lan8742_io_read_reg(LAN8742.DevAddr, LAN8742_SMR, &smr);

  threadx_printf(Netx_Terminal,
                 "[phy] %s: %s(%ld), addr=%lu, BCR=%04lX, BSR=%04lX, SCSR=%04lX, SMR=%04lX\r\n",
                 reason,
                 lan8742_link_state_name(link_state),
                 (long)link_state,
                 (unsigned long)LAN8742.DevAddr,
                 (unsigned long)bcr,
                 (unsigned long)bsr,
                 (unsigned long)physcsr,
                 (unsigned long)smr);

  if ((bcr == 0xFFFFU) || (bsr == 0xFFFFU) ||
      (physcsr == 0xFFFFU) || (smr == 0xFFFFU))
  {
    threadx_printf(Netx_Terminal,
                   "[phy] WARNING: register reads 0xFFFF, check PHY power/reset and MDIO/MDC\r\n");
  }
}
