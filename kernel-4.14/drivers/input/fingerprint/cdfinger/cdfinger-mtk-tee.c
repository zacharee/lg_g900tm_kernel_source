#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/pm_wakeup.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/signal.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>

#if defined(CONFIG_MTK_CLKMGR)
 /* mt_clkmgr.h will be removed after CCF porting is finished. */
#include <mach/mt_clkmgr.h>
#endif                          /* defined(CONFIG_MTK_CLKMGR) */

extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);


#ifdef USE_ONE_BINARY
extern int lge_get_fingerprint_hwinfo(void);
#endif // USE_ONE_BINARY

#ifdef USE_FP_ID_DUAL
static int g_fp_id;

static int get_fp_id_from_gpio(void);
#endif // USE_FP_ID_DUAL

static u8 cdfinger_debug = 0x01;
static int isInKeyMode = 0; // key mode
static int screen_status = 1; // screen on
static int sign_sync = 0; // for poll
static int irq_success = 0; // irq can only be requested once in case of  encryption
typedef struct key_report{
	int key;
	int value;
}key_report_t;

#define LOG_TAG "[cdfinger] "

#define CDFINGER_DBG(fmt, args...) \
	do { \
		if(cdfinger_debug & 0x01) \
			printk(KERN_INFO LOG_TAG "%s: " fmt, __func__, ##args ); \
	} while(0)

#define CDFINGER_FUNCTION(fmt, args...) \
	do { \
		if(cdfinger_debug & 0x02) \
			printk(KERN_INFO LOG_TAG "%s: " fmt, __func__, ##args ); \
	} while(0)

#define CDFINGER_REG(fmt, args...) \
	do { \
		if(cdfinger_debug & 0x04) \
			printk(KERN_INFO LOG_TAG "%s: " fmt, __func__, ##args ); \
	} while(0)

#define CDFINGER_ERR(fmt, args...) \
	do { \
		printk(KERN_ERR LOG_TAG "%s: " fmt, __func__, ##args ); \
	} while(0)

#define HAS_RESET_PIN
#define DTS_PROBE

#define VERSION                         "cdfinger version 2.4"
#define DEVICE_NAME                     "fpsdev0"
#define SPI_DRV_NAME                    "cdfinger"


#define CDFINGER_IOCTL_MAGIC_NO          0xFB
#define CDFINGER_INIT                    _IOW(CDFINGER_IOCTL_MAGIC_NO, 0, uint8_t)
#define CDFINGER_GETIMAGE                _IOW(CDFINGER_IOCTL_MAGIC_NO, 1, uint8_t)
#define CDFINGER_INITERRUPT_MODE         _IOW(CDFINGER_IOCTL_MAGIC_NO, 2, uint8_t)
#define CDFINGER_INITERRUPT_KEYMODE      _IOW(CDFINGER_IOCTL_MAGIC_NO, 3, uint8_t)
#define CDFINGER_INITERRUPT_FINGERUPMODE _IOW(CDFINGER_IOCTL_MAGIC_NO, 4, uint8_t)
#define CDFINGER_RELEASE_WAKELOCK        _IO(CDFINGER_IOCTL_MAGIC_NO, 5)
#define CDFINGER_CHECK_INTERRUPT         _IO(CDFINGER_IOCTL_MAGIC_NO, 6)
#define CDFINGER_SET_SPI_SPEED           _IOW(CDFINGER_IOCTL_MAGIC_NO, 7, uint8_t)
#define CDFINGER_POWERDOWN               _IO(CDFINGER_IOCTL_MAGIC_NO, 11)
#define CDFINGER_ENABLE_IRQ              _IO(CDFINGER_IOCTL_MAGIC_NO, 12)
#define CDFINGER_DISABLE_IRQ             _IO(CDFINGER_IOCTL_MAGIC_NO, 13)
#define CDFINGER_HW_RESET                _IOW(CDFINGER_IOCTL_MAGIC_NO, 14, uint8_t)
#define CDFINGER_GET_STATUS              _IO(CDFINGER_IOCTL_MAGIC_NO, 15)
#define CDFINGER_SPI_CLK                 _IOW(CDFINGER_IOCTL_MAGIC_NO, 16, uint8_t)
#define CDFINGER_KEY_REPORT              _IOW(CDFINGER_IOCTL_MAGIC_NO,19,key_report_t)
#define CDFINGER_INIT_IRQ                _IO(CDFINGER_IOCTL_MAGIC_NO,21)
#define CDFINGER_POWER_ON                _IO(CDFINGER_IOCTL_MAGIC_NO,22)
#define CDFINGER_RESET                   _IO(CDFINGER_IOCTL_MAGIC_NO,23)
#define CDFINGER_RELEASE_DEVICE          _IO(CDFINGER_IOCTL_MAGIC_NO,25)
#define CDFINGER_WAKE_LOCK               _IOW(CDFINGER_IOCTL_MAGIC_NO,26,uint8_t)
#define CDFINGER_POLL_TRIGGER            _IO(CDFINGER_IOCTL_MAGIC_NO,31)
#define CDFINGER_NEW_KEYMODE             _IOW(CDFINGER_IOCTL_MAGIC_NO, 37, uint8_t)
#define KEY_INTERRUPT                    KEY_F11

enum work_mode {
	CDFINGER_MODE_NONE       = 1<<0,
	CDFINGER_INTERRUPT_MODE  = 1<<1,
	CDFINGER_KEY_MODE        = 1<<2,
	CDFINGER_FINGER_UP_MODE  = 1<<3,
	CDFINGER_READ_IMAGE_MODE = 1<<4,
	CDFINGER_MODE_MAX
};

static struct cdfinger_data {
	struct spi_device *spi;
	struct miscdevice *miscdev;
	struct mutex buf_lock;
	unsigned int irq;
	unsigned int irq_gpio;
	int irq_enabled;
	int spi_clk_status;
	int wl_source_status;

	u32 vdd_ldo_enable;
	u32 vio_ldo_enable;
	u32 config_spi_pin;

	struct pinctrl *fps_pinctrl;
	struct pinctrl_state *fps_reset_high;
	struct pinctrl_state *fps_reset_low;
	struct pinctrl_state *fps_power_on;
	struct pinctrl_state *fps_power_off;
	struct pinctrl_state *fps_vio_on;
	struct pinctrl_state *fps_vio_off;
	struct pinctrl_state *cdfinger_spi_miso;
	struct pinctrl_state *cdfinger_spi_mosi;
	struct pinctrl_state *cdfinger_spi_sck;
	struct pinctrl_state *cdfinger_spi_cs;
	struct pinctrl_state *cdfinger_irq;

	int thread_wakeup;
	int process_interrupt;
	int key_report;
	enum work_mode device_mode;
	struct input_dev *cdfinger_inputdev;
	struct task_struct *cdfinger_thread;
	struct fasync_struct *async_queue;
	uint8_t cdfinger_interrupt;
	struct notifier_block notifier;
}*g_cdfinger;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(cdfinger_waitqueue);
static irqreturn_t cdfinger_interrupt_handler(int irq, void *arg);

static void enable_clk(void)
{
	struct cdfinger_data *cdfinger = g_cdfinger;
	if (cdfinger->spi_clk_status == 0) {
		mt_spi_enable_master_clk(cdfinger->spi);
		cdfinger->spi_clk_status = 1;
	}
}

static void disable_clk(void)
{
	struct cdfinger_data *cdfinger = g_cdfinger;
	if (cdfinger->spi_clk_status == 1) {
		mt_spi_disable_master_clk(cdfinger->spi);
		cdfinger->spi_clk_status = 0;
	}
}

static void cdfinger_disable_irq(struct cdfinger_data *cdfinger)
{
	if(cdfinger->irq_enabled == 1)
	{
		disable_irq_nosync(cdfinger->irq);
		cdfinger->irq_enabled = 0;
		CDFINGER_DBG("irq disable\n");
	}
}

static void cdfinger_enable_irq(struct cdfinger_data *cdfinger)
{
	if(cdfinger->irq_enabled == 0)
	{
		enable_irq(cdfinger->irq);
		cdfinger->irq_enabled =1;
		CDFINGER_DBG("irq enable\n");
	}
}
static int cdfinger_getirq_from_platform(struct cdfinger_data *cdfinger)
{
	u32 ints[2];
	int ret = 0;

	if(!(cdfinger->spi->dev.of_node)){
		CDFINGER_ERR("of node not exist!\n");
		return -1;
	}

	of_property_read_u32_array(cdfinger->spi->dev.of_node, "debounce",ints,ARRAY_SIZE(ints));
	cdfinger->irq_gpio = ints[0];
	CDFINGER_DBG("irq_gpio[%d]\n",cdfinger->irq_gpio);

	cdfinger->irq = irq_of_parse_and_map(cdfinger->spi->dev.of_node, 0);
	if(cdfinger->irq < 0)
	{
		CDFINGER_ERR("parse irq failed! irq[%d]\n",cdfinger->irq);
		return -1;
	}
	CDFINGER_DBG("get irq success! irq[%d]\n",cdfinger->irq);

	ret = pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_irq);

	if (ret) {
		CDFINGER_ERR("pinctrl failed! ret = %d\n", ret);
		return ret;
	} else {
		CDFINGER_DBG("pinctrl success\n");
	}

	return 0;
}
static int cdfinger_parse_dts(struct cdfinger_data *cdfinger)
{
	int ret = -1;
	struct device_node *np = cdfinger->spi->dev.of_node;

#ifdef DTS_PROBE
//	cdfinger->spi->dev.of_node = of_find_compatible_node(NULL,NULL,"cdfinger,fps1098");
	cdfinger->spi->dev.of_node = of_find_compatible_node(NULL,NULL,"mediatek,fingerprint");
#endif

	if (np == NULL)
	{
		CDFINGER_ERR("of_node is null\n");
		goto parse_err;
	}

	ret = of_property_read_u32(np, "vdd_ldo_enable", &cdfinger->vdd_ldo_enable);
	if (ret < 0)
	{
		CDFINGER_ERR("vdd_ldo_enable property read error %d\n", ret);
	}

	ret = of_property_read_u32(np, "vio_ldo_enable", &cdfinger->vio_ldo_enable);
	if (ret < 0)
	{
		CDFINGER_ERR("vio_ldo_enable property read error %d\n", ret);
	}

	ret = of_property_read_u32(np, "config_spi_pin", &cdfinger->config_spi_pin);
	if (ret < 0)
	{
		CDFINGER_ERR("config_spi_pin property read error %d\n", ret);
	}

	CDFINGER_DBG("vdd_ldo_enable[%d], vio_ldo_enable[%d], config_spi_pin[%d]\n",
		cdfinger->vdd_ldo_enable, cdfinger->vio_ldo_enable, cdfinger->config_spi_pin);

	cdfinger->fps_pinctrl = devm_pinctrl_get(&cdfinger->spi->dev);
	if (IS_ERR(cdfinger->fps_pinctrl)) {
		ret = PTR_ERR(cdfinger->fps_pinctrl);
		CDFINGER_ERR("Cannot find fingerprint cdfinger->fps_pinctrl! ret=%d\n", ret);
		goto parse_err;
	}

//	cdfinger->cdfinger_irq = pinctrl_lookup_state(cdfinger->fps_pinctrl,"cdfinger_irq");
	cdfinger->cdfinger_irq = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_irq");
	if (IS_ERR(cdfinger->cdfinger_irq))
	{
		ret = PTR_ERR(cdfinger->cdfinger_irq);
		CDFINGER_ERR("cdfinger->cdfinger_irq ret = %d\n",ret);
		goto parse_err;
	}
	
//	cdfinger->fps_reset_low = pinctrl_lookup_state(cdfinger->fps_pinctrl,"cdfinger_rst_low");
	cdfinger->fps_reset_low = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_rst_low");
	if (IS_ERR(cdfinger->fps_reset_low))
	{
		ret = PTR_ERR(cdfinger->fps_reset_low);
		CDFINGER_ERR("cdfinger->fps_reset_low ret = %d\n",ret);
		goto parse_err;
	}
	
//	cdfinger->fps_reset_high = pinctrl_lookup_state(cdfinger->fps_pinctrl,"cdfinger_rst_high");
	cdfinger->fps_reset_high = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_rst_high");
	if (IS_ERR(cdfinger->fps_reset_high))
	{
		ret = PTR_ERR(cdfinger->fps_reset_high);
		CDFINGER_ERR("cdfinger->fps_reset_high ret = %d\n",ret);
		goto parse_err;
	}

#if 0
	if(cdfinger->config_spi_pin == 1)
	{
		cdfinger->cdfinger_spi_miso = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_miso");
		if (IS_ERR(cdfinger->cdfinger_spi_miso))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_miso);
			CDFINGER_ERR("cdfinger->cdfinger_spi_miso ret = %d\n",ret);
			goto parse_err;
		}
		cdfinger->cdfinger_spi_mosi = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_mosi");
		if (IS_ERR(cdfinger->cdfinger_spi_mosi))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_mosi);
			CDFINGER_ERR("cdfinger->cdfinger_spi_mosi ret = %d\n",ret);
			goto parse_err;
		}
		cdfinger->cdfinger_spi_sck = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_sck");
		if (IS_ERR(cdfinger->cdfinger_spi_sck))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_sck);
			CDFINGER_ERR("cdfinger->cdfinger_spi_sck ret = %d\n",ret);
			goto parse_err;
		}
		cdfinger->cdfinger_spi_cs = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_cs");
		if (IS_ERR(cdfinger->cdfinger_spi_cs))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_cs);
			CDFINGER_ERR("cdfinger->cdfinger_spi_cs ret = %d\n",ret);
			goto parse_err;
		}
	}

	if(cdfinger->vdd_ldo_enable == 1)
	{
		cdfinger->fps_power_on = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_power_high");
		if (IS_ERR(cdfinger->fps_power_on))
		{
			ret = PTR_ERR(cdfinger->fps_power_on);
			CDFINGER_ERR("cdfinger->fps_power_on ret = %d\n",ret);
			goto parse_err;
		}

		cdfinger->fps_power_off = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_power_low");
		if (IS_ERR(cdfinger->fps_power_off))
		{
			ret = PTR_ERR(cdfinger->fps_power_off);
			CDFINGER_ERR("cdfinger->fps_power_off ret = %d\n",ret);
			goto parse_err;
		}
	}

	if(cdfinger->vio_ldo_enable == 1)
	{
		cdfinger->fps_vio_on = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_vio_high");
		if (IS_ERR(cdfinger->fps_vio_on))
		{
			ret = PTR_ERR(cdfinger->fps_vio_on);
			CDFINGER_ERR("cdfinger->fps_vio_on ret = %d\n",ret);
			goto parse_err;
		}

		cdfinger->fps_vio_off = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_vio_low");
		if (IS_ERR(cdfinger->fps_vio_off))
		{
			ret = PTR_ERR(cdfinger->fps_vio_off);
			CDFINGER_ERR("cdfinger->fps_vio_off ret = %d\n",ret);
			goto parse_err;
		}
	}
#endif

	return 0;

parse_err:
	CDFINGER_ERR("parse dts failed!\n");

	return ret;
}


static void cdfinger_power_on(struct cdfinger_data *cdfinger)
{
#if 0
	if(cdfinger->config_spi_pin == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_miso);
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_mosi);
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_sck);
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_cs);
	}

	if(cdfinger->vdd_ldo_enable == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_power_on);
	}

	if(cdfinger->vio_ldo_enable == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_vio_on);
	}
#endif
}

#ifdef HAS_RESET_PIN
static void cdfinger_reset(int count)
{
	struct cdfinger_data *cdfinger = g_cdfinger;
	pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_low);
	mdelay(count);
	pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_high);
	mdelay(count);
}
#endif

static int cdfinger_mode_init(struct cdfinger_data *cdfinger, uint8_t arg, enum work_mode mode)
{
	cdfinger->process_interrupt = 1;
	cdfinger->device_mode = mode;
	cdfinger->key_report = 0;

	return 0;
}

static void cdfinger_wake_lock(struct cdfinger_data *cdfinger, int arg)
{
	if (arg == 1 && cdfinger->wl_source_status == 0)
	{
		pm_stay_awake(&cdfinger->spi->dev);

		cdfinger->wl_source_status = 1;
	}
	if (arg == 0 && cdfinger->wl_source_status == 1)
	{
		pm_relax(&cdfinger->spi->dev);
		pm_wakeup_event(&cdfinger->spi->dev, 1000); // 1 sec

		cdfinger->wl_source_status = 0;
	}
}

static int cdfinger_key_report(struct cdfinger_data *cdfinger, unsigned long arg)
{
	key_report_t report;
	if ( copy_from_user(&report, (key_report_t *)arg, sizeof(key_report_t)) )
	{
		CDFINGER_ERR("%s err\n", __func__);
		return -1;
	}
	input_report_key(cdfinger->cdfinger_inputdev, report.key, !!report.value);
	input_sync(cdfinger->cdfinger_inputdev);

	return 0;
}

static unsigned int cdfinger_poll(struct file *filp, struct poll_table_struct *wait)
{
	int mask = 0;
	poll_wait(filp, &cdfinger_waitqueue, wait);
	if (sign_sync == 1)
	{
		mask |= POLLIN|POLLPRI;
	} else if (sign_sync == 2)
	{
		mask |= POLLOUT;
	}
	sign_sync = 0;
	CDFINGER_DBG("mask %u\n",mask);
	return mask;
}

static int cdfinger_eint_gpio_init(struct cdfinger_data *cdfinger)
{
	int error = 0;
	if(!irq_success)
	{
		if(cdfinger_getirq_from_platform(cdfinger)!=0)
		{
			CDFINGER_ERR("cdfinger_getirq_from_platform error\n");
			return -1;
		}

		error = request_threaded_irq(cdfinger->irq, NULL, cdfinger_interrupt_handler,
						IRQF_TRIGGER_RISING | IRQF_ONESHOT, "cdfinger-irq", cdfinger);
		if (error) {
			CDFINGER_ERR("request_threaded_irq error\n");
			return -1;
		}
		enable_irq_wake(cdfinger->irq);
		cdfinger->irq_enabled = 1;
		irq_success = 1;
	}
	return error;
}

static int cdfinger_free_gpio(struct cdfinger_data *cdfinger)
{
	int err = 0;
	CDFINGER_DBG("%s(..) enter.\n", __FUNCTION__);

	if (gpio_is_valid(cdfinger->irq_gpio)) {
		gpio_free(cdfinger->irq_gpio);
		devm_pinctrl_put(cdfinger->fps_pinctrl);
	}

	CDFINGER_DBG("%s(..) ok! exit.\n", __FUNCTION__);

	return err;
}

static long cdfinger_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct cdfinger_data *cdfinger = filp->private_data;
	int ret = 0;

	CDFINGER_FUNCTION("enter cmd=%d\n", cmd);

	if(cdfinger == NULL)
	{
		CDFINGER_ERR("%s: fingerprint please open device first!\n", __func__);
		return -EIO;
	}

	mutex_lock(&cdfinger->buf_lock);
	switch (cmd) {
		case CDFINGER_INIT:
			CDFINGER_DBG("CDFINGER_INIT\n");
			break;
		case CDFINGER_GETIMAGE:
			CDFINGER_DBG("CDFINGER_GETIMAGE\n");
			break;
		case CDFINGER_INIT_IRQ:
			CDFINGER_DBG("CDFINGER_INIT_IRQ\n");
			ret = cdfinger_eint_gpio_init(cdfinger);
			break;
		case CDFINGER_WAKE_LOCK:
			CDFINGER_DBG("CDFINGER_WAKE_LOCK\n");
			cdfinger_wake_lock(cdfinger,arg);
			break;
		case CDFINGER_INITERRUPT_MODE:
			CDFINGER_DBG("CDFINGER_INITERRUPT_MODE\n");
			sign_sync = 0;
			isInKeyMode = 1;  // not key mode
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_INTERRUPT_MODE);
			break;
		case CDFINGER_NEW_KEYMODE:
			CDFINGER_DBG("CDFINGER_NEW_KEYMODE\n");
			isInKeyMode = 0;
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_INTERRUPT_MODE);
			break;
		case CDFINGER_KEY_REPORT:
			CDFINGER_DBG("CDFINGER_KEY_REPORT\n");
			ret = cdfinger_key_report(cdfinger,arg);
			break;
		case CDFINGER_INITERRUPT_FINGERUPMODE:
			CDFINGER_DBG("CDFINGER_INITERRUPT_FINGERUPMODE\n");
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_FINGER_UP_MODE);
			break;
		case CDFINGER_INITERRUPT_KEYMODE:
			CDFINGER_DBG("CDFINGER_INITERRUPT_KEYMODE\n");
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_KEY_MODE);
			break;
		case CDFINGER_CHECK_INTERRUPT:
			CDFINGER_DBG("CDFINGER_CHECK_INTERRUPT\n");
			break;
		case CDFINGER_SET_SPI_SPEED:
			CDFINGER_DBG("CDFINGER_SET_SPI_SPEED\n");
			break;
		case CDFINGER_POWER_ON:
			CDFINGER_DBG("CDFINGER_POWER_ON\n");
			cdfinger_power_on(cdfinger);
			break;
		case CDFINGER_POWERDOWN:
			CDFINGER_DBG("CDFINGER_POWERDOWN\n");
			break;
		case CDFINGER_RESET:
			CDFINGER_DBG("CDFINGER_RESET\n");
			cdfinger_reset(100);
			break;
		case CDFINGER_ENABLE_IRQ:
			CDFINGER_DBG("CDFINGER_ENABLE_IRQ\n");
			cdfinger_enable_irq(cdfinger);
			break;
		case CDFINGER_DISABLE_IRQ:
			CDFINGER_DBG("CDFINGER_DISABLE_IRQ\n");
			cdfinger_disable_irq(cdfinger);
			break;
		case CDFINGER_RELEASE_DEVICE:
			CDFINGER_DBG("CDFINGER_RELEASE_DEVICE\n");
			cdfinger_free_gpio(cdfinger);
			if (cdfinger->cdfinger_inputdev != NULL) {
				input_unregister_device(cdfinger->cdfinger_inputdev);
			}
			misc_deregister(cdfinger->miscdev);
			break;
		case CDFINGER_SPI_CLK:
			//CDFINGER_DBG("CDFINGER_SPI_CLK\n");
			if (arg == 1)
				enable_clk();
			else if (arg == 0)
				disable_clk();
			break;
		case CDFINGER_HW_RESET:
			CDFINGER_DBG("CDFINGER_HW_RESET\n");
			cdfinger_reset(arg);
			break;
		case CDFINGER_GET_STATUS:
			CDFINGER_DBG("CDFINGER_GET_STATUS\n");
			ret = screen_status;
			break;
		case CDFINGER_POLL_TRIGGER:
			CDFINGER_DBG("CDFINGER_POLL_TRIGGER\n");
			sign_sync = 2;
			wake_up_interruptible(&cdfinger_waitqueue);
			ret = 0;
			break;
		default:
			CDFINGER_ERR("unkown case error\n");
			ret = -ENOTTY;
			break;
	}
	mutex_unlock(&cdfinger->buf_lock);
	CDFINGER_FUNCTION("exit\n");

	return ret;
}

static int cdfinger_open(struct inode *inode, struct file *file)
{
	CDFINGER_FUNCTION("enter\n");
	file->private_data = g_cdfinger;
	CDFINGER_FUNCTION("exit\n");

	return 0;
}

static ssize_t cdfinger_write(struct file *file, const char *buff, size_t count, loff_t * ppos)
{
	return 0;
}

static int cdfinger_async_fasync(int fd, struct file *filp, int mode)
{
	struct cdfinger_data *cdfinger = g_cdfinger;

	CDFINGER_FUNCTION("enter\n");
	return fasync_helper(fd, filp, mode, &cdfinger->async_queue);
}

static ssize_t cdfinger_read(struct file *file, char *buff, size_t count, loff_t * ppos)
{
	return 0;
}

static int cdfinger_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations cdfinger_fops = {
	.owner = THIS_MODULE,
	.open = cdfinger_open,
	.write = cdfinger_write,
	.read = cdfinger_read,
	.release = cdfinger_release,
	.fasync = cdfinger_async_fasync,
	.unlocked_ioctl = cdfinger_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cdfinger_ioctl,
#endif
	.poll = cdfinger_poll,
};

static struct miscdevice cdfinger_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &cdfinger_fops,
};

static void cdfinger_async_Report(void)
{
	struct cdfinger_data *cdfinger = g_cdfinger;

	CDFINGER_FUNCTION("enter\n");
	kill_fasync(&cdfinger->async_queue, SIGIO, POLL_IN);
	CDFINGER_FUNCTION("exit\n");
}

static int cdfinger_thread_func(void *arg)
{
	struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;

	do {
		wait_event_interruptible(waiter, cdfinger->thread_wakeup != 0);
		cdfinger->thread_wakeup = 0;
		if (cdfinger->device_mode == CDFINGER_INTERRUPT_MODE) {
			cdfinger->process_interrupt = 0;
			sign_sync = 1;
			wake_up_interruptible(&cdfinger_waitqueue);
			cdfinger_async_Report();
			continue;
		} else if ((cdfinger->device_mode == CDFINGER_KEY_MODE) && (cdfinger->key_report == 0)) {
			input_report_key(cdfinger->cdfinger_inputdev, KEY_INTERRUPT, 1);
			input_sync(cdfinger->cdfinger_inputdev);
			cdfinger->key_report = 1;
		}

	}while(!kthread_should_stop());

	CDFINGER_ERR("thread exit\n");
	return -1;
}

static irqreturn_t cdfinger_interrupt_handler(int irq, void *arg)
{
	struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;

	CDFINGER_DBG("irq = %d\n", irq);

	cdfinger->cdfinger_interrupt = 1;
	if (cdfinger->process_interrupt == 1)
	{
		cdfinger_wake_lock(cdfinger, 1);
		cdfinger->thread_wakeup = 1;
		wake_up_interruptible(&waiter);
	}

	return IRQ_HANDLED;
}

static int cdfinger_create_inputdev(struct cdfinger_data *cdfinger)
{
	cdfinger->cdfinger_inputdev = input_allocate_device();
	if (!cdfinger->cdfinger_inputdev) {
		CDFINGER_ERR("cdfinger->cdfinger_inputdev create faile!\n");
		return -ENOMEM;
	}
	__set_bit(EV_KEY, cdfinger->cdfinger_inputdev->evbit);
#if 0 // remove unused keycode
	__set_bit(KEY_INTERRUPT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F1, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F2, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F3, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F4, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F5, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F6, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_VOLUMEUP, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_VOLUMEDOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_PAGEUP, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_PAGEDOWN, cdfinger->cdfinger_inputdev->keybit);
#endif
	__set_bit(KEY_UP, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_LEFT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_RIGHT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_DOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_ENTER, cdfinger->cdfinger_inputdev->keybit);

	cdfinger->cdfinger_inputdev->id.bustype = BUS_HOST;
	cdfinger->cdfinger_inputdev->name = "cdfinger_inputdev";
	cdfinger->cdfinger_inputdev->dev.init_name = "lge_fingerprint";
	if (input_register_device(cdfinger->cdfinger_inputdev)) {
		CDFINGER_ERR("register inputdev failed\n");
		input_free_device(cdfinger->cdfinger_inputdev);
		return -1;
	}

	return 0;
}

static int cdfinger_fb_notifier_callback(struct notifier_block* self,
		unsigned long event, void* data)
{
	struct fb_event* evdata = data;
	unsigned int blank;
	int retval = 0;

	if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
		return 0;
	}
	blank = *(int*)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
		CDFINGER_DBG("sunlin==FB_BLANK_UNBLANK==\n");
		mutex_lock(&g_cdfinger->buf_lock);
		screen_status = 1;
		if (isInKeyMode == 0) {
			sign_sync = 1;
			wake_up_interruptible(&cdfinger_waitqueue);
			cdfinger_async_Report();
		}
		mutex_unlock(&g_cdfinger->buf_lock);
		break;

	case FB_BLANK_POWERDOWN:
		CDFINGER_DBG("sunlin==FB_BLANK_POWERDOWN==\n");
		mutex_lock(&g_cdfinger->buf_lock);
		screen_status = 0;
		if (isInKeyMode == 0) {
			sign_sync = 1;
			wake_up_interruptible(&cdfinger_waitqueue);
			cdfinger_async_Report();
		}
		mutex_unlock(&g_cdfinger->buf_lock);
		break;
	}

	return retval;
}

#ifdef USE_FP_ID_DUAL
static ssize_t get_fp_id (
		struct device* device,
		struct device_attribute* attribute,
		char* buffer)
{
	//struct cdfinger_data *cdfinger = g_cdfinger;
	(void) device;
	(void) attribute;

	CDFINGER_DBG("fp_id = %d\n", g_fp_id);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", g_fp_id);
}

static ssize_t set_fp_id (
		struct device* device,
		struct device_attribute* attribute,
		const char* buffer,
		size_t count)
{
	//struct cdfinger_data *cdfinger = g_cdfinger;
	(void) device;
	(void) attribute;

	if (*buffer == '1') {
		g_fp_id = 1;
	} else {
		g_fp_id = 0;
	}

	CDFINGER_DBG("fp_id = %d\n", g_fp_id);

	return count;
}

static ssize_t get_fp_id_real (
		struct device* device,
		struct device_attribute* attribute,
		char* buffer)
{
	int fp_id = get_fp_id_from_gpio();
	(void) device;
	(void) attribute;

	CDFINGER_DBG("fp_id = %d\n", fp_id);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", fp_id);
}


static DEVICE_ATTR(fp_id, S_IRUGO | S_IWUSR, get_fp_id, set_fp_id);
static DEVICE_ATTR(fp_id_real, S_IRUGO | S_IWUSR, get_fp_id_real, NULL);

static struct attribute *attributes[] = {
	&dev_attr_fp_id.attr,
	&dev_attr_fp_id_real.attr,
	NULL,
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};
#endif // USE_FP_ID_DUAL

static int cdfinger_probe(struct spi_device *spi)
{
	struct cdfinger_data *cdfinger = NULL;
	int status = -ENODEV;
	CDFINGER_DBG("enter\n");

	cdfinger = kzalloc(sizeof(struct cdfinger_data), GFP_KERNEL);
	if (!cdfinger) {
		CDFINGER_ERR("alloc cdfinger failed!\n");
		return -ENOMEM;;
	}

	g_cdfinger = cdfinger;
	cdfinger->spi = spi;

	if (cdfinger_parse_dts(cdfinger))
	{
		CDFINGER_ERR("%s: parse dts failed!\n", __func__);
		goto free_cdfinger;
	}

	mutex_init(&cdfinger->buf_lock);
	device_init_wakeup(&cdfinger->spi->dev, true);

	status = misc_register(&cdfinger_dev);
	if (status < 0) {
		CDFINGER_ERR("%s: cdev register failed!\n", __func__);
		goto free_lock;
	}
	cdfinger->miscdev = &cdfinger_dev;

	if (cdfinger_create_inputdev(cdfinger) < 0)
	{
		CDFINGER_ERR("%s: inputdev register failed!\n", __func__);
		goto free_device;
	}

#ifdef USE_FP_ID_DUAL
	g_fp_id = FP_ID_VALUE;
#endif

#ifdef USE_FP_ID_DUAL

	if (sysfs_create_group(&cdfinger->cdfinger_inputdev->dev.kobj, &attribute_group) < 0)
	{
		CDFINGER_ERR("cdfinger sysfs create group err %d\n", status);
	}
#endif // USE_FP_ID_DUAL

	cdfinger->cdfinger_thread = kthread_run(cdfinger_thread_func, cdfinger, "cdfinger_thread");
	if (IS_ERR(cdfinger->cdfinger_thread)) {
		CDFINGER_ERR("kthread_run is failed\n");
		goto free_irq;
	}
	cdfinger->notifier.notifier_call = cdfinger_fb_notifier_callback;
	fb_register_client(&cdfinger->notifier);
	CDFINGER_DBG("exit\n");

	return 0;

free_irq:
	free_irq(cdfinger->irq, cdfinger);
	input_unregister_device(cdfinger->cdfinger_inputdev);
	cdfinger->cdfinger_inputdev = NULL;
	input_free_device(cdfinger->cdfinger_inputdev);
free_device:
#ifdef USE_FP_ID_DUAL
	sysfs_remove_group(&cdfinger->cdfinger_inputdev->dev.kobj, &attribute_group);
#endif // USE_FP_ID_DUAL
	misc_deregister(&cdfinger_dev);
free_lock:
	device_init_wakeup(&cdfinger->spi->dev, false);
	mutex_destroy(&cdfinger->buf_lock);

free_cdfinger:
	kfree(cdfinger);
	cdfinger = NULL;

	return -1;
}


static int cdfinger_suspend (struct device *dev)
{
	return 0;
}

static int cdfinger_resume (struct device *dev)
{
	return 0;
}

static int cdfinger_remove(struct spi_device *spi)
{
	struct cdfinger_data *cdfinger = spi_get_drvdata(spi);

	kthread_stop(cdfinger->cdfinger_thread);
	free_irq(cdfinger->irq, cdfinger);
	cdfinger->cdfinger_inputdev = NULL;
	input_free_device(cdfinger->cdfinger_inputdev);
	misc_deregister(&cdfinger_dev);
	device_init_wakeup(&cdfinger->spi->dev, false);
	mutex_destroy(&cdfinger->buf_lock);
	kfree(cdfinger);
	cdfinger = NULL;
	g_cdfinger = NULL;

	return 0;
}

static const struct dev_pm_ops cdfinger_pm = {
	.suspend = cdfinger_suspend,
	.resume = cdfinger_resume
};

static const struct of_device_id cdfinger_of_match[] = {
//	{ .compatible = "cdfinger,fps1098", },
	{ .compatible = "fingerprint,mediatek", },
	{},
};
MODULE_DEVICE_TABLE(of, cdfinger_of_match);


static const struct spi_device_id cdfinger_id[] = {
	{SPI_DRV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(spi, cdfinger_id);

static struct spi_driver cdfinger_driver = {
	.driver = {
		.name = SPI_DRV_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.pm = &cdfinger_pm,
		.of_match_table = of_match_ptr(cdfinger_of_match),
	},
	.id_table	= cdfinger_id,
	.probe = cdfinger_probe,
	.remove = cdfinger_remove,
};

#ifdef USE_FP_ID_DUAL
static int get_fp_id_from_gpio(void)
{
	struct device_node *node = of_find_compatible_node(NULL, NULL, "mediatek,fp_id");
	int fp_gpio = of_get_named_gpio(node, "fp-id-gpios", 0);
	int fp_val = __gpio_get_value(fp_gpio);

	return fp_val;
}
#endif // USE_FP_ID_DUAL

static int __init cdfinger_fp_init(void)
{
	int ret;

#ifdef USE_ONE_BINARY
	{
		int is_support = lge_get_fingerprint_hwinfo();

		if (!is_support) {
			printk(KERN_WARNING "[fingerprint] %s: doesn't support fingerprint, skip register\n", __func__);

			return -1;
		}

		printk(KERN_INFO "[fingerprint] %s: support fingerprint\n", __func__);
	}
#endif // USE_ONE_BINARY

#ifdef USE_FP_ID_DUAL
	{
		int fp_id = get_fp_id_from_gpio();

		if (fp_id != FP_ID_VALUE) {
			printk(KERN_WARNING "[fingerprint] %s: fp_id mismatch, value = %d\n", __func__, fp_id);

			return -1;
		}

		printk(KERN_INFO "[fingerprint] %s: fp_id value = %d\n", __func__, fp_id);
	}
#endif // USE_FP_ID_DUAL

	ret = spi_register_driver(&cdfinger_driver);

	if (ret) {
		CDFINGER_ERR("spi register failed!\n");
	}

	return ret;
}

static void __exit cdfinger_fp_exit(void)
{
	spi_unregister_driver(&cdfinger_driver);
}

late_initcall(cdfinger_fp_init);
module_exit(cdfinger_fp_exit);

MODULE_DESCRIPTION("cdfinger tee Driver");
MODULE_AUTHOR("shuaitao@cdfinger.com");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cdfinger");
