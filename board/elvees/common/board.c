/*
 * Copyright 2012-2013 Henrik Nordstrom <henrik@henriknordstrom.net>
 * Copyright 2013 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 * Copyright 2007-2011 Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * Copyright 2015-2016 ELVEES NeoTek JSC
 * Copyright 2017 RnD Center "ELVEES", JSC
 *
 * Common board initialization code for ELVEES MCom-compatible boards.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#ifdef CONFIG_SPL_BUILD
#include <spl.h>
#include <asm/arch/bootrom.h>
#include <asm/arch/ddr.h>
#include <asm/arch/ddr-calibration.h>
#endif
#include <asm/arch/clock.h>
#include <asm/arch/regs.h>
#include <asm/io.h>
#include <linux/kernel.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

static bool is_ddrmc_active(int ddrmc_id)
{
	cmctr_t *CMCTR = (cmctr_t *)CMCTR_BASE;
	ddrmc_t *DDRMC;
	bool clk_en;

	u32 reg = CMCTR->GATE_CORE_CTR;

	switch (ddrmc_id) {
	case 0:
		DDRMC = (ddrmc_t *)DDRMC0_BASE;
		clk_en = (reg >> 1) & 1;
		break;
	case 1:
		DDRMC = (ddrmc_t *)DDRMC1_BASE;
		clk_en = (reg >> 2) & 1;
		break;
	default:
		puts("is_ddrmc_active: Invalid argument\n");
		return 0;
	}

	/* If DDRMC clock disabled then DDRMC is not active.
	 * If clock enabled we should additionally check
	 * STAT register.
	 */

	if (clk_en && (DDRMC->STAT == 0x1))
		return 1;

	return 0;
}

int dram_init_banksize(void)
{
	/* DDR controller #0 is active since TPL uses it */
	gd->bd->bi_dram[0].start = PHYS_SDRAM_0;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_0_SIZE;

	/* If DDR controller #1 is active increase
	 * available for Linux memory size
	 */
	if (is_ddrmc_active(1)) {
		gd->bd->bi_dram[1].start = PHYS_SDRAM_1;
		gd->bd->bi_dram[1].size = PHYS_SDRAM_1_SIZE;
	} else {
		gd->bd->bi_dram[1].start = 0;
		gd->bd->bi_dram[1].size = 0;
	}

	return 0;
}

__weak int ddr_poweron(void)
{
	return 0;
}

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_0_SIZE;
#ifdef CONFIG_SPL_BUILD
	struct ddr_cfg cfg[2];
	struct ddr_freq freq;
	int i, ret;

	ret = ddr_poweron();
	if (ret) {
		printf("Failed to set DDR power: %d\n", ret);
		hang();
	}

	freq.xti_freq = XTI_FREQ;
	freq.cpll_mult = CPLL_VALUE;
	freq.ddr0_div = DIV_DDR0_CTR_VALUE;
	freq.ddr1_div = DIV_DDR1_CTR_VALUE;

	for (i = 0; i < 2; i++) {
		ret = set_sdram_cfg(&cfg[i],
				    ddr_get_clock_period(i, &freq));
		if (ret)
			return ret;
		cfg[i].ctl_id = i;
	}

#ifdef CONFIG_DDR_CALIBRATION
	set_calib_cfg(cfg);
#endif
	timer_init();

	puts("DDR controllers init started\n");

	u32 status = mcom_ddr_init(&cfg[0], &cfg[1], &freq);

	if (ddr_getrc(status, 0))
		puts("DDR controller #0 init failed\n");
	else
		puts("DDR controller #0 init done\n");

	if (ddr_getrc(status, 1))
		puts("DDR controller #1 init failed\n");
	else
		puts("DDR controller #1 init done\n");

	return status;
#else
	return 0;
#endif
}

#ifdef CONFIG_SPL_BUILD
u32 spl_boot_device(void)
{
	smctr_t *SMCTR = (smctr_t *)SMCTR_BASE;

	switch (SMCTR->BOOT) {
	case SMCTR_BOOT_SPI0:
		return BOOT_DEVICE_SPI;
	case SMCTR_BOOT_SDMMC0:
		return BOOT_DEVICE_MMC1;
	}

	return BOOT_DEVICE_NONE;
}

u32 spl_boot_mode(const u32 boot_device)
{
	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
		return MMCSD_MODE_RAW;
	}

	return 0;
}
#endif