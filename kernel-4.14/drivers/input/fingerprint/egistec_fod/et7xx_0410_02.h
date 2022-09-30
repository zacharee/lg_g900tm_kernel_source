#ifndef _FP_LINUX_DIRVER_H_
#define _FP_LINUX_DIRVER_H_
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>


#define FP_SPI_DEBUG

#ifdef FP_SPI_DEBUG
#define DEBUG_PRINT(fmt, args...) printk(KERN_DEBUG fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

#define ET7XX_MAJOR				100 /* assigned */
#define N_SPI_MINORS				32  /* ... up to 256 */

#define FP_ADDRESS_0				0x00
#define FP_WRITE_ADDRESS			0xAC
#define FP_READ_DATA				0xAF
#define FP_WRITE_DATA				0xAE

/* ------------------------- Register Definition ------------------------*/
/*
 * Sensor Registers
 */

#define FDATA_FP_ADDR				0x00
#define FSTATUS_FP_ADDR				0x01
/*
 * Detect Define
 */
#define FRAME_READY_MASK			0x01

/* ------------------------- Opcode -------------------------------------*/
#define FP_SENSOR_RESET				0x04
#define FP_POWER_ONOFF				0x05
#define FP_RESET_SET				0x07
#define FP_SPICLK_ENABLE			0xaa
#define FP_SPICLK_DISABLE			0xab
#define DELETE_DEVICE_NODE			0xac
#define FP_WAKELOCK_TIMEOUT_ENABLE 	0xb1
#define FP_WAKELOCK_TIMEOUT_DISABLE 0xb2
#define FP_SPIPIN_SETTING			0xad
#define FP_SPIPIN_PULLLOW			0xae
#define FP_BEFORE_AFTER_USING_SENSOR 0xba

/* ------------------------- Structure ------------------------------*/
struct egis_ioc_transfer {
	u8 *tx_buf;
	u8 *rx_buf;

	__u32 len;
	__u32 speed_hz;

	__u16 delay_usecs;
	__u8 bits_per_word;
	__u8 cs_change;
	__u8 opcode;
	__u8 pad[3];

};

#define EGIS_IOC_MAGIC			'k'
#define EGIS_MSGSIZE(N) \
	((((N)*(sizeof(struct egis_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
		? ((N)*(sizeof(struct egis_ioc_transfer))) : 0)
#define EGIS_IOC_MESSAGE(N) _IOW(EGIS_IOC_MAGIC, 0, char[EGIS_MSGSIZE(N)])

struct egistec_data {
	dev_t devt;
	spinlock_t spi_lock;
	struct spi_device  *spi;
	struct platform_device *pd;
	struct list_head device_entry;
	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex buf_lock;
	unsigned users;
	u8 *buffer;

	unsigned int irqPin;	/* interrupt GPIO pin number */
	unsigned int rstPin; 	/* Reset GPIO pin number */

	struct input_dev	*input_dev;
	bool property_navigation_enable;

	bool spi_clk_enabled;
#ifdef USE_FB_NOTIFIER
	struct notifier_block fb_notifier;
#endif
#ifdef CONFIG_OF
	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *pins_irq;
	struct pinctrl_state *pins_reset_high, *pins_reset_low, *pins_v33_on, *pins_v33_off;
	struct pinctrl_state *pins_miso_spi, *pins_miso_pullhigh, *pins_miso_pulllow;
	struct pinctrl_state *pins_mosi_spi, *pins_mosi_pullhigh, *pins_mosi_pulllow;
	struct pinctrl_state *pins_cs_spi, *pins_cs_pullhigh, *pins_cs_pulllow;
	struct pinctrl_state *pins_clk_spi, *pins_clk_pullhigh, *pins_clk_pulllow;

#endif

};

#ifdef USE_FP_ID_DUAL
#include <linux/of_gpio.h>

static inline int get_fp_id_from_gpio(void)
{
	struct device_node *node = of_find_compatible_node(NULL, NULL, "mediatek,fp_id");
	int fp_gpio = of_get_named_gpio(node, "fp-id-gpios", 0);
	int fp_val = __gpio_get_value(fp_gpio);

	return fp_val;
}
#endif // USE_FP_ID_DUAL

#endif
