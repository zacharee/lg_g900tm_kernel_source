/* Copyright (c) 2016 LG Electronics, Inc.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <soc/mediatek/lge/lge_mme.h>

#define DEBUG 0
static DEFINE_SPINLOCK(mme_lock);
static DEFINE_MUTEX(repeat_mutex);


/* predefined cmd */
#define CMD_MME_POWER_ON                30
#define CMD_MME_POWER_OFF               31
#define CMD_MME_SEND_TRACK1_ONCE        33
#define CMD_MME_SEND_TRACK2_ONCE        34
#define CMD_MME_SEND_TRACK3_ONCE        35
#define CMD_MME_TEST_START              38
#define CMD_MME_SEND_STOP               39


/* f2f_data */
static unsigned long f2f_count = 0;
static unsigned int f2f_time = 0;
static unsigned int f2f_half_time = 0;
static char track_bit_buf[COMBO_TRACK_TX_BIT_LENGTH];    // MAX (79*7) + (40*5)

/* global variables
   should be updated from dts */
static int gpio_sleep_n = 0;  // DRV8838_SLEEP_N_Pin, MSM GPIO 3
static int gpio_ph = 0;       // DRV8838_PH pin,      MSM GPIO 0
static int gpio_en = 0;       // DRV8838_EN pin,      MSM GPIO 2

/* track data of test card */
static char track1[TRACK1_SIZE] = "%B1234123412341234^MA/DINGDY                 ^9901120000000000000032123010101?";
static char track2[TRACK2_SIZE] = ";1234567890123456=12345678901234567890?";
static char track3[TRACK3_SIZE] = ";0000000000000000000000000000000000000000000000000000000000000000000==1234123412345=0==0=0000000000000000?";

static unsigned int scale_duration = F2F_TIME;	// default time
static unsigned int command_type = 0;           // send_commnad (odd: once, even: repeat)
static unsigned int test_value = 1;             // test value for continuous emission
static bool is_repeat = 0;                      // for repeat command
#ifdef CONFIG_PM_WAKELOCKS
static struct wakeup_source mme_wakelock;
#else
static struct wake_lock   mme_wakelock;         // wake_lock used for HW test
#endif

static struct class *mme_class;
static struct device *mme_dev;
static int mme_major;

typedef enum tagMME_BIAS_GPIO_STATE {
	MME_GPIO_STATE_SLEEP_N0,
	MME_GPIO_STATE_SLEEP_N1,
	MME_GPIO_STATE_PH0,
	MME_GPIO_STATE_PH1,
	MME_GPIO_STATE_EN0,
	MME_GPIO_STATE_EN1,
	MME_BIAS_GPIO_STATE_MAX,	/* for array size */
} MME_BIAS_GPIO_STATE;

static struct pinctrl *mme_bias_pctrl; /* static pinctrl instance */
static const char *mme_bias_state_name[MME_BIAS_GPIO_STATE_MAX] = {
    "mme_gpio_sleep_n0",
    "mme_gpio_sleep_n1",
    "mme_gpio_ph0",
    "mme_gpio_ph1",
    "mme_gpio_en0",
    "mme_gpio_en1"
};/* DTS state mapping name */

static int mme_bias_set_state(const char *name)
{
    int ret = 0;
    struct pinctrl_state *pState = 0;

    if (!mme_bias_pctrl) {
	pr_info("this pctrl is null\n");
	return -1;
    }

    pState = pinctrl_lookup_state(mme_bias_pctrl, name);
    if (IS_ERR(pState)) {
        pr_err("set state '%s' failed\n", name);
        ret = PTR_ERR(pState);
        goto exit;
    }

    /* select state! */
    pinctrl_select_state(mme_bias_pctrl, pState);

exit:
    return ret; /* Good! */
}

static int mme_bias_gpio_select_state(MME_BIAS_GPIO_STATE s)
{
	int ret = 0;
    BUG_ON(!((unsigned int)(s) < (unsigned int)(MME_BIAS_GPIO_STATE_MAX)));
    ret = mme_bias_set_state(mme_bias_state_name[s]);
	return ret;
}

static void f2f_ms_enable(void)
{
	/* gpio_set_value(gpio_en, 1); */
	mme_bias_gpio_select_state(MME_GPIO_STATE_EN1);
}

static void f2f_ms_disable(void)
{
	/* gpio_set_value(gpio_en, 0); */
	mme_bias_gpio_select_state(MME_GPIO_STATE_EN0);
}

/*
 * Function : f2f_ms_zero_pulse_transmit
 * Card reader check frequency of MS data by initial zero padding.
 * And after LRC need to send some zero padding.
 */
static void f2f_ms_zero_pulse_transmit(void)
{
	unsigned long duration = f2f_time;
	uint8_t i = 0;
//	pr_err("[MME] %s: F2F_INITIAL_ZERO loop\n", __func__);

	for (i = 0; i < F2F_INITIAL_ZERO; i++) {
		if (f2f_count % 2 == 0) {
			/* gpio_set_value(gpio_ph, 1); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_PH1);
		}
		else {
			/* gpio_set_value(gpio_ph, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_PH0);
		}
		f2f_count++;
		udelay(duration);
	}
}

/*
 * Function : f2f_ms_pulse_transmit
 * Make a f2f pulse for 1 or 0.
 */
static uint8_t f2f_ms_pulse_transmit(char value)
{
	uint8_t i = 0, count = 0;
	unsigned long duration = 0;

//	pr_err("[MME] %s: track_bit_buf = %x (this is value) =====\n", __func__, value);
	if (value) {
		count = 2;
		duration = f2f_half_time;
	} else {
		count = 1;
		duration = f2f_time;
	}
//	pr_err("[MME] %s: count(%u) loop\n", __func__, count);

	for (i = 0; i < count; i++) {
		if (f2f_count % 2 == 0) {
			/* gpio_set_value(gpio_ph, 1); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_PH1);
		}
		else {
			/* gpio_set_value(gpio_ph, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_PH0);
		}
		f2f_count++;
//		pr_err("\n");
		udelay(duration);
	}
	return i;
}

/*
 * Function : f2f_make_paritybit
 * Make a paritybit for a character
 * and attach it to end of character.
 */
static void f2f_make_paritybit(char* buf, int type)
{
	int i = 0, numchk = 0, size = 0;
//    pr_info("[MME] %s: f2f_make_paritybit\n", __func__);

	size = (type == TRACK1_FORMAT) ? 6 : 4;

	for (i = 0; i < size; i++) {
		if ((*buf >> i) & 0x1)
			numchk++;
	}
	if (numchk % 2 == 0)
		*buf |= (0x1 << size);
}

/*
 * Function : f2f_ms_lrc_calulator
 * LRC calculation.
 */
static uint8_t f2f_ms_lrc_calulator(char* buf, int size, int type)
{
	int i = 0, j = 0, len = 0, sum = 0;
	char lrc = 0;

	len = (type == TRACK1_FORMAT) ? 6 : 4;

	for (i = 0; i < len; i++) {
		sum = 0;
		for (j = 0; j < size - 1; j++) {
			if (buf[j] & (0x1 << i))
				sum++;
		}
#if DEBUG
		pr_err("[MME] %s: i shift sum = %d\n", __func__, sum);
#endif
		if (sum % 2)
			lrc |= (0x1 << i);
#if DEBUG
		pr_err("[MME] %s: i shift lrc = %d\n", __func__, lrc);
#endif
	}

	f2f_make_paritybit(&lrc, type);
	buf[size-1] = lrc;
	pr_err("[MME] %s: f2f_ms_lrc_calulator LRC = %x\n", __func__, lrc);

	return 0;
}

/*
 * Function : f2f_translator
 * Change raw data to MS Track data form.
 */
static uint8_t f2f_translator(char* buf, int type, int size)
{
	uint8_t ret = 0, i = 0;
	char shifter = 0;

	pr_err("[MME] %s: f2f_translator for type %d\n",__func__, type);
	pr_err("[MME] %s: buf= %s\n", __func__, buf);

	if (type == TRACK1_FORMAT)
		shifter = 0x20;
	else
		shifter = 0x30;

	for (i = 0; i < size - 1; i++) {
#if DEBUG
		pr_err("[MME] %s: before %x",__func__, buf[i]);
#endif
		buf[i] -= shifter;
		f2f_make_paritybit(&buf[i], type);
#if DEBUG
		pr_err(", after %x\n", buf[i]);
#endif
	}
//	pr_err("[MME] %s: after Translator buf= %x\n", __func__, buf);

	f2f_ms_lrc_calulator(buf, size, type);
	return ret;
}

/*
 * Function : f2f_ms_make_transmit_buf
 * Make binary data to transmit by MME.
 * It need to reduce the delay time between transmission data.
 */
static void f2f_ms_make_transmit_buf(char* bbuf, char* buf, int size, int length)
{
	int i = 0, j = 0, blength = 0;
	char ec = 0;
	blength = size * length;
	for (i = 0; i < size; i++) {
		ec = buf[i];
		for (j = 0; j < length; j++) {
			//			pr_err("[MME] %s: idx = %d, ec=%x", __func__, ((length*i) + j), ec);
			bbuf[((length*i) + j)] = (char)(ec & 0x1);
			//            pr_err(", bbuf[%d] = %x\n", ((length*i) + j), bbuf[((length*i) + j)]);
			ec >>= 1;
		}
	}
}

/*
 * TODO : need to make the function to calculate a length of track data
 *        using start & end special character. track1 "% , ?"  track2 "; , ?"
 */
static int f2f_ms_data_generation(char* buf, int type, int size)
{
	int length = 0, blength = 0;
	char *temp_buf;
	char *bit_buf;

	temp_buf = buf;
	bit_buf = track_bit_buf;
	memset(bit_buf, 0x00, COMBO_TRACK_TX_BIT_LENGTH);

	/* track1 */
	if (type & 0x1) {
		length = TRACK1_BIT_FORMAT;
		blength += (length * TRACK1_SIZE);
		f2f_translator(temp_buf, TRACK1_FORMAT, TRACK1_SIZE);
		f2f_ms_make_transmit_buf(bit_buf, temp_buf, TRACK1_SIZE, length);
#if 0
		/* track1 + track2 */
		if (type & 0x8) {
			temp_buf += TRACK1_SIZE;
			bit_buf += TRACK1_TX_BIT_LENGTH;

			length = TRACK2_BIT_FORMAT;
			blength += (length * TRACK2_SIZE);
			f2f_translator(temp_buf, TRACK2_FORMAT, TRACK2_SIZE);
			f2f_ms_make_transmit_buf(bit_buf, temp_buf, TRACK2_SIZE, length);
		}
#endif
	/* track2 */
	} else if (type & 0x2) {
		length = TRACK2_BIT_FORMAT;
		blength += (length * TRACK2_SIZE);
		f2f_translator(temp_buf, TRACK2_FORMAT, TRACK2_SIZE);
		f2f_ms_make_transmit_buf(bit_buf, temp_buf, TRACK2_SIZE, length);
#if 0
        /* track2 + track1 */
        if (type & 0x8) {
            temp_buf += TRACK2_SIZE;
            bit_buf += TRACK2_TX_BIT_LENGTH;

            length = TRACK1_BIT_FORMAT;
            blength += (length * TRACK1_SIZE);
            f2f_translator(temp_buf, TRACK1_FORMAT, TRACK1_SIZE);
            f2f_ms_make_transmit_buf(bit_buf, temp_buf, TRACK1_SIZE, length);
        }
#endif
	/* track3 */
	} else if (type & 0x4) {
		length = TRACK3_BIT_FORMAT;
		blength += (length * TRACK3_SIZE);
		f2f_translator(temp_buf, TRACK3_FORMAT, TRACK3_SIZE);
		f2f_ms_make_transmit_buf(bit_buf, temp_buf, TRACK3_SIZE, length);
	} else {
		pr_err("[MME] Not found matched type");
	}

	return blength;
}

/*
 * Function : f2f_ms_data_transmit
 * Main transfer function.
 */
uint8_t f2f_ms_data_transmit(char* buf, int type, int size)
{
	uint8_t ret = 0;
	int i = 0;
#if DEBUG
	int j = 0;
#endif
	int blength = 0;

	f2f_time = scale_duration;	// 500us
	f2f_half_time = f2f_time/2;

	pr_err("[MME] %s: type:%d, size:%d, T = %d\n", __func__
			, type, size, f2f_time);

	blength = f2f_ms_data_generation(buf, type, size);
#if DEBUG
	pr_err("[MME] %s: bbuf = ", __func__);
	i = 0;
	if (type & 0x1) {
		for (; i < TRACK1_TX_BIT_LENGTH; i++) {
			pr_err("0x%x ", track_bit_buf[i]);
			if ((i+1) % TRACK1_BIT_FORMAT == 0)
				pr_err("\n");
		}
	}
	if (type & 0x2) {
		j = i + TRACK2_TX_BIT_LENGTH;
		for (; i < j; i++) {
			pr_err("0x%x ", track_bit_buf[i]);
			if ((i+1) % TRACK2_BIT_FORMAT == 0)
				pr_err("\n");
		}
	}
#endif
	pr_err("[MME] %s: length %d\n", __func__, blength);

	/* GPIO EN pin enable */
	f2f_ms_enable();
	spin_lock_irq(&mme_lock);

	f2f_ms_zero_pulse_transmit();
//	pr_err("[MME] %s: track_bit_buf array is [%s]\n", __func__, track_bit_buf);

	for (i = 0; i < blength; i++) {
		f2f_ms_pulse_transmit(track_bit_buf[i]);
	}
	f2f_ms_zero_pulse_transmit();

	spin_unlock_irq(&mme_lock);
	/* GPIO EN pin disable */
	f2f_ms_disable();

	f2f_count = 0;

	return ret;
}

/* [TEST] track1 */
static void transmit_track1_data(void)
{
	char *buf;

	pr_err("[MME] %s: Send test track1\n", __func__);

	buf = (char*)kzalloc(TRACK1_SIZE, GFP_KERNEL);
	memcpy(buf, track1, TRACK1_SIZE);

	if (f2f_ms_data_transmit(buf, TRACK1_FORMAT, TRACK1_SIZE)) {
		pr_err("[MME] %s: Failed to transmit card data\n", __func__);
		kfree(buf);
		return;
	}
	pr_err("[MME] %s: f2f_ms_data_transmit complete\n",__func__);
	kfree(buf);
}

/* [TEST] track2 */
static void transmit_track2_data(void)
{
    char *buf;

    pr_err("[MME] %s: Send test track2\n", __func__);

    buf = (char*)kzalloc(TRACK2_SIZE, GFP_KERNEL);
    memcpy(buf, track2, TRACK2_SIZE);

    if (f2f_ms_data_transmit(buf, TRACK2_FORMAT, TRACK2_SIZE)) {
        pr_err("[MME] %s: Failed to transmit card data\n", __func__);
        kfree(buf);
        return;
    }
    pr_err("[MME] %s: f2f_ms_data_transmit complete\n",__func__);
    kfree(buf);
}

/* [TEST] track3 */
static void transmit_track3_data(void)
{
    char *buf;

    pr_err("[MME] %s: Send static track2\n", __func__);

    buf = (char*)kzalloc(TRACK3_SIZE, GFP_KERNEL);
    memcpy(buf, track3, TRACK3_SIZE);

    if (f2f_ms_data_transmit(buf, TRACK3_FORMAT, TRACK3_SIZE)) {
        pr_err("[MME] %s: Failed to transmit card data\n", __func__);
        kfree(buf);
		return;
	}
	pr_err("[MME] %s: f2f_ms_data_transmit complete\n",__func__);
	kfree(buf);
}

/* [TEST] high signal for hw test */
static void mme_high_test(void)
{
	int i = 0;
	pr_err("[MME] mme_high_test\n");

	f2f_time = scale_duration;
	f2f_half_time = f2f_time/2;

//	memset(track_bit_buf, 1, COMBO_TRACK_TX_BIT_LENGTH);
	if(test_value == 0)
		memset(track_bit_buf, 0, COMBO_TRACK_TX_BIT_LENGTH);
	else if(test_value == 1)
		memset(track_bit_buf, 1, COMBO_TRACK_TX_BIT_LENGTH);
	else
		pr_err("[MME] Not suppported test_value(%d)", test_value);

	f2f_ms_enable();
	spin_lock_irq(&mme_lock);
	for (i = 0; i < COMBO_TRACK_TX_BIT_LENGTH; i++) {
		f2f_ms_pulse_transmit(track_bit_buf[i]);
	}
	spin_unlock_irq(&mme_lock);
	f2f_ms_disable();
}

/*
 * [mme_command] node read/write function
 */
static ssize_t lge_show_mme_command (struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", command_type);
}

static ssize_t lge_store_mme_command (struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &command_type);
	pr_err("[MME] %s: Start to send. command_type=%d\n", __func__, command_type);

	switch(command_type) {

		case CMD_MME_POWER_ON:
			pr_err("[MME] CMD_MME_POWER_ON\n");
			/* gpio_set_value(gpio_sleep_n, 1); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N1);
			break;

		case CMD_MME_POWER_OFF:
			pr_err("[MME] CMD_MME_POWER_OFF\n");
			/* gpio_set_value(gpio_sleep_n, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
			break;

		case CMD_MME_SEND_TRACK1_ONCE:
			pr_err("[MME] CMD_MME_SEND_TRACK1_ONCE\n");
			/* gpio_set_value(gpio_sleep_n, 1); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N1);
			transmit_track1_data();
			/* gpio_set_value(gpio_sleep_n, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
			break;

		case CMD_MME_SEND_TRACK2_ONCE:
			pr_err("[MME] CMD_MME_SEND_TRACK2_ONCE\n");
			/* gpio_set_value(gpio_sleep_n, 1); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N1);
			transmit_track2_data();
			/* gpio_set_value(gpio_sleep_n, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
			break;

		case CMD_MME_SEND_TRACK3_ONCE:
			pr_err("[MME] CMD_MME_SEND_TRACK3_ONCE\n");
			/* gpio_set_value(gpio_sleep_n, 1); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N1);
			transmit_track3_data();
			/* gpio_set_value(gpio_sleep_n, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
			break;

		case CMD_MME_TEST_START:
			pr_err("[MME] CMD_MME_TEST_START\n");

			mutex_lock(&repeat_mutex);
			if(is_repeat == 1) {
				pr_err("[MME] repeat command is already busy\n");
				mutex_unlock(&repeat_mutex);
				return -EBUSY;
			}
			else if(is_repeat == 0) {
#ifdef CONFIG_PM_WAKELOCKS
				__pm_stay_awake(&mme_wakelock);
#else
				wake_lock_init(&mme_wakelock, WAKE_LOCK_SUSPEND, "mme_wakelock");
				wake_lock(&mme_wakelock);
#endif
				is_repeat = 1;
			}
			else {
				pr_err("[MME] invalid value\n");
				mutex_unlock(&repeat_mutex);
				return -EINVAL;
			}
			mutex_unlock(&repeat_mutex);

			while (is_repeat == 1) {
				/* gpio_set_value(gpio_sleep_n, 1); */
				mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N1);
				mme_high_test();
				/* gpio_set_value(gpio_sleep_n, 0); */
				mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
			}
			break;

		case CMD_MME_SEND_STOP:
			pr_err("[MME] CMD_MME_SEND_STOP\n");
			mutex_lock(&repeat_mutex);

			if (is_repeat == 1)
#ifdef CONFIG_PM_WAKELOCKS
				__pm_relax(&mme_wakelock);
#else
				wake_lock_destroy(&mme_wakelock);
#endif
			is_repeat = 0;
			mutex_unlock(&repeat_mutex);
			break;

		default:
			pr_err("[MME] Not suppported cmd_id(%d)\n", command_type);
			return -EINVAL;
			break;
	}

	return count;
}

/*
 * [hw_test_value] node read/write function
 */
static ssize_t lge_show_hw_test_value (struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", test_value);
}

static ssize_t lge_store_hw_test_value (struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &test_value);
	pr_err("[MME] Set HW test_value(%d)", test_value);

	return count;
}

/*        sysfs		  name	  perm	   cat function			echo function   */
static DEVICE_ATTR(mme_command, 0664, lge_show_mme_command, lge_store_mme_command);
static DEVICE_ATTR(hw_test_value, 0664, lge_show_hw_test_value, lge_store_hw_test_value);

static struct attribute *lge_mme_attrs[] = {
	&dev_attr_mme_command.attr,
	&dev_attr_hw_test_value.attr,
	NULL
};

static const struct attribute_group lge_mme_files = {
	.attrs  = lge_mme_attrs,
};

static int mme_drv_gpio_init(struct device *dev)
{
        struct pinctrl *pinctrl = NULL;
        struct device_node *node = NULL;
        int ret;

        /* set up GPIO */
        pinctrl = devm_pinctrl_get(dev);
        if (IS_ERR(pinctrl)) {
                pr_err("%s::error, fail to get pinctrl\n", __func__);
                return -ENODEV;
        }

		mme_bias_pctrl = pinctrl;

		ret = mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
		if (ret)
			return ret;

		ret = mme_bias_gpio_select_state(MME_GPIO_STATE_PH0);
		if (ret)
			return ret;

		ret = mme_bias_gpio_select_state(MME_GPIO_STATE_EN0);
		if (ret)
			return ret;

        node = of_find_compatible_node(NULL, NULL, "lge,mme");
        if (of_property_read_u32_index(node, "lge,mme_sleep_n", 0, &gpio_sleep_n)) {
                pr_err("%s::error, fail to get lge,mme_sleep_n\n", __func__);
                return -ENODEV;
        }
        if (of_property_read_u32_index(node, "lge,mme_pn", 0, &gpio_ph)) {
                pr_err("%s::error, fail to get lge,mme_pn\n", __func__);
                return -ENODEV;
        }
        if (of_property_read_u32_index(node, "lge,mme_en", 0, &gpio_en)) {
                pr_err("%s::error, fail to get lge,mme_en\n", __func__);
                return -ENODEV;
        }
        pr_err("%s::gpio_sleep_n=%d, gpio_ph=%d, gpio_en=%d\n",__func__, gpio_sleep_n, gpio_ph, gpio_en);
        return 0;
}
static int __init lge_mme_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_err("[MME] %s: probe enter\n", __func__);

	/* TO BE : mme drv gpio init */
	ret = mme_drv_gpio_init(&pdev->dev);
	if (ret < 0 ) {
		pr_err("[MME] %s: gpio init failed, ret %d\n", __func__, ret);
		return ret;
	}

	mme_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(mme_class)) {
		pr_err("[MME] %s: class_create() failed ENOMEM\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	mme_dev = device_create(mme_class, NULL, MKDEV(mme_major, 0), NULL, "mme_ctrl");
	if (IS_ERR(mme_dev)) {
		pr_err("[MME] %s: device_create() failed\n", __func__);
		ret = PTR_ERR(mme_dev);
		goto exit;
	}

	/* create /sys/class/lge_mme/mme_ctrl/mme_command */
	ret = device_create_file(mme_dev, &dev_attr_mme_command);
	if (ret < 0) {
		pr_err("[MME] %s: device create file fail\n", __func__);
		goto exit;
	}

	/* create /sys/class/lge_mme/mme_ctrl/hw_test_value */
	ret = device_create_file(mme_dev, &dev_attr_hw_test_value);
	if (ret < 0) {
		pr_err("[MME] %s: device create file fail\n", __func__);
		goto exit;
	}

#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&mme_wakelock, "mme_wakelock");
#endif
	pr_info("[MME] %s: probe done\n", __func__);
	return 0;

exit:
	pr_err("[MME] %s: probe fail - %d\n", __func__, ret);
	return ret;
}

static int lge_mme_remove(struct platform_device *pdev)
{
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_trash(&mme_wakelock);
#endif
	return 0;
}

/* device driver structures */
static struct of_device_id mme_match_table[] = {
	{ .compatible = "lge,mme",},
	{},
};

static struct platform_driver lge_mme_driver __refdata = {
	.probe = lge_mme_probe,
	.remove = lge_mme_remove,
	.driver = {
		.name = "lge_mme_test",
		.owner = THIS_MODULE,
		.of_match_table = mme_match_table,
	},
};

/* driver init funcion */
static int __init lge_mme_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&lge_mme_driver);
	if (ret < 0)
		pr_err("[MME] %s : platform_driver_register() err=%d\n", __func__, ret);

	return ret;
}

/* driver exit function */
static void __exit lge_mme_exit(void)
{
	platform_driver_unregister(&lge_mme_driver);
}

module_init(lge_mme_init);
module_exit(lge_mme_exit);

MODULE_DESCRIPTION("LGE MME driver for LG pay test");
MODULE_AUTHOR("jinsol.jo <jinsol.jo@lge.com>");
MODULE_LICENSE("GPL");
