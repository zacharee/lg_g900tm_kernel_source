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
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <mtk_charger_intf.h>

/* BQ25970 Registers */
#define BQ25970_REG_BAT_OVP			(0x00)
#define BQ25970_REG_BAT_OVP_ALM			(0x01)
#define BQ25970_REG_BAT_OCP			(0x02)
#define BQ25970_REG_BAT_OCP_ALM			(0x03)
#define BQ25970_REG_BAT_UCP_ALM			(0x04)
#define BQ25970_REG_AC_PROTECTION		(0x05)
#define BQ25970_REG_BUS_OVP			(0x06)
#define BQ25970_REG_BUS_OVP_ALM			(0x07)
#define BQ25970_REG_BUS_OCP_UCP			(0x08)
#define BQ25970_REG_BUS_OCP_ALM			(0x09)
#define BQ25970_REG_CONVERTER_STATE		(0x0A)
#define BQ25970_REG_CONTROL			(0x0B)
#define BQ25970_REG_CHRG_CTRL			(0x0C)
#define BQ25970_REG_INT_STAT			(0x0D)
#define BQ25970_REG_INT_FLAG			(0x0E)
#define BQ25970_REG_INT_MASK			(0x0F)
#define BQ25970_REG_FLT_STAT			(0x10)
#define BQ25970_REG_FLT_FLAG			(0x11)
#define BQ25970_REG_FLT_MASK			(0x12)
#define BQ25970_REG_PART_INFO			(0x13)
#define BQ25970_REG_ADC_CTRL			(0x14)
#define BQ25970_REG_ADC_FN_DISABLE		(0x15)
#define BQ25970_REG_IBUS_ADC1			(0x16)
#define BQ25970_REG_IBUS_ADC0			(0x17)
#define BQ25970_REG_VBUS_ADC1			(0x18)
#define BQ25970_REG_VBUS_ADC0			(0x19)
#define BQ25970_REG_VAC_ADC1			(0x1A)
#define BQ25970_REG_VAC_ADC0			(0x1B)
#define BQ25970_REG_VOUT_ADC1			(0x1C)
#define BQ25970_REG_VOUT_ADC0			(0x1D)
#define BQ25970_REG_VBAT_ADC1			(0x1E)
#define BQ25970_REG_VBAT_ADC0			(0x1F)
#define BQ25970_REG_IBAT_ADC1			(0x20)
#define BQ25970_REG_IBAT_ADC0			(0x21)
#define BQ25970_REG_TSBUS_ADC1			(0x22)
#define BQ25970_REG_TSBUS_ADC0			(0x23)
#define BQ25970_REG_TSBAT_ADC1			(0x24)
#define BQ25970_REG_TSBAT_ADC0			(0x25)
#define BQ25970_REG_TDIE_ADC1			(0x26)
#define BQ25970_REG_TDIE_ADC0			(0x27)
#define BQ25970_REG_TSBUS_FLT1			(0x28)
#define BQ25970_REG_TSBAT_FLT0			(0x29)
#define BQ25970_REG_TDIE_ALM			(0x2A)
#define BQ25970_REG_REG_CTRL			(0x2B)
#define BQ25970_REG_REG_THRESHOLD		(0x2C)
#define BQ25970_REG_REG_FLAG_MASK		(0x2D)
#define BQ25970_REG_DEGLITCH			(0x2E)

/* BQ25970_REG_BAT_OVP */
#define BQ25970_BAT_OVP_DIS_MASK		BIT(7)
#define BQ25970_BAT_OVP_DIS_SHIFT		(7)
#define BQ25970_BAT_OVP_MASK			(0x3F)

/* BQ25970_REG_BAT_OVP_ALM */
#define BQ25970_BAT_OVP_ALM_DIS_MASK		BIT(7)
#define BQ25970_BAT_OVP_ALM_DIS_SHIFT		(7)
#define BQ25970_BAT_OVP_ALM_MASK		(0x3F)

/* BQ25970_REG_BAT_OCP */
#define BQ25970_BAT_OCP_DIS_MASK		BIT(7)
#define BQ25970_BAT_OCP_DIS_SHIFT		(7)
#define BQ25970_BAT_OCP_MASK			(0x7F)

/* BQ25970_REG_BAT_OCP_ALM */
#define BQ25970_BAT_OCP_ALM_DIS_MASK		BIT(7)
#define BQ25970_BAT_OCP_ALM_DIS_SHIFT		(7)
#define BQ25970_BAT_OCP_ALM_MASK		(0x7F)

/* BQ25970_REG_BAT_UCP_ALM */
#define BQ25970_BAT_UCP_ALM_DIS_MASK		BIT(7)
#define BQ25970_BAT_UCP_ALM_DIS_SHIFT		(7)
#define BQ25970_BAT_UCP_ALM_MASK		(0x7F)

/* BQ25970_REG_AC_PROTECTION */
#define BQ25970_AC_OVP_STAT_MASK		BIT(7)
#define BQ25970_AC_OVP_FLAG_MASK		BIT(6)
#define BQ25970_AC_OVP_MASK			(0x07)

/* BQ25970_REG_BUS_OVP */
#define BQ25970_BUS_OVP_MASK			(0x7F)

/* BQ25970_REG_BUS_OVP_ALM */
#define BQ25970_BUS_OVP_ALM_MASK		(0x7F)
#define BQ25970_BUS_OVP_ALM_DIS_MASK		BIT(7)
#define BQ25970_BUS_OVP_ALM_DIS_SHIFT		(7)

/* BQ25970_REG_BUS_OCP_UCP */
#define BQ25970_IBUS_UCP_RISE_FLAG_MASK		BIT(6)
#define BQ25970_IBUS_UCP_FALL_FLAG_MASK		BIT(4)
#define BQ25970_BUS_OCP_MASK			(0x0F)

/* BQ25970_REG_BUS_OCP_ALM */
#define BQ25970_BUS_OCP_ALM_MASK		(0x7F)

/* BQ25970_REG_CONVERTER_STATE */
#define BQ25970_TSHUT_FLAG_MASK			BIT(7)
#define BQ25970_TSHUT_STAT_MASK			BIT(6)
#define BQ25970_VBUS_ERRORLO_STAT_MASK		BIT(5)
#define BQ25970_VBUS_ERRORHI_STAT_MASK		BIT(4)
#define BQ25970_SS_TIMEOUT_FLAG_MASK		BIT(3)
#define BQ25970_CONV_SWITCHING_STAT_MASK	BIT(2)
#define BQ25970_CONV_OCP_FLAG_MASK		BIT(1)
#define BQ25970_PIN_DIAG_FAIL_FLAG_MASK		BIT(0)

/* BQ25970_REG_CONTROL */
#define BQ25970_FSW_SET_MASK			(0x70)
#define BQ25970_FSW_SET_SHIFT			(4)
#define BQ25970_WD_TIMEOUT_FLAG_MASK		BIT(3)
#define BQ25970_WATCHDOG_DIS_MASK		BIT(2)
#define BQ25970_WATCHDOG_MASK			(0x03)

/* BQ25970_REG_CHRG_CTRL */
#define BQ25970_CHG_EN_MASK			BIT(7)
#define BQ25970_TSBUS_DIS_MASK			BIT(2)
#define BQ25970_TSBAT_DIS_MASK			BIT(1)
#define BQ25970_TDIE_DIS_MASK			BIT(0)

/* BQ25970_REG_INT_STAT */
#define BQ25970_BAT_OVP_ALM_STAT_MASK		BIT(7)
#define BQ25970_BAT_OCP_ALM_STAT_MASK		BIT(6)
#define BQ25970_BUS_OVP_ALM_STAT_MASK		BIT(5)
#define BQ25970_BUS_OCP_ALM_STAT_MASK		BIT(4)
#define BQ25970_BAT_UCP_ALM_STAT_MASK		BIT(3)
#define BQ25970_ADAPTER_INSERT_STAT_MASK	BIT(2)
#define BQ25970_VBAT_INSERT_STAT_MASK		BIT(1)
#define BQ25970_ADC_DONE_STAT_MASK		BIT(0)

/* BQ25970_REG_INT_FLAG */
#define BQ25970_BAT_OVP_ALM_FLAG_MASK		BIT(7)
#define BQ25970_BAT_OCP_ALM_FLAG_MASK		BIT(6)
#define BQ25970_BUS_OVP_ALM_FLAG_MASK		BIT(5)
#define BQ25970_BUS_OCP_ALM_FLAG_MASK		BIT(4)
#define BQ25970_BAT_UCP_ALM_FLAG_MASK		BIT(3)
#define BQ25970_ADAPTER_INSERT_FLAG_MASK	BIT(2)
#define BQ25970_VBAT_INSERT_FLAG_MASK		BIT(1)
#define BQ25970_ADC_DONE_FLAG_MASK		BIT(0)

/* BQ25970_REG_FLT_STAT */
#define BQ25970_BAT_OVP_FLT_STAT_MASK		BIT(7)
#define BQ25970_BAT_OCP_FLT_STAT_MASK		BIT(6)
#define BQ25970_BUS_OVP_FLT_STAT_MASK		BIT(5)
#define BQ25970_BUS_OCP_FLT_STAT_MASK		BIT(4)
#define BQ25970_TSBUS_TSBAT_ALM_STAT_MASK	BIT(3)
#define BQ25970_TSBAT_FLT_STAT_MASK		BIT(2)
#define BQ25970_TSBUS_FLT_STAT_MASK		BIT(1)
#define BQ25970_TDIE_ALM_STAT_MASK		BIT(0)

/* BQ25970_REG_FLT_FLAG */
#define BQ25970_BAT_OVP_FLT_FLAG_MASK		BIT(7)
#define BQ25970_BAT_OCP_FLT_FLAG_MASK		BIT(6)
#define BQ25970_BUS_OVP_FLT_FLAG_MASK		BIT(5)
#define BQ25970_BUS_OCP_FLT_FLAG_MASK		BIT(4)
#define BQ25970_TSBUS_TSBAT_ALM_FLAG_MASK	BIT(3)
#define BQ25970_TSBAT_FLT_FLAG_MASK		BIT(2)
#define BQ25970_TSBUS_FLT_FLAG_MASK		BIT(1)
#define BQ25970_TDIE_ALM_FLAG_MASK		BIT(0)

/* BQ25970_REG_ADC_CTRL */
#define BQ25970_ADC_EN_MASK			BIT(7)
#define BQ25970_ADC_RATE_MASK			BIT(6)
#define BQ25970_ADC_RATE_SHIFT			(6)
#define BQ25970_IBUS_ADC_DIS_MASK		BIT(0)

/* BQ25970_REG_ADC_FN_DISABLE */
#define BQ25970_VBUS_ADC_DIS_MASK		BIT(7)
#define BQ25970_VAC_ADC_DIS_MASK		BIT(6)
#define BQ25970_VOUT_ADC_DIS_MASK		BIT(5)
#define BQ25970_VBAT_ADC_DIS_MASK		BIT(4)
#define BQ25970_IBAT_ADC_DIS_MASK		BIT(3)
#define BQ25970_TSBUS_ADC_DIS_MASK		BIT(2)
#define BQ25970_TSBAT_ADC_DIS_MASK		BIT(1)
#define BQ25970_TDIE_ADC_DIS_MASK		BIT(0)

/* BQ25970_REG_REG_CTRL */
#define BQ25970_SS_TIMEOUT_SET_MASK		(0xE0)
#define BQ25970_SS_TIMEOUT_SET_SHIFT		(5)
#define BQ25970_SET_IBAT_SNS_RES_MASK		BIT(1)

/* BQ25970_REG_REG_THRESHOLD */
#define BQ25970_VBATREG_ACTIVE_STAT_MASK	BIT(3)
#define BQ25970_IBATREG_ACTIVE_STAT_MASK	BIT(2)
#define BQ25970_VDROP_OVP_STAT_MASK		BIT(1)
#define BQ25970_VOUT_OVP_STAT_MASK		BIT(0)

/* BQ25970_REG_REG_FLAG_MASK */
#define BQ25970_VBATREG_ACTIVE_FLAG_MASK	BIT(7)
#define BQ25970_IBATREG_ACTIVE_FLAG_MASK	BIT(6)
#define BQ25970_VDROP_OVP_FLAG_MASK		BIT(5)
#define BQ25970_VOUT_OVP_FLAG_MASK		BIT(4)

/* BQ25970_REG_DEGLITCH */
#define BQ25970_VBUS_ERROR_LO_DG_SET_MASK	BIT(4)
#define BQ25970_IBUS_LOW_DG_SET_MASK		BIT(3)

#define BQ25970_BAT_OVP_MIN_UV			(3475000)
#define BQ25970_BAT_OVP_MAX_UV			(5050000)
#define BQ25970_BAT_OVP_STEP_UV			(25000)
#define BQ25970_BAT_OVP_ALM_MIN_UV		(3500000)
#define BQ25970_BAT_OVP_ALM_MAX_UV		(5075000)
#define BQ25970_BAT_OVP_ALM_STEP_UV		(25000)
#define BQ25970_BAT_OCP_MIN_UA			(2000000)
#define BQ25970_BAT_OCP_MAX_UA			(10000000)
#define BQ25970_BAT_OCP_STEP_UA			(100000)
#define BQ25970_BAT_OCP_ALM_MIN_UA		(2000000)
#define BQ25970_BAT_OCP_ALM_MAX_UA		(14700000)
#define BQ25970_BAT_OCP_ALM_STEP_UA		(100000)
#define BQ25970_BAT_UCP_ALM_MIN_UA		(2000000)
#define BQ25970_BAT_UCP_ALM_MAX_UA		(8300000)
#define BQ25970_BAT_UCP_ALM_STEP_UA		(50000)
#define BQ25970_AC_OVP_MIN_UV			(11000000)
#define BQ25970_AC_OVP_MAX_UV			(17000000)
#define BQ25970_AC_OVP_STEP_UV			(1000000)
#define BQ25970_BUS_OVP_MIN_UV			(5950000)
#define BQ25970_BUS_OVP_MAX_UV			(12300000)
#define BQ25970_BUS_OVP_STEP_UV			(50000)
#define BQ25970_BUS_OVP_ALM_MIN_UV		(6000000)
#define BQ25970_BUS_OVP_ALM_MAX_UV		(12350000)
#define BQ25970_BUS_OVP_ALM_STEP_UV		(50000)
#define BQ25970_BUS_OCP_MIN_UA			(1000000)
#define BQ25970_BUS_OCP_MAX_UA			(4750000)
#define BQ25970_BUS_OCP_STEP_UA			(250000)
#define BQ25970_BUS_OCP_ALM_MIN_UA		(0)
#define BQ25970_BUS_OCP_ALM_MAX_UA		(6350000)
#define BQ25970_BUS_OCP_ALM_STEP_UA		(50000)

enum {
	BQ25970_MASTER,
	BQ25970_SLAVE,
	BQ25970_STANDALONE,
};

enum {
	BQ25970_ADC_CONTINUOUS,
	BQ25970_ADC_ONESHOT,
};

enum {
	BQ25970_ADC_IBUS = 0,
	BQ25970_ADC_VBUS,
	BQ25970_ADC_VAC,
	BQ25970_ADC_VOUT,
	BQ25970_ADC_VBAT,
	BQ25970_ADC_IBAT,
	BQ25970_ADC_TSBUS,
	BQ25970_ADC_TSBAT,
	BQ25970_ADC_TDIE,
};

static const char *bq25970_adc_name[] = {
	[BQ25970_ADC_IBUS] = "ibus",
	[BQ25970_ADC_VBUS] = "vbus",
	[BQ25970_ADC_VAC] = "vac",
	[BQ25970_ADC_VOUT] = "vout",
	[BQ25970_ADC_VBAT] = "vbat",
	[BQ25970_ADC_IBAT] = "ibat",
	[BQ25970_ADC_TSBUS] = "tsbus",
	[BQ25970_ADC_TSBAT] = "tsbat",
	[BQ25970_ADC_TDIE] = "tdie",
};

#define BQ25970_IRQFLAG(x) BQ25970_IRQFLAG_##x
#define BQ25970_STAT(x) BQ25970_STAT_##x
#define BQ25970_IRQFLAG_STAT(x) BQ25970_IRQFLAG(x), \
		BQ25970_STAT(x) = BQ25970_IRQFLAG(x)

enum {
	BQ25970_IRQFLAG_STAT(AC_OVP),		/* index  0 */ /* Byte 0 */
	BQ25970_IRQFLAG(IBUS_UCP_RISE),		/* index  1 */
	BQ25970_IRQFLAG(IBUS_UCP_FALL),		/* index  2 */
	BQ25970_IRQFLAG_STAT(TSHUT),		/* index  3 */
	BQ25970_STAT(VBUS_ERRORLO),		/* index  4 */ /* Byte 1 */
	BQ25970_STAT(VBUS_ERRORHI),		/* index  5 */
	BQ25970_IRQFLAG(SS_TIMEOUT),		/* index  6 */
	BQ25970_STAT(CONV_SWITCHING),		/* index  7 */
	BQ25970_IRQFLAG(CONV_OCP),		/* index  8 */ /* Byte 2 */
	BQ25970_IRQFLAG(PIN_DIAG_FAIL),		/* index  9 */
	BQ25970_IRQFLAG(WD_TIMEOUT),		/* index 10 */
	BQ25970_IRQFLAG_STAT(BAT_OVP_ALM),	/* index 11 */
	BQ25970_IRQFLAG_STAT(BAT_OCP_ALM),	/* index 12 */ /* Byte 3 */
	BQ25970_IRQFLAG_STAT(BUS_OVP_ALM),	/* index 13 */
	BQ25970_IRQFLAG_STAT(BUS_OCP_ALM),	/* index 14 */
	BQ25970_IRQFLAG_STAT(BAT_UCP_ALM),	/* index 15 */
	BQ25970_IRQFLAG_STAT(ADAPTER_INSERT),	/* index 16 */ /* Byte 4 */
	BQ25970_IRQFLAG_STAT(VBAT_INSERT),	/* index 17 */
	BQ25970_IRQFLAG_STAT(ADC_DONE),		/* index 18 */
	BQ25970_IRQFLAG_STAT(BAT_OVP_FLT),	/* index 19 */
	BQ25970_IRQFLAG_STAT(BAT_OCP_FLT),	/* index 20 */ /* Byte 5 */
	BQ25970_IRQFLAG_STAT(BUS_OVP_FLT),	/* index 21 */
	BQ25970_IRQFLAG_STAT(BUS_OCP_FLT),	/* index 22 */
	BQ25970_IRQFLAG_STAT(TSBUS_TSBAT_ALM),	/* index 23 */
	BQ25970_IRQFLAG_STAT(TSBAT_FLT),	/* index 24 */ /* Byte 6 */
	BQ25970_IRQFLAG_STAT(TSBUS_FLT),	/* index 25 */
	BQ25970_IRQFLAG_STAT(TDIE_ALM),		/* index 26 */
	BQ25970_IRQFLAG_STAT(VBATREG_ACTIVE),	/* index 27 */
	BQ25970_IRQFLAG_STAT(IBATREG_ACTIVE),	/* index 28 */ /* Byte 7 */
	BQ25970_IRQFLAG_STAT(VDROP_OVP),	/* index 29 */
	BQ25970_IRQFLAG_STAT(VOUT_OVP),		/* index 30 */
};

static const u32 irqmask_default = ~(u32)BIT(BQ25970_IRQFLAG_ADC_DONE);

struct bq25970_cfg {
	/* household */
	unsigned int ac_ovp;
	unsigned int bat_ucp_alm;
	unsigned int fsw_set;
	unsigned int ibat_sns_res;
	unsigned int ss_timeout;
	unsigned int ibus_low_dg_set;
	/* watchdog */
	bool watchdog_dis;
	unsigned int watchdog;
	/* protection */
	bool bat_ovp_dis;
	bool bat_ovp_alm_dis;
	bool bat_ocp_dis;
	bool bat_ocp_alm_dis;
	bool bat_ucp_alm_dis;
	bool bus_ovp_alm_dis;
	bool bus_ocp_dis;
	bool bus_ocp_alm_dis;
	bool tsbus_dis;
	bool tsbat_dis;
	bool tdie_dis;
	/* adc */
	u16 adc_fn_dis;
	/* irq */
	u32 irqmask;
};

struct bq25970 {
	struct i2c_client *client;
	struct device *dev;
	struct bq25970_cfg cfg;

	struct mutex rw_lock;
	struct mutex ops_lock;
	struct mutex adc_lock;

	u32 irqmask;
	struct completion adc_done;

	struct charger_device *chg_dev;
	struct charger_properties chg_prop;

	struct dentry *debugfs;
	u8 debug_addr;
};

static int __bq25970_read(struct bq25970 *chip, u8 reg, u8 *val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return ret;

	*val = (ret & 0xFF);

	return ret < 0 ? ret : 0;
}

static int __bq25970_write(struct bq25970 *chip, u8 reg, u8 val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);

	return ret < 0 ? ret : 0;
}

static int __bq25970_update_bits(struct bq25970 *chip, u8 reg, u8 mask, u8 val)
{
	u8 tmp;
	s32 ret = 0;

	mutex_lock(&chip->rw_lock);

	ret = __bq25970_read(chip, reg, &tmp);
	if (ret < 0)
		goto out;

	tmp = (val & mask) | (tmp & (~mask));
	ret = __bq25970_write(chip, reg, tmp);

out:
	mutex_unlock(&chip->rw_lock);

	return ret;
}

static int __bq25970_set_bits(struct bq25970 *chip, u8 reg, u8 bits)
{
	return __bq25970_update_bits(chip, reg, bits, 0xFF);
}

static int __bq25970_clr_bits(struct bq25970 *chip, u8 reg, u8 bits)
{
	return __bq25970_update_bits(chip, reg, bits, 0);
}

static bool __bq25970_check_bits(struct bq25970 *chip, u8 reg, u8 bits)
{
	u8 val;
	int ret;

	ret = __bq25970_read(chip, reg, &val);
	if (ret < 0)
		return false;

	return (val & bits) ? true : false;
}

static int __bq25970_read_word(struct bq25970 *chip, u8 reg, u16 *val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		return ret;

	*val = (u16)ret;

	return 0;
}

static int __bq25970_write_word(struct bq25970 *chip, u8 reg, u16 val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_write_word_data(client, reg, val);

	return ret < 0 ? ret : 0;
}

static void __maybe_unused __bq25970_dump_register(struct bq25970 *chip)
{
	u8 reg, val;
	int ret;

	for (reg = BQ25970_REG_BAT_OVP; reg <= BQ25970_REG_DEGLITCH; reg++) {
		ret = __bq25970_read(chip, reg, &val);
		if (ret < 0) {
			dev_info(chip->dev, "[DUMP] 0x%02x = error\n", reg);
			continue;
		}
		dev_info(chip->dev, "[DUMP] 0x%02x = 0x%02x\n", reg, val);
	}
}

static int __bq25970_set_bat_ovp(struct bq25970 *chip, unsigned int uV)
{
	u8 val = (uV - BQ25970_BAT_OVP_MIN_UV)
			/ BQ25970_BAT_OVP_STEP_UV;

	if (uV < BQ25970_BAT_OVP_MIN_UV)
		val = 0x0;
	if (uV > BQ25970_BAT_OVP_MAX_UV)
		val = 0x3F;

	return __bq25970_update_bits(chip, BQ25970_REG_BAT_OVP,
				     BQ25970_BAT_OVP_MASK, val);
}

static int __bq25970_set_bat_ovp_alm(struct bq25970 *chip, unsigned int uV)
{
	u8 val = (uV - BQ25970_BAT_OVP_ALM_MIN_UV)
			/ BQ25970_BAT_OVP_ALM_STEP_UV;

	if (uV < BQ25970_BAT_OVP_ALM_MIN_UV)
		val = 0x0;
	if (uV > BQ25970_BAT_OVP_ALM_MAX_UV)
		val = 0x3F;

	return __bq25970_update_bits(chip, BQ25970_REG_BAT_OVP_ALM,
				     BQ25970_BAT_OVP_ALM_MASK, val);
}

static int __bq25970_set_bat_ocp(struct bq25970 *chip, unsigned int uA)
{
	u8 val = (uA - BQ25970_BAT_OCP_MIN_UA)
			/ BQ25970_BAT_OCP_STEP_UA;

	if (uA < BQ25970_BAT_OCP_MIN_UA)
		val = 0x0;
	if (uA > BQ25970_BAT_OCP_MAX_UA)
		val = 0x7F;

	return __bq25970_update_bits(chip, BQ25970_REG_BAT_OCP,
				     BQ25970_BAT_OCP_MASK, val);
}

static int __bq25970_set_bat_ocp_alm(struct bq25970 *chip, unsigned int uA)
{
	u8 val = (uA - BQ25970_BAT_OCP_ALM_MIN_UA)
			/ BQ25970_BAT_OCP_ALM_STEP_UA;

	if (uA < BQ25970_BAT_OCP_ALM_MIN_UA)
		val = 0x0;
	if (uA > BQ25970_BAT_OCP_ALM_MAX_UA)
		val = 0x7F;

	return __bq25970_update_bits(chip, BQ25970_REG_BAT_OCP_ALM,
				     BQ25970_BAT_OCP_ALM_MASK, val);
}

static int __bq25970_set_bat_ucp_alm(struct bq25970 *chip, unsigned int uA)
{
	u8 val = (uA - BQ25970_BAT_UCP_ALM_MIN_UA)
			/ BQ25970_BAT_UCP_ALM_STEP_UA;

	if (uA < BQ25970_BAT_UCP_ALM_MIN_UA)
		val = 0x0;
	if (uA > BQ25970_BAT_UCP_ALM_MAX_UA)
		val = 0x7F;

	return __bq25970_update_bits(chip, BQ25970_REG_BAT_UCP_ALM,
				     BQ25970_BAT_UCP_ALM_MASK, val);
}

static int __bq25970_set_ac_ovp(struct bq25970 *chip, unsigned int uV)
{
	u8 val = (uV - BQ25970_AC_OVP_MIN_UV) / BQ25970_AC_OVP_STEP_UV;

	if (uV < BQ25970_AC_OVP_MIN_UV)
		val = 0x7;
	if (uV > BQ25970_AC_OVP_MAX_UV)
		val = 0x6;

	return __bq25970_update_bits(chip, BQ25970_REG_AC_PROTECTION,
				     BQ25970_AC_OVP_MASK, val);
}


static int __bq25970_set_bus_ovp(struct bq25970 *chip, unsigned int uV)
{
	u8 val = (uV - BQ25970_BUS_OVP_MIN_UV)
			/ BQ25970_BUS_OVP_STEP_UV;

	if (uV < BQ25970_BUS_OVP_MIN_UV)
		val = 0x0;
	if (uV > BQ25970_BUS_OVP_MAX_UV)
		val = 0x7F;

	return __bq25970_update_bits(chip, BQ25970_REG_BUS_OVP,
				     BQ25970_BUS_OVP_MASK, val);
}


static int __bq25970_set_bus_ovp_alm(struct bq25970 *chip, unsigned int uV)
{
	u8 val = (uV - BQ25970_BUS_OVP_ALM_MIN_UV)
			/ BQ25970_BUS_OVP_ALM_STEP_UV;

	if (uV < BQ25970_BUS_OVP_ALM_MIN_UV)
		val = 0x0;
	if (uV > BQ25970_BUS_OVP_ALM_MAX_UV)
		val = 0x7F;

	return __bq25970_update_bits(chip, BQ25970_REG_BUS_OVP_ALM,
				     BQ25970_BUS_OVP_ALM_MASK, val);
}

static int __bq25970_set_bus_ocp(struct bq25970 *chip, unsigned int uA)
{
	u8 val = (uA - BQ25970_BUS_OCP_MIN_UA)
			/ BQ25970_BUS_OCP_STEP_UA;

	if (uA < BQ25970_BUS_OCP_MIN_UA)
		val = 0x0;
	if (uA > BQ25970_BUS_OCP_MAX_UA)
		val = 0x0F;

	return __bq25970_update_bits(chip, BQ25970_REG_BUS_OCP_UCP,
				     BQ25970_BUS_OCP_MASK, val);
}

static int __bq25970_set_bus_ocp_alm(struct bq25970 *chip, unsigned int uA)
{
	u8 val = (uA - BQ25970_BUS_OCP_ALM_MIN_UA)
			/ BQ25970_BUS_OCP_ALM_STEP_UA;

	if (uA < BQ25970_BUS_OCP_ALM_MIN_UA)
		val = 0x0;
	if (uA > BQ25970_BUS_OCP_ALM_MAX_UA)
		val = 0x7F;

	return __bq25970_update_bits(chip, BQ25970_REG_BUS_OCP_ALM,
				     BQ25970_BUS_OCP_ALM_MASK, val);
}

static int __bq25970_set_fsw(struct bq25970 *chip, unsigned int hz)
{
	const unsigned int fsw_set[] = {
		187500, 250000, 300000, 375000, 500000, 750000
	};
	u8 val;

	for (val = 0; val < ARRAY_SIZE(fsw_set) - 1; val++) {
		if (hz <= fsw_set[val])
			break;
	}

	return __bq25970_update_bits(chip, BQ25970_REG_CONTROL,
				     BQ25970_FSW_SET_MASK,
				     val << BQ25970_FSW_SET_SHIFT);
}

static int __bq25970_enable_watchdog(struct bq25970 *chip, bool en)
{
	return (en ? __bq25970_clr_bits : __bq25970_set_bits)
			(chip, BQ25970_REG_CONTROL, BQ25970_WATCHDOG_DIS_MASK);
}

static int __bq25970_set_watchdog(struct bq25970 *chip, unsigned int usec)
{
	const unsigned int watchdog[] = {
		500000, 1000000, 5000000, 30000000
	};
	u8 val;

	for (val = 0; val < ARRAY_SIZE(watchdog) - 1; val++) {
		if (usec <= watchdog[val])
			break;
	}

	return __bq25970_update_bits(chip, BQ25970_REG_CONTROL,
				     BQ25970_WATCHDOG_MASK, val);
}

static bool __bq25970_is_chg_enabled(struct bq25970 *chip)
{
	return __bq25970_check_bits(chip, BQ25970_REG_CHRG_CTRL,
				    BQ25970_CHG_EN_MASK);
}

static int __bq25970_enable_chg(struct bq25970 *chip, bool en)
{
	return (en ? __bq25970_set_bits : __bq25970_clr_bits)
			(chip, BQ25970_REG_CHRG_CTRL, BQ25970_CHG_EN_MASK);
}

static bool __bq25970_is_adc_done(struct bq25970 *chip)
{
	return __bq25970_check_bits(chip, BQ25970_REG_INT_STAT,
				    BQ25970_ADC_DONE_STAT_MASK);
}

static bool __bq25970_is_adc_oneshot(struct bq25970 *chip)
{
	return __bq25970_check_bits(chip, BQ25970_REG_ADC_CTRL,
				    BQ25970_ADC_RATE_MASK);
}

static int __bq25970_set_adc_oneshot(struct bq25970 *chip, bool oneshot)
{
	return (oneshot ? __bq25970_set_bits : __bq25970_clr_bits)
			(chip, BQ25970_REG_ADC_CTRL, BQ25970_ADC_RATE_MASK);
}

static bool __bq25970_is_adc_enabled(struct bq25970 *chip)
{
	return __bq25970_check_bits(chip, BQ25970_REG_ADC_CTRL,
				    BQ25970_ADC_EN_MASK);
}

static int __bq25970_enable_adc(struct bq25970 *chip, bool en)
{
	return (en ? __bq25970_set_bits : __bq25970_clr_bits)
			(chip, BQ25970_REG_ADC_CTRL, BQ25970_ADC_EN_MASK);
}

static int __bq25970_set_adc_fn_dis(struct bq25970 *chip, u16 adc_fn_dis)
{
	u16 val = ((adc_fn_dis & 0xFF) << 8) | ((adc_fn_dis >> 8) & 0xFF);

	return __bq25970_write_word(chip, BQ25970_REG_ADC_CTRL, val);
}

static int __bq25970_set_ss_timeout(struct bq25970 *chip, unsigned int us)
{
	const unsigned int timeout[] = {
		0, 12500, 25000, 50000, 100000, 400000, 1500000, 100000000
	};
	u8 val;

	for (val = 0; val < ARRAY_SIZE(timeout); val++) {
		if (us <= timeout[val])
			break;
	}

	return __bq25970_update_bits(chip, BQ25970_REG_REG_CTRL,
				     BQ25970_SS_TIMEOUT_SET_MASK,
				     val << BQ25970_SS_TIMEOUT_SET_SHIFT);
}

static int __bq25970_set_ibat_sns_res(struct bq25970 *chip, unsigned int ohm)
{
	return ((ohm == 5) ? __bq25970_set_bits : __bq25970_clr_bits)
			(chip, BQ25970_REG_REG_CTRL,
			 BQ25970_SET_IBAT_SNS_RES_MASK);
}

static int __bq25970_set_ibus_low_dg_set(struct bq25970 *chip, unsigned int us)
{
	return ((us == 5000) ? __bq25970_set_bits : __bq25970_clr_bits)
			(chip, BQ25970_REG_DEGLITCH,
			 BQ25970_IBUS_LOW_DG_SET_MASK);
}

static int __bq25970_convert_adc(struct bq25970 *chip, int channel, u16 val)
{
	s16 sval = (s16)val;
	int conv_val = 0;

	switch (channel) {
	case BQ25970_ADC_IBUS:
	case BQ25970_ADC_VBUS:
	case BQ25970_ADC_VAC:
	case BQ25970_ADC_VOUT:
	case BQ25970_ADC_VBAT:
		/* in micro volt */
		conv_val = sval * 1000;
		dev_dbg(chip->dev, "%s adc: %duV (0x%04x)\n",
				bq25970_adc_name[channel], conv_val, val);
		break;
	case BQ25970_ADC_IBAT:
		/* in micro amp */
		conv_val = sval * 1000;
		dev_dbg(chip->dev, "%s adc: %duA (0x%04x)\n",
				bq25970_adc_name[channel], conv_val, val);
		break;
	case BQ25970_ADC_TDIE:
		/* in degreeC */
		conv_val = (10 + sval) >> 1;
		dev_dbg(chip->dev, "%s adc: %d.%cdegC (0x%04x)\n",
				bq25970_adc_name[channel],
				conv_val, (sval & 0x1 ? '5' : '0'), val);
		break;
	case BQ25970_ADC_TSBAT:
	case BQ25970_ADC_TSBUS:
		/* in percent */
		conv_val = sval * 100 / 1024;
		dev_dbg(chip->dev, "%s adc: %d%% (0x%04x)\n",
				bq25970_adc_name[channel], conv_val, val);
		break;
	}

	return conv_val;
}

static inline int __bq25970_wait_for_adc_done(struct bq25970 *chip)
{
	unsigned long ret;

	if (!__bq25970_is_adc_oneshot(chip)) {
		msleep(300);

		if (!__bq25970_is_adc_enabled(chip))
			return -EIO;

		return 0;
	}

	ret = wait_for_completion_timeout(&chip->adc_done,
			msecs_to_jiffies(300));
	if (ret == 0) {
		if (__bq25970_is_adc_done(chip))
			return 0;

		return -ETIMEDOUT;
	}

	return 0;
}

static int __bq25970_get_adc(struct bq25970 *chip, int channel, int *data)
{
	u16 val = 0;
	bool oneshot = false, wait_for_adc_done = false;
	int try;
	int ret;

	if (channel < BQ25970_ADC_IBUS || channel > BQ25970_ADC_TDIE)
		return -EINVAL;

	mutex_lock(&chip->adc_lock);

	if (!__bq25970_is_chg_enabled(chip))
		oneshot = true;

	for (try = 0; try < 5; try++) {
		if (oneshot || !__bq25970_is_adc_enabled(chip)) {
			wait_for_adc_done = true;
			reinit_completion(&chip->adc_done);
		}

		ret = __bq25970_set_adc_oneshot(chip, oneshot);
		if (ret) {
			dev_err(chip->dev, "failed to set adc rate\n");
			continue;
		}

		ret = __bq25970_enable_adc(chip, true);
		if (ret) {
			dev_err(chip->dev, "failed to enable adc\n");
			continue;
		}

		if (wait_for_adc_done) {
			ret = __bq25970_wait_for_adc_done(chip);
			if (ret) {
				dev_err(chip->dev, "error waiting adc done\n");
				continue;
			}
		}

		break;
	}

	if (!ret) {
		ret = __bq25970_read_word(chip,
				BQ25970_REG_IBUS_ADC1 + (channel << 1), &val);
		if (ret < 0)
			dev_err(chip->dev, "failed to read adc\n");
	}

	/* adc running in continuous mode. left adc enabled */
	if (oneshot) {
		if (__bq25970_enable_adc(chip, false))
			dev_err(chip->dev, "failed to disable adc\n");
	}

	mutex_unlock(&chip->adc_lock);

	if (ret < 0) {
		dev_err(chip->dev, "failed to get %s adc (%d)\n",
				bq25970_adc_name[channel], ret);
		return ret;
	}

	val = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
	*data = __bq25970_convert_adc(chip, channel, val);

	return 0;
}

static u32 __to_flag(u8 reg, u8 val)
{
	u32 flag = 0;

	switch (reg) {
	case BQ25970_REG_AC_PROTECTION:
		if (val & BQ25970_AC_OVP_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_AC_OVP);
		break;
	case BQ25970_REG_BUS_OCP_UCP:
		if (val & BQ25970_IBUS_UCP_RISE_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_IBUS_UCP_RISE);
		if (val & BQ25970_IBUS_UCP_FALL_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_IBUS_UCP_FALL);
		break;
	case BQ25970_REG_CONVERTER_STATE:
		if (val & BQ25970_TSHUT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_TSHUT);
		if (val & BQ25970_SS_TIMEOUT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_SS_TIMEOUT);
		if (val & BQ25970_CONV_OCP_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_CONV_OCP);
		if (val & BQ25970_PIN_DIAG_FAIL_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_PIN_DIAG_FAIL);
		break;
	case BQ25970_REG_CONTROL:
		if (val & BQ25970_WD_TIMEOUT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_WD_TIMEOUT);
		break;
	case BQ25970_REG_INT_FLAG:
		if (val & BQ25970_BAT_OVP_ALM_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BAT_OVP_ALM);
		if (val & BQ25970_BAT_OCP_ALM_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BAT_OCP_ALM);
		if (val & BQ25970_BUS_OVP_ALM_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BUS_OVP_ALM);
		if (val & BQ25970_BUS_OCP_ALM_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BUS_OCP_ALM);
		if (val & BQ25970_BAT_UCP_ALM_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BAT_UCP_ALM);
		if (val & BQ25970_ADAPTER_INSERT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_ADAPTER_INSERT);
		if (val & BQ25970_VBAT_INSERT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_VBAT_INSERT);
		if (val & BQ25970_ADC_DONE_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_ADC_DONE);
		break;
	case BQ25970_REG_FLT_FLAG:
		if (val & BQ25970_BAT_OVP_FLT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BAT_OVP_FLT);
		if (val & BQ25970_BAT_OCP_FLT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BAT_OCP_FLT);
		if (val & BQ25970_BUS_OVP_FLT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BUS_OVP_FLT);
		if (val & BQ25970_BUS_OCP_FLT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_BUS_OCP_FLT);
		if (val & BQ25970_TSBUS_TSBAT_ALM_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_TSBUS_TSBAT_ALM);
		if (val & BQ25970_TSBAT_FLT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_TSBAT_FLT);
		if (val & BQ25970_TSBUS_FLT_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_TSBUS_FLT);
		if (val & BQ25970_TDIE_ALM_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_TDIE_ALM);
		break;
	case BQ25970_REG_REG_FLAG_MASK:
		if (val & BQ25970_VBATREG_ACTIVE_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_VBATREG_ACTIVE);
		if (val & BQ25970_IBATREG_ACTIVE_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_IBATREG_ACTIVE);
		if (val & BQ25970_VDROP_OVP_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_VDROP_OVP);
		if (val & BQ25970_VOUT_OVP_FLAG_MASK)
			flag |= BIT(BQ25970_IRQFLAG_VOUT_OVP);
		break;
	}

	return flag;
}

static u32 __bq25970_get_flag(struct bq25970 *chip)
{
	const u8 reg[] = {
		BQ25970_REG_AC_PROTECTION,
		BQ25970_REG_BUS_OCP_UCP,
		BQ25970_REG_CONVERTER_STATE,
		BQ25970_REG_CONTROL,
		BQ25970_REG_INT_FLAG,
		BQ25970_REG_FLT_FLAG,
		BQ25970_REG_REG_FLAG_MASK
	};
	u8 val;
	u32 flag = 0;
	unsigned int i;
	int ret;

	mutex_lock(&chip->rw_lock);

	for (i = 0; i < ARRAY_SIZE(reg); i++) {
		ret = __bq25970_read(chip, reg[i], &val);
		if (ret) {
			dev_err(chip->dev, "failed to read flag reg %02xh\n",
				reg[i]);
			continue;
		}
		flag |= __to_flag(reg[i], val);
	}

	mutex_unlock(&chip->rw_lock);

	return flag;
}

static u32 __to_stat(u8 reg, u8 val)
{
	u32 stat = 0;

	switch (reg) {
	case BQ25970_REG_AC_PROTECTION:
		if (val & BQ25970_AC_OVP_STAT_MASK)
			stat |= BIT(BQ25970_STAT_AC_OVP);
		break;
	case BQ25970_REG_CONVERTER_STATE:
		if (val & BQ25970_TSHUT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_TSHUT);
		if (val & BQ25970_VBUS_ERRORLO_STAT_MASK)
			stat |= BIT(BQ25970_STAT_VBUS_ERRORLO);
		if (val & BQ25970_VBUS_ERRORHI_STAT_MASK)
			stat |= BIT(BQ25970_STAT_VBUS_ERRORHI);
		if (val & BQ25970_CONV_SWITCHING_STAT_MASK)
			stat |= BIT(BQ25970_STAT_CONV_SWITCHING);
		break;
	case BQ25970_REG_INT_STAT:
		if (val & BQ25970_BAT_OVP_ALM_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BAT_OVP_ALM);
		if (val & BQ25970_BAT_OCP_ALM_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BAT_OCP_ALM);
		if (val & BQ25970_BUS_OVP_ALM_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BUS_OVP_ALM);
		if (val & BQ25970_BUS_OCP_ALM_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BUS_OCP_ALM);
		if (val & BQ25970_BAT_UCP_ALM_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BAT_UCP_ALM);
		if (val & BQ25970_ADAPTER_INSERT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_ADAPTER_INSERT);
		if (val & BQ25970_VBAT_INSERT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_VBAT_INSERT);
		if (val & BQ25970_ADC_DONE_STAT_MASK)
			stat |= BIT(BQ25970_STAT_ADC_DONE);
		break;
	case BQ25970_REG_FLT_STAT:
		if (val & BQ25970_BAT_OVP_FLT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BAT_OVP_FLT);
		if (val & BQ25970_BAT_OCP_FLT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BAT_OCP_FLT);
		if (val & BQ25970_BUS_OVP_FLT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BUS_OVP_FLT);
		if (val & BQ25970_BUS_OCP_FLT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_BUS_OCP_FLT);
		if (val & BQ25970_TSBUS_TSBAT_ALM_STAT_MASK)
			stat |= BIT(BQ25970_STAT_TSBUS_TSBAT_ALM);
		if (val & BQ25970_TSBAT_FLT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_TSBAT_FLT);
		if (val & BQ25970_TSBUS_FLT_STAT_MASK)
			stat |= BIT(BQ25970_STAT_TSBUS_FLT);
		if (val & BQ25970_TDIE_ALM_STAT_MASK)
			stat |= BIT(BQ25970_STAT_TDIE_ALM);
		break;
	case BQ25970_REG_REG_THRESHOLD:
		if (val & BQ25970_VBATREG_ACTIVE_STAT_MASK)
			stat |= BIT(BQ25970_STAT_VBATREG_ACTIVE);
		if (val & BQ25970_IBATREG_ACTIVE_STAT_MASK)
			stat |= BIT(BQ25970_STAT_IBATREG_ACTIVE);
		if (val & BQ25970_VDROP_OVP_STAT_MASK)
			stat |= BIT(BQ25970_STAT_VDROP_OVP);
		if (val & BQ25970_VOUT_OVP_STAT_MASK)
			stat |= BIT(BQ25970_STAT_VOUT_OVP);
		break;
	}

	return stat;
}

static u32 __bq25970_get_stat(struct bq25970 *chip)
{
	const u8 reg[] = {
		BQ25970_REG_AC_PROTECTION,
		BQ25970_REG_CONVERTER_STATE,
		BQ25970_REG_INT_STAT,
		BQ25970_REG_FLT_STAT,
		BQ25970_REG_REG_THRESHOLD
	};
	u8 val;
	u32 stat = 0;
	unsigned int i;
	int ret;

	mutex_lock(&chip->rw_lock);

	for (i = 0; i < ARRAY_SIZE(reg); i++) {
		ret = __bq25970_read(chip, reg[i], &val);
		if (ret) {
			dev_err(chip->dev, "failed to read stat "
				"reg 0x%02x\n", reg[i]);
			continue;
		}
		stat |= __to_stat(reg[i], val);
	}

	mutex_unlock(&chip->rw_lock);

	return stat;
}

static int __bq25970_set_irqmask(struct bq25970 *chip, u32 irqmask)
{
	const u8 reg[] = {
		BQ25970_REG_AC_PROTECTION,
		BQ25970_REG_BUS_OCP_UCP,
		BQ25970_REG_INT_MASK,
		BQ25970_REG_FLT_MASK,
		BQ25970_REG_REG_FLAG_MASK,
	};
	u8 mask[] = {
		BIT(5),		/* BQ25970_REG_AC_PROTECTION */
		BIT(5),		/* BQ25970_REG_BUS_OCP_UCP */
		(0xFF),		/* BQ25970_REG_INT_MASK */
		(0xFF),		/* BQ25970_REG_FLT_MASK */
		(0x0F),		/* BQ25970_REG_REG_FLAG_MASK */
	};
	u8 val[] = {
		/* BQ25970_REG_AC_PROTECTION */
		(irqmask & BIT(BQ25970_IRQFLAG_AC_OVP) ? BIT(5) : 0),
		/* BQ25970_REG_BUS_OCP_UCP */
		(irqmask & BIT(BQ25970_IRQFLAG_IBUS_UCP_RISE) ? BIT(5) : 0),
		/* BQ25970_REG_INT_MASK */
		((irqmask & BIT(BQ25970_IRQFLAG_BAT_OVP_ALM) ? BIT(7) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_BAT_OCP_ALM) ? BIT(6) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_BUS_OVP_ALM) ? BIT(5) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_BUS_OCP_ALM) ? BIT(4) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_BAT_UCP_ALM) ? BIT(3) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_ADAPTER_INSERT) ? BIT(2) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_VBAT_INSERT) ? BIT(1) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_ADC_DONE) ? BIT(0) : 0)),
		/* BQ25970_REG_FLT_MASK */
		((irqmask & BIT(BQ25970_IRQFLAG_BAT_OVP_FLT) ? BIT(7) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_BAT_OCP_FLT) ? BIT(6) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_BUS_OVP_FLT) ? BIT(5) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_BUS_OCP_FLT) ? BIT(4) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_TSBUS_TSBAT_ALM) ? BIT(3) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_TSBAT_FLT) ? BIT(2) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_TSBUS_FLT) ? BIT(1) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_TDIE_ALM) ? BIT(0) : 0)),
		/* BQ25970_REG_REG_FLAG_MASK */
		((irqmask & BIT(BQ25970_IRQFLAG_VBATREG_ACTIVE) ? BIT(3) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_IBATREG_ACTIVE) ? BIT(2) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_VDROP_OVP) ? BIT(1) : 0)
		| (irqmask & BIT(BQ25970_IRQFLAG_VOUT_OVP) ? BIT(0) : 0)),
	};
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(reg); i++) {
		ret = __bq25970_update_bits(chip, reg[i], mask[i], val[i]);
		if (ret) {
			dev_err(chip->dev, "failed to set irqmask "
				"reg 0x%02x\n", reg[i]);
		}
	}

	chip->irqmask = irqmask;

	return 0;
}

static int __bq25970_set_protection(struct bq25970 *chip)
{
	const u8 reg[] = {
		BQ25970_REG_BAT_OVP,
		BQ25970_REG_BAT_OVP_ALM,
		BQ25970_REG_BAT_OCP,
		BQ25970_REG_BAT_OCP_ALM,
		BQ25970_REG_BAT_UCP_ALM,
		BQ25970_REG_BUS_OVP_ALM,
		BQ25970_REG_BUS_OCP_UCP,
		BQ25970_REG_BUS_OCP_ALM,
		BQ25970_REG_CHRG_CTRL,
	};
	u8 mask[] = {
		BIT(7),				/* BQ25970_REG_BAT_OVP */
		BIT(7),				/* BQ25970_REG_BAT_OVP_ALM */
		BIT(7),				/* BQ25970_REG_BAT_OCP */
		BIT(7),				/* BQ25970_REG_BAT_OCP_ALM */
		BIT(7),				/* BQ25970_REG_BAT_UCP_ALM */
		BIT(7),				/* BQ25970_REG_BUS_OVP_ALM */
		BIT(7),				/* BQ25970_REG_BUS_OCP_UCP */
		BIT(7),				/* BQ25970_REG_BUS_OCP_ALM */
		BIT(2) | BIT(1) | BIT(0),	/* BQ25970_REG_CHRG_CTRL */
	};
	u8 val[] = {
		/* BQ25970_REG_BAT_OVP */
		(chip->cfg.bat_ovp_dis ? BIT(7) : 0),
		/* BQ25970_REG_BAT_OVP_ALM */
		(chip->cfg.bat_ovp_alm_dis ? BIT(7) : 0),
		/* BQ25970_REG_BAT_OCP */
		(chip->cfg.bat_ocp_dis ? BIT(7) : 0),
		/* BQ25970_REG_BAT_OCP_ALM */
		(chip->cfg.bat_ocp_alm_dis ? BIT(7) : 0),
		/* BQ25970_REG_BAT_UCP_ALM */
		(chip->cfg.bat_ucp_alm_dis ? BIT(7) : 0),
		/* BQ25970_REG_BUS_OVP_ALM */
		(chip->cfg.bus_ovp_alm_dis ? BIT(7) : 0),
		/* BQ25970_REG_BUS_OCP_UCP */
		(chip->cfg.bus_ocp_dis ? BIT(7) : 0),
		/* BQ25970_REG_BUS_OCP_ALM */
		(chip->cfg.bus_ocp_alm_dis ? BIT(7) : 0),
		/* BQ25970_REG_CHRG_CTRL */
		((chip->cfg.tsbus_dis ? BIT(2) : 0)
		| (chip->cfg.tsbat_dis ? BIT(1) : 0)
		| (chip->cfg.tdie_dis ? BIT(0) : 0)),
	};
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(reg); i++) {
		ret = __bq25970_update_bits(chip, reg[i], mask[i], val[i]);
		if (ret) {
			dev_err(chip->dev, "failed to set protection "
				"reg 0x%02x\n", reg[i]);
		}
	}

	return 0;
}

/* irq handlers */
static void bq25970_irq_ac_ovp(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_AC_OVP)))
		return;

	dev_err(chip->dev, "ac_ovp\n");
}

static void bq25970_irq_ibus_ucp_rise(struct bq25970 *chip, u32 stat)
{
	dev_info(chip->dev, "ibus_ucp_rise\n");
}

static void bq25970_irq_ibus_ucp_fall(struct bq25970 *chip, u32 stat)
{
	dev_warn(chip->dev, "ibus_ucp_fall\n");

	if (!chip->chg_dev)
		return;

	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_IBUSUCP_FALL);
}

static void bq25970_irq_tshut(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_TSHUT)))
		return;

	dev_err(chip->dev, "tshut\n");
}

static void bq25970_irq_ss_timeout(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "ss_timeout\n");
}

static void bq25970_irq_conv_ocp(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "conv_ocp\n");
}

static void bq25970_irq_pin_diag_fail(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "pin_diag_fial\n");
}

static void bq25970_irq_wd_timeout(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "wd_timeout\n");
}

static void bq25970_irq_bat_ovp_alm(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_BAT_OVP_ALM)))
		return;

	dev_info(chip->dev, "bat_ovp_alm\n");

	if (!chip->chg_dev)
		return;
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VBATOVP_ALARM);
}

static void bq25970_irq_bat_ocp_alm(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_BAT_OCP_ALM)))
		return;

	dev_info(chip->dev, "bat_ocp_alm\n");
}

static void bq25970_irq_bus_ovp_alm(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_BUS_OVP_ALM)))
		return;

	dev_info(chip->dev, "bus_ovp_alm\n");

	if (!chip->chg_dev)
		return;
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VBUSOVP_ALARM);
}

static void bq25970_irq_bus_ocp_alm(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_BUS_OCP_ALM)))
		return;

	dev_info(chip->dev, "bus_ocp_alm\n");
}

static void bq25970_irq_bat_ucp_alm(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_BAT_UCP_ALM)))
		return;

	dev_info(chip->dev, "bat_ucp_alm\n");
}

static void bq25970_irq_adc_done(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_ADC_DONE)))
		return;

	complete(&chip->adc_done);
}

static void bq25970_irq_bat_ovp_flt(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "bat_ovp_flt\n");
}

static void bq25970_irq_bat_ocp_flt(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "bat_ocp_flt\n");

	if (!chip->chg_dev)
		return;
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_IBATOCP);
}

static void bq25970_irq_bus_ovp_flt(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "bus_ovp_flt\n");
}

static void bq25970_irq_bus_ocp_flt(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "bus_ocp_flt\n");

	if (!chip->chg_dev)
		return;
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_IBUSOCP);
}

static void bq25970_irq_tsbus_tsbat_alm(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_TSBUS_TSBAT_ALM)))
		return;

	dev_info(chip->dev, "tsbus_tsbat_alm\n");
}

static void bq25970_irq_tsbat_flt(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "tsbat_flt\n");
}

static void bq25970_irq_tsbus_flt(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "tsbus_flt\n");
}

static void bq25970_irq_tdie_alm(struct bq25970 *chip, u32 stat)
{
	if (!(stat & BIT(BQ25970_STAT_TDIE_ALM)))
		return;

	dev_info(chip->dev, "tdie_alm\n");
}

static void bq25970_irq_vdrop_ovp(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "vdrop_ovp\n");

	if (!chip->chg_dev)
		return;
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VOUTOVP);
}

static void bq25970_irq_vout_ovp(struct bq25970 *chip, u32 stat)
{
	dev_err(chip->dev, "vout_ovp\n");

	if (!chip->chg_dev)
		return;
	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VDROVP);
}

static void (*bq25970_irq_handlers[])(struct bq25970 *chip, u32 stat) = {
	[BQ25970_IRQFLAG_AC_OVP] = bq25970_irq_ac_ovp,
	[BQ25970_IRQFLAG_IBUS_UCP_RISE] = bq25970_irq_ibus_ucp_rise,
	[BQ25970_IRQFLAG_IBUS_UCP_FALL] = bq25970_irq_ibus_ucp_fall,
	[BQ25970_IRQFLAG_TSHUT] = bq25970_irq_tshut,
	[BQ25970_IRQFLAG_SS_TIMEOUT] = bq25970_irq_ss_timeout,
	[BQ25970_IRQFLAG_CONV_OCP] = bq25970_irq_conv_ocp,
	[BQ25970_IRQFLAG_PIN_DIAG_FAIL] = bq25970_irq_pin_diag_fail,
	[BQ25970_IRQFLAG_WD_TIMEOUT] = bq25970_irq_wd_timeout,
	[BQ25970_IRQFLAG_BAT_OVP_ALM] = bq25970_irq_bat_ovp_alm,
	[BQ25970_IRQFLAG_BAT_OCP_ALM] = bq25970_irq_bat_ocp_alm,
	[BQ25970_IRQFLAG_BUS_OVP_ALM] = bq25970_irq_bus_ovp_alm,
	[BQ25970_IRQFLAG_BUS_OCP_ALM] = bq25970_irq_bus_ocp_alm,
	[BQ25970_IRQFLAG_BAT_UCP_ALM] = bq25970_irq_bat_ucp_alm,
	[BQ25970_IRQFLAG_ADAPTER_INSERT] = NULL,
	[BQ25970_IRQFLAG_VBAT_INSERT] = NULL,
	[BQ25970_IRQFLAG_ADC_DONE] = bq25970_irq_adc_done,
	[BQ25970_IRQFLAG_BAT_OVP_FLT] = bq25970_irq_bat_ovp_flt,
	[BQ25970_IRQFLAG_BAT_OCP_FLT] = bq25970_irq_bat_ocp_flt,
	[BQ25970_IRQFLAG_BUS_OVP_FLT] = bq25970_irq_bus_ovp_flt,
	[BQ25970_IRQFLAG_BUS_OCP_FLT] = bq25970_irq_bus_ocp_flt,
	[BQ25970_IRQFLAG_TSBUS_TSBAT_ALM] = bq25970_irq_tsbus_tsbat_alm,
	[BQ25970_IRQFLAG_TSBAT_FLT] = bq25970_irq_tsbat_flt,
	[BQ25970_IRQFLAG_TSBUS_FLT] = bq25970_irq_tsbus_flt,
	[BQ25970_IRQFLAG_TDIE_ALM] = bq25970_irq_tdie_alm,
	[BQ25970_IRQFLAG_VBATREG_ACTIVE] = NULL,
	[BQ25970_IRQFLAG_IBATREG_ACTIVE] = NULL,
	[BQ25970_IRQFLAG_VDROP_OVP] = bq25970_irq_vdrop_ovp,
	[BQ25970_IRQFLAG_VOUT_OVP] = bq25970_irq_vout_ovp,
};

static irqreturn_t bq25970_irq_handler(int irq, void *data)
{
	struct bq25970 *chip = data;
	u32 flag, mask, stat;
	unsigned int i;

	flag = __bq25970_get_flag(chip);
	stat = __bq25970_get_stat(chip);
	mask = ~chip->irqmask;

	dev_dbg(chip->dev, "flag: 0x%08x (0x%08x), stat: 0x%08x\n",
			flag, mask, stat);
	flag &= mask;

	for (i = 0; i < ARRAY_SIZE(bq25970_irq_handlers); i++) {
		if ((flag & BIT(i)) && bq25970_irq_handlers[i])
			bq25970_irq_handlers[i](chip, stat);
	}

	return IRQ_HANDLED;
}

/* charger interface */
static int bq25970_enable_chg(struct charger_device *chg_dev, bool en)
{
	static const u32 fault = BIT(BQ25970_STAT_AC_OVP)
			| BIT(BQ25970_STAT_BAT_OVP_FLT)
			| BIT(BQ25970_STAT_BUS_OVP_FLT)
			| BIT(BQ25970_STAT_TSBAT_FLT)
			| BIT(BQ25970_STAT_TSBUS_FLT);

	struct bq25970 *chip = charger_get_data(chg_dev);
	u32 stat;
	int ret;

	if (!en) {
		ret = __bq25970_set_irqmask(chip, irqmask_default);
		if (ret < 0)
			dev_err(chip->dev, "failed to set irqmask\n");

		ret = __bq25970_enable_chg(chip, false);
		if (ret < 0) {
			dev_err(chip->dev, "failed to disable chg\n");
			return ret;
		}

		mutex_lock(&chip->adc_lock);

		ret = __bq25970_enable_adc(chip, false);
		if (ret < 0) {
			dev_err(chip->dev, "failed to disable adc\n");
			return ret;
		}

		mutex_unlock(&chip->adc_lock);

		goto out;
	}

	stat = __bq25970_get_stat(chip);
	if (stat & fault) {
		dev_err(chip->dev, "cannot enable chg. fault = 0x%08x\n",
				stat & fault);
		return -EPERM;
	}

	/* clear irqs */
	__bq25970_get_flag(chip);

	ret = __bq25970_set_irqmask(chip, chip->cfg.irqmask);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set irqmask\n");
		return ret;
	}

	ret = __bq25970_enable_chg(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "failed to enable chg\n");
		return ret;
	}

	if (en && !__bq25970_is_chg_enabled(chip)) {
		dev_err(chip->dev, "chg not enabled\n");
		return -EIO;
	}

out:
	if (chip->cfg.watchdog_dis)
		return 0;

	ret = __bq25970_enable_watchdog(chip, en);
	if (ret < 0) {
		dev_err(chip->dev, "failed to enable watchdog\n");
		return ret;
	}

	return 0;
}

static int bq25970_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
	struct bq25970 *chip = charger_get_data(chg_dev);

	*en = __bq25970_is_chg_enabled(chip);

	return 0;
}

static int bq25970_get_adc(struct charger_device *chg_dev,
			   enum adc_channel chan, int *min, int *max)
{
	struct bq25970 *chip = charger_get_data(chg_dev);
	int channel;
	int ret;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		channel = BQ25970_ADC_VBUS;
		break;
	case ADC_CHANNEL_VBAT:
		channel = BQ25970_ADC_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		channel = BQ25970_ADC_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		channel = BQ25970_ADC_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		channel = BQ25970_ADC_TDIE;
		break;
	case ADC_CHANNEL_VOUT:
		channel = BQ25970_ADC_VOUT;
		break;
	default:
		return -ENOTSUPP;
	}

	if (!min || !max)
		return -EINVAL;

	ret = __bq25970_get_adc(chip, channel, max);
	if (ret < 0)
		*max = 0;

	*min = *max;

	return ret;
}

static int bq25970_get_adc_accuracy(struct charger_device *chg_dev,
				    enum adc_channel chan, int *min, int *max)
{
	switch (chan) {
	case ADC_CHANNEL_VBUS:
		*min = 35000;
		*max = 35000;
		break;
	case ADC_CHANNEL_VBAT:
		*min = 20000;
		*max = 20000;
		break;
	case ADC_CHANNEL_IBUS:
		*min = 150000;
		*max = 150000;
		break;
	case ADC_CHANNEL_IBAT:
		*min = 200000;
		*max = 200000;
		break;
	case ADC_CHANNEL_TEMP_JC:
		*min = 4;
		*max = 4;
		break;
	case ADC_CHANNEL_VOUT:
		*min = 20000;
		*max = 20000;
		break;
	default:
		*min = 0;
		*max = 0;
		break;
	}

	return 0;
}

static int bq25970_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
	struct bq25970 *chip = charger_get_data(chg_dev);

	return __bq25970_set_bus_ovp(chip, uV);
}

static int bq25970_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
	struct bq25970 *chip = charger_get_data(chg_dev);

	/* uA will be 110% of target */
	__bq25970_set_bus_ocp_alm(chip, uA / 110 * 100);

	return __bq25970_set_bus_ocp(chip, uA);
}

static int bq25970_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
	struct bq25970 *chip = charger_get_data(chg_dev);

	return __bq25970_set_bat_ovp(chip, uV);
}

static int bq25970_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	struct bq25970 *chip = charger_get_data(chg_dev);
	int ret;

	mutex_lock(&chip->ops_lock);

	ret = __bq25970_set_bat_ovp_alm(chip, uV);

	mutex_unlock(&chip->ops_lock);

	return ret;
}

static int bq25970_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
	struct bq25970 *chip = charger_get_data(chg_dev);

	mutex_lock(&chip->ops_lock);

	__bq25970_set_bits(chip, BQ25970_REG_BAT_OVP_ALM,
			BQ25970_BAT_OVP_ALM_DIS_MASK);
	__bq25970_clr_bits(chip, BQ25970_REG_BAT_OVP_ALM,
			BQ25970_BAT_OVP_ALM_DIS_MASK);

	mutex_unlock(&chip->ops_lock);

	return 0;
}

static int bq25970_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	struct bq25970 *chip = charger_get_data(chg_dev);
	int ret;

	mutex_lock(&chip->ops_lock);

	ret = __bq25970_set_bus_ovp_alm(chip, uV);

	mutex_unlock(&chip->ops_lock);

	return ret;
}

static int bq25970_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
	struct bq25970 *chip = charger_get_data(chg_dev);
	u32 stat = __bq25970_get_stat(chip);

	*err = false;
	if (stat & BIT(BQ25970_STAT_VBUS_ERRORLO))
		*err = true;

	return 0;
}

static int bq25970_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
	struct bq25970 *chip = charger_get_data(chg_dev);

	mutex_lock(&chip->ops_lock);

	__bq25970_set_bits(chip, BQ25970_REG_BUS_OVP_ALM,
			BQ25970_BUS_OVP_ALM_DIS_MASK);
	__bq25970_clr_bits(chip, BQ25970_REG_BUS_OVP_ALM,
			BQ25970_BUS_OVP_ALM_DIS_MASK);

	mutex_unlock(&chip->ops_lock);

	return 0;
}

static int bq25970_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
	struct bq25970 *chip = charger_get_data(chg_dev);

	/* uA will be 110% of target */
	__bq25970_set_bat_ocp_alm(chip, uA / 110 * 100);

	return __bq25970_set_bat_ocp(chip, uA);
}
/* dummy function for init_chip */
static int bq25970_init_chip(struct charger_device *chg_dev)
{
	return 0;
}

static const struct charger_ops bq25970_chg_ops = {
	.enable = bq25970_enable_chg,
	.is_enabled = bq25970_is_chg_enabled,
	.get_adc = bq25970_get_adc,
	.set_vbusovp = bq25970_set_vbusovp,
	.set_ibusocp = bq25970_set_ibusocp,
	.set_vbatovp = bq25970_set_vbatovp,
	.set_ibatocp = bq25970_set_ibatocp,
	.set_vbatovp_alarm = bq25970_set_vbatovp_alarm,
	.reset_vbatovp_alarm = bq25970_reset_vbatovp_alarm,
	.set_vbusovp_alarm = bq25970_set_vbusovp_alarm,
	.reset_vbusovp_alarm = bq25970_reset_vbusovp_alarm,
	.is_vbuslowerr = bq25970_is_vbuslowerr,
	.get_adc_accuracy = bq25970_get_adc_accuracy,
	.init_chip = bq25970_init_chip,
};

/* debugfs interface */
static int debugfs_get_data(void *data, u64 *val)
{
	struct bq25970 *chip = data;
	int ret;
	u8 temp;

	ret = __bq25970_read(chip, chip->debug_addr, &temp);
	if (ret)
		return -EAGAIN;

	*val = temp;

	return 0;
}

static int debugfs_set_data(void *data, u64 val)
{
	struct bq25970 *chip = data;
	int ret;
	u8 temp;

	temp = (u8)val;
	ret = __bq25970_write(chip, chip->debug_addr, temp);
	if (ret)
		return -EAGAIN;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(data_debugfs_ops,
	debugfs_get_data, debugfs_set_data, "0x%02llx\n");

static int dump_debugfs_show(struct seq_file *m, void *start)
{
	struct bq25970 *chip = m->private;
	u8 reg, val;
	int ret;

	for (reg = BQ25970_REG_BAT_OVP; reg < BQ25970_REG_DEGLITCH; reg++) {
		ret = __bq25970_read(chip, reg, &val);
		if (ret) {
			seq_printf(m, "0x%02x = error\n", reg);
			continue;
		}

		seq_printf(m, "0x%02x = 0x%02x\n", reg, val);
	}

	return 0;
}

static int dump_debugfs_open(struct inode *inode, struct file *file)
{
	struct bq25970 *chip = inode->i_private;

	return single_open(file, dump_debugfs_show, chip);
}

static const struct file_operations dump_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debugfs_entries(struct bq25970 *chip)
{
	struct dentry *ent;

	chip->debugfs = debugfs_create_dir(chip->chg_prop.alias_name, NULL);
	if (!chip->debugfs) {
		dev_err(chip->dev, "failed to create debugfs\n");
		return -ENODEV;
	}

	ent = debugfs_create_x8("addr", S_IFREG | S_IWUSR | S_IRUGO,
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

static int bq25970_charger_device_register(struct bq25970 *chip)
{
	chip->chg_prop.alias_name = "bq25970_standalone";
	chip->chg_dev = charger_device_register("primary_divider_chg",
			chip->dev, chip, &bq25970_chg_ops, &chip->chg_prop);
	if (!chip->chg_dev)
		return -EINVAL;

	return 0;
}

static int bq25970_irq_init(struct bq25970 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int irq_gpio, irq;
	int ret = 0;

	irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (irq_gpio < 0)
		return -EINVAL;

	irq = gpio_to_irq(irq_gpio);
	if (irq < 0)
		return -ENODEV;

	ret = devm_gpio_request_one(chip->dev, irq_gpio, GPIOF_DIR_IN,
				    "bq25970_irq");
	if (ret) {
		dev_err(chip->dev, "failed to request gpio\n");
		return ret;
	}

	ret = devm_request_threaded_irq(chip->dev, irq, NULL,
					bq25970_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"bq25970_irq", chip);
	if (ret) {
		dev_err(chip->dev, "failed to request irq %d\n", irq);
		return ret;
	}

	return ret;
}

static int bq25970_hw_init(struct bq25970 *chip)
{
	int ret = 0;

	ret = __bq25970_set_bat_ucp_alm(chip, 2000000);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bat_ucp_alm\n");
		return ret;
	}

	ret = __bq25970_set_ac_ovp(chip, chip->cfg.ac_ovp);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set ac_ovp\n");
		return ret;
	}

	ret = __bq25970_set_fsw(chip, chip->cfg.fsw_set);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set fsw\n");
		return ret;
	}

	ret = __bq25970_set_watchdog(chip, chip->cfg.watchdog);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set watchdog\n");
		return ret;
	}

	ret = __bq25970_enable_watchdog(chip, false);
	if (ret < 0) {
		dev_err(chip->dev, "failed to disable watchdog\n");
		return ret;
	}

	ret = __bq25970_set_adc_fn_dis(chip, chip->cfg.adc_fn_dis);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set adc channel\n");
		return ret;
	}

	ret = __bq25970_set_ibat_sns_res(chip, chip->cfg.ibat_sns_res);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set ibat_sns_res\n");
		return ret;
	}

	ret = __bq25970_set_ss_timeout(chip, chip->cfg.ss_timeout);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set ss_timeout\n");
		return ret;
	}

	ret = __bq25970_set_ibus_low_dg_set(chip, chip->cfg.ibus_low_dg_set);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set ibus_low_dg_set\n");
		return ret;
	}

	ret = __bq25970_set_irqmask(chip, irqmask_default);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set mask\n");
		return ret;
	}

	ret = __bq25970_set_protection(chip);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set protection\n");
		return ret;
	}

	/* clear irqs */
	__bq25970_get_flag(chip);

	return ret;
}

static void bq25970_parse_dt_adc(struct bq25970 *chip, struct device_node *np)
{
	chip->cfg.adc_fn_dis = 0;

	if (of_property_read_bool(np, "ibus_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(8);
	if (of_property_read_bool(np, "vbus_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(7);
	if (of_property_read_bool(np, "vac_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(6);
	if (of_property_read_bool(np, "vout_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(5);
	if (of_property_read_bool(np, "vbat_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(4);
	if (of_property_read_bool(np, "ibat_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(3);
	if (of_property_read_bool(np, "tsbus_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(2);
	if (of_property_read_bool(np, "tsbat_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(1);
	if (of_property_read_bool(np, "tdie_adc_dis"))
		chip->cfg.adc_fn_dis |= BIT(0);
}

static void bq25970_parse_dt_irq(struct bq25970 *chip, struct device_node *np)
{
	chip->cfg.irqmask = 0;

	if (of_property_read_bool(np, "ac_ovp_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_AC_OVP);
	if (of_property_read_bool(np, "ibus_ucp_rise_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_IBUS_UCP_RISE);
	if (of_property_read_bool(np, "bat_ovp_alm_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BAT_OVP_ALM);
	if (of_property_read_bool(np, "bat_ocp_alm_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BAT_OCP_ALM);
	if (of_property_read_bool(np, "bus_ovp_alm_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BUS_OVP_ALM);
	if (of_property_read_bool(np, "bus_ocp_alm_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BUS_OCP_ALM);
	if (of_property_read_bool(np, "bat_ucp_alm_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BAT_UCP_ALM);
	if (of_property_read_bool(np, "adapter_insert_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_ADAPTER_INSERT);
	if (of_property_read_bool(np, "vbat_insert_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_VBAT_INSERT);
	if (of_property_read_bool(np, "adc_done_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_ADC_DONE);
	if (of_property_read_bool(np, "bat_ovp_flt_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BAT_OVP_FLT);
	if (of_property_read_bool(np, "bat_ocp_flt_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BAT_OCP_FLT);
	if (of_property_read_bool(np, "bus_ovp_flt_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BUS_OVP_FLT);
	if (of_property_read_bool(np, "bus_ocp_flt_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_BUS_OCP_FLT);
	if (of_property_read_bool(np, "tsbus_tsbat_alm_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_TSBUS_TSBAT_ALM);
	if (of_property_read_bool(np, "tsbat_flt_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_TSBAT_FLT);
	if (of_property_read_bool(np, "tsbus_flt_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_TSBUS_FLT);
	if (of_property_read_bool(np, "tdie_alm_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_TDIE_ALM);
	if (of_property_read_bool(np, "vbatreg_active_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_VBATREG_ACTIVE);
	if (of_property_read_bool(np, "ibatreg_active_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_IBATREG_ACTIVE);
	if (of_property_read_bool(np, "vdrop_ovp_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_VDROP_OVP);
	if (of_property_read_bool(np, "vout_ovp_mask"))
		chip->cfg.irqmask |= BIT(BQ25970_IRQFLAG_VOUT_OVP);
}

static void bq25970_parse_dt_protection(struct bq25970 *chip,
					struct device_node *np)
{
	chip->cfg.bat_ovp_dis = of_property_read_bool(np, "bat_ovp_dis");
	chip->cfg.bat_ovp_alm_dis =
			of_property_read_bool(np, "bat_ovp_alm_dis");
	chip->cfg.bat_ocp_dis = of_property_read_bool(np, "bat_ocp_dis");
	chip->cfg.bat_ocp_alm_dis =
			of_property_read_bool(np, "bat_ocp_alm_dis");
	chip->cfg.bat_ucp_alm_dis =
			of_property_read_bool(np, "bat_ucp_alm_dis");
	chip->cfg.bus_ovp_alm_dis =
			of_property_read_bool(np, "bus_ovp_alm_dis");
	chip->cfg.bus_ocp_dis = of_property_read_bool(np, "bus_ocp_dis");
	chip->cfg.bus_ocp_alm_dis =
			of_property_read_bool(np, "bus_ocp_alm_dis");
	chip->cfg.tsbus_dis = of_property_read_bool(np, "tsbus_dis");
	chip->cfg.tsbat_dis = of_property_read_bool(np, "tsbat_dis");
	chip->cfg.tdie_dis = of_property_read_bool(np, "tdie_dis");
}

static int bq25970_parse_dt(struct bq25970 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret;

	if (!np)
		return -ENODEV;

	ret = of_property_read_u32(np, "ac_ovp", &chip->cfg.ac_ovp);
	if (ret)
		chip->cfg.ac_ovp = 17000000;
	ret = of_property_read_u32(np, "bat_ucp_alm", &chip->cfg.bat_ucp_alm);
	if (ret)
		chip->cfg.bat_ucp_alm = 2000000;
	ret = of_property_read_u32(np, "ss_timeout", &chip->cfg.ss_timeout);
	if (ret)
		chip->cfg.ss_timeout = 0;
	ret = of_property_read_u32(np, "fsw_set", &chip->cfg.fsw_set);
	if (ret)
		chip->cfg.fsw_set = 500000;
	ret = of_property_read_u32(np, "ibat_sns_res", &chip->cfg.ibat_sns_res);
	if (ret)
		chip->cfg.ibat_sns_res = 5;
	ret = of_property_read_u32(np, "ibus_low_dg_set",
			&chip->cfg.ibus_low_dg_set);
	if (ret)
		chip->cfg.ibus_low_dg_set = 10;

	chip->cfg.watchdog_dis = of_property_read_bool(np, "watchdog_dis");
	ret = of_property_read_u32(np, "watchdog", &chip->cfg.watchdog);
	if (ret)
		chip->cfg.watchdog = 30000000;
	if (!chip->cfg.watchdog)
		chip->cfg.watchdog_dis = true;

	bq25970_parse_dt_protection(chip, np);
	bq25970_parse_dt_irq(chip, np);
	bq25970_parse_dt_adc(chip, np);

	return 0;
}

static int bq25970_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct bq25970 *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(client, chip);
	chip->client = client;
	chip->dev = &client->dev;
	mutex_init(&chip->rw_lock);
	mutex_init(&chip->ops_lock);
	mutex_init(&chip->adc_lock);
	init_completion(&chip->adc_done);

	ret = bq25970_parse_dt(chip);
	if (ret)
		return ret;

	ret = bq25970_hw_init(chip);
	if (ret)
		return ret;

	ret = bq25970_irq_init(chip);
	if (ret)
		return ret;

	ret = bq25970_charger_device_register(chip);
	if (ret)
		return ret;

	create_debugfs_entries(chip);

	return ret;
}

static int bq25970_remove(struct i2c_client *client)
{
	struct bq25970 *chip = i2c_get_clientdata(client);

	charger_device_unregister(chip->chg_dev);

	return 0;
}

static const struct of_device_id bq25970_of_match[] = {
	{ .compatible = "ti,bq25970", },
	{ },
};

static const struct i2c_device_id bq25970_i2c_id[] = {
	{ .name = "bq25970", },
	{ },
};

static struct i2c_driver bq25970_driver = {
	.probe = bq25970_probe,
	.remove = bq25970_remove,
	.driver = {
		.name = "bq25970",
		.of_match_table = bq25970_of_match,
	},
	.id_table = bq25970_i2c_id,
};
module_i2c_driver(bq25970_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dajin Kim <dajin.kim@lge.com>");
MODULE_DESCRIPTION("Texas Instruments BQ25970 Switched Cap Fast Charger");
MODULE_VERSION("1.0");
