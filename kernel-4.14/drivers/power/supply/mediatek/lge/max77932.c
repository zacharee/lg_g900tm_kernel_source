#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <mtk_charger_intf.h>
#include <mtk_gauge_class.h>

/* MAX77932 Registers */
#define MAX77932_REG_INT_SRC (0x00)
#define MAX77932_REG_INT_SRC_M (0x01)
#define MAX77932_REG_STATUS (0x02)
#define MAX77932_REG_SCC_EN (0x03)
#define MAX77932_REG_SCC_CFG1 (0x04)
#define MAX77932_REG_SCC_CFG2 (0x05)
#define MAX77932_REG_OVP_UVLO (0x06)
#define MAX77932_REG_OCP1 (0x07)
#define MAX77932_REG_OCP2 (0x08)
#define MAX77932_REG_OOVP (0x09)
#define MAX77932_REG_SS_CFG (0x0A)
#define MAX77932_REG_EN_CFG1 (0x0B)
#define MAX77932_REG_EN_CFG2 (0x0C)
#define MAX77932_REG_I2C_CFG (0x14)
#define MAX77932_REG_CHIP_REV (0x15)
#define MAX77932_REG_DEVICE_ID (0x16)

/* MAX77932_REG_STATUS */
#define MAX77932_T_ALM1_MASK (0x08)
#define MAX77932_T_ALM2_MASK (0x04)
#define MAX77932_T_SHDN_MASK (0x02)

/* MAX77932_REG_SCC_EN */
#define MAX77932_SCC_EN_MASK (0x01)
#define MAX77932_SCC_EN_SHIFT (0)

/* MAX77932_REG_SCC_CFG1 */
#define MAX77932_FIX_FREQ_MASK (0x01)
#define MAX77932_FIX_FREQ_SHIFT (0)

/* MAX77932_REG_SCC_CFG2 */
#define MAX77932_DTHR_MASK (0x30)
#define MAX77932_DTHR_SHIFT (4)
#define MAX77932_FREQ_MASK (0x07)
#define MAX77932_FREQ_SHIFT (0)

/* MAX77932_REG_OVP_UVLO */
#define MAX77932_IOVP_R_MASK (0x30)
#define MAX77932_IOVP_R_SHIFT (4)
#define MAX77932_UVLO_F_MASK (0x03)
#define MAX77932_UVLO_F_SHIFT (0)

/* MAX77932_REG_OCP1 */
#define MAX77932_OCP1_MASK (0x1F)

/* MAX77932_REG_OCP2 */
#define MAX77932_OCP2_MASK (0x0F)

/* MAX77932_REG_OOVP */
#define MAX77932_OOVP_R_MASK (0x1F)

/* MAX77932_REG_DEVICE_ID */
#define MAX77932_DEVICE_ID (0x60)

enum {
	MAX77932_IRQ_SS_FLT,
	MAX77932_IRQ_T_SHDN,
	MAX77932_IRQ_T_ALM2,
	MAX77932_IRQ_T_ALM1,
	MAX77932_IRQ_OCP,
	MAX77932_IRQ_OC_ALM,
	MAX77932_IRQ_OOVP,
	MAX77932_IRQ_IOVP,
};

struct max77932 {
	struct regmap *regmap;
	struct device *dev;
	int irq_gpio;
	int irq;

	int freq;
	int uvlo;
	int ocp2;

	struct completion ss_fault;

	struct charger_device *chg_dev;
	struct charger_properties chg_prop;

	/* for report adc */
	struct charger_device *cdev;
	struct gauge_device *gdev;
};

static int max77932_get_status(struct max77932 *chip)
{
	unsigned int val = 0;
	int ret;

	ret = regmap_read(chip->regmap, MAX77932_REG_STATUS, &val);
	if (ret < 0)
		return 0;

	return val;
}

static int max77932_enable(struct max77932 *chip, bool en)
{
	unsigned int val = en ? MAX77932_SCC_EN_MASK : 0x00;

	return regmap_update_bits(chip->regmap, MAX77932_REG_SCC_EN,
				  MAX77932_SCC_EN_MASK, val);
}

static bool max77932_is_enabled(struct max77932 *chip)
{
	unsigned int val = 0;
	int ret;

	ret = regmap_read(chip->regmap, MAX77932_REG_SCC_EN, &val);
	if (ret < 0)
		return false;

	return (val & MAX77932_SCC_EN_MASK) ? true : false;
}

const static int max77932_freq[] = {
	250000, 500000, 750000, 1000000, 1200000, 1500000,
};

static int max77932_set_freq(struct max77932 *chip, int freq)
{
	unsigned int val;

	for (val = 0; val < ARRAY_SIZE(max77932_freq); val++) {
		if (freq <= max77932_freq[val])
			break;
	}

	return regmap_update_bits(chip->regmap, MAX77932_REG_SCC_CFG2,
				  MAX77932_FREQ_MASK, val);
}

static int max77932_set_uvlo(struct max77932 *chip, int uV)
{
	unsigned int val = 0;

	if (uV < 4100000)
		uV = 4100000;
	if (uV > 4700000)
		uV = 4700000;

	val = (uV - 4100000) / 200000;

	return regmap_update_bits(chip->regmap, MAX77932_REG_OVP_UVLO,
				  MAX77932_UVLO_F_MASK, val);
}

const static int max77932_iovp[] = {
	9500000, 1000000, 1050000, 1100000,
};

static int max77932_set_iovp(struct max77932 *chip, int uV)
{
	unsigned int val;

	for (val = 0; val < ARRAY_SIZE(max77932_iovp) - 1; val++) {
		if (uV <= max77932_iovp[val])
			break;
	}

	return regmap_update_bits(chip->regmap, MAX77932_REG_OVP_UVLO,
				  MAX77932_IOVP_R_MASK,
				  val << MAX77932_IOVP_R_SHIFT);
}

static int max77932_set_ocp1(struct max77932 *chip, int uA)
{
	unsigned int val;

	if (uA > 9600000)
		uA = 9600000;

	val = (uA - 4200000) / 200000;

	return regmap_update_bits(chip->regmap, MAX77932_REG_OCP1,
				  MAX77932_OCP1_MASK, val);
}

const static int max77932_ocp2[] = {
	110000, 120000, 130000, 140000, 150000, 160000, 170000, 180000,
	190000, 200000, 210000, 220000, 230000, 240000, 310000,
};

static int max77932_set_ocp2(struct max77932 *chip, int uV)
{
	unsigned int val = 0x0F;

	if (uV <= 0)
		goto out;

	for (val = 0; val < ARRAY_SIZE(max77932_ocp2); val++) {
		if (uV <= max77932_ocp2[val])
			break;
	}

out:
	return regmap_update_bits(chip->regmap, MAX77932_REG_OCP2,
				  MAX77932_OCP2_MASK, val);
}

const static int max77932_oovp[] = {
	4150000, 4175000, 4200000, 4225000, 4250000, 4275000, 4300000, 4325000,
	4350000, 4375000, 4400000, 4425000, 4450000, 4500000, 4600000, 4700000,
	4800000, 4900000, 5000000, 5100000, 5200000, 5300000, 5400000, 5500000,
};

static int max77932_set_oovp(struct max77932 *chip, int uV)
{
	unsigned int val;

	if (uV < 4150000)
		uV = 4150000;
	if (uV > 5500000)
		uV = 5500000;

	for (val = 0; val < ARRAY_SIZE(max77932_oovp); val++) {
		if (uV <= max77932_oovp[val])
			break;
	}

	return regmap_update_bits(chip->regmap, MAX77932_REG_OOVP,
				  MAX77932_OOVP_R_MASK, val);
}

static bool max77932_is_available(struct max77932 *chip)
{
	unsigned int val = 0;
	int ret;

	ret = regmap_read(chip->regmap, MAX77932_REG_DEVICE_ID, &val);
	if (ret < 0)
		return false;

	if (val != MAX77932_DEVICE_ID)
		return false;

	return true;
}

static struct charger_device *max77932_get_charger(struct max77932 *chip)
{
	if (!chip->cdev)
		chip->cdev = get_charger_by_name("primary_chg");

	return chip->cdev;
}

static struct gauge_device *max77932_get_gauge(struct max77932 *chip)
{
	if (!chip->gdev)
		chip->gdev = get_gauge_by_name("gauge");
	return chip->gdev;
}

static int max77932_enable_chg(struct charger_device *chg_dev, bool en)
{
	struct max77932 *chip = charger_get_data(chg_dev);
	long fault;
	int ret;

	if (!en)
		return max77932_enable(chip, false);

	ret = max77932_set_freq(chip, chip->freq);
	if (ret) {
		dev_err(chip->dev, "failed to set freq\n");
		return ret;
	}

	reinit_completion(&chip->ss_fault);
	ret = max77932_enable(chip, true);
	if (ret) {
		dev_err(chip->dev, "failed to enable\n");
		return ret;
	}

	/* wait for soft start fault */
	fault = wait_for_completion_interruptible_timeout(&chip->ss_fault,
			msecs_to_jiffies(130));
	if (fault) {
		dev_err(chip->dev, "soft start fault occured\n");

		/* soft start failed. disable scc */
		ret = max77932_enable(chip, false);
		if (ret)
			return ret;

		return -EFAULT;
	}

	/* recheck chip properly enabled */
	if (!max77932_is_enabled(chip))
		return -EAGAIN;

	return 0;
}

static int max77932_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
	struct max77932 *chip = charger_get_data(chg_dev);

	*en = max77932_is_enabled(chip);

	return 0;
}

static int max77932_get_adc(struct charger_device *chg_dev,
			    enum adc_channel chan, int *min, int *max)
{
	struct max77932 *chip = charger_get_data(chg_dev);
	struct charger_device *chg = max77932_get_charger(chip);
	struct gauge_device *fg = max77932_get_gauge(chip);
	u32 vbus, ibus;
 	bool is_charging;
 	int bat_voltage, bat_current;
 	int ret;

	if (!max77932_is_available(chip))
		return -ENODEV;

 	if (!chg || !fg)
 		return -ENODEV;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		ret = charger_dev_get_vbus(chg, &vbus);
		if (ret)
			return ret;
		*min = *max = vbus;
		break;
	case ADC_CHANNEL_VBAT:
		bat_voltage = battery_get_bat_voltage();
		*min = *max = bat_voltage * 1000;
		break;
	case ADC_CHANNEL_IBUS:
		ret = charger_dev_get_ibus(chg, &ibus);
		if (ret)
			return ret;
		*min = *max = ibus;
		break;
	case ADC_CHANNEL_IBAT:
		gauge_dev_get_current(fg, &is_charging, &bat_current);
		if (!is_charging)
			bat_current = 0;

		*min = *max = bat_current * 100;
		break;
	case ADC_CHANNEL_TEMP_JC:
		ret = max77932_get_status(chip);
		if (ret & MAX77932_T_SHDN_MASK)
			*min = *max = 155;
		else if (ret & MAX77932_T_ALM2_MASK)
			*min = *max = 120;
		else if (ret & MAX77932_T_ALM1_MASK)
			*min = *max = 100;
		else
			*min = *max = 25;
		break;
	case ADC_CHANNEL_VOUT:
		bat_voltage = battery_get_bat_voltage();
		*min = *max = bat_voltage * 1000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max77932_get_adc_accuracy(struct charger_device *chg_dev,
				     enum adc_channel chan, int *min, int *max)
{
	switch (chan) {
	case ADC_CHANNEL_VBUS:
		*min = *max = 75000; /* 75 mV */
		break;
	case ADC_CHANNEL_IBUS:
		*min = *max = 150000; /* 150 mA */
		break;
	default:
		*min = *max = 0;
		break;
	}

	return 0;
}

static int max77932_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
	struct max77932 *chip = charger_get_data(chg_dev);

	return max77932_set_iovp(chip, uV);
}

static int max77932_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
	struct max77932 *chip = charger_get_data(chg_dev);
	struct charger_device *chg = max77932_get_charger(chip);

	if (!chg)
		return -ENODEV;

	return charger_dev_set_input_current(chg, uA);
}

static int max77932_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
	struct max77932 *chip = charger_get_data(chg_dev);

	return max77932_set_oovp(chip, uV);
}

static int max77932_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	/* not support */
	return 0;
}

static int max77932_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
	/* not support */
	return 0;
}

static int max77932_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	/* not support */
	return 0;
}

static int max77932_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
	/* not support */
	return 0;
}

static int max77932_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
	struct max77932 *chip = charger_get_data(chg_dev);
	int ret;

	/* update UVLO */
	ret = max77932_set_uvlo(chip, chip->uvlo);
	if (ret)
		return ret;

	*err = max77932_is_available(chip) ? false : true;

	return 0;
}

static int max77932_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
	struct max77932 *chip = charger_get_data(chg_dev);

	if (max77932_set_ocp2(chip, chip->ocp2))
		dev_err(chip->dev, "failed to set ocp2\n");

	return max77932_set_ocp1(chip, uA);
}

static const struct charger_ops max77932_ops = {
	.enable_direct_charging = max77932_enable_chg,
	.is_enabled = max77932_is_chg_enabled,
	.get_adc = max77932_get_adc,
	.set_direct_charging_vbusov = max77932_set_vbusovp,
	.set_direct_charging_ibusoc = max77932_set_ibusocp,
	.set_direct_charging_vbatov = max77932_set_vbatovp,
	.set_direct_charging_ibatoc = max77932_set_ibatocp,
	.set_direct_charging_vbatov_alarm = max77932_set_vbatovp_alarm,
	.reset_direct_charging_vbatov_alarm = max77932_reset_vbatovp_alarm,
	.set_direct_charging_vbusov_alarm = max77932_set_vbusovp_alarm,
	.reset_direct_charging_vbusov_alarm = max77932_reset_vbusovp_alarm,
	.is_direct_charging_vbuslowerr = max77932_is_vbuslowerr,
	.get_adc_accuracy = max77932_get_adc_accuracy,
};

static int max77932_init_chgdev(struct max77932 *chip)
{
	chip->chg_prop.alias_name = "max77932";
	chip->chg_dev = charger_device_register("primary_divider_chg",
						chip->dev, chip, &max77932_ops,
						&chip->chg_prop);
	if (!chip->chg_dev)
		return -EINVAL;

	return 0;
}

static void max77932_irq_ss_flt(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
	complete(&chip->ss_fault);
}

static void max77932_irq_t_shdn(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
}

static void max77932_irq_t_alm2(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
}

static void max77932_irq_t_alm1(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
}

static void max77932_irq_ocp(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_IBATOCP);
}

static void max77932_irq_oc_alm(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
}

static void max77932_irq_oovp(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VOUTOVP);
}

static void max77932_irq_iovp(struct max77932 *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
}

static void (*max77932_irq_handlers[])(struct max77932 *chip) = {
	[MAX77932_IRQ_SS_FLT] = max77932_irq_ss_flt,
	[MAX77932_IRQ_T_SHDN] = max77932_irq_t_shdn,
	[MAX77932_IRQ_T_ALM2] = max77932_irq_t_alm2,
	[MAX77932_IRQ_T_ALM1] = max77932_irq_t_alm1,
	[MAX77932_IRQ_OCP] = max77932_irq_ocp,
	[MAX77932_IRQ_OC_ALM] = max77932_irq_oc_alm,
	[MAX77932_IRQ_OOVP] = max77932_irq_oovp,
	[MAX77932_IRQ_IOVP] = max77932_irq_iovp,
};

static irqreturn_t max77932_irq_handler(int irq, void *data)
{
	struct max77932 *chip = data;
	unsigned int val;
	unsigned int i;
	int ret;

	ret = regmap_read(chip->regmap, MAX77932_REG_INT_SRC, &val);
	if (ret < 0) {
		dev_err(chip->dev, "faild to read irq\n");
		return IRQ_HANDLED;
	}

	dev_info(chip->dev, "%s: int_src = 0x%02x\n", __func__, val);

	if (!max77932_is_enabled(chip))
		return IRQ_HANDLED;

	for (i = 0; i < ARRAY_SIZE(max77932_irq_handlers); i++) {
		if (!max77932_irq_handlers[i])
			continue;
		if (BIT(i) & val)
			max77932_irq_handlers[i](chip);
	}

	return IRQ_HANDLED;
}

static int max77932_init_irq(struct max77932 *chip)
{
	int ret;

	ret = devm_gpio_request_one(chip->dev, chip->irq_gpio, GPIOF_DIR_IN,
				    "max77932_irq");
	if (ret) {
		dev_err(chip->dev, "failed to request gpio, %d\n", ret);
		return ret;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq < 0)
		return -ENODEV;

	ret = devm_request_threaded_irq(chip->dev, gpio_to_irq(chip->irq_gpio),
					NULL, max77932_irq_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"max77932_irq", chip);
	if (ret) {
		dev_err(chip->dev, "failed to request irq, %d\n", ret);
		return ret;
	}

	return 0;
}

static int max77932_parse_dt(struct max77932 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret;

	chip->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (chip->irq_gpio < 0)
		return -EINVAL;

	ret = of_property_read_u32(np, "freq", &chip->freq);
	if (ret)
		chip->freq = 250000;

	ret = of_property_read_u32(np, "uvlo", &chip->uvlo);
	if (ret)
		chip->uvlo = 4100000;

	ret = of_property_read_u32(np, "ocp2", &chip->ocp2);
	if (ret)
		chip->ocp2 = 0;

	return 0;
}

static const struct regmap_range max77932_readable_ranges[] = {
	regmap_reg_range(MAX77932_REG_INT_SRC, MAX77932_REG_EN_CFG2),
	regmap_reg_range(MAX77932_REG_I2C_CFG, MAX77932_REG_DEVICE_ID),
};

static const struct regmap_access_table max77932_readable_table = {
	.yes_ranges = max77932_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(max77932_readable_ranges),
};

static const struct regmap_range max77932_writable_ranges[] = {
	regmap_reg_range(MAX77932_REG_INT_SRC_M, MAX77932_REG_INT_SRC_M),
	regmap_reg_range(MAX77932_REG_SCC_EN, MAX77932_REG_EN_CFG2),
};

static const struct regmap_access_table max77932_writable_table = {
	.yes_ranges = max77932_writable_ranges,
	.n_yes_ranges = ARRAY_SIZE(max77932_writable_ranges),
};

static const struct regmap_config max77932_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX77932_REG_DEVICE_ID,
	.rd_table = &max77932_readable_table,
	.wr_table = &max77932_writable_table,
};

static int max77932_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct max77932 *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	i2c_set_clientdata(client, chip);
	chip->dev = &client->dev;
	init_completion(&chip->ss_fault);

	chip->regmap = devm_regmap_init_i2c(client, &max77932_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(chip->dev, "failed to initialize regmap: %d\n", ret);
		return ret;
	}

	ret = max77932_parse_dt(chip);
	if (ret) {
		dev_err(chip->dev, "failed to parse dt: %d\n", ret);
		return ret;
	}

	ret = max77932_init_irq(chip);
	if (ret) {
		dev_err(chip->dev, "failed to init irq: %d\n", ret);
		return ret;
	}

	ret = max77932_init_chgdev(chip);
	if (ret) {
		dev_err(chip->dev, "failed to init chgdev: %d\n", ret);
		return ret;
	}

	return ret;
}

static int max77932_remove(struct i2c_client *client)
{
	struct max77932 *chip = i2c_get_clientdata(client);

	charger_device_unregister(chip->chg_dev);

	return 0;
}

static void max77932_shutdown(struct i2c_client *client)
{
	return;
}

static const struct i2c_device_id max77932_i2c_id[] = {
	{ "max77932" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77932_i2c_id);

static const struct of_device_id max77932_of_match[] = {
	{ .compatible = "maxim,max77932", },
	{ },
};
MODULE_DEVICE_TABLE(of, max77932_of_match);

static struct i2c_driver max77932_driver = {
	.probe = max77932_probe,
	.remove = max77932_remove,
	.shutdown = max77932_shutdown,
	.driver = {
		.name = "max77932",
		.of_match_table = max77932_of_match,
	},
	.id_table = max77932_i2c_id,
};
module_i2c_driver(max77932_driver);

MODULE_AUTHOR("Dajin Kim <dajin.kim@lge.com>");
MODULE_DESCRIPTION("MAX77932 Dual Phase Switched Capacitor Converter");
MODULE_LICENSE("GPL v2");
