/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/debugfs.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#ifdef wakelock
#include <linux/wakelock.h>
#endif
#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#ifdef CONFIG_MTK_TIMER_TIMESYNC
#ifndef ADSP_EARLY_PORTING_BYPASS
#include <mtk_sys_timer.h>
#include <mtk_sys_timer_typedefs.h>
#endif
#endif
/* FIXME: ADSP has own mtcmos in mt6789 */
#ifndef ADSP_EARLY_PORTING_BYPASS
#include <mtk_spm_sleep.h>
#endif
#include <plat_debug_api.h>

#include "adsp_helper.h"
#include "adsp_excep.h"
#include "adsp_dvfs.h"
#include "adsp_clk.h"
#include "adsp_service.h"
#include "adsp_logger.h"
#include "adsp_bus_monitor.h"
#include "adsp_mbox.h"

#define ADSP_READY_TIMEOUT (40 * HZ) /* 40 seconds*/

//struct adsp_regs adspreg;
struct adsp_common_t adsp_common;
struct adsp_core_t adsp_core[ADSP_CORE_TOTAL];
char *adsp_core_ids[ADSP_CORE_TOTAL] = {"ADSP A"};
unsigned int adsp_ready[ADSP_CORE_TOTAL];
static struct dentry *adsp_debugfs;

/* adsp workqueue & work */
struct workqueue_struct *adsp_workqueue;
static void adsp_notify_ws(struct work_struct *ws);
static void adsp_timeout_ws(struct work_struct *ws);
static DECLARE_WORK(adsp_notify_work, adsp_notify_ws);
static DECLARE_DELAYED_WORK(adsp_timeout_work, adsp_timeout_ws);

/* adsp notify */
static DEFINE_MUTEX(adsp_A_notify_mutex);
static BLOCKING_NOTIFIER_HEAD(adsp_A_notifier_list);

#ifdef CFG_RECOVERY_SUPPORT
static unsigned int adsp_timeout_times;
static struct workqueue_struct *adsp_reset_workqueue;
unsigned int adsp_recovery_flag[ADSP_CORE_TOTAL];
atomic_t adsp_reset_status = ATOMIC_INIT(ADSP_RESET_STATUS_STOP);
struct completion adsp_sys_reset_cp;
struct adsp_work_t adsp_sys_reset_work;
struct wakeup_source adsp_reset_lock;
DEFINE_SPINLOCK(adsp_reset_spinlock);
#endif

/*
 * register apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:   notifier block struct
 */
void adsp_A_register_notify(struct notifier_block *nb)
{
	mutex_lock(&adsp_A_notify_mutex);
	blocking_notifier_chain_register(&adsp_A_notifier_list, nb);

	pr_debug("[ADSP] register adsp A notify callback..\n");

	if (is_adsp_ready(ADSP_A_ID) == 1)
		nb->notifier_call(nb, ADSP_EVENT_READY, NULL);
	mutex_unlock(&adsp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(adsp_A_register_notify);
/*
 * unregister apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:     notifier block struct
 */
void adsp_A_unregister_notify(struct notifier_block *nb)
{
	mutex_lock(&adsp_A_notify_mutex);
	blocking_notifier_chain_unregister(&adsp_A_notifier_list, nb);
	mutex_unlock(&adsp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(adsp_A_unregister_notify);

#ifdef CFG_RECOVERY_SUPPORT
static int adsp_event_receive(struct notifier_block *this, unsigned long event,
			    void *ptr)
{
	adsp_read_status_release(event);
	return 0;
}

static struct notifier_block adsp_ready_notifier1 = {
	.notifier_call = adsp_event_receive,
	.priority = AUDIO_HAL_FEATURE_PRI,
};

void adsp_extern_notify(enum ADSP_NOTIFY_EVENT notify_status)
{
	blocking_notifier_call_chain(&adsp_A_notifier_list,
				     notify_status, NULL);
}

/*
 * adsp_set_reset_status, set and return scp reset status function
 * return value:
 *   0: scp not in reset status
 *   1: scp in reset status
 */
unsigned int adsp_set_reset_status(void)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&adsp_reset_spinlock, spin_flags);
	if (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START) {
		spin_unlock_irqrestore(&adsp_reset_spinlock, spin_flags);
		return 1;
	}
	/* adsp not in reset status, set it and return*/
	atomic_set(&adsp_reset_status, ADSP_RESET_STATUS_START);
	spin_unlock_irqrestore(&adsp_reset_spinlock, spin_flags);
	return 0;
}

/*
 * callback function for work struct
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
void adsp_sys_reset_ws(struct work_struct *ws)
{
	struct adsp_work_t *sws = container_of(ws, struct adsp_work_t, work);
	unsigned int adsp_reset_type = sws->flags;
	unsigned int adsp_reset_flag = 0; /* make sure adsp is in idle state */
	int timeout = 100; /* max wait 2s */

	/*set adsp not ready*/
	adsp_recovery_flag[ADSP_A_ID] = ADSP_RECOVERY_START;
	adsp_ready[ADSP_A_ID] = 0;
	adsp_register_feature(SYSTEM_FEATURE_ID);
	adsp_extern_notify(ADSP_EVENT_STOP);

	/* wake lock AP*/
	__pm_stay_awake(&adsp_reset_lock);

	pr_info("%s(): adsp_aed_reset\n", __func__);
	if (adsp_reset_type == ADSP_RESET_TYPE_AWAKE)
		adsp_aed_reset(EXCEP_KERNEL, ADSP_A_ID);
	else
		adsp_aed_reset(EXCEP_RUNTIME, ADSP_A_ID);

	/*wait adsp ee finished in 10s*/
	if (wait_for_completion_interruptible_timeout(&adsp_sys_reset_cp,
					msecs_to_jiffies(10000)) == 0) {
		pr_info("%s: adsp ee time out\n", __func__);
		/*timeout check adsp status again*/
		if (is_adsp_ready(ADSP_A_ID) != -1)
			goto END;
	}

	/* enable clock for access ADSP Reg*/
	adsp_enable_dsp_clk(true);
	/* dump bus status if has bus hang*/
	if (is_adsp_bus_monitor_alert()) {
#ifdef CONFIG_MTK_EMI
		dump_emi_outstanding(); /* check infra, dump all info*/
		lastbus_timeout_dump(); /* check infra/peri, dump both info */
#endif
		adsp_bus_monitor_dump();
	}

	if (adsp_reset_type == ADSP_RESET_TYPE_AWAKE)
		pr_info("%s(): adsp awake fail, wait system back\n", __func__);

	/* make sure adsp is in idle state */
	while (--timeout) {
		if ((readl(ADSP_SLEEP_STATUS_REG) & ADSP_A_IS_WFI) &&
		    (readl(ADSP_DBG_PEND_CNT) == 0)) {
			adsp_reset();
			if (readl(ADSP_SLEEP_STATUS_REG) & ADSP_A_IS_ACTIVE) {
				adsp_reset_flag = 1;
				break;
			}
		}
		msleep(20);
	}

	if (!adsp_reset_flag) {
		if (readl(ADSP_DBG_PEND_CNT))
			pr_info("%s(): failed, bypass and wait\n", __func__);
		else
			adsp_reset();
	}
#if ADSP_BOOT_TIME_OUT_MONITOR
	if (!work_busy(&adsp_timeout_work.work))
		queue_delayed_work(adsp_workqueue, &adsp_timeout_work,
			jiffies + ADSP_READY_TIMEOUT);
#endif
END:
	adsp_enable_dsp_clk(false);
}
/*
 */
void adsp_send_reset_wq(enum ADSP_RESET_TYPE type, enum adsp_core_id core_id)
{
	adsp_sys_reset_work.flags = (unsigned int) type;
	queue_work(adsp_reset_workqueue, &adsp_sys_reset_work.work);
}

void adsp_recovery_init(void)
{
	/*create wq for scp reset*/
	adsp_reset_workqueue = create_singlethread_workqueue("ADSP_RESET_WQ");
	/*init reset work*/
	INIT_WORK(&adsp_sys_reset_work.work, adsp_sys_reset_ws);
	/*init completion for identify adsp aed finished*/
	init_completion(&adsp_sys_reset_cp);
	wakeup_source_init(&adsp_reset_lock, "adsp reset wakelock");
	/* init reset by cmd flag*/
	adsp_recovery_flag[ADSP_A_ID] = ADSP_RECOVERY_OK;
}
#endif
/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void adsp_notify_ws(struct work_struct *ws)
{
#ifdef CFG_RECOVERY_SUPPORT
	if (adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START) {
		mutex_lock(&adsp_A_notify_mutex);
		adsp_recovery_flag[ADSP_A_ID] = ADSP_RECOVERY_OK;
		atomic_set(&adsp_reset_status, ADSP_RESET_STATUS_STOP);
		adsp_extern_notify(ADSP_EVENT_READY);
		adsp_deregister_feature(SYSTEM_FEATURE_ID);
		mutex_unlock(&adsp_A_notify_mutex);
		__pm_relax(&adsp_reset_lock);
	}
#endif
}


/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void adsp_timeout_ws(struct work_struct *ws)
{
#ifdef CFG_RECOVERY_SUPPORT
	if (adsp_timeout_times < 5) {
		adsp_timeout_times++;
		__pm_relax(&adsp_reset_lock);
		pr_debug("%s(): cnt (%d)\n", __func__, adsp_timeout_times);
		adsp_send_reset_wq(ADSP_RESET_TYPE_AWAKE, ADSP_A_ID);
	} else
		BUG_ON(1); /* reboot */
#else
	adsp_aed(EXCEP_BOOTUP, ADSP_A_ID);
#endif
}

void adsp_reset_ready(enum adsp_core_id id)
{
	adsp_ready[id] = 0;
}

/*
 * handle notification from adsp
 * mark adsp is ready for running tasks
 * @param id:   ipi id
 * @param data: ipi data
 * @param len:  length of ipi data
 */
void adsp_A_ready_ipi_handler(int id, void *data, unsigned int len)
{
	unsigned int adsp_image_size = *(unsigned int *)data;

	if (!adsp_ready[ADSP_A_ID]) {
#if ADSP_BOOT_TIME_OUT_MONITOR
		cancel_delayed_work(&adsp_timeout_work);
#endif
		/* set adsp ready flag and clear SPM interrupt */
		adsp_ready[ADSP_A_ID] = 1;

		if (adsp_is_suspend)
			complete(&adsp_resume_cp);

#ifdef CFG_RECOVERY_SUPPORT
		adsp_timeout_times = 0;
		queue_work(adsp_workqueue, &adsp_notify_work);
#endif
	}
	/* verify adsp image size */
	if (adsp_image_size != ADSP_A_TCM_SIZE) {
		pr_info("[ADSP]image size ERROR! AP=0x%x,ADSP=0x%x\n",
			ADSP_A_TCM_SIZE, adsp_image_size);
		WARN_ON(1);
	}
}

/*
 * @return: 1 if adsp is ready for running tasks
 */
int is_adsp_ready(enum adsp_core_id id)
{
	if (id >= ADSP_CORE_TOTAL)
		return -EINVAL;
#ifdef CFG_RECOVERY_SUPPORT
	/* exception */
	if (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START ||
	    adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START)
		return -1;
#endif
	return adsp_ready[id];
}

/*
 * power on adsp
 * generate error if power on fail
 * @return:         1 if success
 */
uint32_t adsp_power_on(uint32_t enable)
{
	if (enable) {
		adsp_enable_clock();
		adsp_sw_reset(ADSP_A_ID);
		adsp_sw_reset(ADSP_B_ID);
		adsp_set_clock_freq(CLK_DEFAULT_INIT_CK);
		adsp_A_send_spm_request(true);
	} else {
		adsp_set_clock_freq(CLK_DEFAULT_26M_CK);
		adsp_disable_clock();
	}
	pr_debug("-%s (%x)\n", __func__, enable);
	return 1;
}

static inline ssize_t adsp_A_status_show(struct device *kobj,
					 struct device_attribute *attr,
					 char *buf)
{
	unsigned int status = 0;
	char *adsp_status;

	adsp_enable_dsp_clk(true);
	status = readl(ADSP_A_SYS_STATUS);
	adsp_enable_dsp_clk(false);

	switch (status) {
	case ADSP_STATUS_ACTIVE:
		adsp_status = "ADSP A is active";
		break;
	case ADSP_STATUS_SUSPEND:
		adsp_status = "ADSP A is suspend";
		break;
	case ADSP_STATUS_SLEEP:
		adsp_status = "ADSP A is sleep";
		break;
	case ADSP_STATUS_RESET:
		adsp_status = "ADSP A is reset";
		break;
	default:
		adsp_status = "ADSP A in unknown status";
		break;
	}
	return scnprintf(buf, PAGE_SIZE, "%s\n", adsp_status);
}
DEVICE_ATTR_RO(adsp_A_status);

static inline ssize_t adsp_A_db_test_show(struct device *kobj,
					  struct device_attribute *attr,
					  char *buf)
{
#ifdef CFG_RECOVERY_SUPPORT
	adsp_send_reset_wq(ADSP_RESET_TYPE_AWAKE, ADSP_A_ID);
#else
	adsp_aed(EXCEP_KERNEL, ADSP_A_ID);
#endif
	if (is_adsp_ready(ADSP_A_ID) == 1)
		return scnprintf(buf, PAGE_SIZE, "dumping ADSP A db\n");
	else
		return scnprintf(buf, PAGE_SIZE, "not ready, try dump EE\n");
}
DEVICE_ATTR_RO(adsp_A_db_test);

static inline ssize_t adsp_ipi_test_store(struct device *kobj,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	enum adsp_ipi_status ret;
	int value = 0;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	adsp_register_feature(SYSTEM_FEATURE_ID);

	if (is_adsp_ready(ADSP_A_ID) == 1) {
		ret = adsp_ipi_send(ADSP_IPI_TEST1, &value, sizeof(value),
				    0, ADSP_A_ID);
	}

	/*
	 * BE CAREFUL! this cmd shouldn't let adsp process over 1s.
	 * Otherwise, you should register other feature before.
	 */
	adsp_deregister_feature(SYSTEM_FEATURE_ID);

	return count;
}

static inline ssize_t adsp_ipi_test_show(struct device *kobj,
					 struct device_attribute *attr,
					 char *buf)
{
	unsigned int value = 0x5A5A;
	enum adsp_ipi_status ret;

	if (is_adsp_ready(ADSP_A_ID) == 1) {
		ret = adsp_ipi_send(ADSP_IPI_TEST1, &value, sizeof(value),
				    0, ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}
DEVICE_ATTR_RW(adsp_ipi_test);

static inline ssize_t adsp_uart_switch_store(struct device *kobj,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	enum adsp_ipi_status ret;
	unsigned int value = 0;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	if (is_adsp_ready(ADSP_A_ID) == 1) {
		ret = adsp_ipi_send(ADSP_IPI_TEST1, &value, sizeof(value),
				    0, ADSP_A_ID);
	}
	return count;
}
DEVICE_ATTR_WO(adsp_uart_switch);

static inline ssize_t adsp_suspend_cmd_show(struct device *kobj,
					     struct device_attribute *attr,
					     char *buf)
{
	return adsp_dump_feature_state(buf, PAGE_SIZE);
}

static inline ssize_t adsp_suspend_cmd_store(struct device *kobj,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t id = 0;
	char *temp = NULL, *token1 = NULL, *token2 = NULL;
	char *pin = NULL;
	char delim[] = " ,\t\n";

	temp = kstrdup(buf, GFP_KERNEL);
	pin = temp;
	token1 = strsep(&pin, delim);
	token2 = strsep(&pin, delim);

	id = adsp_get_feature_index(token2);

	if ((strcmp(token1, "regi") == 0) && (id >= 0))
		adsp_register_feature(id);

	if ((strcmp(token1, "deregi") == 0) && (id >= 0))
		adsp_deregister_feature(id);

	kfree(temp);
	return count;
}
DEVICE_ATTR_RW(adsp_suspend_cmd);

#ifdef CFG_RECOVERY_SUPPORT
static ssize_t adsp_recovery_flag_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", adsp_recovery_flag[ADSP_A_ID]);
}
DEVICE_ATTR_RO(adsp_recovery_flag);
#endif

static struct attribute *adsp_default_attrs[] = {
	&dev_attr_adsp_A_status.attr,
	&dev_attr_adsp_A_db_test.attr,
	&dev_attr_adsp_ipi_test.attr,
	&dev_attr_adsp_uart_switch.attr,
	&dev_attr_adsp_suspend_cmd.attr,
#ifdef CFG_RECOVERY_SUPPORT
	&dev_attr_adsp_recovery_flag.attr,
#endif
	NULL,
};

static struct attribute_group adsp_default_attr_group = {
	.attrs = adsp_default_attrs,
};

static const struct attribute_group *adsp_attr_groups[] = {
	&adsp_default_attr_group,
	&adsp_awake_attr_group,
	&adsp_dvfs_attr_group,
	&adsp_logger_attr_group,
	&adsp_excep_attr_group,
	NULL,
};

const struct file_operations adsp_device_fops = {
	.owner = THIS_MODULE,
	.read = adsp_A_log_if_read,
	.open = adsp_A_log_if_open,
	.poll = adsp_A_log_if_poll,
	.unlocked_ioctl = adsp_driver_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = adsp_driver_compat_ioctl,
#endif
};

static struct miscdevice adsp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "adsp",
	.groups = adsp_attr_groups,
	.fops = &adsp_device_fops,
};

static ssize_t adsp_debug_read(struct file *file, char __user *buf,
			       size_t count, loff_t *pos)
{
	char *buffer = NULL; /* for reduce kernel stack */
	int ret = 0;
	size_t n = 0, max_size;

	buffer = adsp_get_reserve_mem_virt(ADSP_A_DEBUG_DUMP_MEM_ID);
	max_size = adsp_get_reserve_mem_size(ADSP_A_DEBUG_DUMP_MEM_ID);

	n = strnlen(buffer, max_size);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	return ret;
}

static ssize_t adsp_debug_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[64];

	if (copy_from_user(buf, buffer, min(count, sizeof(buf))))
		return -EFAULT;

	if (adsp_register_feature(SYSTEM_FEATURE_ID) == 0) {
		adsp_ipi_send(ADSP_IPI_ADSP_TIMER, buf,
			min(count, sizeof(buf)), 0, ADSP_A_ID);
		adsp_deregister_feature(SYSTEM_FEATURE_ID);
	}

	return count;
}

static const struct file_operations adsp_debug_ops = {
	.open = simple_open,
	.read = adsp_debug_read,
	.write = adsp_debug_write,
};

void adsp_update_memory_protect_info(void)
{
	struct adsp_mpu_info_t *mpu_info;
	phys_addr_t nc_addr = adsp_get_reserve_mem_phys(ADSP_MPU_NONCACHE_ID);

	mpu_info = (struct adsp_mpu_info_t *)ADSP_A_MPUINFO_BUFFER;
	mpu_info->data_non_cache_addr = nc_addr;
	mpu_info->data_non_cache_size =
			  adsp_get_reserve_mem_phys(ADSP_SHARED_MEM_END)
			  + adsp_get_reserve_mem_size(ADSP_SHARED_MEM_END)
			  - nc_addr;
}

void adsp_enable_dsp_clk(bool enable)
{
	if (enable) {
		pr_debug("enable dsp clk\n");
		adsp_enable_clock();
	} else {
		pr_debug("disable dsp clk\n");
		adsp_disable_clock();
	}
}

static int adsp_system_sleep_suspend(struct device *dev)
{
	mutex_lock(&adsp_suspend_mutex);
	if ((is_adsp_ready(ADSP_A_ID) == 1) || adsp_feature_is_active()) {
#ifdef CONFIG_MTK_TIMER_TIMESYNC
#ifndef ADSP_EARLY_PORTING_BYPASS
	/* FIXME: build error */
		sys_timer_timesync_sync_adsp(SYS_TIMER_TIMESYNC_FLAG_FREEZE);
#endif
#endif
		adsp_awake_unlock_adsppll(ADSP_A_ID, 1);
	}
	mutex_unlock(&adsp_suspend_mutex);
	return 0;
}

static int adsp_system_sleep_resume(struct device *dev)
{
	mutex_lock(&adsp_suspend_mutex);
	if ((is_adsp_ready(ADSP_A_ID) == 1) || adsp_feature_is_active()) {
		/*wake adsp up*/
		adsp_awake_unlock_adsppll(ADSP_A_ID, 0);
#ifdef CONFIG_MTK_TIMER_TIMESYNC
#ifndef ADSP_EARLY_PORTING_BYPASS
		/* FIXME: build error */
		sys_timer_timesync_sync_adsp(SYS_TIMER_TIMESYNC_FLAG_UNFREEZE);
#endif
#endif
}
	mutex_unlock(&adsp_suspend_mutex);

	return 0;
}

static int adsp_syscore_suspend(void)
{
	if ((is_adsp_ready(ADSP_A_ID) != 1) && !adsp_feature_is_active()) {
		/* FIXME: adsp has own mtcmos in mt6789 */
#ifndef ADSP_EARLY_PORTING_BYPASS
		adsp_bus_sleep_protect(true);
		spm_adsp_mem_protect();
#endif
	}
	return 0;
}

static void adsp_syscore_resume(void)
{
	if ((is_adsp_ready(ADSP_A_ID) != 1) && !adsp_feature_is_active()) {
		/* FIXME: adsp has own mtcmos in mt6789 */
#ifndef ADSP_EARLY_PORTING_BYPASS
		spm_adsp_mem_unprotect();
		adsp_bus_sleep_protect(false);
#endif
		/* release adsp sw_reset,
		 * let ap is able to write adsp cfg/dtcm
		 * no matter adsp is suspend.
		 */
		adsp_enable_clock();
		writel((ADSP_A_SW_RSTN | ADSP_A_SW_DBG_RSTN),
		      ADSP_CFGREG_SW_RSTN);
		udelay(1);
		writel(0, ADSP_CFGREG_SW_RSTN);
#ifdef CONFIG_MTK_TIMER_TIMESYNC
#ifndef ADSP_EARLY_PORTING_BYPASS
		/* FIXME: build error */
		sys_timer_timesync_sync_adsp(SYS_TIMER_TIMESYNC_FLAG_UNFREEZE);
#endif
#endif
#if ADSP_BUS_MONITOR_INIT_ENABLE
		adsp_bus_monitor_init(); /* reinit bus monitor hw */
#endif
		adsp_disable_clock();
	}
}

static int adsp_device_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;

	/* adsp_common */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adsp_common.cfg = devm_ioremap_resource(dev, res);
	adsp_common.cfg_size = resource_size(res);
	if (IS_ERR(adsp_common.cfg))
		goto ERROR;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	adsp_common.cfg_secure = devm_ioremap_resource(dev, res);
	adsp_common.cfgsec_size = resource_size(res);
	if (IS_ERR(adsp_common.cfg_secure))
		goto ERROR;

	adsp_reserve_memory_ioremap(adsp_common.sharedram,
				    adsp_common.shared_size);

	adsp_common.clkctrl = adsp_common.cfg + ADSP_CLK_CTRL_OFFSET;
	adsp_clk_device_probe(&pdev->dev);

#if ENABLE_ADSP_EMI_PROTECTION
	set_adsp_mpu(adsp_common.sharedram, adsp_common.shared_size);
#endif
	adsp_dts_mapping();

	return 0;
ERROR:
	pr_err("%s() error\n", __func__);
	return -ENODEV;
}

static int adsp_device_remove(struct platform_device *pdev)
{
	adsp_clk_device_remove(&pdev->dev);
	return 0;
}

static int adsp_core_device_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	u32 core_id;
	int ret;
	u32 sysram[2] = {0, 0};

	of_property_read_u32(pdev->dev.of_node, "core_id", &core_id);

	/* memory */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adsp_core[core_id].iram = devm_ioremap_resource(dev, res);
	adsp_core[core_id].i_tcmsize = resource_size(res);
	if (IS_ERR(adsp_core[core_id].iram))
		goto ERROR;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	adsp_core[core_id].dram = devm_ioremap_resource(dev, res);
	adsp_core[core_id].d_tcmsize = resource_size(res);
	if (IS_ERR(adsp_core[core_id].dram))
		goto ERROR;

	adsp_core[core_id].total_tcmsize = adsp_core[core_id].i_tcmsize
					   + adsp_core[core_id].d_tcmsize;

	ret = of_property_read_u32(pdev->dev.of_node,
			"sysram", &sysram[0]);
	if (ret)
		goto ERROR;
	ret = of_property_read_u32(pdev->dev.of_node,
			"sysram_size", &sysram[1]);
	if (ret)
		goto ERROR;
	adsp_core[core_id].sysram = ioremap_wc(sysram[0], sysram[1]);
	adsp_core[core_id].sysram_size = sysram[1];

	/* irq */
	adsp_core[core_id].ipc_irq = platform_get_irq(pdev, 0);
	if (request_irq(adsp_core[core_id].ipc_irq, adsp_ipc_dispatch,
			IRQF_TRIGGER_LOW, "ADSP A IPC2HOST", NULL))
		goto ERROR;

	adsp_core[core_id].wdt_irq = platform_get_irq(pdev, 1);
	if (request_irq(adsp_core[core_id].wdt_irq, adsp_wdt_dispatch,
			IRQF_TRIGGER_LOW, "ADSP A WDT", NULL))
		goto ERROR;

	adsp_core[core_id].audioipc_irq = platform_get_irq(pdev, 2);
	if (request_irq(adsp_core[core_id].audioipc_irq, adsp_audioipc_dispatch,
			IRQF_TRIGGER_LOW, "ADSP A AUDIOIPC", NULL))
		goto ERROR;

	return 0;

ERROR:
	pr_err("%s()error\n", __func__);
	return -ENODEV;

}

static int adsp_core_device_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id adsp_of_ids[] = {
	{ .compatible = "mediatek,adsp_common", },
	{}
};

static const struct dev_pm_ops adsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(adsp_system_sleep_suspend,
				adsp_system_sleep_resume)
};

static struct syscore_ops adsp_syscore_ops = {
	.resume = adsp_syscore_resume,
	.suspend = adsp_syscore_suspend,
};

static struct platform_driver mtk_adsp_device = {
	.probe = adsp_device_probe,
	.remove = adsp_device_remove,
	.driver = {
		.name = "adsp_common",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = adsp_of_ids,
#endif
#ifdef CONFIG_PM
		.pm = &adsp_pm_ops,
#endif
	},
};


static const struct of_device_id adsp_core0_of_ids[] = {
	{ .compatible = "mediatek,adsp_core_0", },
};

static struct platform_driver mtk_adsp_core0_device = {
	.probe = adsp_core_device_probe,
	.remove = adsp_core_device_remove,
	.driver = {
		.name = "adsp_core0",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = adsp_core0_of_ids,
#endif
	},
};

static const struct of_device_id adsp_core1_of_ids[] = {
	{ .compatible = "mediatek,adsp_core_1", },
};

static struct platform_driver mtk_adsp_core1_device = {
	.probe = adsp_core_device_probe,
	.remove = adsp_core_device_remove,
	.driver = {
		.name = "adsp_core1",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = adsp_core1_of_ids,
#endif
	},
};

static struct platform_driver * const drivers[] = {
	&mtk_adsp_device,
	&mtk_adsp_core0_device,
	&mtk_adsp_core1_device,
};

/*
 * driver initialization entry point
 */
static int __init adsp_init(void)
{
	int ret = 0;

	ret = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	if (unlikely(ret != 0)) {
		pr_err("[ADSP] platform driver register fail\n");
		return ret;
	}

	ret = misc_register(&adsp_device);
	if (unlikely(ret != 0)) {
		pr_err("[ADSP] misc register failed\n");
		return ret;
	}

	register_syscore_ops(&adsp_syscore_ops);

	return ret;

}

static int __init adsp_module_init(void)
{
	int ret = 0;

	adsp_workqueue = create_workqueue("ADSP_WQ");
	adsp_debugfs = debugfs_create_file("audiodsp", S_IFREG | 0644, NULL,
					(void *)&adsp_device, &adsp_debug_ops);
	if (IS_ERR(adsp_debugfs))
		return PTR_ERR(adsp_debugfs);

	/* adsp initialize */
	adsp_power_on(true);
	adsp_update_memory_protect_info();
	adsp_awake_init();

	ret = adsp_excep_init();
	if (ret)
		goto ERROR;

	adsp_suspend_init();
	adsp_A_irq_init();
	adsp_ipi_init();
	adsp_ipi_registration(ADSP_IPI_ADSP_A_READY, adsp_A_ready_ipi_handler,
			      "adsp_A_ready");

#if ADSP_LOGGER_ENABLE
	ret = adsp_logger_init();
	if (ret)
		goto ERROR;
#endif
#ifdef CFG_RECOVERY_SUPPORT
	adsp_recovery_init();
	adsp_A_register_notify(&adsp_ready_notifier1);
#endif
#if ADSP_BUS_MONITOR_INIT_ENABLE
	adsp_bus_monitor_init();
#endif
	adsp_release_runstall(ADSP_A_ID, true);

#if ADSP_BOOT_TIME_OUT_MONITOR
	queue_delayed_work(adsp_workqueue, &adsp_timeout_work,
			jiffies + ADSP_READY_TIMEOUT);
#endif
	pr_debug("[ADSP] driver_init_done\n");

ERROR:
	return ret;
}

/*
 * driver exit point
 */
static void __exit adsp_exit(void)
{
	free_irq(adsp_core[ADSP_A_ID].wdt_irq, NULL);

	misc_deregister(&adsp_device);
	debugfs_remove(adsp_debugfs);

	flush_workqueue(adsp_workqueue);
	destroy_workqueue(adsp_workqueue);
#ifdef CFG_RECOVERY_SUPPORT
	flush_workqueue(adsp_reset_workqueue);
	destroy_workqueue(adsp_reset_workqueue);
#endif
}
subsys_initcall(adsp_init);
module_init(adsp_module_init);
module_exit(adsp_exit);

