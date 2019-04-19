/*
 * Copyright 2015-2016 ELVEES NeoTek JSC
 * Copyright 2017 RnD Center "ELVEES", OJSC
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#ifdef CONFIG_SPL_BUILD
#include <spl.h>
#include <asm/arch/ddr.h>
#endif
#include <asm/arch/clock.h>
#include <asm/arch/regs.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <linux/kernel.h>

#include <i2c.h>
#include <libfdt.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SPL_BUILD

/* Set parameters for  Micron MT41K256M16HA-125 SDRAM */
int set_sdram_cfg(struct ddr_cfg *cfg, int tck)
{
	int i;

	if (!cfg) {
		printf("Invalid pointer to DDR configuration\n");
		return -EINVAL;
	}

	if (!tck) {
		printf("Invalid clock period\n");
		return -EINVAL;
	}

	/* TODO: This function works only for exactly 1066 MHz, because memory
	 * timings could be different for different frequency. */

	cfg->type = MCOM_SDRAM_TYPE_DDR3;

	/* This is the best combination of ODS/ODT parameters.
	 * It was found during DDR calibration for board types:
	 * Salute-EL24PM1 r1.0.
	 */
	cfg->impedance.ods_mc = 40;
	cfg->impedance.ods_dram = 40;
	cfg->impedance.odt_mc = 0;
	cfg->impedance.odt_dram = 0;

	cfg->ctl.dqs_gating_override = 1;
	/* TODO: Replace this configuration by the optimal one. */
	for (i = 0; i < 4; i++)
		cfg->ctl.dqs_gating[i] = 0xa001;

	cfg->common.ranks = MCOM_SDRAM_ONE_RANK;
	cfg->common.banks = 8;
	cfg->common.columns = 1024;
	cfg->common.rows = 32768;
	cfg->common.bl = 8;
	cfg->common.cl = 7;
	cfg->common.cwl = 6;
	cfg->common.twr = max(MCOM_DDR_MIN_TWR, to_clocks(15000, tck));
	cfg->common.tfaw = to_clocks(50000, tck);
	cfg->common.tras = to_clocks(37500, tck);
	cfg->common.tras_max = to_clocks(9 * 7800000, tck);
	cfg->common.trc = to_clocks(50625, tck);
	cfg->common.txp = to_clocks(max(3 * tck, 7500), tck);
	cfg->common.trtp = to_clocks(max(4 * tck, 7500), tck);
	cfg->common.twtr = to_clocks(max(4 * tck, 7500), tck);
	cfg->common.trcd = to_clocks(13125, tck);
	cfg->common.trrd = to_clocks(max(4 * tck, 10000), tck);
	cfg->common.tccd = 4;
	cfg->common.tcke = to_clocks(max(3 * tck, 7500), tck);
	cfg->common.tckesr = cfg->common.tcke + 1;
	cfg->common.tzqcs = 64;
	cfg->common.trefi = to_clocks(7800000, tck);
	cfg->ddr3.trp = to_clocks(13125, tck);
	cfg->ddr3.txpdll = to_clocks(max(10 * tck, 24000), tck);
	cfg->ddr3.tmrd = 4;
	cfg->ddr3.tmod = to_clocks(max(12 * tck, 15000), tck);
	cfg->ddr3.tcksre = to_clocks(max(5 * tck, 10000), tck);
	cfg->ddr3.tcksrx = to_clocks(max(5 * tck, 10000), tck);
	cfg->ddr3.tzqoper = 256;
	cfg->ddr3.trfc = to_clocks(260000, tck);
	cfg->ddr3.tdllk = 512;
	cfg->ddr3.txsdll = cfg->ddr3.tdllk;
	cfg->ddr3.txs = max(5, to_clocks(10000, tck) + (int)cfg->ddr3.trfc);

	return 0;
}

#endif

int set_variable(const char *name, char *value)
{
		char *oldval = getenv(name);		
		if (oldval && strcmp(value, oldval))
		{
			printf("Warning: \"%s\" variable value (%s) does not match the EEPROM value (%s)\n", name, oldval, value);
		}
	
		if (!oldval)
		{
			int ret = setenv(name, value);	
			if (ret)
			{
				printf("Unable to set variable \"%s\", error: %d\n", name, ret);
				return 1;
			}		
		}
		return 0;
}


void fill_mac_addr(void)
{
 	uint8_t buf[ARP_HLEN];
 	char strbuf[ARP_HLEN_ASCII + 1];
	int ret, nodeoffset;	

	//EEPROM
	ret = i2c_probe(0x50);
	if (ret)
	{
		printf("Unable to access EEPROM, error: %d\n", ret);
		return;
	}

	ret = i2c_read(0x50, 0xfa, 1, buf, 6);
	if (ret)
	{
		printf("Unable to read MAC address, error: %d\n", ret);
		return;
	}

	//local-mac-address
	nodeoffset = fdt_path_offset(gd->fdt_blob, "/ethernet@3800f000");
	if (nodeoffset < 0)
	{
		printf("Unable to get Ethernet FDT node, error: %s\n", fdt_strerror(nodeoffset));
		return;
	}
	ret = fdt_setprop((void *) gd->fdt_blob, nodeoffset, "local-mac-address", buf, ARP_HLEN);
	if (ret < 0)
	{
		printf("Unable to set FDT MAC address, error: %s\n", fdt_strerror(ret));
		return;
	}
	
	//Environment variable
	sprintf(strbuf, "%pM", buf);
	if (!set_variable("ethaddr", strbuf))
	{
		printf("MAC:   %s\n", strbuf);
	}	
}

int16_t read_eeprom_16(unsigned char address, const char *name)
{
	int16_t result = 0;
 	int ret = i2c_probe(0x50);
	if (ret)
	{
		printf("Unable to access EEPROM, error: %d\n", ret);
		return 0;
	}

	ret = i2c_read(0x50, address, 1, (unsigned char *) &result, 2);
	if (ret)
	{
		printf("Unable to read %s, error: %d\n", name, ret);
		return 0;
	}
	if (!result || (result == -1))
	{
		printf("Parameter %s is not set in the EEPROM\n", name);
		return 0;		
	}
	return result;
}

void fill_serial_and_revision(void)
{
 	char strbuf[6];

	int16_t serial = read_eeprom_16(0, "serial");
	int16_t revision = read_eeprom_16(2, "revision");

	if (serial)
	{
		sprintf(strbuf, "%d", serial);
		if (!set_variable("serial#", strbuf))
		{
			printf("Ser#:  %s\n", strbuf);	
		}
	}
	if (revision)
	{
		sprintf(strbuf, "%d", revision);
		if (!set_variable("revision#", strbuf))
		{
			printf("Rev#:  %s\n", strbuf);	
		}
	}

}

#ifdef CONFIG_SERIAL_TAG
void get_board_serial(struct tag_serialnr *serialnr)
{
        char *serial_string;
        unsigned long long serial;

        serial_string = getenv("serial#");

        if (serial_string)
        {
			serial = simple_strtoull(serial_string, NULL, 16);
            serialnr->high = (unsigned int) (serial >> 32);
            serialnr->low = (unsigned int) (serial & 0xffffffff);
        } else
        {
            serialnr->high = 0;
            serialnr->low = 0;
        }
}
#endif

#ifdef CONFIG_REVISION_TAG
u32 get_board_rev(void)
{

        if (getenv("revision#") != NULL)
        {
			return simple_strtoul(getenv("revision#"), NULL, 10);
		}
        return 0;
}
#endif


int board_late_init(void)
{
	fill_serial_and_revision();
	fill_mac_addr();
	
	return 0;
}
