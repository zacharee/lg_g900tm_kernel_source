/*
* refcharger.c -- reference charger driver controlled by i2c
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

/* TODO : define register here [START] */

/* TODO : define register here [END] */

const static int regs_to_dump[] = {
	/* TODO : fill register number to dump */
};

struct refcharger {
	struct i2c_client *client;
	struct device *dev;

	/* charger class */
	struct charger_device *chg_dev;
	struct charger_device *ls_dev;
	struct charger_properties chg_props;
	struct charger_properties ls_props;

	struct mutex lock;
	struct mutex ops_lock;

	/* device tree */
	bool is_slave;
	bool is_load_switch;
	bool is_polling_mode;
	int irq_gpio;

	/* debugfs */
	struct dentry *debugfs;
	u32 debug_addr;
};

static int refcharger_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int rc;

	rc = i2c_smbus_read_byte_data(client, reg);
	if (rc < 0)
		dev_err(&client->dev, "failed to read. rc=%d\n", rc);

	*data = (rc & 0xFF);

	return rc < 0 ? rc : 0;
}

static int refcharger_write(struct i2c_client *client, u8 reg, u8 data)
{
	int rc;

	rc = i2c_smbus_write_byte_data(client, reg, data);
	if (rc < 0)
		dev_err(&client->dev, "failed to write. rc=%d\n", rc);

	return rc;
}

static int refcharger_masked_write(struct i2c_client *client, u8 reg, u8 data, u8 mask)
{
	struct refcharger *chip = i2c_get_clientdata(client);
	u8 tmp;
	int rc = 0;

	mutex_lock(&chip->lock);

	rc = refcharger_read(client, reg, &tmp);
	if (rc < 0)
		goto out;

	tmp = (data & mask) | (tmp & (~mask));
	rc = refcharger_write(client, reg, tmp);

out:
	mutex_unlock(&chip->lock);
	return rc;
}

/* TODO : internal functions [START] */

/* TODO : internal functions [END] */

static irqreturn_t refcharger_irq_handler(int irq, void *data)
{
	struct refcharger *chip = data;

	dev_info(chip->dev, "refcharger_irq_handler\n");

	/* TODO : handle irq */
	refcharger_masked_write(chip->client, 0, 0, 0);	/* dummy */

	return IRQ_HANDLED;
}

/* charger class interface */
/* TODO : implemention related to charger [START] */
static int refcharger_plug_out(struct charger_device *chg_dev)
{
	/* when cable plug out detected, this function will be called */
	/* TODO : implement */
	return 0;
}

static int refcharger_plug_in(struct charger_device *chg_dev)
{
	/* when cable plug in detected, this function will be called */
	/* TODO : implement */
	return 0;
}

static int refcharger_enable_charging(struct charger_device *chg_dev, bool en)
{
	/* enable battery charging */
	/* TODO : implement */
	return 0;
}

static int refcharger_get_charging_current(struct charger_device *chg_dev, u32 *uA)
{
	/* report battery charging current setting */
	/* TODO : implement */
	*uA = 0;
	return 0;
}

static int refcharger_set_charging_current(struct charger_device *chg_dev, u32 uA)
{
	/* set battery charging current */
	/* TODO : implement */
	return 0;
}

static int refcharger_get_min_charging_current(struct charger_device *chg_dev, u32 *uA)
{
	/* report minimum battery charging current setting */
	/* TODO : implement */
	*uA = 0;
	return 0;
}

static int refcharger_get_constant_voltage(struct charger_device *chg_dev, u32 *uV)
{
	/* report constant voltage setting */
	/* TODO : implement */
	*uV = 0;
	return 0;
}

static int refcharger_set_constant_voltage(struct charger_device *chg_dev, u32 uV)
{
	/* set constant voltage */
	/* TODO : implement */
	return 0;
}

static int refcharger_get_input_current(struct charger_device *chg_dev, u32 *uA)
{
	/* get input current setting */
	/* TODO : implement */
	*uA = 0;
	return 0;
}

static int refcharger_set_input_current(struct charger_device *chg_dev, u32 uA)
{
	/* set input current */
	/* TODO : implement */
	return 0;
}

static int refcharger_get_min_input_current(struct charger_device *chg_dev, u32 *uA)
{
	/* report minimum input current setting */
	/* TODO : implement */
	*uA = 0;
	return 0;
}

static int refcharger_get_eoc_current(struct charger_device *chg_dev, u32 *uA)
{
	/* report end of charge current */
	/* TODO : implement */
	*uA = 0;
	return 0;
}

static int refcharger_set_eoc_current(struct charger_device *chg_dev, u32 uA)
{
	/* set end of charge current */
	/* TODO : implement */
	return 0;
}

static int refcharger_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}

static int refcharger_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	/* set aicl low threshold */
	/* TODO : implement */
	return 0;
}

static int refcharger_enable_power_path(struct charger_device *chg_dev, bool en)
{
	/* enable power path (input) */
	/* TODO : implement */
	return 0;
}
static int refcharger_is_power_path_enabled(struct charger_device *chg_dev, bool *en)
{
	/* report power path enabled */
	/* TODO : implement */
	*en = false;
	return 0;
}

static int refcharger_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	/* report safety timer enabled */
	/* TODO : implement */
	*en = false;
	return 0;
}

static int refcharger_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	/* enable safety timer */
	/* TODO : implement */
	return 0;
}

static int refcharger_enable_termination(struct charger_device *chg_dev, bool en)
{
	/* enable auto stop charging when end of charge detected */
	/* TODO : implement */
	return 0;
}

static int refcharger_enable_otg(struct charger_device *chg_dev, bool en)
{
	/* enable otg boost */
	/* TODO : implement */
	return 0;
}

static int refcharger_set_otg_current_limit(struct charger_device *chg_dev, u32 uA)
{
	/* set otg boost current limit */
	/* TODO : implement */
	return 0;
}


static int refcharger_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	/* report end of charge state */
	/* TODO : implement */
	*done = false;
	return 0;
}

#define SIZE_DUMP 256
static int refcharger_dump_register(struct charger_device *chg_dev)
{
	struct refcharger *chip = dev_get_drvdata(&chg_dev->dev);
	char buffer[SIZE_DUMP];
	char *buf = buffer;
	int size = SIZE_DUMP;
	int written = 0;
	u8 data;
	int rc;
	int i;

	for (i = 0; i < ARRAY_SIZE(regs_to_dump); i++) {
		rc = refcharger_read(chip->client, regs_to_dump[i], &data);
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
/* TODO : implemention related to charger [END] */

static struct charger_ops refcharger_ops = {
	/* cable plug in/out */
	.plug_out = refcharger_plug_out,
	.plug_in = refcharger_plug_in,

	/* enable/disable charger */
	.enable = refcharger_enable_charging,
	.is_enabled = NULL,

	/* enable/disable chip */
	.enable_chip = NULL,
	.is_chip_enabled = NULL,

	/* get/set charging current*/
	.get_charging_current = refcharger_get_charging_current,
	.set_charging_current = refcharger_set_charging_current,
	.get_min_charging_current = refcharger_get_min_charging_current,

	/* set cv */
	.get_constant_voltage = refcharger_get_constant_voltage,
	.set_constant_voltage = refcharger_set_constant_voltage,

	/* set input_current */
	.get_input_current = refcharger_get_input_current,
	.set_input_current = refcharger_set_input_current,
	.get_min_input_current = refcharger_get_min_input_current,

	/* set termination current */
	.get_eoc_current = refcharger_get_eoc_current,
	.set_eoc_current = refcharger_set_eoc_current,

	/* kick wdt */
	.kick_wdt = NULL,

	.event = refcharger_do_event,

	/* PE+/PE+2.0 */
	.send_ta_current_pattern = NULL,
	.send_ta20_current_pattern = NULL,
	.reset_ta = NULL,
	.enable_cable_drop_comp = NULL,

	.set_mivr = refcharger_set_mivr,
	.get_mivr_state = NULL,

	/* enable/disable powerpath */
	.is_powerpath_enabled = refcharger_is_power_path_enabled,
	.enable_powerpath = refcharger_enable_power_path,

	/* enable/disable vbus ovp */
	.enable_vbus_ovp = NULL,

	/* enable/disable charging safety timer */
	.is_safety_timer_enabled = refcharger_is_safety_timer_enabled,
	.enable_safety_timer = refcharger_enable_safety_timer,

	/* enable term */
	.enable_termination = refcharger_enable_termination,

	/* OTG */
	.enable_otg = refcharger_enable_otg,
	.enable_discharge = NULL,
	.set_boost_current_limit = refcharger_set_otg_current_limit,

	/* charger type detection */
	.enable_chg_type_det = NULL,

	/* run AICL */
	.run_aicl = NULL,

	/* reset EOC state */
	.reset_eoc_state = NULL,

	.safety_check = NULL,

	.is_charging_done = refcharger_is_charging_done,
	.set_pe20_efficiency_table = NULL,
	.dump_registers = refcharger_dump_register,
	.get_ibus_adc = NULL,
	.get_tchg_adc = NULL,
	.get_zcv = NULL,
};

/* TODO : implement related to load switch [START] */

/* TODO : implement related to load switch [END] */

static struct charger_ops refcharger_ls_ops = {
	/* direct charging */
	.enable_direct_charging = NULL,
	.kick_direct_charging_wdt = NULL,
	.set_direct_charging_ibusoc = NULL,
	.set_direct_charging_vbusov = NULL,

	.get_ibus_adc = NULL,
	.get_tchg_adc = NULL,
};

/* debugfs interface */
static int debugfs_get_data(void *data, u64 *val)
{
	struct refcharger *chip = data;
	int rc;
	u8 temp;

	rc = refcharger_read(chip->client, chip->debug_addr, &temp);
	if (rc)
		return -EAGAIN;

	*val = temp;

	return 0;
}

static int debugfs_set_data(void *data, u64 val)
{
	struct refcharger *chip = data;
	int rc;
	u8 temp;

	temp = (u8)val;
	rc = refcharger_write(chip->client, chip->debug_addr, temp);
	if (rc)
		return -EAGAIN;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(data_debugfs_ops,
	debugfs_get_data, debugfs_set_data, "0x%02llx\n");

static int dump_debugfs_show(struct seq_file *m, void *start)
{
	struct refcharger *chip = m->private;
	u8 data;
	int rc;
	int i;

	for (i = 0; i < ARRAY_SIZE(regs_to_dump); i++) {
		rc = refcharger_read(chip->client, regs_to_dump[i], &data);
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
	struct refcharger *chip = inode->i_private;

	return single_open(file, dump_debugfs_show, chip);
}

static const struct file_operations dump_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debugfs_entries(struct refcharger *chip)
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

static int refcharger_charger_device_register(struct refcharger *chip)
{
	int rc;

	chip->chg_props.alias_name = chip->is_slave
			? "refcharger_slave" : "refcharger";
	chip->chg_dev = charger_device_register(
			chip->is_slave ? "secondary_chg" : "primary_chg",
			chip->dev, chip,
			&refcharger_ops, &chip->chg_props);
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

	if (!chip->is_load_switch)
		return 0;

	chip->ls_props.alias_name = "refcharger_ls";
	chip->ls_dev = charger_device_register(
			"primary_load_switch", chip->dev, chip,
			&refcharger_ls_ops, &chip->ls_props);
	if (IS_ERR_OR_NULL(chip->ls_dev)) {
		dev_err(chip->dev, "failed to register load switch\n");
		return 0;
	}
	chip->ls_dev->is_polling_mode = chip->is_polling_mode;
	if (!gpio_is_valid(chip->irq_gpio)) {
		dev_warn(chip->dev, "force set polling mode for %s\n",
				chip->ls_props.alias_name);
		chip->ls_dev->is_polling_mode = true;
	}

	return 0;
}

static int refcharger_hw_init(struct refcharger *chip)
{
	int rc = 0;

	if (!gpio_is_valid(chip->irq_gpio))
		goto irq_init_done;

	rc = devm_gpio_request_one(chip->dev, chip->irq_gpio, GPIOF_DIR_IN,
			chip->is_slave ? "refcharger_slave" : "refcharger");
	if (rc) {
		dev_err(chip->dev, "failed to request gpio, rc=%d\n", rc);
		return rc;
	}

	rc = devm_request_threaded_irq(chip->dev, gpio_to_irq(chip->irq_gpio),
			NULL, refcharger_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			chip->is_slave ? "refcharger_slave" : "refcharger", chip);
	if (rc) {
		dev_err(chip->dev, "failed to request irq, rc=%d\n", rc);
		return rc;
	}
irq_init_done:

	/* TODO : init settings */

	return rc;
}

static int refcharger_parse_dt(struct refcharger *chip)
{
	struct device_node *np = chip->dev->of_node;
	int rc = 0;

	if (!np)
		return -ENODEV;

	chip->is_slave = of_property_read_bool(np, "slave-charger");
	chip->is_load_switch = of_property_read_bool(np, "load-switch");

	chip->is_polling_mode = of_property_read_bool(np, "polling_mode");

	chip->irq_gpio = of_get_named_gpio_flags(np, "irq-gpio", 0, NULL);

	return rc;
}

static int refcharger_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct refcharger *chip;
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
	mutex_init(&chip->ops_lock);

	rc = refcharger_parse_dt(chip);
	if (rc)
		return rc;

	rc = refcharger_hw_init(chip);
	if (rc)
		return rc;

	rc = refcharger_charger_device_register(chip);
	if (rc)
		return rc;

	create_debugfs_entries(chip);

	return rc;
}

static int refcharger_remove(struct i2c_client *client)
{
	struct refcharger *chip = i2c_get_clientdata(client);

	if (!IS_ERR_OR_NULL(chip->ls_dev))
		charger_device_unregister(chip->ls_dev);
	charger_device_unregister(chip->chg_dev);

	return 0;
}

static void refcharger_shutdown(struct i2c_client *client)
{
	/* TODO : force otg boost off if support */

	return;
}

static const struct of_device_id refcharger_of_match[] = {
	{
		.compatible = "vendor,refcharger",
	},
	{},
};

static const struct i2c_device_id refcharger_i2c_id[] = {
	{
		.name = "refcharger",
		.driver_data = 0,
	},
	{},
};

static struct i2c_driver refcharger_driver = {
	.probe = refcharger_probe,
	.remove = refcharger_remove,
	.shutdown = refcharger_shutdown,
	.driver = {
		.name = "refcharger",
		.of_match_table = refcharger_of_match,
	},
	.id_table = refcharger_i2c_id,
};

static int __init refcharger_init(void)
{
	return i2c_add_driver(&refcharger_driver);
}

static void __exit refcharger_exit(void)
{
	i2c_del_driver(&refcharger_driver);
}

module_init(refcharger_init);
module_exit(refcharger_exit);
