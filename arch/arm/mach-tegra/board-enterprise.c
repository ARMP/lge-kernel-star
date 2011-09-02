/*
 * arch/arm/mach-tegra/board-enterprise.c
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/spi/spi.h>
#include <linux/tegra_uart.h>
#include <linux/fsl_devices.h>
#include <linux/i2c/atmel_mxt_ts.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>

#include "board.h"
#include "clock.h"
#include "board-enterprise.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"

/* !!!TODO: Change for enterprise (Taken from Cardhu) */
static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[1] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[2] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 8,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
};

#ifdef CONFIG_BCM4329_RFKILL
static struct resource enterprise_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = TEGRA_GPIO_PE6,
		.end    = TEGRA_GPIO_PE6,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device enterprise_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id		= -1,
	.num_resources  = ARRAY_SIZE(enterprise_bcm4329_rfkill_resources),
	.resource       = enterprise_bcm4329_rfkill_resources,
};

static noinline void __init enterprise_bt_rfkill(void)
{
	platform_device_register(&enterprise_bcm4329_rfkill_device);

	return;
}
#else
static inline void enterprise_bt_rfkill(void) { }
#endif

static void __init enterprise_setup_bluesleep(void)
{
	struct platform_device *pdev = NULL;
	struct resource *res;

	pdev = platform_device_alloc("bluesleep", 0);
	if (!pdev) {
		pr_err("unable to allocate platform device for bluesleep");
		return;
	}

	res = kzalloc(sizeof(struct resource) * 3, GFP_KERNEL);
	if (!res) {
		pr_err("unable to allocate resource for bluesleep\n");
		goto err_free_dev;
	}

	res[0].name   = "gpio_host_wake";
	res[0].start  = TEGRA_GPIO_PS2;
	res[0].end    = TEGRA_GPIO_PS2;
	res[0].flags  = IORESOURCE_IO;

	res[1].name   = "gpio_ext_wake";
	res[1].start  = TEGRA_GPIO_PE7;
	res[1].end    = TEGRA_GPIO_PE7;
	res[1].flags  = IORESOURCE_IO;

	res[2].name   = "host_wake";
	res[2].start  = gpio_to_irq(TEGRA_GPIO_PS2);
	res[2].end    = gpio_to_irq(TEGRA_GPIO_PS2);
	res[2].flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE ;

	if (platform_device_add_resources(pdev, res, 3)) {
		pr_err("unable to add resources to bluesleep device\n");
		goto err_free_res;
	}

	if (platform_device_add(pdev)) {
		pr_err("unable to add bluesleep device\n");
		goto err_free_res;
	}
	tegra_gpio_enable(TEGRA_GPIO_PS2);
	tegra_gpio_enable(TEGRA_GPIO_PE7);

	return;

err_free_res:
	kfree(res);
err_free_dev:
	platform_device_put(pdev);
	return;
}

static __initdata struct tegra_clk_init_table enterprise_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		true},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x","pll_p",	48000000,	false},
	{ "pwm",	"clk_32k",	32768,		false},
	{ "blink",	"clk_32k",	32768,		true},
	{ "pll_a",	NULL,		564480000,	false},
	{ "pll_a_out0",	NULL,		11289600,	false},
	{ NULL,		NULL,		0,		0},
};

static struct tegra_i2c_platform_data enterprise_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PC4, 0},
	.sda_gpio		= {TEGRA_GPIO_PC5, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_clkon_always = true,
	.scl_gpio		= {TEGRA_GPIO_PT5, 0},
	.sda_gpio		= {TEGRA_GPIO_PT6, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PBB1, 0},
	.sda_gpio		= {TEGRA_GPIO_PBB2, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PV4, 0},
	.sda_gpio		= {TEGRA_GPIO_PV5, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio		= {TEGRA_GPIO_PZ7, 0},
	.arb_recovery = arb_lost_recovery,
};

static void enterprise_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &enterprise_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &enterprise_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &enterprise_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &enterprise_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &enterprise_i2c5_platform_data;

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);
}

static struct platform_device *enterprise_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
};
static struct tegra_uart_platform_data enterprise_uart_pdata;

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *c;

	/* UARTD is the debug port. */
	pr_info("Selecting UARTD as the debug console\n");
	enterprise_uart_devices[3] = &debug_uartd_device;
	debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
	debug_uart_clk = clk_get_sys("serial8250.0", "uartd");

	/* Clock enable for the debug channel */
	if (!IS_ERR_OR_NULL(debug_uart_clk)) {
		rate = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->uartclk;
		pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
		c = tegra_get_clock_by_name("pll_p");
		if (IS_ERR_OR_NULL(c))
			pr_err("Not getting the parent clock pll_p\n");
		else
			clk_set_parent(debug_uart_clk, c);

		clk_enable(debug_uart_clk);
		clk_set_rate(debug_uart_clk, rate);
	} else {
		pr_err("Not getting the clock %s for debug console\n",
				debug_uart_clk->name);
	}
}

static void __init enterprise_uart_init(void)
{
	int i;
	struct clk *c;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	enterprise_uart_pdata.parent_clk_list = uart_parent_clk;
	enterprise_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uarta_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartb_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartc_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartd_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uarte_device.dev.platform_data = &enterprise_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(enterprise_uart_devices,
				ARRAY_SIZE(enterprise_uart_devices));
}



static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct platform_device *enterprise_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_SND_HDA_TEGRA)
	&tegra_hda_device,
#endif
#if defined(CONFIG_TEGRA_IOVMM_SMMU)
	&tegra_smmu_device,
#endif
	&tegra_wdt_device,
	&tegra_avp_device,
	&tegra_camera,
	&tegra_spi_device4,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra_se_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
};

#define MXT_CONFIG_CRC 0x62F903
/*
 * Config converted from memory-mapped cfg-file with
 * following version information:
 *
 *
 *
 *      FAMILY_ID=128
 *      VARIANT=1
 *      VERSION=32
 *      BUILD=170
 *      VENDOR_ID=255
 *      PRODUCT_ID=TBD
 *      CHECKSUM=0xC189B6
 *
 *
 */

static const u8 config[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0x32, 0x0A, 0x00, 0x05, 0x01, 0x00,
        0x00, 0x1E, 0x0A, 0x8B, 0x00, 0x00, 0x13, 0x0B,
        0x00, 0x10, 0x32, 0x03, 0x03, 0x00, 0x03, 0x01,
        0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0xBF, 0x03, 0x1B,
        0x02, 0x00, 0x00, 0x37, 0x37, 0x00, 0x00, 0x00,
        0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xA9, 0x7F, 0x9A, 0x0E, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x03, 0x23, 0x00, 0x00, 0x00, 0x0A,
        0x0F, 0x14, 0x19, 0x03, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x08, 0x10,
        0x00
};

static struct mxt_platform_data atmel_mxt_info = {
        .x_line         = 19,
        .y_line         = 11,
        .x_size         = 960,
        .y_size         = 540,
        .blen           = 0x10,
        .threshold      = 0x32,
        .voltage        = 3300000,              /* 3.3V */
        .orient         = 3,
        .config         = config,
        .config_length  = 168,
        .config_crc     = MXT_CONFIG_CRC,
        .irqflags       = IRQF_TRIGGER_FALLING,
/*      .read_chg       = &read_chg, */
        .read_chg       = NULL,
};

static struct i2c_board_info __initdata atmel_i2c_info[] = {
	{
		I2C_BOARD_INFO("atmel_mxt_ts", MXT224_I2C_ADDR1),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH6),
		.platform_data = &atmel_mxt_info,
	}
};

static int __init enterprise_touch_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PH6);
	tegra_gpio_enable(TEGRA_GPIO_PF5);

	gpio_request(TEGRA_GPIO_PH6, "atmel-irq");
	gpio_direction_input(TEGRA_GPIO_PH4);

	gpio_request(TEGRA_GPIO_PF5, "atmel-reset");
	gpio_direction_output(TEGRA_GPIO_PF5, 0);
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PF5, 1);
	msleep(100);

	i2c_register_board_info(1, atmel_i2c_info, 1);

	return 0;
}

static struct usb_phy_plat_data tegra_usb_phy_pdata[] = {
	[0] = {
			.instance = 0,
			.vbus_gpio = -1,
			.vbus_reg_supply = "usb_vbus",
	},
	[1] = {
			.instance = 1,
			.vbus_gpio = -1,
	},
	[2] = {
			.instance = 2,
			.vbus_gpio = -1,
	},
};


static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
			.phy_config = &utmi_phy_config[0],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
	[1] = {
			.phy_config = &utmi_phy_config[1],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
	[2] = {
			.phy_config = &utmi_phy_config[2],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
};

static struct platform_device *tegra_usb_otg_host_register(void)
{
	struct platform_device *pdev;
	void *platform_data;
	int val;

	pdev = platform_device_alloc(tegra_ehci1_device.name,
		tegra_ehci1_device.id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, tegra_ehci1_device.resource,
		tegra_ehci1_device.num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask =  tegra_ehci1_device.dev.dma_mask;
	pdev->dev.coherent_dma_mask = tegra_ehci1_device.dev.coherent_dma_mask;

	platform_data = kmalloc(sizeof(struct tegra_ehci_platform_data),
		GFP_KERNEL);
	if (!platform_data)
		goto error;

	memcpy(platform_data, &tegra_ehci_pdata[0],
				sizeof(struct tegra_ehci_platform_data));
	pdev->dev.platform_data = platform_data;

	val = platform_device_add(pdev);
	if (val)
		goto error_add;

	return pdev;

error_add:
	kfree(platform_data);
error:
	pr_err("%s: failed to add the host contoller device\n", __func__);
	platform_device_put(pdev);
	return NULL;
}

static void tegra_usb_otg_host_unregister(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}

static struct tegra_otg_platform_data tegra_otg_pdata = {
	.host_register = &tegra_usb_otg_host_register,
	.host_unregister = &tegra_usb_otg_host_unregister,
};

static void enterprise_usb_init(void)
{
	struct	fsl_usb2_platform_data *udc_pdata;

	tegra_usb_phy_init(tegra_usb_phy_pdata, ARRAY_SIZE(tegra_usb_phy_pdata));

	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata[2];
	platform_device_register(&tegra_ehci3_device);

	udc_pdata = tegra_udc_device.dev.platform_data;
	udc_pdata->charge_regulator ="usb_bat_chg";
}

static void enterprise_gps_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PE4);
	tegra_gpio_enable(TEGRA_GPIO_PE5);
}

static void enterprise_baseband_init(void)
{
	int modem_id = tegra_get_modem_id();

	switch (modem_id) {
	case 1: /* PH450 ULPI */
		enterprise_modem_init();
		break;
	case 2: /* 6260 HSIC */
		break;
	}
}

static void __init tegra_enterprise_init(void)
{
	tegra_clk_init_from_table(enterprise_clk_init_table);
	enterprise_pinmux_init();
	enterprise_i2c_init();
	enterprise_uart_init();
	enterprise_usb_init();
	enterprise_tsensor_init();
	platform_add_devices(enterprise_devices, ARRAY_SIZE(enterprise_devices));
	enterprise_regulator_init();
	enterprise_sdhci_init();
#ifdef CONFIG_TEGRA_EDP_LIMITS
	enterprise_edp_init();
#endif
	/* enterprise_kbc_init(); */
	enterprise_touch_init();
	enterprise_gps_init();
	enterprise_baseband_init();
	enterprise_panel_init();
	enterprise_bt_rfkill();
	enterprise_setup_bluesleep();
	enterprise_emc_init();
	enterprise_sensors_init();
	enterprise_suspend_init();
	tegra_release_bootloader_fb();
}

static void __init tegra_enterprise_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_4M, SZ_8M);
#else
	tegra_reserve(SZ_128M, SZ_4M, SZ_8M);
#endif
}

MACHINE_START(TEGRA_ENTERPRISE, "tegra_enterprise")
	.boot_params    = 0x80000100,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_enterprise_reserve,
	.init_early	= tegra_init_early,
	.init_irq       = tegra_init_irq,
	.timer          = &tegra_timer,
	.init_machine   = tegra_enterprise_init,
MACHINE_END
