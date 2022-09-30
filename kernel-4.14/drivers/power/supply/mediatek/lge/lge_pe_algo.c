#define pr_fmt(fmt) "[PE] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <mtk_charger_intf.h>
#include <mt-plat/charger_class.h>

#define PATTERN_LOW (100000)
#define PATTERN_HIGH (500000)

struct pattern_delay {
	int target;
	int target_min;
	int target_max;

	int actual;
};
static struct mutex pattern_lock;

static void pe_mdelay(u64 msec)
{
	ktime_t to = ms_to_ktime(msec);

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_hrtimeout(&to, HRTIMER_MODE_REL);
}

/* Pump Express Plus */
int lge_pe_algo_send_ta_current_pattern(struct charger_device *chg_dev,
					bool is_increase)
{
	if (!chg_dev->ops->set_input_current)
		return -ENOTSUPP;

	mutex_lock(&pattern_lock);

	pr_info("try to %s vbus", (is_increase ? "increase" : "decrease"));

	/* settle time for previous pattern */
	pe_mdelay(50);

	chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);

	if (is_increase) {
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
		pe_mdelay(281);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
		pe_mdelay(281);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
		pe_mdelay(281);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
		pe_mdelay(85);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
		pe_mdelay(485);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
		pe_mdelay(50);
		chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
		pe_mdelay(200);
	} else {
		chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
	}

	mutex_unlock(&pattern_lock);

	return 0;
}
EXPORT_SYMBOL(lge_pe_algo_send_ta_current_pattern);

#define PE20_BITS (5)		/* 5 bits data */

/* zero bit high pattern delay. 50 ms */
#define PE20_DELAY_G (45)
#define PE20_DELAY_G_MIN (30)
#define PE20_DELAY_G_MAX (65)
/* zero bit low pattern delay. 100 ms */
#define PE20_DELAY_D (98)
#define PE20_DELAY_D_MIN (88)
#define PE20_DELAY_D_MAX (115)
/* one bit high pattern delay. 100 ms */
#define PE20_DELAY_F (98)
#define PE20_DELAY_F_MIN (88)
#define PE20_DELAY_F_MAX (115)
/* one bit low pattern delay. 50 ms */
#define PE20_DELAY_H (45)
#define PE20_DELAY_H_MIN (30)
#define PE20_DELAY_H_MAX (65)
/* end pattern delay. 160ms */
#define PE20_DELAY_E (160)
#define PE20_DELAY_E_MIN (150)
#define PE20_DELAY_E_MAX (240)
/* wdt delay. 240ms */
#define PE_DELAY_WDT (240)

#define PE20_VOLTAGE_MIN (5500000)	/* 5.5V */
#define PE20_VOLTAGE_MAX (20000000)	/* 20V */
#define PE20_VOLTAGE_STEP (500000)	/* 0.5V */

/* save result for debugging */
static struct pattern_delay pe20_result[(PE20_BITS * 2) + 1];
static int pe20_result_index = 0;
static struct timespec pe20_time;

static int pe20_get_time_elapsed(void)
{
	struct timespec now, elapsed;

	get_monotonic_boottime(&now);
	elapsed = timespec_sub(now, pe20_time);

	pe20_time = now;

	return elapsed.tv_nsec/1000000;
}

static void pe20_result_dump(void)
{
	pr_info("target: %3d %3d, %3d %3d, %3d %3d, %3d %3d, %3d %3d, %3d\n",
			pe20_result[0].target, pe20_result[1].target,	/* Bit 4 */
			pe20_result[2].target, pe20_result[3].target,	/* Bit 3 */
			pe20_result[4].target, pe20_result[5].target,	/* Bit 2 */
			pe20_result[6].target, pe20_result[7].target,	/* Bit 1 */
			pe20_result[8].target, pe20_result[9].target,	/* Bit 0 */
			pe20_result[10].target);

	pr_info("result: %3d %3d, %3d %3d, %3d %3d, %3d %3d, %3d %3d, %3d\n",
			pe20_result[0].actual, pe20_result[1].actual,	/* Bit 4 */
			pe20_result[2].actual, pe20_result[3].actual,	/* Bit 3 */
			pe20_result[4].actual, pe20_result[5].actual,	/* Bit 2 */
			pe20_result[6].actual, pe20_result[7].actual,	/* Bit 1 */
			pe20_result[8].actual, pe20_result[9].actual,	/* Bit 0 */
			pe20_result[10].actual);
}

static void pe20_result_record(struct pattern_delay pd)
{
	pe20_result_index++;

	if (pe20_result_index >= ARRAY_SIZE(pe20_result)
			|| pe20_result_index < 0)
		return;


	pe20_result[pe20_result_index] = pd;
}

static int pe20_send_bit(struct charger_device *chg_dev, int bit, int data)
{
	struct pattern_delay pd_high, pd_low;
	int ret = 0;

	if (data == 0) {
		pd_high.target = PE20_DELAY_G;
		pd_high.target_min = PE20_DELAY_G_MIN;
		pd_high.target_max = PE20_DELAY_G_MAX;

		pd_low.target = PE20_DELAY_D;
		pd_low.target_min = PE20_DELAY_D_MIN;
		pd_low.target_max = PE20_DELAY_D_MAX;
	} else {
		pd_high.target = PE20_DELAY_F;
		pd_high.target_min = PE20_DELAY_F_MIN;
		pd_high.target_max = PE20_DELAY_F_MAX;

		pd_low.target = PE20_DELAY_H;
		pd_low.target_min = PE20_DELAY_H_MIN;
		pd_low.target_max = PE20_DELAY_H_MAX;
	}

	/* pattern high */
	chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
	pe_mdelay(pd_high.target);

	pd_high.actual = pe20_get_time_elapsed();
	pe20_result_record(pd_high);

	/* pattern low */
	chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
	pe_mdelay(pd_low.target);

	pd_low.actual = pe20_get_time_elapsed();
	pe20_result_record(pd_low);

	if (pd_high.actual < pd_high.target_min
			|| pd_high.actual > pd_high.target_max) {
		pr_err("failed to send bit %d high. target:%d actual:%d\n",
				bit, pd_high.target, pd_high.actual);
		ret = -EIO;
	}

	if (pd_low.actual < pd_low.target_min
			|| pd_low.actual > pd_low.target_max) {
		pr_err("failed to send bit %d low. target:%d actual:%d\n",
				bit, pd_low.target, pd_low.actual);
		ret = -EIO;
	}

	return ret;
}

static int pe20_send_end(struct charger_device *chg_dev)
{
	struct pattern_delay pd = {
		.target = PE20_DELAY_E,
		.target_min = PE20_DELAY_E_MIN,
		.target_max = PE20_DELAY_E_MAX,
	};

	/* end sending data */
	chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
	pe_mdelay(pd.target);

	pd.actual = pe20_get_time_elapsed();
	pe20_result_record(pd);

	if (pd.actual < pd.target_min || pd.actual > pd.target_max) {
		pr_err("failed to send end. target:%d actual:%d\n",
				pd.target, pd.actual);
		return -EIO;
	}

	return 0;
}

int lge_pe_algo_send_ta20_current_pattern(struct charger_device *chg_dev, u32 uV)
{
	int pattern; 	/* 9V patten=00111=0x07, 8.5V patten=00110=0x06 */
	int bit;	/* bit 4 ~ 0 */
	int data;
	int ret = 0;

	if (!chg_dev->ops->set_input_current)
		return -ENOTSUPP;

	mutex_lock(&pattern_lock);

	/* Pump Express Plus 2.0 support 5.5V ~ 20V */
	if (uV < PE20_VOLTAGE_MIN)
		uV = PE20_VOLTAGE_MIN;
	if (uV > PE20_VOLTAGE_MAX)
		uV = PE20_VOLTAGE_MAX;

	pattern = (uV - PE20_VOLTAGE_MIN) / PE20_VOLTAGE_STEP;
	pr_info("vbus target=%duV, pattern=0x%02x", uV, pattern);

	/* prepare for send data */
	chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);
	chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
	pe_mdelay(70);

	/* mark start time */
	get_monotonic_boottime(&pe20_time);
	pe20_result_index = 0;

	/* send 5 bits + end */
	bit = PE20_BITS;
	do {
		/* data order : bit 4 ... bit 0 */
		bit--;
		data = pattern & (1 << bit);

		/* send bit pattern */
		ret = pe20_send_bit(chg_dev, bit, data);
		if (ret)
			goto out;
	} while (bit > 0);
	/* send end pattern */
	ret = pe20_send_end(chg_dev);
	if (ret)
		goto out;

	chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
	pe_mdelay(30);
	chg_dev->ops->set_input_current(chg_dev, PATTERN_HIGH);

	pe20_result_dump();
out:
	mutex_unlock(&pattern_lock);

	return ret;
}
EXPORT_SYMBOL(lge_pe_algo_send_ta20_current_pattern);

int lge_pe_algo_reset_ta(struct charger_device *chg_dev)
{
	if (!chg_dev->ops->set_input_current)
		return -ENOTSUPP;

	mutex_lock(&pattern_lock);

	chg_dev->ops->set_input_current(chg_dev, PATTERN_LOW);
	pe_mdelay(PE_DELAY_WDT);

	mutex_unlock(&pattern_lock);

	return 0;
}
EXPORT_SYMBOL(lge_pe_algo_reset_ta);

int lge_pe_algo_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
	struct charger_manager *chg_mgr = NULL;

	chg_mgr = charger_dev_get_drvdata(chg_dev);
	if (!chg_mgr)
		return -EINVAL;

	chg_mgr->pe2.profile[0].vchr = 8000000;
	chg_mgr->pe2.profile[1].vchr = 8000000;
	chg_mgr->pe2.profile[2].vchr = 8000000;
	chg_mgr->pe2.profile[3].vchr = 8500000;
	chg_mgr->pe2.profile[4].vchr = 8500000;
	chg_mgr->pe2.profile[5].vchr = 8500000;
	chg_mgr->pe2.profile[6].vchr = 9000000;
	chg_mgr->pe2.profile[7].vchr = 9000000;
	chg_mgr->pe2.profile[8].vchr = 9500000;
	chg_mgr->pe2.profile[9].vchr = 9500000;

	return 0;
}
EXPORT_SYMBOL(lge_pe_algo_set_pep20_efficiency_table);

static void __exit lge_pe_algo_exit(void)
{
	mutex_destroy(&pattern_lock);
}

static int __init lge_pe_algo_init(void)
{
	mutex_init(&pattern_lock);

	return 0;
}

subsys_initcall(lge_pe_algo_init);
module_exit(lge_pe_algo_exit);

MODULE_DESCRIPTION("Pump Express Plus Software Pattern Module");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL");
