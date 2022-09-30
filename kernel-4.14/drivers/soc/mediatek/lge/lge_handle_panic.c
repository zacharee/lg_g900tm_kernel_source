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

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kdebug.h>
#include <asm/setup.h>
#include <linux/module.h>

#ifdef CONFIG_CPU_CP15_MMU
#include <linux/ptrace.h>
#endif

#include <soc/mediatek/lge/board_lge.h>
#include <soc/mediatek/lge/lge_handle_panic.h>
#include <linux/input.h>

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#endif

#define PANIC_HANDLER_NAME	"panic-handler"

unsigned int lge_crash_reason_magic;

static DEFINE_SPINLOCK(lge_panic_lock);


static struct _lge_crash_footprint *crash_fp;
static int dummy_arg;

#define KEY_CRASH_TIMEOUT 5000
static int gen_key_panic = 0;
static int key_crash_cnt = 0;
static unsigned long key_crash_last_time = 0;

inline static void lge_set_key_crash_cnt(int key, int* is_sequence_valid)
{
	unsigned long cur_time = 0;
	unsigned long key_input_gap = 0;

	cur_time = jiffies_to_msecs(jiffies);
	key_input_gap = cur_time - key_crash_last_time;

	/* check the gap time between two key inputs except the first down key input*/
	if ((key_crash_cnt != 0) && (key_input_gap > KEY_CRASH_TIMEOUT)) {
		pr_debug("%s: Ready to panic %d : over time %ld!\n", __func__, key, key_input_gap);
		return;
	}

	*is_sequence_valid = 1;
	key_crash_cnt++;
	key_crash_last_time = cur_time;

	pr_info("%s: Ready to panic %d : count %d, time gap %ld!\n", __func__, key, key_crash_cnt, key_input_gap);
}


#ifdef CONFIG_LGE_USB_DEBUGGER
#define UART_KEY1 KEY_VOLUMEUP
// KEY_TV is AI key
#define UART_KEY2 KEY_TV
#define KEY1_PRESSED 0x1
#define KEY2_PRESSED 0x2
#define UART_KEY_PRESSED (KEY1_PRESSED|KEY2_PRESSED)
static int uart_key_press_status = 0;
bool lge_get_uart_key_press_status(void){
	return uart_key_press_status ==  UART_KEY_PRESSED ;
}
void lge_uart_enable(int enable){
	extern int mt_need_uart_console;
	extern void mt_disable_uart(void);
	extern void mt_enable_uart(void);
	if (enable) {
		mt_need_uart_console = 1;
		mt_enable_uart();
	} else {
		mt_need_uart_console = 0;
		mt_disable_uart();

	}
}
#endif

void lge_gen_key_panic(int key, int key_status)
{
	int is_sequence_valid = 0;
	int key_order = key_crash_cnt % 3;

	/* the flag is set when the correct key is pressed. But if there are other inputs after the key press, the flag is cleared to zero */
	static int no_other_key_input = 0;

	/* check the lge crash handler enabled */
	if (lge_get_crash_handle_status() != 1)
			return;


#ifdef CONFIG_LGE_USB_DEBUGGER
	if(key == UART_KEY1) {
		uart_key_press_status = key_status ? (uart_key_press_status | KEY1_PRESSED) : 0;
	} else if(key == UART_KEY2) {
		uart_key_press_status = key_status ? (uart_key_press_status | KEY2_PRESSED) : 0;
	}
#endif

	/* First, check the order of key input.									*/
	/* If the order is right, and then check the key_status. (0:release, 1:press)				*/
	/* In the press status case,										*/
	/*	set no_other_key_input flag and then return.							*/
	/* In the release status case,										*/
	/*	check that there is no other input after the key press.						*/
	/*	And then clear the flag and call the lge_set_key_crash_cnt function to increase key_crash_cnt	*/
	if(((key == KEY_VOLUMEDOWN) && (key_order == 0))
		|| ((key == KEY_POWER) && (key_order == 1))
		|| ((key == KEY_VOLUMEUP) && (key_order == 2))) {
		if(key_status == 0) {
			if(no_other_key_input == 1) {
				no_other_key_input = 0;
				lge_set_key_crash_cnt(key, &is_sequence_valid);
			}
		} else {
			no_other_key_input = 1;
			return;
		}
	}

	/* If the sequence is invalid, clear key_crash_cnt to zero */
	if (is_sequence_valid == 0) {
		if(no_other_key_input == 1)
			no_other_key_input = 0;

		if(key_crash_cnt > 0)
			key_crash_cnt = 0;

		pr_debug("%s: Ready to panic %d : cleared!\n", __func__, key);
		return;
	}

	/* If the value of key_crash_cnt is 7, generate panic */
	if (key_crash_cnt == 7) {
		gen_key_panic = 1;
		panic("%s: Generate panic by key!\n", __func__);
	}
}

static int gen_bug(const char *val, const struct kernel_param *kp)
{
	BUG();

	return 0;
}
module_param_call(gen_bug, gen_bug,
		param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

static int gen_panic(const char *val, const struct kernel_param *kp)
{
	panic("LGE Panic Handler Panic Test");

	return 0;
}
module_param_call(gen_panic, gen_panic,
		param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

static int gen_null_pointer_exception(const char *val, const struct kernel_param *kp)
{
	int *panic_test;

	panic_test = 0;
	*panic_test = 0xa11dead;

	return 0;
}
module_param_call(gen_null_pointer_exception, gen_null_pointer_exception,
		param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

static DEFINE_SPINLOCK(watchdog_timeout_lock);
static int gen_wdog(const char *val, const struct kernel_param *kp)
{
	unsigned long flags;

	spin_lock_irqsave(&watchdog_timeout_lock, flags);
	while(1);

	return 0;
}
module_param_call(gen_wdog, gen_wdog, param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

extern void die(const char *str, struct pt_regs *regs, int err);
static int gen_die(const char *val, const struct kernel_param *kp)
{
	struct pt_regs *reg;

	reg = (struct pt_regs *)crash_fp->apinfo.regs;

	die("test die", reg, 0);

	return 0;
}
module_param_call(gen_die, gen_die, param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

#ifdef CONFIG_LGE_DRAM_WR_BIAS_OFFSET
static int wr_bias_offset;
static int wr_bias_set_offset(const char *val, const struct kernel_param *kp)
{
	int wr_bias_for_passzone = 0;
	if (!param_set_int(val, kp))
		wr_bias_for_passzone = *(int *)kp->arg;
	else
		return -EINVAL;

	crash_fp->wr_bias_for_passzone = wr_bias_for_passzone;
	return 0;
}

static int wr_bias_get_offset(char *val, const struct kernel_param *kp)
{
	return sprintf(val, "%d", lge_get_wr_bias_offset());
}
module_param_call(wr_bias_offset, wr_bias_set_offset,
		wr_bias_get_offset, &wr_bias_offset, S_IWUSR | S_IWGRP | S_IRUGO);
#endif

static u32 _lge_get_reboot_reason(void)
{
	return crash_fp->reboot_reason;
}

u32 lge_get_reboot_reason(void)
{
	return _lge_get_reboot_reason();
}

static void _lge_set_reboot_reason(u32 rr)
{
	crash_fp->reboot_reason = rr;

	return;
}

void lge_set_reboot_reason(u32 rr)
{
	u32 prev_rr;

	prev_rr = _lge_get_reboot_reason();

	if (prev_rr == LGE_CRASH_UNKNOWN ||
			prev_rr == LGE_REBOOT_REASON_NORMAL) {
		_lge_set_reboot_reason(rr);
	} else {
		/* pr_warning("block to set boot reason : "
				"prev_rr = 0x%x, new rr = 0x%x\n", prev_rr, rr); */
	}

	return;
}

void lge_set_modem_info(u32 modem_info)
{
	crash_fp->modeminfo.modem_info = modem_info;

	return;
}

u32 lge_get_modem_info(void)
{
	return crash_fp->modeminfo.modem_info;
}

void lge_set_modem_will(const char *modem_will)
{
	memset_io(crash_fp->modeminfo.modem_will, 0x0, LGE_MODEM_WILL_SIZE);
	strncpy(crash_fp->modeminfo.modem_will,
			modem_will, LGE_MODEM_WILL_SIZE - 1);

	return;
}

void lge_set_scp_title(const char *scp_title)
{
	memset_io(crash_fp->scpinfo.scp_title,
			0x0, sizeof(crash_fp->scpinfo.scp_title));
	strncpy(crash_fp->scpinfo.scp_title,
			scp_title, sizeof(crash_fp->scpinfo.scp_title) - 1);

	return;
}

void lge_set_scp_will(const char *scp_will)
{
	memset_io(crash_fp->scpinfo.scp_will,
			0x0, sizeof(crash_fp->scpinfo.scp_will));
	strncpy(crash_fp->scpinfo.scp_will,
			scp_will, sizeof(crash_fp->scpinfo.scp_will) - 1);

	return;
}

char* lge_get_bootprof(void)
{
	return crash_fp->bootprof;
}

void lge_set_bootprof(const char *bootprof)
{
	memset_io(crash_fp->bootprof, 0x0, LGE_BOOT_STR_SIZE);
	strncpy(crash_fp->bootprof,
			bootprof, LGE_BOOT_STR_SIZE - 1);

	return;
}

static int crash_handle_status = 0;

void lge_set_crash_handle_status(u32 enable)
{
	unsigned long flags;

	spin_lock_irqsave(&lge_panic_lock, flags);

	if (enable) {
		crash_handle_status = 1;
	} else {
		crash_handle_status = 0;
	}
	lge_crash_reason_magic = 0x6D630000;
	spin_unlock_irqrestore(&lge_panic_lock, flags);
}

int lge_get_crash_handle_status(void)
{
	return crash_handle_status;
}

static int read_crash_handle_enable(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%d", crash_handle_status);
}

module_param_call(crash_handle_enable, NULL,
		read_crash_handle_enable, &dummy_arg, S_IRUGO);


void lge_set_atf_info(const u32 buf_addr, const u32 buf_size)
{
	crash_fp->atfinfo.atf_buf_addr = buf_addr;
	crash_fp->atfinfo.atf_buf_size = buf_size;
}

void lge_set_ram_console_addr(unsigned int addr, unsigned int size)
{
	crash_fp->oopsinfo.ramconsole_paddr = addr;
	crash_fp->oopsinfo.ramconsole_size = size;
}

static int scp_dump_status = 0;

void lge_set_scp_dump_status(u32 enable)
{
	if (enable)
		scp_dump_status = 1;
	else
		scp_dump_status = 0;
}

int lge_get_scp_dump_status(void)
{
	return scp_dump_status;
}

#ifdef CONFIG_OF_RESERVED_MEM

static void lge_panic_handler_free_page(unsigned long mem_addr,
					unsigned long size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;

	pfn_start = mem_addr >> PAGE_SHIFT;
	pfn_end = (mem_addr + size) >> PAGE_SHIFT;

	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++) {
		free_reserved_page(pfn_to_page(pfn_idx));
	}
}

static void lge_panic_handler_reserve_cleanup(unsigned long addr,
						unsigned long size)
{
	pr_info("reserved-memory free[@0x%lx+@0x%lx)\n", addr, size);

	if (addr == 0 || size == 0)
		return;

	memblock_free(addr, size);

	lge_panic_handler_free_page(addr, size);
}

static u32 sraminfo_address = 0;
static u32 sraminfo_size = 0;
static u32 atfinfo_address = 0;
static u32 atfinfo_size = 0;
static u32 atflogbackup_address = 0;
static u32 atflogbackup_size = 0;
static u32 preloader_mem_addr = 0;
static u32 preloader_mem_size = 0;
static u32 lk_mem_addr = 0;
static u32 lk_mem_size = 0;

static void _lge_set_reserved_4sram(u32 addr, u32 size)
{
	printk(KERN_INFO "%s : addr = 0x%x, size = 0x%x\n", __func__, addr, size);

	crash_fp->sraminfo.addr = addr;
	crash_fp->sraminfo.size = size;
}

static int lge_get_reserved_4sram(struct reserved_mem *rmem)
{
	sraminfo_address = (u32) rmem->base;
	sraminfo_size = (u32) rmem->size;

	return 0;
}

static void _lge_set_reserved_4atf(u32 addr, u32 size)
{
	printk(KERN_INFO "%s : addr = 0x%x, size = 0x%x\n", __func__, addr, size);

	crash_fp->atfinfo.atf_backup_addr = addr;
	crash_fp->atfinfo.atf_backup_size = size;
}

static int lge_get_reserved_4atf(struct reserved_mem *rmem)
{
	atfinfo_address = (u32) rmem->base;
	atfinfo_size = (u32) rmem->size;

	return 0;
}

static void _lge_set_reserved_4atflog(u32 addr, u32 size)
{
	printk(KERN_INFO "%s : addr = 0x%x, size = 0x%x\n", __func__, addr, size);

	crash_fp->atfinfo.atflog_backup_addr = addr;
	crash_fp->atfinfo.atflog_backup_size = size;
}

static int lge_get_reserved_4atflog(struct reserved_mem *rmem)
{
	atflogbackup_address = (u32) rmem->base;
	atflogbackup_size = (u32) rmem->size;

	return 0;
}

static int lge_get_preloader_reserve_mem(struct reserved_mem *rmem)
{
	preloader_mem_addr = rmem->base;
	preloader_mem_size = rmem->size;
	return 0;
}

static int lge_get_lk_reserve_mem(struct reserved_mem *rmem)
{
	lk_mem_addr = rmem->base;
	lk_mem_size = rmem->size;
	return 0;
}

RESERVEDMEM_OF_DECLARE(lge_reserved_4sram_init,
		"lge,smembackup", lge_get_reserved_4sram);
RESERVEDMEM_OF_DECLARE(lge_reserved_4atf_init,
		"lge,atfbackup", lge_get_reserved_4atf);
RESERVEDMEM_OF_DECLARE(lge_reserved_4atflog_init,
		"lge,atflogbackup", lge_get_reserved_4atflog);
RESERVEDMEM_OF_DECLARE(lge_panic_handler_pre_reserved_memory,
		"mediatek,preloader", lge_get_preloader_reserve_mem);
RESERVEDMEM_OF_DECLARE(lge_panic_handler_lk_reserved_memory,
		"mediatek,lk", lge_get_lk_reserve_mem);

static void release_reserved_mem(void)
{
	if (lge_get_crash_handle_status() != 0) {
		pr_info("Crash handler enabled..\n");
		return;
	}

	if (sraminfo_address != 0 && sraminfo_size != 0) {
		lge_panic_handler_reserve_cleanup(sraminfo_address,
				sraminfo_size);
		sraminfo_address = 0;
		sraminfo_size = 0;

		_lge_set_reserved_4sram(sraminfo_address, sraminfo_size);
	}

	if (atfinfo_address != 0 && atfinfo_size != 0) {
		lge_panic_handler_reserve_cleanup(atfinfo_address,
				atfinfo_size);
		atfinfo_address = 0;
		atfinfo_size = 0;

		_lge_set_reserved_4atf(atfinfo_address, atfinfo_size);
	}

	if (atflogbackup_address != 0 && atflogbackup_size != 0) {
		lge_panic_handler_reserve_cleanup(atflogbackup_address,
				atflogbackup_size);
		atflogbackup_address = 0;
		atflogbackup_size = 0;

		_lge_set_reserved_4atf(atflogbackup_address, atflogbackup_size);
	}

	if (preloader_mem_addr != 0 &&  preloader_mem_size != 0) {
		lge_panic_handler_reserve_cleanup(preloader_mem_addr,
				preloader_mem_size);
		preloader_mem_addr = 0;
		preloader_mem_size = 0;
	}

	if (lk_mem_addr != 0 && lk_mem_addr != 0) {
		lge_panic_handler_reserve_cleanup(lk_mem_addr, lk_mem_size);
		lk_mem_addr = 0;
		lk_mem_size = 0;
	}
}

static int reserved_mem_check(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned long flags;

	ret = param_set_int(val, kp);
	if (ret) {
		return ret;
	}

	spin_lock_irqsave(&lge_panic_lock, flags);

	release_reserved_mem();

	spin_unlock_irqrestore(&lge_panic_lock, flags);

	return 0;
}
module_param_call(reserved_mem_check, reserved_mem_check,
		param_get_int, &dummy_arg, S_IWUSR | S_IWGRP | S_IRUGO);
#endif

static inline void lge_save_ctx(struct pt_regs *regs)
{
#ifdef CONFIG_64BIT
	u64 sctlr_el1, tcr_el1, ttbr0_el1, ttbr1_el1, sp_el0, sp_el1;
	struct pt_regs context;
	u64 mpidr;
	int i;

	mpidr = read_cpuid_mpidr();
	crash_fp->apinfo.fault_cpu = (mpidr & 0xff) + ((mpidr & 0xff00) >> 6);

	if (regs == NULL) {
		asm volatile ("stp x0, x1, [%0]\n\t"
				"stp x2, x3, [%0, #16]\n\t"
				"stp x4, x5, [%0, #32]\n\t"
				"stp x6, x7, [%0, #48]\n\t"
				"stp x8, x9, [%0, #64]\n\t"
				"stp x10, x11, [%0, #80]\n\t"
				"stp x12, x13, [%0, #96]\n\t"
				"stp x14, x15, [%0, #112]\n\t"
				"stp x16, x17, [%0, #128]\n\t"
				"stp x18, x19, [%0, #144]\n\t"
				"stp x20, x21, [%0, #160]\n\t"
				"stp x22, x23, [%0, #176]\n\t"
				"stp x24, x25, [%0, #192]\n\t"
				"stp x26, x27, [%0, #208]\n\t"
				"stp x28, x29, [%0, #224]\n\t"
				"str x30, [%0, #240]\n\t" : :
				"r" (&context.user_regs) : "memory");
		asm volatile ("mrs x8, currentel\n\t"
				"mrs x9, daif\n\t"
				"orr x8, x8, x9\n\t"
				"mrs x9, nzcv\n\t"
				"orr x8, x8, x9\n\t"
				"mrs x9, spsel\n\t"
				"orr x8, x8, x9\n\t"
				"str x8, [%2]\n\t"
				"mov x8, sp\n\t"
				"str x8, [%0]\n\t"
				"1:\n\t"
				"adr x8, 1b\n\t"
				"str x8, [%1]\n\t"
				: : "r" (&context.user_regs.sp), "r"(&context.user_regs.pc),
				"r"(&context.user_regs.pstate)  : "x8", "x9", "memory");

		/* save cpu register for simulation */
		for (i = 0; i < 31; i++) {
			crash_fp->apinfo.regs[i] = context.user_regs.regs[i];
		}
		crash_fp->apinfo.regs[31] = context.user_regs.sp;
		crash_fp->apinfo.regs[32] = context.user_regs.pc;
		crash_fp->apinfo.regs[33] = context.user_regs.pstate;
	} else {
		memcpy(crash_fp->apinfo.regs, regs, sizeof(u64) * 34);
	}

	asm volatile ("mrs %0, sctlr_el1\n\t"
				"mrs %1, tcr_el1\n\t"
				"mrs %2, ttbr0_el1\n\t"
				"mrs %3, ttbr1_el1\n\t"
				"mrs %4, sp_el0\n\t"
				"mov %5, sp\n\t"
				: "=r"(sctlr_el1), "=r"(tcr_el1),
				"=r"(ttbr0_el1), "=r"(ttbr1_el1),
				"=r"(sp_el0), "=r"(sp_el1)
				: : "memory");

	printk(KERN_INFO "sctlr_el1: %016llx  tcr_el1: %016llx\n", sctlr_el1, tcr_el1);
	printk(KERN_INFO "ttbr0_el1: %016llx  ttbr1_el1: %016llx\n", ttbr0_el1, ttbr1_el1);
	printk(KERN_INFO "sp_el0: %016llx  sp_el1: %016llx\n", sp_el0, sp_el1);

	/* save mmu register for simulation */
	crash_fp->apinfo.regs[34] = sctlr_el1;
	crash_fp->apinfo.regs[35] = tcr_el1;
	crash_fp->apinfo.regs[36] = ttbr0_el1;
	crash_fp->apinfo.regs[37] = ttbr1_el1;
	crash_fp->apinfo.regs[38] = sp_el0;
	crash_fp->apinfo.regs[39] = sp_el1;
#else
	unsigned int sctrl, ttbr0, ttbr1, ttbcr;
	struct pt_regs context;
	int id, current_cpu;

	asm volatile ("mrc p15, 0, %0, c0, c0, 5 @ Get CPUID\n":"=r"(id));
	current_cpu = (id & 0x3) + ((id & 0xF00) >> 6);

	crash_fp->apinfo.fault_cpu = current_cpu;

	if (regs == NULL) {

		asm volatile ("stmia %1, {r0 - r15}\n\t"
				"mrs %0, cpsr\n"
				:"=r" (context.uregs[16])
				: "r"(&context)
				: "memory");

		/* save cpu register for simulation */
		crash_fp->apinfo.regs[0] = context.ARM_r0;
		crash_fp->apinfo.regs[1] = context.ARM_r1;
		crash_fp->apinfo.regs[2] = context.ARM_r2;
		crash_fp->apinfo.regs[3] = context.ARM_r3;
		crash_fp->apinfo.regs[4] = context.ARM_r4;
		crash_fp->apinfo.regs[5] = context.ARM_r5;
		crash_fp->apinfo.regs[6] = context.ARM_r6;
		crash_fp->apinfo.regs[7] = context.ARM_r7;
		crash_fp->apinfo.regs[8] = context.ARM_r8;
		crash_fp->apinfo.regs[9] = context.ARM_r9;
		crash_fp->apinfo.regs[10] = context.ARM_r10;
		crash_fp->apinfo.regs[11] = context.ARM_fp;
		crash_fp->apinfo.regs[12] = context.ARM_ip;
		crash_fp->apinfo.regs[13] = context.ARM_sp;
		crash_fp->apinfo.regs[14] = context.ARM_lr;
		crash_fp->apinfo.regs[15] = context.ARM_pc;
		crash_fp->apinfo.regs[16] = context.ARM_cpsr;
	} else {
		memcpy(crash_fp->apinfo.regs, regs, sizeof(unsigned int) * 17);
	}


	/*
	 * SCTRL, TTBR0, TTBR1, TTBCR
	 */
	asm volatile ("mrc p15, 0, %0, c1, c0, 0\n" : "=r" (sctrl));
	asm volatile ("mrc p15, 0, %0, c2, c0, 0\n" : "=r" (ttbr0));
	asm volatile ("mrc p15, 0, %0, c2, c0, 1\n" : "=r" (ttbr1));
	asm volatile ("mrc p15, 0, %0, c2, c0, 2\n" : "=r" (ttbcr));

	printk(KERN_INFO "SCTRL: %08x  TTBR0: %08x\n", sctrl, ttbr0);
	printk(KERN_INFO "TTBR1: %08x  TTBCR: %08x\n", ttbr1, ttbcr);

	/* save mmu register for simulation */
	crash_fp->apinfo.regs[17] = sctrl;
	crash_fp->apinfo.regs[18] = ttbr0;
	crash_fp->apinfo.regs[19] = ttbr1;
	crash_fp->apinfo.regs[20] = ttbcr;
#endif

	return;
}

static int lge_handler_panic(struct notifier_block *this,
			unsigned long event,
			void *ptr)
{
	unsigned long flags;

	spin_lock_irqsave(&lge_panic_lock, flags);

	printk(KERN_CRIT "%s called\n", __func__);

	lge_save_ctx(NULL);

	if (_lge_get_reboot_reason() == LGE_CRASH_UNKNOWN ||
		_lge_get_reboot_reason() == LGE_REBOOT_REASON_NORMAL) {
		if (!gen_key_panic) {
			_lge_set_reboot_reason(LGE_CRASH_KERNEL_PANIC);
		} else {
			_lge_set_reboot_reason(LGE_CRASH_KERNEL_PANIC_BY_KEY);
		}
	}

	spin_unlock_irqrestore(&lge_panic_lock, flags);

	return NOTIFY_DONE;
}

static int lge_handler_die(struct notifier_block *self,
			unsigned long cmd,
			void *ptr)
{
	struct die_args *dargs = (struct die_args *) ptr;
	unsigned long flags;

	spin_lock_irqsave(&lge_panic_lock, flags);

	printk(KERN_CRIT "%s called\n", __func__);

	lge_save_ctx(dargs->regs);

	/*
	 * reboot reason setting..
	 */
	if (_lge_get_reboot_reason() == LGE_CRASH_UNKNOWN ||
		_lge_get_reboot_reason() == LGE_REBOOT_REASON_NORMAL) {
		_lge_set_reboot_reason(LGE_CRASH_KERNEL_OOPS);
	}

	spin_unlock_irqrestore(&lge_panic_lock, flags);

	return NOTIFY_DONE;
}

static int lge_handler_reboot(struct notifier_block *self,
			unsigned long cmd,
			void *ptr)
{
	unsigned long flags;

	spin_lock_irqsave(&lge_panic_lock, flags);

	printk(KERN_CRIT "%s called\n", __func__);

	if (_lge_get_reboot_reason() == LGE_CRASH_UNKNOWN) {
		_lge_set_reboot_reason(LGE_REBOOT_REASON_NORMAL);
	}

	spin_unlock_irqrestore(&lge_panic_lock, flags);

	return NOTIFY_DONE;
}

static struct notifier_block lge_panic_blk = {
	.notifier_call  = lge_handler_panic,
	.priority	= 1004,
};

static struct notifier_block lge_die_blk = {
	.notifier_call	= lge_handler_die,
	.priority	= 1004,
};

static struct notifier_block lge_reboot_blk = {
	.notifier_call	= lge_handler_reboot,
	.priority	= 1004,
};

static void lge_print_mtk_poweroff_reason(void * buffer)
{
#ifdef CONFIG_MTK_PMIC_CHIP_MT6359P
	const char *poff_reason[] = {
		"UVLO evnet",
		"PWRGOOD failure",
		"default on BUCK OC",
		"thermal shutdown",
		"warm reset",
		"cold reset",
		"power key(s) long press",
		"PWRHOLD clear(NORMOFF)",
		"BWDT",
		"DDLO occured after system on",
		"AP Watchdog",
		"Power off for power source missing",
		"Critical power is turn off during system on",
		"PWRKEY short press",
		"OVLO event"
	};
	const char *pon_reason[] = {
		"PWRKEY press",
		"RTC alarm",
		"charger insertion",
		"SPAR Event",
		"cold reset"
	};
#else
	const char *poff_reason[] = { NULL };
	const char *pon_reason[] = { NULL };
#endif
	struct _lge_crash_footprint *local_crash_fp = (struct _lge_crash_footprint *) buffer;
	int i;
	int rst_sts;
	int poff_sts;
	int pon_sts;

	if (local_crash_fp == NULL)
		return;
	rst_sts = local_crash_fp->pmic_poweroff_reason.TOP_RST_STATUS;
	pon_sts = local_crash_fp->pmic_poweroff_reason.PONSTS;
	poff_sts = local_crash_fp->pmic_poweroff_reason.POFFSTS;

	pr_info("[pmic_status] TOP_RST_STATUS=0x%x\n", rst_sts);
	pr_info("[pmic_status] POFFSTS=0x%x\n", poff_sts);

	if ( poff_sts != 0 ) {
		for (i = 0; i < ARRAY_SIZE(poff_reason); i++) {
			if (!poff_reason[i])
				continue;
			if (!(poff_sts & BIT(i)))
				continue;

			pr_info("[pmic_status] power off reason is %s \n",
				poff_reason[i]);
		}
	}

	pr_info("[pmic_status] PONSTST=0x%x\n", pon_sts);

	if ( pon_sts != 0 ) {
		for (i = 0; i < ARRAY_SIZE(pon_reason); i++) {
			if (!pon_reason[i])
				continue;
			if (!(pon_sts & BIT(i)))
				continue;

			pr_info("[pmic_status] power on reason is %s \n",
				pon_reason[i]);
		}
	}
}
static int __init lge_panic_handler_early_init(void)
{
	size_t start;
	size_t size;
	void *buffer;

	start = LGE_BSP_RAM_CONSOLE_PHY_ADDR;
	size = LGE_BSP_RAM_CONSOLE_SIZE;

	pr_info("LG console start addr : 0x%x\n", (unsigned int) start);
	pr_info("LG console end addr : 0x%x\n", (unsigned int) (start + size));

	buffer = ioremap_wc(start, size);
	if (buffer == NULL) {
		pr_err("lge_panic_handler: failed to map memory\n");
		return -ENOMEM;
	}

	lge_print_mtk_poweroff_reason(buffer);

	memset_io(buffer, 0x0, size);
	crash_fp = (struct _lge_crash_footprint *) buffer;
	crash_fp->magic = LGE_CONSOLE_MAGIC_KEY;
	_lge_set_reboot_reason(LGE_CRASH_UNKNOWN);

#ifdef CONFIG_OF_RESERVED_MEM
	_lge_set_reserved_4sram(sraminfo_address, sraminfo_size);
	_lge_set_reserved_4atf(atfinfo_address, atfinfo_size);
	_lge_set_reserved_4atflog(atflogbackup_address, atflogbackup_size);
#endif

	atomic_notifier_chain_register(&panic_notifier_list, &lge_panic_blk);
	register_die_notifier(&lge_die_blk);
	register_reboot_notifier(&lge_reboot_blk);

	return 0;
}
early_initcall(lge_panic_handler_early_init);

static int __init lge_panic_handler_probe(struct platform_device *pdev)
{
	int ret = 0;

	return ret;
}

static int lge_panic_handler_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver panic_handler_driver __refdata = {
	.probe	= lge_panic_handler_probe,
	.remove = lge_panic_handler_remove,
	.driver = {
		.name = PANIC_HANDLER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lge_panic_handler_init(void)
{
	return platform_driver_register(&panic_handler_driver);
}

static void __exit lge_panic_handler_exit(void)
{
	platform_driver_unregister(&panic_handler_driver);
}


module_init(lge_panic_handler_init);
module_exit(lge_panic_handler_exit);

MODULE_DESCRIPTION("LGE panic handler driver");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
