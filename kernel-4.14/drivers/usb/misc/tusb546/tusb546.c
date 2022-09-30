#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/async.h>
#include <linux/regulator/consumer.h>

#include "tusb546.h"
#include "tusb546_debugfs.h"

struct tusb546 *g_tusb546 = NULL;

#ifdef CONFIG_LGE_DUAL_SCREEN
extern bool is_ds_connected(void);
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
extern void fusb251_enable_moisture(bool);
#endif

bool is_tusb546_dp(void)
{
	struct i2c_client *i2c = g_tusb546->i2c;
	int val	= 0;

	if (g_tusb546 == NULL)	{
		pr_err("%s: redriver is not Ready\n", __func__);
		return false;
	}

	if (!atomic_read(&g_tusb546->pwr_on)) {
		pr_err("%s: power is off\n", __func__);
		return false;
	}

	val = tusb546_read_reg(i2c, GENERAL_REGISTER);
	if (val < 0) {
		pr_err("%s: fail to read GENERAL_REGISTER\n", __func__);
		return false;
	}

	if (val&0x02) {
		return true;
	}

	return false;
}

bool is_tusb546_pwr_on(void)
{
	if (atomic_read(&g_tusb546->pwr_on))
		return true;
	else
		return false;
}

void set_tusb546_off(void)
{
	tusb546_pwr_on(g_tusb546, 0);
}

void set_tusb546_rollback(void)
{
	if (g_tusb546->mode == LGE_TUSB_MODE_DISABLE) {
		pr_err("%s Need NOT: tusb is disabled.\n", __func__);
		return;
	}
	tusb546_update_cross_switch(g_tusb546->mode);
}

void tusb546_read_cross_switch(void)
{
	struct i2c_client *i2c = g_tusb546->i2c;
	int val	= 0;
	int val_ctlsel	= 0;

	if (g_tusb546 == NULL)	{
		pr_err("%s: redriver is not Ready\n", __func__);
		return;
	}

	if (!atomic_read(&g_tusb546->pwr_on)) {
		pr_err("%s: power is off\n", __func__);
		return;
	}

	val = tusb546_read_reg(i2c, GENERAL_REGISTER);
	if (val < 0) {
		pr_err("%s: fail to read GENERAL_REGISTER\n", __func__);
	} else {
		val_ctlsel = val & B_GR_CTLSEL;
		switch (val_ctlsel) {
		case 0:
			pr_info("%s CTLSEL - DISABLED\n",__func__);
			break;
		case 1:
			pr_info("%s CTLSEL - USB3.1 Only\n",__func__);
			break;
		case 2:
			pr_info("%s CTLSEL - DP - 4 Lane\n",__func__);
			break;
		case 3:
			pr_info("%s CTLSEL - DP - 2 Lane & 1 USB 3.1\n",__func__);
			break;
		default:
			pr_info("%s CTLSEL - Unknown\n",__func__);
			break;
		}
	}
}

void resetting_ds3_register()
{
	struct i2c_client *i2c = g_tusb546->i2c;
	pr_info("%s: : ++\n", __func__);
	__tusb546_write_reg(i2c, DISPLAY_CTRL_STS_4, 0x90);			// AUX Snoop Enabled
	pr_info("%s: : --\n", __func__);
}

void tusb546_update_cross_switch(int mode)
{
	const char *tusb_mode_name[] = {
		[LGE_TUSB_MODE_DISABLE] = "Disable",
		[LGE_TUSB_MODE_USB3_ONLY] = "Connect USB3.1 Only",
		[LGE_TUSB_MODE_DP_4_LANE] = "Connect DP - 4 Lane",
		[LGE_TUSB_MODE_DP_2_LANE] = "Connect DP - 2 Lane & One USB 3.1",
		[LGE_TUSB_MODE_USB3_ONLY_FLIP] = "Connect USB3.1 Only Flip",
		[LGE_TUSB_MODE_DP_4_LANE_FLIP] = "Connect DP - 4 Lane Flip" ,
		[LGE_TUSB_MODE_DP_2_LANE_FLIP] = "Connect DP - 2 Lane & One USB 3.1 Flip",
		[LGE_TUSB_MODE_MAX] = "Not action",
	};
	struct i2c_client *i2c = g_tusb546->i2c;
	int val	= 0;
	int dp_val	= 0;
	u8 dp_sts1_value = 0;
	u8 dp_sts2_value = 0;
	u8 ss_sts1_value = 0;
	u8 ss_sts2_value = 0;

	if (g_tusb546 == NULL)	{
		pr_err("%s: Not Ready\n", __func__);
		return;
	}

	if (!atomic_read(&g_tusb546->pwr_on)) {
		tusb546_pwr_on(g_tusb546, 1);
	}

	dp_sts1_value =	((g_tusb546->dp1eq_sel<<4) & B_DCS1_DP1EQ_SEL) | \
					(g_tusb546->dp0eq_sel & B_DCS1_DP0EQ_SEL);
	dp_sts2_value =	((g_tusb546->dp3eq_sel<<4) & B_DCS2_DP3EQ_SEL) | \
					(g_tusb546->dp2eq_sel & B_DCS2_DP2EQ_SEL);
	ss_sts1_value =	((g_tusb546->ssrx_eq2_sel<<4) & B_UCS1_EQ2_SEL) | \
					(g_tusb546->ssrx_eq1_sel & B_UCS1_EQ1_SEL);
	ss_sts2_value =	(g_tusb546->sstx_eq_sel & B_UCS2_SSEQ_SEL);

	/* Set INIT Value */
	tusb546_i2c_lock(i2c);
	__tusb546_write_reg(i2c, GENERAL_REGISTER, 0x18);			// EQ OVERRIDE & HPDIN_OVRRIDE ENABLED
	__tusb546_write_reg(i2c, DISPLAY_CTRL_STS_1, dp_sts1_value);	// EQ LEVEL DP Lane 0 & 1
	if ((mode == LGE_TUSB_MODE_DP_4_LANE) || (mode == LGE_TUSB_MODE_DP_4_LANE_FLIP)) {
		__tusb546_write_reg(i2c, DISPLAY_CTRL_STS_2, dp_sts2_value);	// EQ LEVEL DP Lane 2 & 3
	} else {
		__tusb546_write_reg(i2c, DISPLAY_CTRL_STS_2, 0x00);	// EQ LEVEL DP Lane 2 & 3
	}

#ifdef CONFIG_LGE_DUAL_SCREEN
	if (is_ds_connected()) {
		__tusb546_write_reg(i2c, DISPLAY_CTRL_STS_4, 0x9C);			// AUX Snoop Disabled
	} else {
		__tusb546_write_reg(i2c, DISPLAY_CTRL_STS_4, 0x00);			// AUX Snoop Enabled
	}
#else
	__tusb546_write_reg(i2c, DISPLAY_CTRL_STS_4, 0x00);			// AUX Snoop Enabled
#endif
	__tusb546_write_reg(i2c, USB3_CTRL_STS_1, ss_sts1_value);	// EQ LEVEL RX1, RX2
	__tusb546_write_reg(i2c, USB3_CTRL_STS_2, ss_sts2_value);	// EQ LEVEL SSTX
	tusb546_i2c_unlock(i2c);

	/* It must be work flip */
	val = tusb546_read_reg(i2c, GENERAL_REGISTER);
	if (val < 0) {
		pr_err("%s: fail to read GENERAL_REGISTER\n", __func__);
		return;
	}

	val = (val & ~(B_GR_CTLSEL|B_GR_FLIPSEL)) | mode;

	tusb546_i2c_lock(i2c);

	__tusb546_write_reg(i2c, GENERAL_REGISTER, (u8)val);

	tusb546_i2c_unlock(i2c);

	pr_info("tusb546: set mode to %s", tusb_mode_name[mode]);
	g_tusb546->mode	= mode;

	/* debug */
	val = tusb546_read_reg(i2c, GENERAL_REGISTER);
	dp_val	= tusb546_read_reg(i2c, DISPLAY_CTRL_STS_3);
	pr_info("tusb546: GE- 0x%02x    DP_STS3- 0x%02x \n", val, dp_val);

	/* Check this logic */
	if (mode == LGE_TUSB_MODE_DISABLE) {
		tusb546_pwr_on(g_tusb546, 0);
	}

#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
	if (mode&0x2)
		fusb251_enable_moisture(false);
	else
		fusb251_enable_moisture(true);
#endif
}

void tusb546_update_eq_val(int eq_num, u8 value)
{
	struct i2c_client *i2c = g_tusb546->i2c;
	u8 w_reg	= 0;
	u8 u_value	= 0;
	bool upper	= 0;

	if ((eq_num >= LGE_TUSB_EQ_MAX) || (value > 15)) {
		pr_err("%s could not update eq value!\n",__func__);
		return;
	}

	pr_err("%s - eq_num: %d  value: %d", __func__, eq_num, value);

	switch (eq_num) {
	case LGE_TUSB_DP0_EQ:
		g_tusb546->dp0eq_sel = value;
		w_reg	= DISPLAY_CTRL_STS_1;
		upper	= false;
		break;
	case LGE_TUSB_DP1_EQ:
		g_tusb546->dp1eq_sel = value;
		w_reg	= DISPLAY_CTRL_STS_1;
		upper	= true;
		break;
	case LGE_TUSB_DP2_EQ:
		g_tusb546->dp2eq_sel = value;
		w_reg	= DISPLAY_CTRL_STS_2;
		upper	= false;
		break;
	case LGE_TUSB_DP3_EQ:
		g_tusb546->dp3eq_sel = value;
		w_reg	= DISPLAY_CTRL_STS_2;
		upper	= true;
		break;
	case LGE_TUSB_SSRX1_EQ:
		g_tusb546->ssrx_eq1_sel = value;
		w_reg	= USB3_CTRL_STS_1;
		upper	= false;
		break;
	case LGE_TUSB_SSRX2_EQ:
		g_tusb546->ssrx_eq2_sel = value;
		w_reg	= USB3_CTRL_STS_1;
		upper	= true;
		break;
	case LGE_TUSB_SSTX_EQ:
		g_tusb546->sstx_eq_sel = value;
		w_reg	= USB3_CTRL_STS_2;
		upper	= false;
		break;
	default:
		break;
	}

	if (w_reg) {
		u_value	= tusb546_read_reg(i2c, w_reg);
		if (upper)
			u_value	= (u_value&0x0F)|((value<<4)&0xF0);
		else
			u_value	= (u_value&0xF0)|(value&0x0F);

		tusb546_i2c_lock(i2c);
		__tusb546_write_reg(i2c, w_reg, u_value);
		tusb546_i2c_unlock(i2c);
	}
}

void __tusb546_pwr_on(struct tusb546 *tusb)
{
	struct i2c_client *i2c = tusb->i2c;
	struct device *cdev = &i2c->dev;

	if (!tusb) {
		pr_err("%s is not ready!\n",__func__);
		return;
	}

	gpio_set_value(tusb->pwr_en_gpio, 1);
	tusb_dbg_event("PWR EN", 1);
	mdelay(10);

	atomic_set(&tusb->pwr_on, 1);
	dev_info_ratelimited(cdev, "tusb546 power on\n");
}

void __tusb546_pwr_down(struct tusb546 *tusb)
{
	struct device *cdev = &tusb->i2c->dev;

	if (!tusb) {
		pr_err("%s is not ready!\n",__func__);
		return;
	}

	atomic_set(&tusb->pwr_on, 0);
	dev_info_ratelimited(cdev, "tusb546 power down\n");

	gpio_set_value(tusb->pwr_en_gpio, 0);
	tusb_dbg_event("PWR EN", 0);
	mdelay(1);
}

int tusb546_pwr_on(struct tusb546 *tusb, int is_on)
{
	struct i2c_client *i2c = tusb->i2c;
	struct device *cdev = &i2c->dev;

	dev_info_ratelimited(cdev, "%s(%d)\n", __func__, is_on);

	if (atomic_read(&tusb->pwr_on) == is_on) {
		dev_dbg(cdev, "tusb546 power is already %s\n",
				is_on ? "on" : "down");
		return 0;
	}

	if (is_on) {
		__tusb546_pwr_on(tusb);

		tusb_dbg_event("INIT", 0);
	} else {
		__tusb546_pwr_down(tusb);
	}

	return 0;
}

static int tusb546_gpio_configure(struct tusb546 *tusb, bool on)
{
	struct device *dev = &tusb->i2c->dev;
	int rc = 0;

	if (!on) {
		goto gpio_free_all;
	}

	if (gpio_is_valid(tusb->pwr_en_gpio)) {
		/* configure tusb546 pwr_en gpio */
		rc = gpio_request(tusb->pwr_en_gpio, "tusb546_pwr_en_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				tusb->pwr_en_gpio);
			goto err_pwr_en_gpio_req;
		}
		rc = gpio_direction_output(tusb->pwr_en_gpio, 0);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				tusb->pwr_en_gpio);
			goto err_pwr_en_gpio_dir;
		}
	} else {
		dev_err(dev, "pwr_en gpio not provided\n");
		rc = -EINVAL;
		goto err_pwr_en_gpio_req;
	}

	return 0;

gpio_free_all:
err_pwr_en_gpio_dir:
	if (gpio_is_valid(tusb->pwr_en_gpio))
		gpio_free(tusb->pwr_en_gpio);
err_pwr_en_gpio_req:

	if (rc < 0)
		dev_err(dev, "gpio configure failed: rc=%d\n", rc);
	return rc;
}

#ifdef CONFIG_OF
static int tusb546_parse_dt(struct device *dev, struct tusb546 *tusb)
{
	struct device_node *np = dev->of_node;
	int ret	= 0;

	/* gpio */
	tusb->pwr_en_gpio = of_get_named_gpio(np, "tusb546,pwr-en", 0);

	/* DP EQ value Set */
	ret = of_property_read_u8(np,"dp0eq_sel",&tusb->dp0eq_sel);
	if (ret < 0) {
		tusb->dp0eq_sel = DEFAULT_DP_EQ_VALUE;
		pr_err("%s: Defaut DP0EQ VALUE %d\n", __func__, DEFAULT_DP_EQ_VALUE);
	}

	ret = of_property_read_u8(np,"dp1eq_sel",&tusb->dp1eq_sel);
	if (ret < 0) {
		tusb->dp1eq_sel = DEFAULT_DP_EQ_VALUE;
		pr_err("%s: Defaut DP1EQ VALUE%d\n", __func__, DEFAULT_DP_EQ_VALUE);
	}

	ret = of_property_read_u8(np,"dp2eq_sel",&tusb->dp2eq_sel);
	if (ret < 0) {
		tusb->dp2eq_sel = DEFAULT_DP_EQ_VALUE;
		pr_err("%s: Defaut DP2EQ VALUE%d\n", __func__, DEFAULT_DP_EQ_VALUE);
	}

	ret = of_property_read_u8(np,"dp3eq_sel",&tusb->dp3eq_sel);
	if (ret < 0) {
		tusb->dp3eq_sel = DEFAULT_DP_EQ_VALUE;
		pr_err("%s: Defaut DP3EQ VALUE%d\n", __func__, DEFAULT_DP_EQ_VALUE);
	}

	/* SS EQ value Set */
	ret = of_property_read_u8(np,"ssrx_eq1_sel",&tusb->ssrx_eq1_sel);
	if (ret < 0) {
		tusb->ssrx_eq1_sel = DEFAULT_SS_EQ_VALUE;
		pr_err("%s: Defaut SSRX1EQ VALUE", __func__, DEFAULT_SS_EQ_VALUE);
	}

	ret = of_property_read_u8(np,"ssrx_eq2_sel",&tusb->ssrx_eq2_sel);
	if (ret < 0) {
		tusb->ssrx_eq2_sel = DEFAULT_SS_EQ_VALUE;
		pr_err("%s: Defaut SSRX2EQ VALUE", __func__, DEFAULT_SS_EQ_VALUE);
	}

	ret = of_property_read_u8(np, "sstx_eq_sel",&tusb->sstx_eq_sel);
	if (ret < 0) {
		tusb->sstx_eq_sel = DEFAULT_SS_EQ_VALUE;
		pr_err("%s: Defaut SSTXEQ VALUE", __func__, DEFAULT_SS_EQ_VALUE);
	}

	return 0;
}
#else
static inline int tusb546_parse_dt(struct device *dev, struct tusb546 *tusb)
{
	return 0;
}
#endif

static int tusb546_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct tusb546 *tusb;
	int rc;

	pr_info("%s start\n", __func__);

	if (!i2c_check_functionality(i2c->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c_check_functionality failed\n", __func__);
		return -EIO;
	}

	if (i2c->dev.of_node) {
		tusb = devm_kzalloc(&i2c->dev, sizeof(struct tusb546), GFP_KERNEL);
		if (!tusb) {
			pr_err("%s: devm_kzalloc\n", __func__);
			return -ENOMEM;
		}

		rc = tusb546_parse_dt(&i2c->dev, tusb);
		if (rc) {
			dev_err(&i2c->dev, "parse dts fail(%d)\n");
			return rc;
		}
	} else {
		tusb = i2c->dev.platform_data;
	}

	if (!tusb) {
		dev_err(&i2c->dev,
				"%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	dev_set_drvdata(&i2c->dev, tusb);
	tusb->i2c = i2c;

	rc = tusb546_gpio_configure(tusb, true);
	if (rc) {
		dev_err(&i2c->dev, "gpio configure failed\n");
		goto err_gpio_config;
	}

	tusb546_debugfs_init(tusb);

	tusb->mode	= LGE_TUSB_MODE_DISABLE;

	g_tusb546 = tusb;

	pr_info("%s end\n", __func__);

	/* tusb546_pwr_on(g_tusb546, 1); */

	return 0;

err_gpio_config:
	return rc;
}

static int tusb546_remove(struct i2c_client *i2c)
{
	struct device *cdev = &i2c->dev;
	struct tusb546 *tusb = dev_get_drvdata(cdev);

	pr_info("%s\n", __func__);

	if (atomic_read(&tusb->pwr_on))
		__tusb546_pwr_down(tusb);

	tusb546_gpio_configure(tusb, false);

	return 0;
}


static const struct i2c_device_id tusb546_idtable[] = {
	{ "tusb546", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, tusb546_idtable);

#ifdef CONFIG_OF
static const struct of_device_id tusb546_of_match[] = {
	{ .compatible = "ti,tusb546" },
	{},
};
MODULE_DEVICE_TABLE(of, tusb546_of_match);
#endif

#ifdef CONFIG_PM_SLEEP
static int tusb546_pm_resume(struct device *dev)
{
	/* pr_info("%s\n", __func__); */

	#if	0
	if (!is_tusb546_pwr_on() && (g_tusb546->mode != LGE_TUSB_MODE_DISABLE))
		tusb546_update_cross_switch(g_tusb546->mode);
	#endif

	return 0;
}

static int tusb546_pm_suspend(struct device *dev)
{
	/* pr_info("%s\n", __func__); */

	if (is_tusb546_pwr_on())
		tusb546_pwr_on(g_tusb546, 0);

	return 0;
}

static const struct dev_pm_ops tusb_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tusb546_pm_suspend, tusb546_pm_resume)
};
#endif

static struct i2c_driver tusb546_driver = {
	.driver  = {
		.owner  = THIS_MODULE,
		.name  = "tusb546",
		.of_match_table = of_match_ptr(tusb546_of_match),
		#ifdef CONFIG_PM_SLEEP
		.pm		= &tusb_dev_pm_ops,
		#endif
	},
	.id_table = tusb546_idtable,
	.probe  = tusb546_probe,
	.remove = tusb546_remove,
};

static void tusb546_async_init(void *data, async_cookie_t cookie)
{
	int rc;

	pr_info("%s\n", __func__);

	rc = i2c_add_driver(&tusb546_driver);
	if (rc < 0)
		pr_err("%s: i2c_add_driver failed %d\n", __func__, rc);
}

static int __init tusb546_init(void)
{
	pr_info("%s\n", __func__);
	async_schedule(tusb546_async_init, NULL);
	return 0;
}

static void __exit tusb546_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&tusb546_driver);
}

module_init(tusb546_init);
module_exit(tusb546_exit);

MODULE_AUTHOR("hansun.lee@lge.com");
MODULE_DESCRIPTION("TUSB546 driver");
MODULE_LICENSE("GPL");
