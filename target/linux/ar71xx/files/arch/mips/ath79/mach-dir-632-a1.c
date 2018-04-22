/*
 *  D-Link DIR-632 rev. A1 board support
 *
 *  Copyright (C) 2017 Konstantin Kuzov <master.nosferatu@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <linux/ar8216_platform.h>

#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/irq.h>
#include <asm/mach-ath79/ar71xx_regs.h>

#include <linux/platform_data/phy-at803x.h>

#include "common.h"
#include "dev-ap9x-pci.h"
#include "dev-eth.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "dev-wmac.h"
#include "dev-usb.h"
#include "machtypes.h"
#include "nvram.h"

#define DIR_632_A1_GPIO_LED_WPS			0
#define DIR_632_A1_GPIO_LED_POWER_AMBER		1
#define DIR_632_A1_GPIO_LED_POWER_GREEN		6
#define DIR_632_A1_GPIO_LED_WAN_AMBER		7
#define DIR_632_A1_GPIO_LED_WAN_GREEN		17

#define DIR_632_A1_GPIO_LED_USB			11
#define DIR_632_A1_GPIO_LED_WLAN		0

#define DIR_632_A1_GPIO_BTN_RESET		8
#define DIR_632_A1_GPIO_BTN_WPS			12

#define DIR_632_A1_KEYS_POLL_INTERVAL		20	/* msecs */
#define DIR_632_A1_KEYS_DEBOUNCE_INTERVAL (3 * DIR_632_A1_KEYS_POLL_INTERVAL)

#define DIR_632_A1_NVRAM_ADDR			0x1ffe0000
#define DIR_632_A1_NVRAM_SIZE			0x10000

#define DIR_632_A1_MAC_ADDR			0x1ffb0000
#define DIR_632_A1_ART_ADDR			0x1fff0000
#define DIR_632_A1_EEPROM_ADDR			0x1fff1000

#define DIR_632_A1_MAC0_OFFSET			0
#define DIR_632_A1_MAC1_OFFSET			6

#define DIR_632A1_LAN_PHYMASK			BIT(8)
#define DIR_632A1_WAN_PHYMASK			BIT(0)

#define AR724X_GPIO_FUNCTION_PCIEPHY_TST_EN	BIT(16)
#define AR724X_GPIO_FUNCTION_CLK_OBS6_ENABLE	BIT(20)

static struct gpio_led dir_632_a1_leds_gpio[] __initdata = {
	{
		.name		= "d-link:green:power",
		.gpio		= DIR_632_A1_GPIO_LED_POWER_GREEN,
	}, {
		.name		= "d-link:amber:power",
		.gpio		= DIR_632_A1_GPIO_LED_POWER_AMBER,
	}, {
		.name		= "d-link:amber:wan",
		.gpio		= DIR_632_A1_GPIO_LED_WAN_AMBER,
	}, {
		.name		= "d-link:green:wan",
		.gpio		= DIR_632_A1_GPIO_LED_WAN_GREEN,
		.active_low	= 1,
	}, {
		.name		= "d-link:blue:wps",
		.gpio		= DIR_632_A1_GPIO_LED_WPS,
		.active_low	= 1,
	}, {
		.name           = "d-link:green:usb",
		.gpio           = DIR_632_A1_GPIO_LED_USB,
		.active_low     = 1,
	}
};

static struct gpio_keys_button dir_632_a1_gpio_keys[] __initdata = {
	{
		.desc		= "reset",
		.type		= EV_KEY,
		.code		= KEY_RESTART,
		.debounce_interval = DIR_632_A1_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= DIR_632_A1_GPIO_BTN_RESET,
		.active_low	= 1,
	}, {
		.desc		= "wps",
		.type		= EV_KEY,
		.code		= KEY_WPS_BUTTON,
		.debounce_interval = DIR_632_A1_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= DIR_632_A1_GPIO_BTN_WPS,
		.active_low	= 1,
	}
};

static struct flash_platform_data dir_632_a1_flash_data = {
	.type = "mx25l6405d",
};

static void __init dir_632_a1_setup(void)
{
	const char *nvram = (char *) KSEG1ADDR(DIR_632_A1_NVRAM_ADDR);
	u8 *ee = (u8*)KSEG1ADDR(DIR_632_A1_EEPROM_ADDR);
	u8 *art = (u8*)KSEG1ADDR(DIR_632_A1_ART_ADDR);
	u8 *mac = (u8*)KSEG1ADDR(DIR_632_A1_MAC_ADDR);
	u8 mac_buff[6];

	ath79_register_m25p80(&dir_632_a1_flash_data);

	/* Make LEDs on GPIO6/7 work */
	ath79_gpio_function_enable(AR724X_GPIO_FUNC_JTAG_DISABLE);

	ath79_gpio_function_disable(
				    AR71XX_GPIO_FUNC_STEREO_EN |
				    AR724X_GPIO_FUNCTION_CLK_OBS6_ENABLE |
				    AR724X_GPIO_FUNC_UART_RTS_CTS_EN |
				    AR724X_GPIO_FUNCTION_PCIEPHY_TST_EN |
				    AR724X_GPIO_FUNC_ETH_SWITCH_LED0_EN |
				    AR724X_GPIO_FUNC_ETH_SWITCH_LED1_EN |
				    AR724X_GPIO_FUNC_ETH_SWITCH_LED2_EN |
				    AR724X_GPIO_FUNC_ETH_SWITCH_LED3_EN |
				    AR724X_GPIO_FUNC_ETH_SWITCH_LED4_EN);

	ath79_register_leds_gpio(-1, ARRAY_SIZE(dir_632_a1_leds_gpio),
				 dir_632_a1_leds_gpio);

	ath79_register_gpio_keys_polled(-1, DIR_632_A1_KEYS_POLL_INTERVAL,
					ARRAY_SIZE(dir_632_a1_gpio_keys),
					dir_632_a1_gpio_keys);

	if (ath79_nvram_parse_mac_addr(nvram, DIR_632_A1_NVRAM_SIZE,
				       "ath0_hwaddr=", mac_buff) == 0) {
		mac = mac_buff;
		pr_info("dir_632_a1_setup(): NVRAM ath0_hwaddr=%pM", mac);
	}

	ath79_register_mdio(0, ~(DIR_632A1_LAN_PHYMASK));
	ath79_register_mdio(1, ~(DIR_632A1_WAN_PHYMASK));

	ath79_switch_data.phy4_mii_en = 1;
	ath79_switch_data.phy_poll_mask |= DIR_632A1_LAN_PHYMASK;

	ath79_init_mac(ath79_eth0_data.mac_addr, mac, 0);
	ath79_eth0_data.mii_bus_dev = &ath79_mdio0_device.dev;
	ath79_eth0_data.phy_if_mode = PHY_INTERFACE_MODE_MII;
	ath79_eth0_data.phy_mask = DIR_632A1_LAN_PHYMASK;
	ath79_eth0_data.speed = SPEED_100;
	ath79_eth0_data.duplex = DUPLEX_FULL;
	ath79_register_eth(0);

	ath79_init_mac(ath79_eth1_data.mac_addr, mac, 1);
	ath79_eth1_data.mii_bus_dev = &ath79_mdio1_device.dev;
	ath79_eth1_data.phy_if_mode = PHY_INTERFACE_MODE_GMII;
	ath79_eth1_data.phy_mask = DIR_632A1_WAN_PHYMASK;
	ath79_eth1_data.switch_data = &ath79_switch_data;
	ath79_register_eth(1);

	ath79_register_usb();

	ap9x_pci_setup_wmac_led_pin(0, DIR_632_A1_GPIO_LED_WLAN);
	ap91_pci_init(ee, art);
}

MIPS_MACHINE(ATH79_MACH_DIR_632_A1, "DIR-632-A1", "D-Link DIR-632 rev. A1",
	     dir_632_a1_setup);

