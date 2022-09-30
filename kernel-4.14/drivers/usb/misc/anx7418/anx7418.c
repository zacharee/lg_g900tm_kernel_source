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

#include "anx7418.h"
#include "anx7418_firmware.h"
#include "anx7418_debugfs.h"

static int intf_irq_mask = 0xFF;

#define OCM_STARTUP_TIMEOUT 3200
struct anx7418 *g_anx7418 = NULL;

void anx7418_read_cross_switch(void)
{
	struct i2c_client *i2c = g_anx7418->i2c;
	int val;

	if (g_anx7418 == NULL)	{
		pr_err("%s: Not Ready\n", __func__);
		return;
	}

	val = anx7418_read_reg(i2c, ANALOG_CTRL_1);
	if (val < 0) {
		pr_err("%s: fail to read ANALOG_CTRL_1\n", __func__);
	} else {
		g_anx7418->ml_ss_path1 = val;
		if (val & BIT(0))
			pr_info("anx7418: ML0 - Rx1\n");
		if (val & BIT(1))
			pr_info("anx7418: ML0 - Rx0\n");
		if (val & BIT(4))
			pr_info("anx7418: SSRX - Rx1\n");
		if (val & BIT(5))
			pr_info("anx7418: SSRX - Rx0\n");
		if (val & BIT(6))
			pr_info("anx7418: ML1 - Tx1\n");
		if (val & BIT(7))
			pr_info("anx7418: ML1 - Tx0\n");
	}

	val = anx7418_read_reg(i2c, ANALOG_CTRL_5);
	if (val < 0) {
		pr_err("%s: fail to read ANALOG_CTRL_5\n", __func__);
	} else {
		g_anx7418->ml_ss_path2 = val;
		if (val & BIT(4))
			pr_info("anx7418: SSTX - Tx1\n");
		if (val & BIT(5))
			pr_info("anx7418: SSTX - Tx0\n");
	}

	val = anx7418_read_reg(i2c, ANALOG_CTRL_2);
	if (val < 0) {
		pr_err("%s: fail to read ANALOG_CTRL_2\n", __func__);
	} else {
		g_anx7418->edu_aux = val;

		if (val & BIT(4))
			pr_info("anx7418: aux_p connect SBU1\n");
		if (val & BIT(5))
			pr_err("anx7418: aux_n connect SBU2\n");
		if (val & BIT(6))
			pr_err("anx7418: aux_p connect SBU2\n");
		if (val & BIT(7))
			pr_err("anx7418: aux_n connect SBU1\n");
	}
}

void anx7418_update_cross_switch(int mode)
{
	const char *anx_mode_name[] = {
		[LGE_ANX_MODE_DISABLE] = "Disable",
		[LGE_ANX_MODE_DP1] = "Connect DP1",
		[LGE_ANX_MODE_DP2] = "Connect DP2",
		[LGE_ANX_MODE_SS1] = "Connect SS1",
		[LGE_ANX_MODE_SS2] = "Connect SS2",
#if defined(CONFIG_LGE_DUAL_SCREEN)
		[LGE_ANX_MODE_DS] = "Connect DS",
#endif
		[LGE_ANX_MODE_MAX] = "Not action",
	};
	struct i2c_client *i2c = g_anx7418->i2c;

	if (g_anx7418 == NULL)	{
		pr_err("%s: Not Ready\n", __func__);
		return;
	}

	switch(mode) {
	case LGE_ANX_MODE_DISABLE:
		g_anx7418->edu_aux &= 0x0F;
		g_anx7418->ml_ss_path1 &= 0x0C;
		g_anx7418->ml_ss_path2 &= 0x3F;
		break;

	case LGE_ANX_MODE_DP1:		/* Normal */
		g_anx7418->edu_aux = (g_anx7418->edu_aux & 0x0F) | 0x30;
		g_anx7418->ml_ss_path1 = (g_anx7418->ml_ss_path1 & 0x0C) | 0x61;
		g_anx7418->ml_ss_path2 = (g_anx7418->ml_ss_path2 & 0x3F) | 0x80;
		break;

	case LGE_ANX_MODE_DP2:		/* flip */
		g_anx7418->edu_aux = (g_anx7418->edu_aux & 0x0F) | 0xC0;
		g_anx7418->ml_ss_path1 = (g_anx7418->ml_ss_path1 & 0x0C) | 0x92;
		g_anx7418->ml_ss_path2 = (g_anx7418->ml_ss_path2 & 0x3F) | 0x40;
		break;

	case LGE_ANX_MODE_SS1:
		g_anx7418->edu_aux = g_anx7418->edu_aux & 0x0F;
		g_anx7418->ml_ss_path1 = (g_anx7418->ml_ss_path1 & 0x0C) | 0x20;
		g_anx7418->ml_ss_path2 = (g_anx7418->ml_ss_path2 & 0x3F) | 0x80;
		break;

	case LGE_ANX_MODE_SS2:
		g_anx7418->edu_aux = g_anx7418->edu_aux & 0x0F;
		g_anx7418->ml_ss_path1 = (g_anx7418->ml_ss_path1 & 0x0C) | 0x10;
		g_anx7418->ml_ss_path2 = (g_anx7418->ml_ss_path2 & 0x3F) | 0x40;
		break;
#if defined(CONFIG_LGE_DUAL_SCREEN)
	case LGE_ANX_MODE_DS:		/* using CC2 and aux reverse position */
		g_anx7418->edu_aux = (g_anx7418->edu_aux & 0x0F) | 0x30;
		g_anx7418->ml_ss_path1 = (g_anx7418->ml_ss_path1 & 0x0C) | 0x92;
		g_anx7418->ml_ss_path2 = (g_anx7418->ml_ss_path2 & 0x3F) | 0x40;
		break;

#endif
	case LGE_ANX_MODE_MAX:
		break;
	}

	if (!atomic_read(&g_anx7418->pwr_on)) {
		anx7418_pwr_on(g_anx7418, 1);
	}

	anx7418_i2c_lock(i2c);

	__anx7418_write_reg(i2c, ANALOG_CTRL_1, (u8)(g_anx7418->ml_ss_path1 & 0xFF));
	__anx7418_write_reg(i2c, ANALOG_CTRL_5, (u8)(g_anx7418->ml_ss_path2 & 0xFF));
	__anx7418_write_reg(i2c, ANALOG_CTRL_2, (u8)(g_anx7418->edu_aux & 0xFF));

	anx7418_i2c_unlock(i2c);

	pr_info("anx7418: anx set mode to %s", anx_mode_name[mode]);

	if (mode == LGE_ANX_MODE_DISABLE) {
		anx7418_pwr_on(g_anx7418, 0);
	}
}

int anx7418_reg_init(struct anx7418 *anx)
{
	struct i2c_client *i2c = anx->i2c;

	if (!anx->otp)
		return 0;

	anx7418_i2c_lock(i2c);

	/* Not use irq for cayman2 */
	#if	0
	/* Interface and Status Interrupt Mask. Offset : 0x17
	 * 0 : RECVD_MSG_INT_MASK
	 * 1 : Reserved
	 * 2 : VCONN_CHG_INT_MASK
	 * 3 : VBUS_CHG_INT_MASK
	 * 4 : CC_STATUS_CHG_INT_MASK
	 * 5 : DATA_ROLE_CHG_INT_MASK
	 * 6 : Reserved
	 * 7 : Reserved
	 */
	if (IS_INTF_IRQ_SUPPORT(anx)) {
		if (anx->rom_ver >= 0x12) {
			intf_irq_mask = 0x83;

			// AUTO-PD
			__anx7418_write_reg(i2c, MAX_VOLT_RDO, 0x5A);
			__anx7418_write_reg(i2c, MAX_POWER_SYSTEM, 0x24);
			__anx7418_write_reg(i2c, MIN_POWER_SYSTEM, 0x06);
			__anx7418_write_reg(i2c, FUNCTION_OPTION,
					__anx7418_read_reg(i2c, FUNCTION_OPTION) | AUTO_PD_EN);

			/*
			 * 1:0 : To control the timing between PS_RDY and
			 *       vbus on during PR_SWAP.
			 *       0x00: 50ms; 0x01: 100ms; 0x02: 150ms; 0x03: 200ms
			 * 3:2 : To control the timing between PS_RDY and
			 *       vbus off during PR_SWAP.
			 *       0x00: 50ms; 0x01: 100ms; 0x02: 150ms; 0x03: 200ms
			 * 5:4 : To control the timing between the first cc message
			 *       and vbus on.
			 *       0x00: 10ms; 0x01: 40ms; 0x02: 70ms; 0x03: 100ms
			 */
			__anx7418_write_reg(i2c, TIME_CONTROL, 0x18);

			// skip check vbus
			__anx7418_write_reg(i2c, 0x6E,
					__anx7418_read_reg(i2c, 0x6E) | 1);
		} else {
			intf_irq_mask = 0xC2;
		}

		__anx7418_write_reg(i2c, IRQ_INTF_MASK, intf_irq_mask);

	}
	#else
	__anx7418_write_reg(i2c, IRQ_INTF_MASK, intf_irq_mask);
	#endif

	anx7418_i2c_unlock(i2c);

	return 0;
}

int __anx7418_pwr_on(struct anx7418 *anx)
{
	struct i2c_client *i2c = anx->i2c;
	struct device *cdev = &i2c->dev;
	int i;
	int rc;

	/* Check AVDD33 check */
	if(!regulator_is_enabled(anx->avdd)) {
		regulator_enable(anx->avdd);
		pr_err("%s: Oops regulator must be enabled", __func__);
		mdelay(100);
	}

	gpio_set_value(anx->pwr_en_gpio, 1);
	anx_dbg_event("PWR EN", 1);
	mdelay(10);

	gpio_set_value(anx->resetn_gpio, 1);
	anx_dbg_event("RESETN", 1);
	mdelay(1);

	for (i = 0; i < OCM_STARTUP_TIMEOUT; i++) {
		rc = i2c_smbus_read_byte_data(i2c, TX_STATUS);
		if (rc < 0) {
			/* it seemed to be EEPROM. */
			mdelay(OCM_STARTUP_TIMEOUT);
			rc = i2c_smbus_read_byte_data(i2c, TX_STATUS);
			if (rc < 0) {
				i = OCM_STARTUP_TIMEOUT;
				break;
			}
		}

		if (rc > 0 && rc & OCM_STARTUP) {
			break;
		}

		mdelay(1);
	}
	anx_dbg_event("OCM STARTUP", rc);

	atomic_set(&anx->pwr_on, 1);
	dev_info_ratelimited(cdev, "anx7418 power on\n");

	return i >= OCM_STARTUP_TIMEOUT ? -EIO : 0;
 }

void __anx7418_pwr_down(struct anx7418 *anx)
{
	struct device *cdev = &anx->i2c->dev;

	atomic_set(&anx->pwr_on, 0);
	dev_info_ratelimited(cdev, "anx7418 power down\n");

	gpio_set_value(anx->resetn_gpio, 0);
	anx_dbg_event("RESETN", 0);
	mdelay(1);

	gpio_set_value(anx->pwr_en_gpio, 0);
	anx_dbg_event("PWR EN", 0);
	mdelay(1);
}

int anx7418_pwr_on(struct anx7418 *anx, int is_on)
{
	struct i2c_client *i2c = anx->i2c;
	struct device *cdev = &i2c->dev;

	dev_info_ratelimited(cdev, "%s(%d)\n", __func__, is_on);

	if (!is_on && anx->is_dbg_acc) {
		/* Need dbg access */
#if	0	//#ifdef CONFIG_LGE_USB_TYPE_C
		prop.intval = 1;
		rc = anx->batt_psy->set_property(anx->batt_psy,
				POWER_SUPPLY_PROP_DP_ALT_MODE, &prop);
		if (rc < 0)
			dev_err(cdev, "set_property(DP_ALT_MODE) error %d\n", rc);
#endif
		//gpio_set_value(anx->sbu_sel_gpio, 0);
		anx->is_dbg_acc = false;
	}

	down_write(&anx->rwsem);

	if (atomic_read(&anx->pwr_on) == is_on) {
		dev_dbg(cdev, "anx7418 power is already %s\n",
				is_on ? "on" : "down");
		up_write(&anx->rwsem);
		return 0;
	}

	if (is_on) {
		__anx7418_pwr_on(anx);

		anx_dbg_event("INIT START", 0);

		anx7418_reg_init(anx);

		anx_dbg_event("INIT DONE", 0);

	} else {
		__anx7418_pwr_down(anx);
	}

	up_write(&anx->rwsem);
	return 0;
}

static int firmware_update(struct anx7418 *anx)
{
	struct anx7418_firmware *fw;
	struct i2c_client *i2c = anx->i2c;
	struct device *cdev = &i2c->dev;
	ktime_t start_time;
	ktime_t diff;
	int rc;

	__anx7418_pwr_on(anx);

	anx->rom_ver = anx7418_read_reg(i2c, ANALOG_CTRL_3);
	dev_info(cdev, "rom ver: %02X\n", anx->rom_ver);

	/* Check EEPROM or OTP */
	rc = anx7418_read_reg(i2c, DEBUG_EE_0);
	anx->otp = (rc & R_EE_DEBUG_STATE) == 1 ? true : false;
	if (anx->rom_ver == 0x00)
		anx->otp = true;

	if (!anx->otp) {
		dev_err(cdev, "EEPROM update not suppored\n");
		rc = 0;
		goto err;
	}
	rc = 0;
	/* On cayman2, anx7418 is used for mux */
	goto block_firmware_update;

	fw = anx7418_firmware_alloc(anx);
	if (ZERO_OR_NULL_PTR(fw)) {
		dev_err(cdev, "anx7418_firmware_alloc failed\n");
		rc = -ENOMEM;
		goto err;
	}
	dev_info(cdev, "new ver: %02X\n", fw->ver);

	dev_info(cdev, "firmware update start\n");
	start_time = ktime_get();

	if (!anx7418_firmware_update_needed(fw, anx->rom_ver)) {
		dev_info(cdev, "firmware updata not needed\n");
		rc = 0;
#ifdef FIRMWARE_PROFILE
		if (anx->rom_ver != fw->ver)
#endif
		goto update_not_needed;
	}

	dev_info(cdev, "firmware open\n");
	rc = anx7418_firmware_open(fw);
#ifdef FIRMWARE_PROFILE
	if (anx->rom_ver == fw->ver) {
		dev_info(cdev, "update profile test\n");
	}
	else
#endif
	if (rc < 0) {
		if (rc == -EEXIST) {
			rc = 0;
			goto err_open;

		} else if (rc == -ENOSPC) {
			dev_err(cdev, "no space for update\n");
			rc = 0;
			goto err_open;
		}

		dev_err(cdev, "firmware open failed %d\n", rc);
		goto err_open;
	}


	dev_info(cdev, "firmware update\n");
#ifdef FIRMWARE_PROFILE
	if (anx->rom_ver == fw->ver)
		rc = anx7418_firmware_profile(fw);
	else
#endif
	rc = anx7418_firmware_update(fw);
	if (rc < 0)
		dev_err(cdev, "firmware update failed %d\n", rc);
	else
		anx->rom_ver = fw->ver;

	dev_info(cdev, "firmware release\n");
	anx7418_firmware_release(fw);

err_open:
update_not_needed:
	diff = ktime_sub(ktime_get(), start_time);
	dev_info(cdev, "firmware update end (%lld ms)\n", ktime_to_ms(diff));

	anx7418_firmware_free(fw);
err:
block_firmware_update:
	__anx7418_pwr_down(anx);
	return rc;
}

static int anx7418_gpio_configure(struct anx7418 *anx, bool on)
{
	struct device *dev = &anx->i2c->dev;
	int rc = 0;

	if (!on) {
		goto gpio_free_all;
	}

	if (gpio_is_valid(anx->pwr_en_gpio)) {
		/* configure anx7418 pwr_en gpio */
		rc = gpio_request(anx->pwr_en_gpio, "anx7418_pwr_en_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->pwr_en_gpio);
			goto err_pwr_en_gpio_req;
		}
		rc = gpio_direction_output(anx->pwr_en_gpio, 0);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->pwr_en_gpio);
			goto err_pwr_en_gpio_dir;
		}
	} else {
		dev_err(dev, "pwr_en gpio not provided\n");
		rc = -EINVAL;
		goto err_pwr_en_gpio_req;
	}

	if (gpio_is_valid(anx->resetn_gpio)) {
		/* configure anx7418 resetn gpio */
		rc = gpio_request(anx->resetn_gpio, "anx7418_resetn_gpio");
		if (rc) {
			dev_err(dev,
				"unable to request gpio[%d]\n",
				anx->resetn_gpio);
			goto err_pwr_en_gpio_dir;
		}
		rc = gpio_direction_output(anx->resetn_gpio, 0);
		if (rc) {
			dev_err(dev,
				"unable to set dir for gpio[%d]\n",
				anx->resetn_gpio);
			goto err_resetn_gpio_dir;
		}
	} else {
		dev_err(dev, "resetn gpio not provided\n");
		rc = -EINVAL;
		goto err_pwr_en_gpio_dir;
	}

	return 0;

gpio_free_all:
err_resetn_gpio_dir:
	if (gpio_is_valid(anx->resetn_gpio))
		gpio_free(anx->resetn_gpio);
err_pwr_en_gpio_dir:
	if (gpio_is_valid(anx->pwr_en_gpio))
		gpio_free(anx->pwr_en_gpio);
err_pwr_en_gpio_req:

	if (rc < 0)
		dev_err(dev, "gpio configure failed: rc=%d\n", rc);
	return rc;
}

#ifdef CONFIG_OF
static int anx7418_parse_dt(struct device *dev, struct anx7418 *anx)
{
	struct device_node *np = dev->of_node;

	/* gpio */
	anx->pwr_en_gpio = of_get_named_gpio(np, "anx7418,pwr-en", 0);
	pr_err("%s: pwr_en_gpio [%d]\n", __func__, anx->pwr_en_gpio);

	anx->resetn_gpio = of_get_named_gpio(np, "anx7418,resetn", 0);
	pr_err("%s: resetn_gpio [%d]\n", __func__, anx->resetn_gpio);

	return 0;
}
#else
static inline int anx7418_parse_dt(struct device *dev, struct anx7418 *anx)
{
	return 0;
}
#endif

static int anx7418_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct anx7418 *anx;
	int rc;

	pr_info("%s start\n", __func__);

	if (!i2c_check_functionality(i2c->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c_check_functionality failed\n", __func__);
		return -EIO;
	}

	if (i2c->dev.of_node) {
		anx = devm_kzalloc(&i2c->dev, sizeof(struct anx7418), GFP_KERNEL);
		if (!anx) {
			pr_err("%s: devm_kzalloc\n", __func__);
			return -ENOMEM;
		}

		rc = anx7418_parse_dt(&i2c->dev, anx);
		if (rc) {
			dev_err(&i2c->dev, "parse dts fail(%d)\n");
			return rc;
		}
	} else {
		anx = i2c->dev.platform_data;
	}

	if (!anx) {
		dev_err(&i2c->dev,
				"%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	anx->avdd = devm_regulator_get(&i2c->dev, "avdd");
	if (IS_ERR(anx->avdd)) {
		dev_err(&i2c->dev, "avdd regulator_get failed\n");
		return -EPROBE_DEFER;
	}

	rc = regulator_set_voltage(anx->avdd, 3300000, 3300000);
	if (rc) {
		dev_err(&i2c->dev,
			"Regulator set_vtg failed avdd rc=%d\n",
			rc);
		return rc;
	}

	rc = regulator_enable(anx->avdd);
	if (rc) {
		dev_err(&i2c->dev, "Unable to regulator enable\n");
		return rc;
	}

	dev_set_drvdata(&i2c->dev, anx);
	anx->i2c = i2c;

	anx->wq = alloc_workqueue("anx_wq",
			WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_CPU_INTENSIVE,
			3);
	if (!anx->wq) {
		dev_err(&i2c->dev, "unable to create workqueue anx_wq\n");
		return -ENOMEM;
	}
	init_rwsem(&anx->rwsem);

	rc = anx7418_gpio_configure(anx, true);
	if (rc) {
		dev_err(&i2c->dev, "gpio configure failed\n");
		goto err_gpio_config;
	}

	rc = firmware_update(anx);
	if (rc == -ENODEV)
		goto err_update;

	anx7418_debugfs_init(anx);

	g_anx7418 = anx;

	pr_info("%s end\n", __func__);

	/* anx7418_pwr_on(g_anx7418, 1); */

	return 0;

err_update:
	anx7418_gpio_configure(anx, false);
err_gpio_config:
	destroy_workqueue(anx->wq);
	return rc;
}

static int anx7418_remove(struct i2c_client *i2c)
{
	struct device *cdev = &i2c->dev;
	struct anx7418 *anx = dev_get_drvdata(cdev);

	pr_info("%s\n", __func__);

	if (atomic_read(&anx->pwr_on))
		__anx7418_pwr_down(anx);

	anx7418_gpio_configure(anx, false);

	return 0;
}

static const struct i2c_device_id anx7418_idtable[] = {
	{ "anx7418", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, anx7418_idtable);

#ifdef CONFIG_OF
static const struct of_device_id anx7418_of_match[] = {
	{ .compatible = "analogix,anx7418" },
	{},
};
MODULE_DEVICE_TABLE(of, anx7418_of_match);
#endif

static struct i2c_driver anx7418_driver = {
	.driver  = {
		.owner  = THIS_MODULE,
		.name  = "anx7418",
		.of_match_table = of_match_ptr(anx7418_of_match),
	},
	.id_table = anx7418_idtable,
	.probe  = anx7418_probe,
	.remove = anx7418_remove,
};

static void anx7418_async_init(void *data, async_cookie_t cookie)
{
	int rc;

	pr_info("%s\n", __func__);

	rc = i2c_add_driver(&anx7418_driver);
	if (rc < 0)
		pr_err("%s: i2c_add_driver failed %d\n", __func__, rc);
}

static int __init anx7418_init(void)
{
	pr_info("%s\n", __func__);
	async_schedule(anx7418_async_init, NULL);
	return 0;
}

static void __exit anx7418_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&anx7418_driver);
}

module_init(anx7418_init);
module_exit(anx7418_exit);

MODULE_AUTHOR("hansun.lee@lge.com");
MODULE_DESCRIPTION("ANX7418 driver");
MODULE_LICENSE("GPL");
