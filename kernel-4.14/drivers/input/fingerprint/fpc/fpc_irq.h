#ifndef _FPC_IRQ_H_

#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/module.h>
//#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>

#define RELEASE_WAKELOCK_W_V "release_wakelock_with_verification"
#define RELEASE_WAKELOCK "release_wakelock"
#define START_IRQS_RECEIVED_CNT "start_irqs_received_counter"

struct fpc_gpio_info;

struct fpc_data {
	//struct device *dev;
	//struct platform_device *pldev;
	struct WRAP_DEVICE *wdev;
#ifdef USE_VIRTUAL_INPUT
	struct input_dev *idev;
#endif
#ifdef USE_FB_NOTIFIER
	struct notifier_block fb_notifier;
#endif

	int irq_gpio;
	int rst_gpio;

	int nbr_irqs_received;
	int nbr_irqs_received_counter_start;

	bool clocks_enabled;

	bool wakeup_enabled;

	//struct wake_lock ttw_wl;

	struct regulator *vdd_tx;

	bool power_enabled;
	bool use_regulator_for_bezel;
	const struct fpc_gpio_info *hwabs;

	struct mutex mutex;
};

struct fpc_gpio_info {
	int (*init)(struct fpc_data *fpc);
	int (*configure)(struct fpc_data *fpc, int *irq_num, int *trigger_flags);
	int (*get_val)(unsigned gpio);
	void (*set_val)(unsigned gpio, int val);
	ssize_t (*clk_enable_set)(struct fpc_data *fpc, const char *buf, size_t count);
	void (*irq_handler)(int irq, struct fpc_data *fpc);
	void *priv;
};

extern int fpc_probe(struct WRAP_DEVICE *wdev,
			struct fpc_gpio_info *fpc_gpio_ops);

extern int fpc_remove(struct WRAP_DEVICE *wdev);

#endif
