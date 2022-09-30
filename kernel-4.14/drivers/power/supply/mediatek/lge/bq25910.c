/*
* bq25910.c -- Texas Instruments bq25910 slave charger driver
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/power_supply.h>
#include <mtk_charger_intf.h>

/* Battery Voltage Limit Register */
#define REG00			(0x00)
#define VREG			(0xFF)
#define VREG_SHIFT		(0)
/* Charge Current Limit Register */
#define REG01			(0x01)
#define ICHG			(0x7F)
#define ICHG_SHIFT		(0)
/* Input Voltage Limit Register */
#define REG02			(0x02)
#define VINDPM			(0x7F)
#define VINDPM_SHIFT		(0)
/* Input Current Limit Register */
#define REG03			(0x03)
#define INDPM			(0x3F)
#define INDPM_SHIFT		(0)
/* Reserved Register */
#define REG04			(0x04)
/* Charger Control 1 Register */
#define REG05			(0x05)
#define EN_TERM			(0x80)
#define EN_TERM_SHIFT		(7)
#define WD_RST			(0x40)
#define WD_RST_SHIFT		(6)
#define WATCHDOG		(0x30)
#define WATCHDOG_SHIFT		(4)
#define EN_TIMER		(0x08)
#define EN_TIMER_SHIFT		(3)
#define CHG_TIMER		(0x06)
#define CHG_TIMER_SHIFT		(1)
#define TMR2X_EN		(0x01)
#define TMR2X_EN_SHIFT		(0)
/* Charger Control 2 Register */
#define REG06			(0x06)
#define TREG			(0x30)
#define TREG_SHIFT		(4)
#define EN_CHG			(0x08)
#define EN_CHG_SHIFT		(3)
#define VBATLOWV		(0x03)
#define VBATLOWV_SHIFT		(0)
/* INT Status Register */
#define REG07			(0x07)
#define PG_STAT			(0x80)
#define PG_STAT_SHIFT		(7)
#define INDPM_STAT		(0x40)
#define INDPM_STAT_SHIFT	(6)
#define VINDPM_STAT		(0x20)
#define VINDPM_STAT_SHIFT	(5)
#define TREG_STAT		(0x10)
#define TREG_STAT_SHIFT		(4)
#define WD_STAT			(0x08)
#define WD_STAT_SHIFT		(3)
#define CHRG_STAT		(0x07)
#define CHRG_STAT_SHIFT		(0)
/* FAULT Status Register */
#define REG08			(0x08)
#define VBUS_OVP_STAT		(0x80)
#define VBUS_OVP_STAT_SHIFT	(7)
#define TSHUT_STAT		(0x40)
#define TSHUT_STAT_SHIFT	(6)
#define BATOVP_STAT		(0x20)
#define BATOVP_STAT_SHIFT	(5)
#define CFLY_STAT		(0x10)
#define CFLY_STAT_SHIFT		(4)
#define CAP_COND_STAT		(0x04)
#define CAP_COND_STAT_SHIFT	(2)
#define POORSRC_STAT		(0x02)
#define POORSRC_STAT_SHIFT	(1)
/* INT Flag Register */
#define REG09			(0x09)
#define PG_FLAG			(0x80)
#define PG_FLAG_SHIFT		(7)
#define INDPM_FLAG		(0x40)
#define INDPM_FLAG_SHIFT	(6)
#define VINDPM_FLAG		(0x20)
#define VINDPM_FLAG_SHIFT	(5)
#define TREG_FLAG		(0x10)
#define TREG_FLAG_SHIFT		(4)
#define WD_FLAG			(0x08)
#define WD_FLAG_SHIFT		(3)
#define CHRG_TERM_FLAG		(0x04)
#define CHRG_TERM_FLAG_SHIFT	(2)
#define CHRG_FLAG		(0x01)
#define CHRG_FLAG_SHIFT		(0)
/* FAULT Flag Register */
#define REG0A			(0x0A)
#define VBUS_OVP_FLAG		(0x80)
#define VBUS_OVP_FLAG_SHIFT	(7)
#define TSHUT_FLAG		(0x40)
#define TSHUT_FLAG_SHIFT	(6)
#define BATOVP_FLAG		(0x20)
#define BATOVP_FLAG_SHIFT	(5)
#define CFLY_FLAG		(0x10)
#define CFLY_FLAG_SHIFT		(4)
#define CAP_COND_FLAG		(0x04)
#define CAP_COND_FLAG_SHIFT	(2)
#define POORSRC_FLAG		(0x02)
#define POORSRC_FLAG_SHIFT	(1)
/* INT Mask Register */
#define REG0B			(0x0B)
#define PG_MASK			(0x80)
#define PG_MASK_SHIFT		(7)
#define INDPM_MASK		(0x40)
#define INDPM_MASK_SHIFT	(6)
#define VINDPM_MASK		(0x20)
#define VINDPM_MASK_SHIFT	(5)
#define TREG_MASK		(0x10)
#define TREG_MASK_SHIFT		(4)
#define WD_MASK			(0x08)
#define WD_MASK_SHIFT		(3)
#define CHRG_TERM_MASK		(0x04)
#define CHRG_TERM_MASK_SHIFT	(2)
#define CHRG_MASK		(0x01)
#define CHRG_MASK_SHIFT		(0)
/* FAULT Mask Register */
#define REG0C			(0x0C)
#define VBUS_OVP_MASK		(0x80)
#define VBUS_OVP_MASK_SHIFT	(7)
#define TSHUT_MASK		(0x40)
#define TSHUT_MASK_SHIFT	(6)
#define BATOVP_MASK		(0x20)
#define BATOVP_MASK_SHIFT	(5)
#define CFLY_MASK		(0x10)
#define CFLY_MASK_SHIFT		(4)
#define CAP_COND_MASK		(0x04)
#define CAP_COND_MASK_SHIFT	(2)
#define POORSRC_MASK		(0x02)
#define POORSRC_MASK_SHIFT	(1)
/* Part Information Register */
#define REG0D			(0x0D)
#define REG_RST			(0x80)
#define REG_RST_SHIFT		(7)
#define PN			(0x78)
#define PN_SHIFT		(3)
#define DEV_REV			(0x07)
#define DEV_REV_SHIFT		(0)

const static int regs_to_dump[] = {
	REG00, REG01, REG02, REG03, REG04,
	REG05, REG06, REG07, REG08, REG09,
	REG0A, REG0B, REG0C, REG0D,
};

struct bq25910 {
	struct i2c_client *client;
	struct device *dev;

	/* charger class */
	struct charger_device *chg_dev;
	struct charger_properties chg_props;

	struct mutex lock;

	/* device tree */
	bool is_polling_mode;
	int irq_gpio;
	int vindpm;
	bool en_term;
	bool en_timer;
	int chg_timer;

	/* debugfs */
	struct dentry *debugfs;
	u32 debug_addr;
};

static int bq25910_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int rc;

	rc = i2c_smbus_read_byte_data(client, reg);
	if (rc < 0)
		dev_err(&client->dev, "failed to read. rc=%d\n", rc);

	*data = (rc & 0xFF);

	return rc < 0 ? rc : 0;
}

static int bq25910_write(struct i2c_client *client, u8 reg, u8 data)
{
	int rc;

	rc = i2c_smbus_write_byte_data(client, reg, data);
	if (rc < 0)
		dev_err(&client->dev, "failed to write. rc=%d\n", rc);

	return rc;
}

static int bq25910_masked_write(struct i2c_client *client, u8 reg, u8 data, u8 mask)
{
	struct bq25910 *chip = i2c_get_clientdata(client);
	u8 tmp;
	int rc = 0;

	mutex_lock(&chip->lock);

	rc = bq25910_read(client, reg, &tmp);
	if (rc < 0)
		goto out;

	tmp = (data & mask) | (tmp & (~mask));
	rc = bq25910_write(client, reg, tmp);

out:
	mutex_unlock(&chip->lock);
	return rc;
}

#define VREG_DEFAULT (4350000U)
#define VREG_MIN (3500000U)
#define VREG_MAX (4775000U)
#define VREG_STEP (5000U)
static int bq25910_get_vreg(struct bq25910 *chip, int *uV)
{
	u8 data;
	int rc;

	rc = bq25910_read(chip->client, REG00, &data);
	if (rc) {
		*uV = VREG_DEFAULT;
		return rc;
	}

	*uV = VREG_MIN + (data * VREG_STEP);

	return 0;
}

static int bq25910_set_vreg(struct bq25910 *chip, int uV)
{
	u8 data;

	if (uV < VREG_MAX)
		uV = VREG_MIN;
	if (uV > VREG_MAX)
		uV = VREG_MAX;

	data = (uV - VREG_MIN) / VREG_STEP;

	return bq25910_write(chip->client, REG00, data);
}

#define ICHG_DEFAULT (3500000U)
#define ICHG_MIN (300000U)
#define ICHG_MAX (6000000U)
#define ICHG_STEP (50000U)
static int bq25910_get_ichg(struct bq25910 *chip, int *uA)
{
	u32 ichg;
	u8 data;
	int rc;

	rc = bq25910_read(chip->client, REG01, &data);
	if (rc) {
		*uA = ICHG_DEFAULT;
		return rc;
	}

	ichg = (data & ICHG) * ICHG_STEP;
	if (ichg < ICHG_MIN)
		ichg = 0;
	if (ichg > ICHG_MAX)
		ichg = ICHG_MAX;

	*uA = ichg;

	return 0;
}

static int bq25910_set_ichg(struct bq25910 *chip, int uA)
{
	u8 data;

	if (uA < ICHG_MIN)
		uA = 0;
	if (uA > ICHG_MAX)
		uA = ICHG_MAX;

	data = uA / ICHG_STEP;

	return bq25910_write(chip->client, REG01, data);
}

#define VINDPM_DEFAULT (4300000U)
#define VINDPM_MIN (3900000U)
#define VINDPM_MAX (14000000U)
#define VINDPM_STEP (100000U)
static int bq25910_set_vindpm(struct bq25910 *chip, int uV)
{
	u8 data;

	if (uV < VINDPM_MIN)
		uV = VINDPM_MIN;
	if (uV > VINDPM_MAX)
		uV = VINDPM_MAX;

	data = (uV - VINDPM_MIN) / VINDPM_STEP;

	return bq25910_write(chip->client, REG02, data);
}

#define INDPM_DEFAULT (2400000U)
#define INDPM_MIN (500000U)
#define INDPM_MAX (3600000U)
#define INDPM_STEP (100000U)
static int bq25910_get_iindpm(struct bq25910 *chip, int *uA)
{
	u32 indpm;
	u8 data;
	int rc;

	rc = bq25910_read(chip->client, REG03, &data);
	if (rc) {
		*uA = INDPM_DEFAULT;
		return rc;
	}

	indpm = INDPM_MIN + (data & INDPM) * INDPM_STEP;
	if (indpm > INDPM_MAX)
		indpm = INDPM_MAX;

	*uA = indpm;

	return 0;
}

static int bq25910_set_iindpm(struct bq25910 *chip, int uA)
{
	u8 data;

	if (uA < INDPM_MIN)
		uA = INDPM_MIN;
	if (uA > INDPM_MAX)
		uA = INDPM_MAX;

	data = (uA - INDPM_MIN) / INDPM_STEP;

	return bq25910_write(chip->client, REG03, data);
}

static int bq25910_set_en_term(struct bq25910 *chip, bool en)
{
	u8 data = (en ? 1 : 0);

	data <<= EN_TERM_SHIFT;

	return bq25910_masked_write(chip->client, REG05, data, EN_TERM);
}

static int bq25910_get_en_timer(struct bq25910 *chip, bool *en)
{
	u8 data;
	int rc;

	rc = bq25910_read(chip->client, REG05, &data);
	if (rc) {
		*en = true;
		return rc;
	}

	data = (data & EN_TIMER) >> EN_TIMER_SHIFT;
	*en = (data ? true : false);

	return 0;
}

static int bq25910_set_en_timer(struct bq25910 *chip, bool en)
{
	u8 data = (en ? 1 : 0);

	data <<= EN_TIMER_SHIFT;

	return bq25910_masked_write(chip->client, REG05, data, EN_TIMER);
}

static int bq25910_set_chg_timer(struct bq25910 *chip, int hour)
{
	u8 data;

	if (hour >= 20)
		data = 3;
	else if (hour >= 12)
		data = 2;
	else if (hour >= 8)
		data = 1;
	else
		data = 0;

	data <<= CHG_TIMER_SHIFT;

	return bq25910_masked_write(chip->client, REG05, data, CHG_TIMER);
}

static int bq25910_get_en_chg(struct bq25910 *chip, bool *en)
{
	u8 data;
	int rc;

	rc = bq25910_read(chip->client, REG06, &data);
	if (rc) {
		*en = true;
		return rc;
	}

	data = (data & EN_CHG) >> EN_CHG_SHIFT;
	*en = (data ? true : false);

	return 0;
}

static int bq25910_set_en_chg(struct bq25910 *chip, bool en)
{
	u8 data = (en ? 1 : 0);

	data <<= EN_CHG_SHIFT;

	return bq25910_masked_write(chip->client, REG06, data, EN_CHG);
}

static irqreturn_t bq25910_irq_handler(int irq, void *data)
{
	struct bq25910 *chip = data;
	u8 int_flag = 0;
	u8 fault_flag = 0;
	int rc;

	dev_info(chip->dev, "bq25910_irq_handler\n");

	rc = bq25910_read(chip->client, REG09, &int_flag);
	if (rc)
		dev_err(chip->dev, "failed to read int flag\n");
	rc = bq25910_read(chip->client, REG0A, &fault_flag);
	if (rc)
		dev_err(chip->dev, "failed to read fault flag\n");

	dev_info(chip->dev, "int=0x%x, fault=0x%x\n", int_flag, fault_flag);

	if (IS_ERR_OR_NULL(chip->chg_dev)) {
		dev_warn(chip->dev, "not registered as charger device.\n");
		return IRQ_HANDLED;
	}

	if (chip->chg_dev->is_polling_mode) {
		dev_info(chip->dev, "polling mode. ignore irq\n");
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

/* charger class interface */
static int bq25910_enable_charging(struct charger_device *chg_dev, bool en)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_set_en_chg(chip, en);
}

static int bq25910_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_get_en_chg(chip, en);
}

static int bq25910_get_charging_current(struct charger_device *chg_dev, u32 *uA)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_get_ichg(chip, uA);
}

static int bq25910_set_charging_current(struct charger_device *chg_dev, u32 uA)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_set_ichg(chip, uA);
}

static int bq25910_get_min_charging_current(struct charger_device *chg_dev, u32 *uA)
{
	*uA = ICHG_MIN;

	return 0;
}

static int bq25910_get_constant_voltage(struct charger_device *chg_dev, u32 *uV)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_get_vreg(chip, uV);
}

static int bq25910_set_constant_voltage(struct charger_device *chg_dev, u32 uV)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_set_vreg(chip, uV);
}

static int bq25910_get_input_current(struct charger_device *chg_dev, u32 *uA)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_get_iindpm(chip, uA);
}

static int bq25910_set_input_current(struct charger_device *chg_dev, u32 uA)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_set_iindpm(chip, uA);
}

static int bq25910_get_min_input_current(struct charger_device *chg_dev, u32 *uA)
{
	*uA = INDPM_MIN;

	return 0;
}

static int bq25910_kick_wdt(struct charger_device *chg_dev)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_masked_write(chip->client, REG05, WD_RST, WD_RST);
}

static int bq25910_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_set_vindpm(chip, uV);
}

static int bq25910_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);
	u8 data;
	int rc;

	rc = bq25910_read(chip->client, REG07, &data);
	if (rc) {
		*in_loop = false;
		return rc;
	}

	*in_loop = ((data & VINDPM_STAT) ? true : false);

	return 0;
}

static int bq25910_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_get_en_timer(chip, en);
}

static int bq25910_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_set_en_timer(chip, en);
}

static int bq25910_enable_termination(struct charger_device *chg_dev, bool en)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);

	return bq25910_set_en_term(chip, en);
}

#define SIZE_DUMP 256
static int bq25910_dump_register(struct charger_device *chg_dev)
{
	struct bq25910 *chip = dev_get_drvdata(&chg_dev->dev);
	char buffer[SIZE_DUMP];
	char *buf = buffer;
	int size = SIZE_DUMP;
	int written = 0;
	u8 data;
	int rc;
	int i;

	for (i = 0; i < ARRAY_SIZE(regs_to_dump); i++) {
		rc = bq25910_read(chip->client, regs_to_dump[i], &data);
		if (rc)
			continue;

		written = snprintf(buf, size, "0x%02x=0x%02x ",
				regs_to_dump[i], data);
		if (written < 0)
			continue;

		buf += written;
		size -= written;
	}

	dev_info(chip->dev, "dump: %s\n", buffer);

	return 0;
}

static struct charger_ops bq25910_ops = {
	/* enable/disable charger */
	.enable = bq25910_enable_charging,
	.is_enabled = bq25910_is_charging_enabled,

	/* get/set charging current*/
	.get_charging_current = bq25910_get_charging_current,
	.set_charging_current = bq25910_set_charging_current,
	.get_min_charging_current = bq25910_get_min_charging_current,

	/* set cv */
	.get_constant_voltage = bq25910_get_constant_voltage,
	.set_constant_voltage = bq25910_set_constant_voltage,

	/* set input_current */
	.get_input_current = bq25910_get_input_current,
	.set_input_current = bq25910_set_input_current,
	.get_min_input_current = bq25910_get_min_input_current,

	/* kick wdt */
	.kick_wdt = bq25910_kick_wdt,

	.set_mivr = bq25910_set_mivr,
	.get_mivr_state = bq25910_get_mivr_state,

	/* enable/disable charging safety timer */
	.is_safety_timer_enabled = bq25910_is_safety_timer_enabled,
	.enable_safety_timer = bq25910_enable_safety_timer,

	/* enable term */
	.enable_termination = bq25910_enable_termination,

	.dump_registers = bq25910_dump_register,
};

/* debugfs interface */
static int debugfs_get_data(void *data, u64 *val)
{
	struct bq25910 *chip = data;
	int rc;
	u8 temp;

	rc = bq25910_read(chip->client, chip->debug_addr, &temp);
	if (rc)
		return -EAGAIN;

	*val = temp;

	return 0;
}

static int debugfs_set_data(void *data, u64 val)
{
	struct bq25910 *chip = data;
	int rc;
	u8 temp;

	temp = (u8)val;
	rc = bq25910_write(chip->client, chip->debug_addr, temp);
	if (rc)
		return -EAGAIN;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(data_debugfs_ops,
	debugfs_get_data, debugfs_set_data, "0x%02llx\n");

static int dump_debugfs_show(struct seq_file *m, void *start)
{
	struct bq25910 *chip = m->private;
	u8 data;
	int rc;
	int i;

	for (i = 0; i < ARRAY_SIZE(regs_to_dump); i++) {
		rc = bq25910_read(chip->client, regs_to_dump[i], &data);
		if (rc) {
			seq_printf(m, "0x%02x=error\n", regs_to_dump[i]);
			continue;
		}

		seq_printf(m, "0x%02x=0x%02x\n", regs_to_dump[i], data);
	}

	return 0;
}

static int dump_debugfs_open(struct inode *inode, struct file *file)
{
	struct bq25910 *chip = inode->i_private;

	return single_open(file, dump_debugfs_show, chip);
}

static const struct file_operations dump_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debugfs_entries(struct bq25910 *chip)
{
	struct dentry *ent;

	chip->debugfs = debugfs_create_dir(chip->chg_props.alias_name, NULL);
	if (!chip->debugfs) {
		dev_err(chip->dev, "failed to create debugfs\n");
		return -ENODEV;
	}

	ent = debugfs_create_x32("addr", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, &chip->debug_addr);
	if (!ent)
		dev_err(chip->dev, "failed to create addr debugfs\n");

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, chip, &data_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create data debugfs\n");

	ent = debugfs_create_file("dump", S_IFREG | S_IRUGO,
		chip->debugfs, chip, &dump_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create dump debugfs\n");

	return 0;
}

static int bq25910_charger_device_register(struct bq25910 *chip)
{
	int rc;

	chip->chg_props.alias_name = "bq25910_slave";
	chip->chg_dev = charger_device_register(
			"secondary_chg", chip->dev, chip,
			&bq25910_ops, &chip->chg_props);
	if (IS_ERR_OR_NULL(chip->chg_dev)) {
		rc = PTR_ERR(chip->chg_dev);
		return rc;
	}
	chip->chg_dev->is_polling_mode = chip->is_polling_mode;
	if (!gpio_is_valid(chip->irq_gpio)) {
		dev_warn(chip->dev, "force set polling mode for %s\n",
				chip->chg_props.alias_name);
		chip->chg_dev->is_polling_mode = true;
	}

	return 0;
}

static int bq25910_hw_init(struct bq25910 *chip)
{
	int rc = 0;

	if (!gpio_is_valid(chip->irq_gpio))
		goto irq_init_done;

	rc = devm_gpio_request_one(chip->dev, chip->irq_gpio, GPIOF_DIR_IN,
			"bq25910_slave");
	if (rc) {
		dev_err(chip->dev, "failed to request gpio, rc=%d\n", rc);
		return rc;
	}

	rc = devm_request_threaded_irq(chip->dev, gpio_to_irq(chip->irq_gpio),
			NULL, bq25910_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"bq25910_slave", chip);
	if (rc) {
		dev_err(chip->dev, "failed to request irq, rc=%d\n", rc);
		return rc;
	}
irq_init_done:

	/* this is slave charger. stop charge here */
	bq25910_set_en_chg(chip, false);

	bq25910_set_vindpm(chip, chip->vindpm);

	bq25910_set_chg_timer(chip, chip->chg_timer);
	bq25910_set_en_timer(chip, chip->en_timer);
	bq25910_set_en_term(chip, chip->en_term);

	return rc;
}

static int bq25910_parse_dt(struct bq25910 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int rc = 0;

	if (!np)
		return -ENODEV;

	chip->is_polling_mode = of_property_read_bool(np, "polling_mode");

	chip->irq_gpio = of_get_named_gpio_flags(np, "irq-gpio", 0, NULL);

	rc = of_property_read_u32(np, "vindpm", &chip->vindpm);
	if (rc)
		chip->vindpm = VINDPM_DEFAULT;

	chip->en_term = of_property_read_bool(np, "en-term");
	chip->en_timer = of_property_read_bool(np, "en-timer");

	rc = of_property_read_u32(np, "chg-timer", &chip->chg_timer);
	if (rc)
		chip->chg_timer = 12;

	return 0;
}

static int bq25910_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct bq25910 *chip;
	int rc;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	i2c_set_clientdata(client, chip);

	mutex_init(&chip->lock);

	rc = bq25910_parse_dt(chip);
	if (rc)
		return rc;

	rc = bq25910_hw_init(chip);
	if (rc)
		return rc;

	rc = bq25910_charger_device_register(chip);
	if (rc)
		return rc;

	create_debugfs_entries(chip);

	return rc;
}

static int bq25910_remove(struct i2c_client *client)
{
	struct bq25910 *chip = i2c_get_clientdata(client);

	charger_device_unregister(chip->chg_dev);

	return 0;
}

static void bq25910_shutdown(struct i2c_client *client)
{
	struct bq25910 *chip = i2c_get_clientdata(client);

	/* disable charge */
	bq25910_set_en_chg(chip, false);

	return;
}

static const struct of_device_id bq25910_of_match[] = {
	{
		.compatible = "ti,bq25910",
	},
	{},
};

static const struct i2c_device_id bq25910_i2c_id[] = {
	{
		.name = "bq25910",
		.driver_data = 0,
	},
	{},
};

static struct i2c_driver bq25910_driver = {
	.probe = bq25910_probe,
	.remove = bq25910_remove,
	.shutdown = bq25910_shutdown,
	.driver = {
		.name = "bq25910",
		.of_match_table = bq25910_of_match,
	},
	.id_table = bq25910_i2c_id,
};

static int __init bq25910_init(void)
{
	return i2c_add_driver(&bq25910_driver);
}

static void __exit bq25910_exit(void)
{
	i2c_del_driver(&bq25910_driver);
}

module_init(bq25910_init);
module_exit(bq25910_exit);
