/*
 * arch/arm/mach-msm/lge/lge_handle_panic.c
 *
 * Copyright (C) 2010 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LGE_HANDEL_PANIC_H__
#define __LGE_HANDEL_PANIC_H__

#define LGE_BSP_RAM_CONSOLE_PHY_ADDR	0x00115400
#define LGE_BSP_RAM_CONSOLE_SIZE	(3 * SZ_1K)

#define LGE_CONSOLE_MAGIC_KEY		0x4C475243

extern unsigned int lge_crash_reason_magic;

#define LGE_CRASH_REASON_MASK           0xFFFF0000

#define LGE_CRASH_SYS_MASK              0x0000F000
#define LGE_CRASH_KERNEL                0x1000
#define LGE_CRASH_MODEM                 0x2000
#define LGE_CRASH_TZ                    0x3000
#define LGE_CRASH_LAF                   0x4000
#define LGE_CRASH_THERMAL               0x5000
#define LGE_CRASH_EMI_MPU               0x6000
#define LGE_CRASH_BLDR_CRASH            0x7000
#define LGE_CRASH_SCP                   0x8000
#define LGE_BOOT_DMVERITY_CORRUPTED     0x77665567

#define LGE_CRASH_DESC_MASK             0x0000000F

/*******************************************************************************
 * kernel crash
 ******************************************************************************/
#define LGE_CRASH_KERNEL_PANIC          (lge_crash_reason_magic | \
						LGE_CRASH_KERNEL | 0x0000)
#define LGE_CRASH_KERNEL_OOPS           (lge_crash_reason_magic | \
						LGE_CRASH_KERNEL | 0x0001)
#define LGE_CRASH_KERNEL_WDT            (lge_crash_reason_magic | \
						LGE_CRASH_KERNEL | 0x0002)
#define LGE_CRASH_KERNEL_BUS_HANG	(lge_crash_reason_magic | \
						LGE_CRASH_KERNEL | 0x0003)
#define LGE_CRASH_KERNEL_PANIC_BY_KEY	(lge_crash_reason_magic | \
						LGE_CRASH_KERNEL | 0x0004)


/*******************************************************************************
 * modem crash
 ******************************************************************************/
#define LGE_CRASH_MODEM_ASSERT_BSP	(lge_crash_reason_magic | \
						LGE_CRASH_MODEM | 0x0000)
#define LGE_CRASH_MODEM_ASSERT_RF	(lge_crash_reason_magic | \
						LGE_CRASH_MODEM | 0x0001)
#define LGE_CRASH_MODEM_ASSERT_AUDIO	(lge_crash_reason_magic | \
						LGE_CRASH_MODEM | 0x0002)
#define LGE_CRASH_MODEM_ASSERT_GPS	(lge_crash_reason_magic | \
						LGE_CRASH_MODEM | 0x0003)
#define LGE_CRASH_MODEM_ASSERT_PS	(lge_crash_reason_magic | \
						LGE_CRASH_MODEM | 0x0004)
#define LGE_CRASH_MODEM_FATAL		(lge_crash_reason_magic | \
						LGE_CRASH_MODEM | 0x0005)
#define LGE_CRASH_MODEM_UNKNOWN_CRASH	(lge_crash_reason_magic | \
						LGE_CRASH_MODEM | 0x0006)

/*******************************************************************************
 * sensor hub
 ******************************************************************************/
#define LGE_CRASH_SCP_PANIC		(lge_crash_reason_magic | \
						LGE_CRASH_SCP | 0x0000)
/*******************************************************************************
 * Thermal Crash
 * this is not used in kernel.
 * LGE_CRASH_THERMAL_HW_WDT is directly triggered by HW
 ******************************************************************************/
#define LGE_CRASH_THERMAL_HW_WDT        (lge_crash_reason_magic | \
						LGE_CRASH_THERMAL | 0x0000)

/*******************************************************************************
 * EMI MPU
 ******************************************************************************/
#define LGE_CRASH_EMI_MPU_VIOLATION	(lge_crash_reason_magic | \
						LGE_CRASH_EMI_MPU | 0x0000)

#define LGE_CRASH_PRELOADER		(lge_crash_reason_magic | \
						LGE_CRASH_BLDR_CRASH | 0x0000)
#define LGE_CRASH_LK			(lge_crash_reason_magic | \
						LGE_CRASH_BLDR_CRASH | 0x0001)
#define LGE_CRASH_EARLY_KERNEL		(lge_crash_reason_magic | \
						LGE_CRASH_BLDR_CRASH | 0x0002)

#define LGE_CRASH_UNKNOWN		(lge_crash_reason_magic | 0x0000)

#define LGE_REBOOT_REASON_NORMAL		0x00000000
#define LGE_REBOOT_REASON_DLOAD			0x6C616664
#define LGE_REBOOT_REASON_LAF_RESTART		0x6F656D52
#define LGE_REBOOT_REASON_DMVERITY_CORRUPTED	0x77665567
#define LGE_REBOOT_REASON_FOTA			0x77665566

#ifdef CONFIG_LGE_LCD_OFF_DIMMING
#define LGE_REBOOT_REASON_FOTA_LCD_OFF		0x77665560
#define LGE_REBOOT_REASON_FOTA_OUT_LCD_OFF	0x77665561
#define LGE_REBOOT_REASON_LCD_OFF		0x77665562
#endif

#define LGE_REBOOT_REASON_OPID_MISMATCHED	0x77665568

/*******************************************************************************
 * LGE MEMTEST WHOLE DRAM
 * This is not for kernel.
 * It will be used in preloader & lk.
 ******************************************************************************/
#define LGE_RR_MEMTEST_WHOLE			0x99999999 // 4 lge memtest
#define LGE_RR_MEMTEST_WRONG			0xB17F119F // BITFLIP Happened
#define LGE_RR_MEMTEST_NORMAL			0xB17F1190 // No BITFLIP


#define LGE_MODEM_WILL_SIZE			256
#define LGE_BOOT_STR_SIZE           256

struct crash_cpuinfo {
#ifdef CONFIG_64BIT
	u32 fault_cpu;
	u64 regs[40];
#else
	int fault_cpu;
	u32 regs[21];
#endif
};

struct crash_modeminfo {
	u32 modem_info;
	char modem_will[LGE_MODEM_WILL_SIZE];
};

struct crash_scpinfo {
	char scp_title[32];
	char scp_will[LGE_MODEM_WILL_SIZE];
};

struct ramoops_info {
	u32 ramconsole_paddr;
	u32 ramconsole_size;
};

struct reserved_4sram {
	u32 addr;
	u32 size;
};

struct atf_buf_info {
	u32 atf_buf_addr;
	u32 atf_buf_size;
	u32 atf_backup_addr;
	u32 atf_backup_size;
	u32 atflog_backup_addr;
	u32 atflog_backup_size;
};

struct pmic_poweroff_reason {
	u32 TOP_RST_STATUS;
	u32 PONSTS;
	u32 POFFSTS;
};

typedef enum {
	BLDR_ASSERT = 0,
	BLDR_ERROR = 1,
	BLDR_REASON_MAX,
} bldr_crash_reason;

#define BLDR_CRASH_FILE_LENGTH 100
struct crash_bldr_info {
	bldr_crash_reason reason;
	char file_name[BLDR_CRASH_FILE_LENGTH];
	int	 file_line;
};

struct _lge_crash_footprint {
	u32 magic;
	u32 reboot_reason;
	struct crash_cpuinfo apinfo;
	struct crash_modeminfo modeminfo;
	struct crash_scpinfo scpinfo;
	struct ramoops_info oopsinfo;
	struct reserved_4sram sraminfo;
	struct atf_buf_info atfinfo;
	struct pmic_poweroff_reason pmic_poweroff_reason;
#ifdef CONFIG_LGE_DRAM_WR_BIAS_OFFSET
	s32 wr_bias_for_passzone;
#endif
	struct crash_bldr_info bldrinfo;
	char memtest_result[320];
	char bootprof[LGE_BOOT_STR_SIZE];
};

extern void lge_set_reboot_reason(u32 rr);
extern u32 lge_get_reboot_reason(void);
extern void lge_set_modem_info(u32 modem_info);
extern u32 lge_get_modem_info(void);
extern void lge_set_modem_will(const char *modem_will);
extern void lge_set_scp_title(const char *scp_title);
extern void lge_set_scp_will(const char *scp_will);
extern void lge_set_ram_console_addr(unsigned int addr, unsigned int size);
extern void lge_set_atf_info(const u32 buf_addr, const u32 buf_size);
extern void lge_set_crash_handle_status(u32 enable);
extern int lge_get_crash_handle_status(void);
extern void lge_gen_key_panic(int key, int key_status);
extern char* lge_get_bootprof(void);
extern void lge_set_bootprof(const char *bootprof);
extern void lge_set_scp_dump_status(u32 enable);
extern int lge_get_scp_dump_status(void);
#endif
