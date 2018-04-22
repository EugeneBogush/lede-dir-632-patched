/*
 * rtl8309g.c: RTL8309G switch driver
 *
 * Copyright (C) 2017 Konstantin Kuzov <master.nosferatu@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/if.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <net/genetlink.h>
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/phy.h>

#define RTL8309G_MAGIC 0x8309
#define RTL8309G_NAME "RTL8309G"
#define RTL8309G_NUM_VLANS 9
#define RTL8309G_NUM_PORTS 9
#define RTL8309G_PORT_CPU 8

struct rtl8309g_reg {
	uint8_t phy;
	uint8_t reg;
	uint8_t bits;
	uint8_t shift;
};

#define RTL8309G_PHY_REGOFS(name) \
	(RTL8309G_PHY_1_PORT_##name - RTL8309G_PHY_0_PORT_##name)

#define RTL8309G_PHY_REG(id, reg) \
	(RTL8309G_PHY_0_PORT_##reg + (id * RTL8309G_PHY_REGOFS(reg)))

enum rtl8309g_regidx {
#define RTL8309G_PORT_ENUM(id) \
	RTL8309G_PHY_##id##_PORT_CTRL_RESET, \
	RTL8309G_PHY_##id##_PORT_CTRL_LOOPBACK, \
	RTL8309G_PHY_##id##_PORT_CTRL_SPEED, \
	RTL8309G_PHY_##id##_PORT_CTRL_ANEG, \
	RTL8309G_PHY_##id##_PORT_CTRL_POWER_DOWN, \
	RTL8309G_PHY_##id##_PORT_CTRL_ISOLATE, \
	RTL8309G_PHY_##id##_PORT_CTRL_ANEG_RESTART, \
	RTL8309G_PHY_##id##_PORT_CTRL_DUPLEX, \
	RTL8309G_PHY_##id##_PORT_STATUS_100BASE_TX_FD, \
	RTL8309G_PHY_##id##_PORT_STATUS_100BASE_TX_HD, \
	RTL8309G_PHY_##id##_PORT_STATUS_10BASE_T_FD, \
	RTL8309G_PHY_##id##_PORT_STATUS_10BASE_T_HD, \
	RTL8309G_PHY_##id##_PORT_STATUS_MF_PREAM_SUPR, \
	RTL8309G_PHY_##id##_PORT_STATUS_ANEG_COMPLETE, \
	RTL8309G_PHY_##id##_PORT_STATUS_REM_FAULT, \
	RTL8309G_PHY_##id##_PORT_STATUS_ANEG_ABILITY, \
	RTL8309G_PHY_##id##_PORT_STATUS_LINK_STATUS, \
	RTL8309G_PHY_##id##_PORT_STATUS_JABBER_DETECT, \
	RTL8309G_PHY_##id##_PORT_ANEG_ADV_REM_FAULT, \
	RTL8309G_PHY_##id##_PORT_ANEG_ADV_FLOW_CTRL, \
	RTL8309G_PHY_##id##_PORT_ANEG_ADV_100BASE_TX_FD, \
	RTL8309G_PHY_##id##_PORT_ANEG_ADV_100BASE_TX_HD, \
	RTL8309G_PHY_##id##_PORT_ANEG_ADV_10BASE_T_FD, \
	RTL8309G_PHY_##id##_PORT_ANEG_ADV_10BASE_T_HD, \
	RTL8309G_PHY_##id##_PORT_CTRL0_NULL_VID_REPLACE, \
	RTL8309G_PHY_##id##_PORT_CTRL0_DISCARD_NON_PVID_PKT, \
	RTL8309G_PHY_##id##_PORT_CTRL0_DISABLE_PRIORITY, \
	RTL8309G_PHY_##id##_PORT_CTRL0_DISABLE_DIFF_PRIORITY, \
	RTL8309G_PHY_##id##_PORT_CTRL0_DISABLE_PORT_BASE_PRIORITY, \
	RTL8309G_PHY_##id##_PORT_CTRL0_VLAN_TAG, \
	RTL8309G_PHY_##id##_PORT_CTRL1_ENABLE_TRANSMISSION, \
	RTL8309G_PHY_##id##_PORT_CTRL1_ENABLE_RECEPTION, \
	RTL8309G_PHY_##id##_PORT_CTRL1_ENABLE_LEARNING, \
	RTL8309G_PHY_##id##_PORT_CTRL2_VLAN_INDEX, \
	RTL8309G_PHY_##id##_PORT_CTRL2_VLAN_MEMBERSHIP, \
	RTL8309G_PHY_##id##_PORT_VLAN_ID
	RTL8309G_PORT_ENUM(0),
	RTL8309G_PORT_ENUM(1),
	RTL8309G_PORT_ENUM(2),
	RTL8309G_PORT_ENUM(3),
	RTL8309G_PORT_ENUM(4),
	RTL8309G_PORT_ENUM(5),
	RTL8309G_PORT_ENUM(6),
	RTL8309G_PORT_ENUM(7),
	RTL8309G_PORT_ENUM(8),

#define RTL8309G_PORT_ENUM_NC(id) \
	RTL8309G_PHY_##id##_PORT_ANEG_LINK_REM_FAULT, \
	RTL8309G_PHY_##id##_PORT_ANEG_LINK_FLOW_CTRL, \
	RTL8309G_PHY_##id##_PORT_ANEG_LINK_100BASE_TX_FD, \
	RTL8309G_PHY_##id##_PORT_ANEG_LINK_100BASE_TX_HD, \
	RTL8309G_PHY_##id##_PORT_ANEG_LINK_10BASE_T_FD, \
	RTL8309G_PHY_##id##_PORT_ANEG_LINK_10BASE_T_HD, \
	RTL8309G_PHY_##id##_PORT_CTRL0_LOOPBACK, \
	RTL8309G_PHY_##id##_PORT_CTRL1_LOOP_STATUS, \
	RTL8309G_PHY_##id##_PORT_CTRL1_LINK_QUALITY
	RTL8309G_PORT_ENUM_NC(0),
	RTL8309G_PORT_ENUM_NC(1),
	RTL8309G_PORT_ENUM_NC(2),
	RTL8309G_PORT_ENUM_NC(3),
	RTL8309G_PORT_ENUM_NC(4),
	RTL8309G_PORT_ENUM_NC(5),
	RTL8309G_PORT_ENUM_NC(6),
	RTL8309G_PORT_ENUM_NC(7),

	RTL8309G_GCTRL0_LED_MODE,
	RTL8309G_GCTRL0_RESET,
	RTL8309G_GCTRL0_DISABLE_VLAN,
	RTL8309G_GCTRL0_DISABLE_VLAN_TAG_AWARE,
	RTL8309G_GCTRL0_DISABLE_VLAN_INGRESS_FILTER,
	RTL8309G_GCTRL0_DISABLE_VLAN_TAG_CONTROL,
	RTL8309G_GCTRL0_EEPROM_EXIST,
	RTL8309G_GCTRL0_DISABLE_ERROR_FILTER,
	RTL8309G_GCTRL0_ENABLE_FLOW_CTRL_TRANSMIT,
	RTL8309G_GCTRL0_ENABLE_FLOW_CTRL_RECEIVE,
	RTL8309G_GCTRL0_ENABLE_BROADCAST,
	RTL8309G_GCTRL0_ENABLE_AGING,
	RTL8309G_GCTRL0_ENABLE_FAST_AGING,
	RTL8309G_GCTRL0_ENABLE_ISP_MAC_TRANSLATION,

	RTL8309G_GCTRL1_PRIORITY,
	RTL8309G_GCTRL1_TRUNKING_PORT,
	RTL8309G_GCTRL1_QUEUE_WEIGHT,
	RTL8309G_GCTRL1_DISABLE_IP_PRIORITY_A,
	RTL8309G_GCTRL1_DISABLE_IP_PRIORITY_B,

	RTL8309G_GCTRL2_ENABLE_DIFF_SCP_A,
	RTL8309G_GCTRL2_DIFF_SCP_A,
	RTL8309G_GCTRL2_ENABLE_DIFF_SCP_B,
	RTL8309G_GCTRL2_DIFF_SCP_B,

	RTL8309G_GCTRL3_ENABLE_DROP_SRAM,
	RTL8309G_GCTRL3_TX_IPG_COMPENSATION,
	RTL8309G_GCTRL3_DISABLE_LOOP_DETECT,
	RTL8309G_GCTRL3_ENABLE_LOOKUP_TBL,

	RTL8309G_IPPRIO_IP_ADDR_A_1,
	RTL8309G_IPPRIO_IP_ADDR_A_2,

	RTL8309G_IPPRIO_IP_ADDR_B_1,
	RTL8309G_IPPRIO_IP_ADDR_B_2,

	RTL8309G_IPPRIO_IP_MASK_A_1,
	RTL8309G_IPPRIO_IP_MASK_A_2,

	RTL8309G_IPPRIO_IP_MASK_B_1,
	RTL8309G_IPPRIO_IP_MASK_B_2,

	RTL8309G_SW_MAC_ADDR_1,
	RTL8309G_SW_MAC_ADDR_2,
	RTL8309G_SW_MAC_ADDR_3,

	RTL8309G_ISP_MAC_ADDR_1,
	RTL8309G_ISP_MAC_ADDR_2,
	RTL8309G_ISP_MAC_ADDR_3,

	RTL8309G_MII_ENABLE_TRANSMISSION,
	RTL8309G_MII_ENABLE_RECEPTION,
	RTL8309G_MII_ENABLE_LEARNING,
	RTL8309G_MII_DISABLE_PRIORITY,
	RTL8309G_MII_DISABLE_DIFF_PRIORITY,
	RTL8309G_MII_DISABLE_PORT_PRIORITY,
	RTL8309G_MII_VLAN_TAG,

	RTL8309G_WAN_PORT,
	RTL8309G_CPU_PORT,

	RTL8309G_IACTRL_COMMAND_EXECUTION,
	RTL8309G_IACTRL_OPERATION_CYCLE,
	RTL8309G_IACTRL_DATA_1,
	RTL8309G_IACTRL_DATA_2,
	RTL8309G_IACTRL_DATA_3,
	RTL8309G_IACTRL_DATA_4
};

static const struct rtl8309g_reg rtl8309g_regs[] = {
#define RTL8309G_PORT_REG(id) \
	[RTL8309G_PHY_##id##_PORT_CTRL_RESET] = { id, 0, 1, 15 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL_LOOPBACK] = { id, 0, 1, 14 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL_SPEED] = { id, 0, 1, 13 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL_ANEG] = { id, 0, 1, 12 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL_POWER_DOWN] = { id, 0, 1, 11 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL_ISOLATE] = { id, 0, 1, 10 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL_ANEG_RESTART] = { id, 0, 1, 9 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL_DUPLEX] = { id, 0, 1, 8 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_100BASE_TX_FD] = { id, 1, 1, 14 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_100BASE_TX_HD] = { id, 1, 1, 13 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_10BASE_T_FD] = { id, 1, 1, 12 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_10BASE_T_HD] = { id, 1, 1, 11 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_MF_PREAM_SUPR] = { id, 1, 1, 6 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_ANEG_COMPLETE] = { id, 1, 1, 5 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_REM_FAULT] = { id, 1, 1, 4 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_ANEG_ABILITY] = { id, 1, 1, 3 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_LINK_STATUS] = { id, 1, 1, 2 }, \
	[RTL8309G_PHY_##id##_PORT_STATUS_JABBER_DETECT] = { id, 1, 1, 1 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_ADV_REM_FAULT] = { id, 4, 1, 13 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_ADV_FLOW_CTRL] = { id, 4, 1, 10 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_ADV_100BASE_TX_FD] = { id, 4, 1, 8 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_ADV_100BASE_TX_HD] = { id, 4, 1, 7 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_ADV_10BASE_T_FD] = { id, 4, 1, 6 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_ADV_10BASE_T_HD] = { id, 4, 1, 5 }

#define RTL8309G_PORT_REG_D(id) \
	[RTL8309G_PHY_##id##_PORT_CTRL0_NULL_VID_REPLACE] = { id, 22, 1, 12 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL0_DISCARD_NON_PVID_PKT] = { id, 22, 1, 11 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL0_DISABLE_PRIORITY] = { id, 22, 1, 10 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL0_DISABLE_DIFF_PRIORITY] = { id, 22, 1, 9 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL0_DISABLE_PORT_BASE_PRIORITY] = { id, 22, 1, 8 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL0_VLAN_TAG] = { id, 22, 2, 0 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL1_ENABLE_TRANSMISSION] = { id, 23, 1, 11 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL1_ENABLE_RECEPTION] = { id, 23, 1, 10 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL1_ENABLE_LEARNING] = { id, 23, 1, 9 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL2_VLAN_INDEX] = { id, 24, 4, 12 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL2_VLAN_MEMBERSHIP] = { id, 24, 9, 0 }, \
	[RTL8309G_PHY_##id##_PORT_VLAN_ID] = { id, 25, 12, 0 }

	RTL8309G_PORT_REG(0),
	RTL8309G_PORT_REG_D(0),
	RTL8309G_PORT_REG(1),
	RTL8309G_PORT_REG_D(1),
	RTL8309G_PORT_REG(2),
	RTL8309G_PORT_REG_D(2),
	RTL8309G_PORT_REG(3),
	RTL8309G_PORT_REG_D(3),
	RTL8309G_PORT_REG(4),
	RTL8309G_PORT_REG_D(4),
	RTL8309G_PORT_REG(5),
	RTL8309G_PORT_REG_D(5),
	RTL8309G_PORT_REG(6),
	RTL8309G_PORT_REG_D(6),
	RTL8309G_PORT_REG(7),
	RTL8309G_PORT_REG_D(7),
	RTL8309G_PORT_REG(8),
	[RTL8309G_PHY_8_PORT_CTRL0_NULL_VID_REPLACE] = { 5, 17, 1, 15 },
	[RTL8309G_PHY_8_PORT_CTRL0_DISCARD_NON_PVID_PKT] = { 5, 17, 1, 14 },
	[RTL8309G_PHY_8_PORT_CTRL0_DISABLE_PRIORITY] = { 5, 16, 1, 11 },
	[RTL8309G_PHY_8_PORT_CTRL0_DISABLE_DIFF_PRIORITY] = { 5, 16, 1, 10 },
	[RTL8309G_PHY_8_PORT_CTRL0_DISABLE_PORT_BASE_PRIORITY] = { 5, 16, 1, 9 },
	[RTL8309G_PHY_8_PORT_CTRL0_VLAN_TAG] = { 5, 16, 2, 0 },
	[RTL8309G_PHY_8_PORT_CTRL1_ENABLE_TRANSMISSION] = { 5, 16, 1, 15 },
	[RTL8309G_PHY_8_PORT_CTRL1_ENABLE_RECEPTION] = { 5, 16, 1, 14 },
	[RTL8309G_PHY_8_PORT_CTRL1_ENABLE_LEARNING] = { 5, 16, 1, 13 },
	[RTL8309G_PHY_8_PORT_CTRL2_VLAN_INDEX] = { 5, 17, 4, 9 },
	[RTL8309G_PHY_8_PORT_CTRL2_VLAN_MEMBERSHIP] = { 5, 17, 9, 0 },
	[RTL8309G_PHY_8_PORT_VLAN_ID] = { 5, 18, 12, 0 },

#define RTL8309G_PORT_REG_C(id) \
	[RTL8309G_PHY_##id##_PORT_ANEG_LINK_REM_FAULT] = { id, 5, 1, 13 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_LINK_FLOW_CTRL] = { id, 5, 1, 10 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_LINK_100BASE_TX_FD] = { id, 5, 1, 8 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_LINK_100BASE_TX_HD] = { id, 5, 1, 7 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_LINK_10BASE_T_FD] = { id, 5, 1, 6 }, \
	[RTL8309G_PHY_##id##_PORT_ANEG_LINK_10BASE_T_HD] = { id, 5, 1, 5 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL0_LOOPBACK] = { id, 22, 1, 13 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL1_LOOP_STATUS] = { id, 23, 1, 8 }, \
	[RTL8309G_PHY_##id##_PORT_CTRL1_LINK_QUALITY] = { id, 23, 4, 4 }
	RTL8309G_PORT_REG_C(0),
	RTL8309G_PORT_REG_C(1),
	RTL8309G_PORT_REG_C(2),
	RTL8309G_PORT_REG_C(3),
	RTL8309G_PORT_REG_C(4),
	RTL8309G_PORT_REG_C(5),
	RTL8309G_PORT_REG_C(6),
	RTL8309G_PORT_REG_C(7),

	[RTL8309G_GCTRL0_LED_MODE] = { 0, 16, 3, 13 },
	[RTL8309G_GCTRL0_RESET] = { 0, 16, 1, 12 },
	[RTL8309G_GCTRL0_DISABLE_VLAN] = { 0, 16, 1, 11 },
	[RTL8309G_GCTRL0_DISABLE_VLAN_TAG_AWARE] = { 0, 16, 1, 10 },
	[RTL8309G_GCTRL0_DISABLE_VLAN_INGRESS_FILTER] = { 0, 16, 1, 9 },
	[RTL8309G_GCTRL0_DISABLE_VLAN_TAG_CONTROL] = { 0, 16, 1, 8 },
	[RTL8309G_GCTRL0_EEPROM_EXIST] = { 0, 16, 1, 7 },
	[RTL8309G_GCTRL0_DISABLE_ERROR_FILTER] = { 0, 16, 1, 6 },
	[RTL8309G_GCTRL0_ENABLE_FLOW_CTRL_TRANSMIT] = { 0, 16, 1, 5 },
	[RTL8309G_GCTRL0_ENABLE_FLOW_CTRL_RECEIVE] = { 0, 16, 1, 4 },
	[RTL8309G_GCTRL0_ENABLE_BROADCAST] = { 0, 16, 1, 3 },
	[RTL8309G_GCTRL0_ENABLE_AGING] = { 0, 16, 1, 2 },
	[RTL8309G_GCTRL0_ENABLE_FAST_AGING] = { 0, 16, 1, 1 },
	[RTL8309G_GCTRL0_ENABLE_ISP_MAC_TRANSLATION] = { 0, 16, 1, 0 },

	[RTL8309G_GCTRL1_PRIORITY] = { 0, 17, 3, 13 },
	[RTL8309G_GCTRL1_TRUNKING_PORT] = { 0, 17, 1, 12 },
	[RTL8309G_GCTRL1_QUEUE_WEIGHT] = { 0, 17, 2, 10 },
	[RTL8309G_GCTRL1_DISABLE_IP_PRIORITY_A] = { 0, 17, 1, 9 },
	[RTL8309G_GCTRL1_DISABLE_IP_PRIORITY_B] = { 0, 17, 1, 8 },

	[RTL8309G_GCTRL2_ENABLE_DIFF_SCP_A] = { 0, 18, 1, 15 },
	[RTL8309G_GCTRL2_DIFF_SCP_A] = { 0, 18, 6, 8 },
	[RTL8309G_GCTRL2_ENABLE_DIFF_SCP_B] = { 0, 18, 1, 7 },
	[RTL8309G_GCTRL2_DIFF_SCP_B] = { 0, 18, 6, 0 },

	[RTL8309G_GCTRL3_ENABLE_DROP_SRAM] = { 0, 19, 1, 15 },
	[RTL8309G_GCTRL3_TX_IPG_COMPENSATION] = { 0, 19, 1, 13 },
	[RTL8309G_GCTRL3_DISABLE_LOOP_DETECT] = { 0, 19, 1, 12 },
	[RTL8309G_GCTRL3_ENABLE_LOOKUP_TBL] = { 0, 19, 1, 11 },

	[RTL8309G_IPPRIO_IP_ADDR_A_1] = { 1, 16, 16, 0 },
	[RTL8309G_IPPRIO_IP_ADDR_A_2] = { 1, 17, 16, 0 },

	[RTL8309G_IPPRIO_IP_ADDR_B_1] = { 1, 18, 16, 0 },
	[RTL8309G_IPPRIO_IP_ADDR_B_2] = { 1, 19, 16, 0 },

	[RTL8309G_IPPRIO_IP_MASK_A_1] = { 2, 16, 16, 0 },
	[RTL8309G_IPPRIO_IP_MASK_A_2] = { 2, 17, 16, 0 },

	[RTL8309G_IPPRIO_IP_MASK_B_1] = { 2, 18, 16, 0 },
	[RTL8309G_IPPRIO_IP_MASK_B_2] = { 2, 19, 16, 0 },

	[RTL8309G_SW_MAC_ADDR_1] = { 3, 16, 16, 0 },
	[RTL8309G_SW_MAC_ADDR_2] = { 3, 17, 16, 0 },
	[RTL8309G_SW_MAC_ADDR_3] = { 3, 18, 16, 0 },

	[RTL8309G_ISP_MAC_ADDR_1] = { 4, 16, 16, 0 },
	[RTL8309G_ISP_MAC_ADDR_2] = { 4, 17, 16, 0 },
	[RTL8309G_ISP_MAC_ADDR_3] = { 4, 18, 16, 0 },

	[RTL8309G_MII_ENABLE_TRANSMISSION] = { 5, 16, 1, 15 },
	[RTL8309G_MII_ENABLE_RECEPTION] = { 5, 16, 1, 14 },
	[RTL8309G_MII_ENABLE_LEARNING] = { 5, 16, 1, 13 },
	[RTL8309G_MII_DISABLE_PRIORITY] = { 5, 16, 1, 11 },
	[RTL8309G_MII_DISABLE_DIFF_PRIORITY] = { 5, 16, 1, 10 },
	[RTL8309G_MII_DISABLE_PORT_PRIORITY] = { 5, 16, 1, 9 },
	[RTL8309G_MII_VLAN_TAG] = { 5, 16, 2, 0 },

	[RTL8309G_WAN_PORT] = { 5, 19, 4, 4 },
	[RTL8309G_CPU_PORT] = { 5, 19, 4, 0 },

	[RTL8309G_IACTRL_COMMAND_EXECUTION] = { 7, 16, 1, 1 },
	[RTL8309G_IACTRL_OPERATION_CYCLE] = { 7, 16, 1, 0 },
	[RTL8309G_IACTRL_DATA_1] = { 7, 17, 16, 0 },
	[RTL8309G_IACTRL_DATA_2] = { 7, 18, 16, 0 },
	[RTL8309G_IACTRL_DATA_3] = { 7, 19, 16, 0 },
	[RTL8309G_IACTRL_DATA_4] = { 7, 20, 16, 0 }
};

#define to_rtl(_dev) container_of(_dev, struct rtl_priv, dev)

struct rtl_priv {
	struct list_head list;
	struct switch_dev dev;
	int type;
	struct mii_bus *bus;
	char hwname[16];
	bool fixup;
};

static LIST_HEAD(phydevs);

static uint16_t
poslen_mask(uint8_t pos, uint8_t len)
{
	int i;
	uint16_t mask = 0;

	for (i = 0; i < len; i++)
		mask+= 1 << (pos + i);

	return mask;
}

static uint16_t
rtl_read(struct mii_bus *bus, uint8_t phy, uint8_t reg)
{
	return (bus->read(bus, phy, reg));
}

static void
rtl_write(struct mii_bus *bus, uint8_t phy, uint8_t reg, uint16_t val)
{
	bus->write(bus, phy, reg, val);
}

static uint16_t
rtl_read_reg(struct mii_bus *bus, enum rtl8309g_regidx s)
{
	const struct rtl8309g_reg *r;

	BUG_ON(s >= ARRAY_SIZE(rtl8309g_regs));

	r = &rtl8309g_regs[s];

	return ((rtl_read(bus, r->phy, r->reg) & poslen_mask(r->shift, r->bits)) >> r->shift);
}

static void
rtl_write_reg(struct mii_bus *bus, enum rtl8309g_regidx s, uint16_t val)
{
	const struct rtl8309g_reg *r;
	uint16_t reg;
	uint16_t mask;

	BUG_ON(s >= ARRAY_SIZE(rtl8309g_regs));

	r = &rtl8309g_regs[s];
	mask = poslen_mask(r->shift, r->bits);

	reg = rtl_read(bus, r->phy, r->reg);
	rtl_write(bus, r->phy, r->reg, (reg & ~(mask)) | ((val << r->shift) & mask));
}

static int
rtl8309g_probe(struct phy_device *pdev)
{
	struct rtl_priv *priv;

	list_for_each_entry(priv, &phydevs, list) {
		/*
		 * share one rtl_priv instance between virtual phy
		 * devices on the same bus
		 */
		if (priv->bus == pdev->mdio.bus)
			goto found;
	}

	priv = kzalloc(sizeof(struct rtl_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->bus = pdev->mdio.bus;

found:
	pdev->priv = priv;

	return 0;
}

static int
rtl_get_vlan(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	val->value.i = rtl_read_reg(bus, RTL8309G_GCTRL0_DISABLE_VLAN) == 0 ? 1 : 0;

	return 0;
}

static int
rtl_set_vlan(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;
	int en = val->value.i ? 0 : 1;

	rtl_write_reg(bus, RTL8309G_GCTRL0_DISABLE_VLAN_INGRESS_FILTER, en);
	rtl_write_reg(bus, RTL8309G_GCTRL0_DISABLE_VLAN_TAG_AWARE, en);
	rtl_write_reg(bus, RTL8309G_GCTRL0_DISABLE_VLAN, en);

	return 0;
}

static int
rtl_get_led_mode(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	val->value.i = rtl_read_reg(bus, RTL8309G_GCTRL0_LED_MODE);

	return 0;
}

static int
rtl_set_led_mode(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;
	int en = val->value.i;

	rtl_write_reg(bus, RTL8309G_GCTRL0_LED_MODE, en);

	return 0;
}

static int
rtl_get_aging(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;
	int en = 0;

	if (rtl_read_reg(bus, RTL8309G_GCTRL0_ENABLE_AGING)) {
		en++;
		if (rtl_read_reg(bus, RTL8309G_GCTRL0_ENABLE_FAST_AGING))
			en++;
	}

	val->value.i = en;

	return 0;
}

static int
rtl_set_aging(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	rtl_write_reg(bus, RTL8309G_GCTRL0_ENABLE_AGING, val->value.i > 0 ? 1 : 0);
	rtl_write_reg(bus, RTL8309G_GCTRL0_ENABLE_FAST_AGING, val->value.i == 2 ? 1 : 0);

	return 0;
}

static int
rtl_get_vlan_vid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	val->value.i = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, VLAN_ID));

	return 0;
}

static int
rtl_set_vlan_vid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;
	int en = val->value.i;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	rtl_write_reg(bus, RTL8309G_PHY_REG(val->port_vlan, VLAN_ID), en);

	return 0;
}

static int
rtl_get_traffic(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	val->value.i = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_ENABLE_RECEPTION)) &
			rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_ENABLE_TRANSMISSION))
			? 1 : 0;

	return 0;
}

static int
rtl_set_traffic(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	rtl_write_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_ENABLE_RECEPTION), val->value.i);
	rtl_write_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_ENABLE_TRANSMISSION), val->value.i);

	return 0;
}

static int
rtl_get_learning(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	val->value.i = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_ENABLE_LEARNING));

	return 0;
}

static int
rtl_set_learning(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	rtl_write_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_ENABLE_LEARNING), val->value.i);

	return 0;
}

static int
rtl_get_loop_status(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS - 1)
		return -EINVAL;

	val->value.i = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_LOOP_STATUS));

	return 0;
}

static int
rtl_get_link_quality(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS - 1)
		return -EINVAL;

	val->value.i = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL1_LINK_QUALITY));

	return 0;
}

static int
rtl_get_port_pvid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	val->value.i = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL2_VLAN_INDEX));

	return 0;
}

static int
rtl_set_port_pvid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	rtl_write_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL2_VLAN_INDEX), val->value.i);

	return 0;
}

static int
rtl_get_vlan_ports(struct switch_dev *dev, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;
	uint16_t pmask;
	int i;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	pmask = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL2_VLAN_MEMBERSHIP));
	for (i = 0; i < RTL8309G_NUM_PORTS; i++) {
		struct switch_port *port;

		if (!(pmask & (1 << i)))
			continue;

		port = &val->value.ports[val->len];
		port->id = i;

		if (rtl_read_reg(bus, RTL8309G_PHY_REG(i, CTRL0_VLAN_TAG)) == 2)
			port->flags = (1 << SWITCH_PORT_FLAG_TAGGED);

		val->len++;
	}

	return 0;
}

static int
rtl_set_vlan_ports(struct switch_dev *dev, struct switch_val *val)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;
	uint16_t pmask = 0, omask, dmask;
	int i;

	if (val->port_vlan >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	for (i = 0; i < val->len; i++) {
		struct switch_port *port = &val->value.ports[i];
		bool tagged = false;

		pmask|= 1 << port->id;

		if (port->flags & (1 << SWITCH_PORT_FLAG_TAGGED))
			tagged = true;

		if (!tagged)
			rtl_write_reg(bus, RTL8309G_PHY_REG(port->id, CTRL2_VLAN_INDEX), val->port_vlan);

		rtl_write_reg(bus, RTL8309G_PHY_REG(port->id, CTRL0_DISCARD_NON_PVID_PKT), (tagged ? 0 : 1));
		rtl_write_reg(bus, RTL8309G_PHY_REG(port->id, CTRL0_NULL_VID_REPLACE), (tagged ? 1 : 0));
		rtl_write_reg(bus, RTL8309G_PHY_REG(port->id, CTRL0_VLAN_TAG), (tagged ? 2 : 1));
	}

	omask = rtl_read_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL2_VLAN_MEMBERSHIP));
	rtl_write_reg(bus, RTL8309G_PHY_REG(val->port_vlan, CTRL2_VLAN_MEMBERSHIP), pmask);

	if (val->port_vlan == 0)
		return 0;

	dmask = rtl_read_reg(bus, RTL8309G_PHY_REG(0, CTRL2_VLAN_MEMBERSHIP));
	dmask&= ~(pmask & ~(1 << dev->cpu_port));
	rtl_write_reg(bus, RTL8309G_PHY_REG(0, CTRL2_VLAN_MEMBERSHIP), dmask);

	omask &= ~pmask;
	for (i = 0; i < RTL8309G_NUM_PORTS; i++) {
		if (!(omask & (1 << i)))
			continue;

		if (i == dev->cpu_port)
			continue;

		if (rtl_read_reg(bus, RTL8309G_PHY_REG(i, CTRL2_VLAN_INDEX)) == val->port_vlan)
			rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL2_VLAN_INDEX), 0);
	}

	return 0;
}

static int
rtl_get_port_link(struct switch_dev *dev, int port, struct switch_port_link *link)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;

	if (port >= RTL8309G_NUM_PORTS)
		return -EINVAL;

	// TODO: optimize reg reads
	link->aneg = rtl_read_reg(bus, RTL8309G_PHY_REG(port, STATUS_ANEG_COMPLETE));
	link->link = rtl_read_reg(bus, RTL8309G_PHY_REG(port, STATUS_LINK_STATUS));
	link->speed = rtl_read_reg(bus, RTL8309G_PHY_REG(port, CTRL_SPEED)) ? SWITCH_PORT_SPEED_100 : SWITCH_PORT_SPEED_10;
	link->duplex = rtl_read_reg(bus, RTL8309G_PHY_REG(port, CTRL_DUPLEX)) ? DUPLEX_FULL : DUPLEX_HALF;
	if (port != dev->cpu_port && rtl_read_reg(bus, RTL8309G_GCTRL0_ENABLE_FLOW_CTRL_TRANSMIT))
		link->tx_flow = rtl_read_reg(bus, RTL8309G_PHY_REG(port, ANEG_ADV_FLOW_CTRL));
	else
		link->tx_flow = 0;
	if (port != dev->cpu_port && rtl_read_reg(bus, RTL8309G_GCTRL0_ENABLE_FLOW_CTRL_RECEIVE))
		link->rx_flow = rtl_read_reg(bus, RTL8309G_PHY_REG(port, ANEG_ADV_FLOW_CTRL));
	else
		link->rx_flow = 0;

	return 0;
}

static int
rtl_hw_apply(struct switch_dev *dev)
{
	return 0;
}

static int
rtl_reset(struct switch_dev *dev)
{
	struct rtl_priv *priv = to_rtl(dev);
	struct mii_bus *bus = priv->bus;
	int i;

	/* Disable rx/tx on PHYs */
	for (i = 0; i < RTL8309G_NUM_PORTS - 1; i++) {
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL1_ENABLE_TRANSMISSION), 0);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL1_ENABLE_RECEPTION), 0);
	}

	rtl_write_reg(bus, RTL8309G_GCTRL0_DISABLE_VLAN, 1);
	rtl_write_reg(bus, RTL8309G_GCTRL0_DISABLE_VLAN_INGRESS_FILTER, 1);
	rtl_write_reg(bus, RTL8309G_CPU_PORT, dev->cpu_port);

	/* Reset switch */
	rtl_write_reg(bus, RTL8309G_GCTRL0_RESET, 1);
	for (i = 0; i < 10; i++) {
		if (rtl_read_reg(bus, RTL8309G_GCTRL0_RESET) == 0)
			break;

		msleep(1);
	}

	/* Reset PHYs */
	for (i = 1; i < 9; i++)
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL_RESET), 1);

	/* All ports to VLAN 0 with PVID 0 */
	for (i = 0; i < RTL8309G_NUM_PORTS; i++) {
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL2_VLAN_INDEX), 0);
		if (i == 0)
			rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL2_VLAN_MEMBERSHIP), 0x1FF);
		else
			rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL2_VLAN_MEMBERSHIP), 0);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL0_NULL_VID_REPLACE), 0);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL0_DISCARD_NON_PVID_PKT), 0);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL0_VLAN_TAG), 3);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL_ANEG), 1);
	}

	/* Enable rx/tx on PHYs */
	for (i = 0; i < RTL8309G_NUM_PORTS - 1; i++) {
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL1_ENABLE_TRANSMISSION), 1);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL1_ENABLE_RECEPTION), 1);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL_ANEG_RESTART), 1);
	}

	return 0;
}

static struct switch_attr rtl_globals[] = {
	{
		.type = SWITCH_TYPE_INT,
		.name = "enable_vlan",
		.description = "Enable VLAN mode",
		.set = rtl_set_vlan,
		.get = rtl_get_vlan,
		.max = 1,
	}, {
		.type = SWITCH_TYPE_INT,
		.name = "enable_aging",
		.description = "Enable aging and fast aging",
		.set = rtl_set_aging,
		.get = rtl_get_aging,
		.max = 2,
	}, {
		.type = SWITCH_TYPE_INT,
		.name = "led_mode",
		.description = "Get/Set led mode (0 - 7)",
		.set = rtl_set_led_mode,
		.get = rtl_get_led_mode,
		.max = 7,
	}
};

static struct switch_attr rtl_port[] = {
	{
		.type = SWITCH_TYPE_INT,
		.name = "pvid",
		.description = "Port PVID",
		.set = rtl_set_port_pvid,
		.get = rtl_get_port_pvid,
		.max = RTL8309G_NUM_VLANS - 1,
	}, {
		.type = SWITCH_TYPE_INT,
		.name = "enable_traffic",
		.description = "Enable packet transmission",
		.set = rtl_set_traffic,
		.get = rtl_get_traffic,
		.max = 1,
	}, {
		.type = SWITCH_TYPE_INT,
		.name = "enable_learning",
		.description = "Enable learning",
		.set = rtl_set_learning,
		.get = rtl_get_learning,
		.max = 1,
	}, {
		.type = SWITCH_TYPE_INT,
		.name = "loop_status",
		.description = "Loop status",
		.get = rtl_get_loop_status,
		.max = 1,
	}, {
		.type = SWITCH_TYPE_INT,
		.name = "link_quality",
		.description = "Link quality (0 - highest, 15 - lowest)",
		.get = rtl_get_link_quality,
		.max = 15,
	}
};

static struct switch_attr rtl_vlan[] = {
	{
		.type = SWITCH_TYPE_INT,
		.name = "vid",
		.description = "VLAN ID (1-4095)",
		.max = 4095,
		.set = rtl_set_vlan_vid,
		.get = rtl_get_vlan_vid,
	}
};

static const struct switch_dev_ops rtl8309g_ops = {
	.attr_global = {
		.attr = rtl_globals,
		.n_attr = ARRAY_SIZE(rtl_globals),
	},
	.attr_port = {
		.attr = rtl_port,
		.n_attr = ARRAY_SIZE(rtl_port),
	},
	.attr_vlan = {
		.attr = rtl_vlan,
		.n_attr = ARRAY_SIZE(rtl_vlan),
	},
	.get_vlan_ports = rtl_get_vlan_ports,
	.set_vlan_ports = rtl_set_vlan_ports,
	.apply_config = rtl_hw_apply,
	.reset_switch = rtl_reset,
	.get_port_link = rtl_get_port_link,
};

static int
rtl8309g_config_init(struct phy_device *pdev)
{
	struct net_device *netdev = pdev->attached_dev;
	struct rtl_priv *priv = pdev->priv;
	struct switch_dev *dev = &priv->dev;
	struct switch_val val;
	int err;

	val.value.i = 1;
	priv->dev.cpu_port = RTL8309G_PORT_CPU;
	priv->dev.ports = RTL8309G_NUM_PORTS;
	priv->dev.vlans = RTL8309G_NUM_VLANS;
	priv->dev.ops = &rtl8309g_ops;
	priv->bus = pdev->mdio.bus;

	strncpy(priv->hwname, RTL8309G_NAME, sizeof(priv->hwname));

	dev->name = priv->hwname;

	err = register_switch(dev, netdev);
	if (err < 0) {
		kfree(priv);
		return err;
	}

	printk(KERN_INFO "Registered Realtek %s switch\n", priv->hwname);

	return 0;
}

static int
rtl8309g_fixup(struct phy_device *pdev)
{
	struct rtl_priv priv;
	struct mii_bus *bus = pdev->mdio.bus;
	u16 mac1, mac2, mac3;

	memset(&priv, 0, sizeof(priv));
	priv.fixup = true;
	priv.bus = bus;

	mac1 = rtl_read_reg(bus, RTL8309G_SW_MAC_ADDR_1);
	mac2 = rtl_read_reg(bus, RTL8309G_SW_MAC_ADDR_2);
	mac3 = rtl_read_reg(bus, RTL8309G_SW_MAC_ADDR_3);

	if (mac1 == 0x5452 && mac2 == 0x834C && mac3 == 0xB009)
		pdev->phy_id = RTL8309G_MAGIC;

	return 0;
}

static void
rtl8309g_remove(struct phy_device *pdev)
{
	struct rtl_priv *priv = pdev->priv;

	unregister_switch(&priv->dev);
	printk(KERN_INFO "Unregistered Realtek %s switch\n", priv->hwname);

	kfree(priv);
}

static int
rtl8309g_config_aneg(struct phy_device *pdev)
{
	struct rtl_priv *priv = pdev->priv;
	struct mii_bus *bus = priv->bus;
	int i;

	/* Restart autonegotiation */
	for (i = 0; i < 9; i++) {
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL_ANEG), 1);
		rtl_write_reg(bus, RTL8309G_PHY_REG(i, CTRL_ANEG_RESTART), 1);
	}

	return 0;
}

static int
rtl8309g_read_status(struct phy_device *pdev)
{
	pdev->speed = SPEED_100;
	pdev->duplex = DUPLEX_FULL;
	pdev->link = 1;

	if (pdev->link) {
		pdev->state = PHY_RUNNING;
		netif_carrier_on(pdev->attached_dev);
		pdev->adjust_link(pdev->attached_dev);
	} else {
		pdev->state = PHY_NOLINK;
		netif_carrier_off(pdev->attached_dev);
		pdev->adjust_link(pdev->attached_dev);
	}

	return 0;
}

static struct phy_driver rtl8309g_driver = {
	.name		= "Realtek RTL8309G",
	.flags		= PHY_HAS_MAGICANEG,
	.phy_id		= RTL8309G_MAGIC,
	.phy_id_mask	= 0xffffffff,
	.features	= PHY_BASIC_FEATURES,
	.probe		= &rtl8309g_probe,
	.remove		= &rtl8309g_remove,
	.config_init	= &rtl8309g_config_init,
	.config_aneg	= &rtl8309g_config_aneg,
	.read_status	= &rtl8309g_read_status,
};

static int __init
rtl_init(void)
{
	phy_register_fixup_for_id(PHY_ANY_ID, rtl8309g_fixup);
	return phy_driver_register(&rtl8309g_driver, THIS_MODULE);
}

static void __exit
rtl_exit(void)
{
	phy_driver_unregister(&rtl8309g_driver);
}

module_init(rtl_init);
module_exit(rtl_exit);
MODULE_LICENSE("GPL");

