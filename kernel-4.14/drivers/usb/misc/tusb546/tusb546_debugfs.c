#ifdef CONFIG_DEBUG_FS

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include "tusb546_debugfs.h"

/*
 * tusb546_dbgfs common open/release
 */
#define DEBUG_BUF_SIZE 4096
struct tusb546_dbgfs {
	struct tusb546 *tusb;
	void *private_data;
};

static int tusb546_dbgfs_open(struct inode *inode, struct file *file)
{
	struct tusb546_dbgfs *dbgfs;

	dbgfs = kzalloc(sizeof(struct tusb546_dbgfs), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(dbgfs)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	dbgfs->tusb = inode->i_private;
	file->private_data = dbgfs;
	return 0;
}

static int tusb546_dbgfs_release(struct inode *inode, struct file *file)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	kfree(dbgfs);
	return 0;
}

/*
 * tusb546_dbgfs_power
 */
static ssize_t tusb546_dbgfs_power_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	char *buf;
	unsigned int buf_size;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = scnprintf(buf, buf_size, "%d\n", atomic_read(&tusb->pwr_on));
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t tusb546_dbgfs_power_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	char *buf;
	unsigned int buf_size;
	long is_on;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, PAGE_SIZE, ppos, ubuf, buf_size);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &is_on);
	if (rc < 0)
		goto err;

	tusb546_pwr_on(tusb, is_on);
	rc = count;
err:
	kfree(buf);
	return rc;
}

static const struct file_operations tusb546_dbgfs_power_ops = {
	.open = tusb546_dbgfs_open,
	.read = tusb546_dbgfs_power_read,
	.write = tusb546_dbgfs_power_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_status
 */
static ssize_t tusb546_dbgfs_status_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	struct i2c_client *i2c = tusb->i2c;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (!atomic_read(&tusb->pwr_on)) {
		temp = scnprintf(buf, buf_size, "power down.\n");
		goto done;
	}

	rc = tusb546_read_reg(i2c, GENERAL_REGISTER);
	temp += scnprintf(buf + temp, buf_size - temp,
			"General Register : %02x\n", rc);
	pr_info("%s General Register : %02x\n", __func__, rc);

	rc = tusb546_read_reg(i2c, DISPLAY_CTRL_STS_1);
	temp += scnprintf(buf + temp, buf_size - temp,
			"DisplayPort Status(0x10) : %02x\n", rc);

	rc = tusb546_read_reg(i2c, DISPLAY_CTRL_STS_2);
	temp += scnprintf(buf + temp, buf_size - temp,
			"DisplayPort Status(0x11) : %02x\n", rc);

	rc = tusb546_read_reg(i2c, DISPLAY_CTRL_STS_3);
	temp += scnprintf(buf + temp, buf_size - temp,
			"DisplayPort Status(0x12) : %02x\n", rc);

	rc = tusb546_read_reg(i2c, DISPLAY_CTRL_STS_4);
	temp += scnprintf(buf + temp, buf_size - temp,
			"DisplayPort Status(0x13) : %02x\n", rc);

	rc = tusb546_read_reg(i2c, USB3_CTRL_STS_1);
	temp += scnprintf(buf + temp, buf_size - temp,
			"USB 3.1 Status(0x20) : %02x\n", rc);

	rc = tusb546_read_reg(i2c, USB3_CTRL_STS_2);
	temp += scnprintf(buf + temp, buf_size - temp,
			"USB 3.1 Status(0x21) : %02x\n", rc);

	rc = tusb546_read_reg(i2c, USB3_CTRL_STS_3);
	temp += scnprintf(buf + temp, buf_size - temp,
			"USB 3.1 Status(0x22) : %02x\n", rc);

done:
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
	return rc;
}

static const struct file_operations tusb546_dbgfs_status_ops = {
	.open = tusb546_dbgfs_open,
	.read = tusb546_dbgfs_status_read,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_dump
 */
static ssize_t tusb546_dbgfs_dump_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	char *buf;
	unsigned int buf_size;
	int temp = 0;
	int i, j;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (!atomic_read(&tusb->pwr_on)) {
		temp = scnprintf(buf, buf_size, "power down.\n");
		goto done;
	}

	temp += scnprintf(buf + temp, buf_size - temp, "          ");
	for (i = 0; i < 16; i++) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"%02x ", i);
	}
	temp += scnprintf(buf + temp, buf_size - temp, "\n");

	for (i = 0; i < 256; i += 16) {
		temp += scnprintf(buf + temp, buf_size - temp,
				"%.8x: ", i);
		for (j = 0; j < 16; j++) {
			rc = tusb546_read_reg(tusb->i2c, i + j);
#if 0
			if (rc < 0) {
				temp += scnprintf(buf + temp,
						buf_size - temp,
						"err %d", rc);
				goto err;
			}
#endif
			temp += scnprintf(buf + temp,
					buf_size - temp,
					"%02x ", rc);
		}
		temp += scnprintf(buf + temp, buf_size - temp, "\n");
	}
done:
#if 0
err:
#endif
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
	return rc;
}

static const struct file_operations tusb546_dbgfs_dump_ops = {
	.open = tusb546_dbgfs_open,
	.read = tusb546_dbgfs_dump_read,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_reg
 */
static u8 dbgfs_reg;

static ssize_t tusb546_dbgfs_reg_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = scnprintf(buf, buf_size, "%02x\n", dbgfs_reg);
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t tusb546_dbgfs_reg_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long reg;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, PAGE_SIZE, ppos, ubuf, buf_size);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &reg);
	if (rc < 0)
		goto err;

	dbgfs_reg = reg;
	rc = count;
err:
	kfree(buf);
	return rc;
}

static const struct file_operations tusb546_dbgfs_reg_ops = {
	.read = tusb546_dbgfs_reg_read,
	.write = tusb546_dbgfs_reg_write,
};

/*
 * tusb546_dbgfs_val
 */
static ssize_t tusb546_dbgfs_val_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	char *buf;
	unsigned int buf_size;
	int val;
	int rc = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	val = tusb546_read_reg(tusb->i2c, dbgfs_reg);
	if (val < 0)
		rc = scnprintf(buf, buf_size, "%d\n", val);
	else
		rc = scnprintf(buf, buf_size, "%02x\n", val);

	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t tusb546_dbgfs_val_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	rc = tusb546_write_reg(tusb->i2c, dbgfs_reg, val);
	if (rc < 0)
		goto err;

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_val_ops = {
	.open = tusb546_dbgfs_open,
	.read = tusb546_dbgfs_val_read,
	.write = tusb546_dbgfs_val_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_mode
 */
static ssize_t tusb546_dbgfs_mode_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	char *buf;
	unsigned int buf_size;
	int rc = 0;
	int temp = 0;

	if (*ppos != 0)
		return 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	if (!atomic_read(&tusb->pwr_on)) {
		temp = scnprintf(buf, buf_size, "power down.\n");
		goto mode_read_done;
	}

	/*
		LGE_TUSB_MODE_DISABLE = 0,
		LGE_TUSB_MODE_DP1,
		LGE_TUSB_MODE_DP2,
		LGE_TUSB_MODE_SS1,
		LGE_TUSB_MODE_SS2,
		LGE_TUSB_MODE_MAX,
	*/

	tusb546_read_cross_switch();

mode_read_done:
	rc = simple_read_from_buffer(ubuf, count, ppos, buf, rc);
	kfree(buf);
	return rc;
}

static ssize_t tusb546_dbgfs_mode_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct tusb546_dbgfs *dbgfs = file->private_data;
	struct tusb546 *tusb = dbgfs->tusb;
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	if (!atomic_read(&tusb->pwr_on))
		tusb546_pwr_on(tusb, 1);

	tusb546_update_cross_switch(val);
	if (rc < 0)
		goto err;

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_mode_ops = {
	.open = tusb546_dbgfs_open,
	.read = tusb546_dbgfs_mode_read,
	.write = tusb546_dbgfs_mode_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_dp0_eq
 */
static ssize_t tusb546_dbgfs_dp0_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	tusb546_update_eq_val(LGE_TUSB_DP0_EQ, val);

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_dp0_ops = {
	.open = tusb546_dbgfs_open,
	.write = tusb546_dbgfs_dp0_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_dp1_eq
 */
static ssize_t tusb546_dbgfs_dp1_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	tusb546_update_eq_val(LGE_TUSB_DP1_EQ, val);

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_dp1_ops = {
	.open = tusb546_dbgfs_open,
	.write = tusb546_dbgfs_dp1_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_dp2_eq
 */
static ssize_t tusb546_dbgfs_dp2_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	tusb546_update_eq_val(LGE_TUSB_DP2_EQ, val);

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_dp2_ops = {
	.open = tusb546_dbgfs_open,
	.write = tusb546_dbgfs_dp2_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_dp3_eq
 */
static ssize_t tusb546_dbgfs_dp3_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	tusb546_update_eq_val(LGE_TUSB_DP3_EQ, val);

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_dp3_ops = {
	.open = tusb546_dbgfs_open,
	.write = tusb546_dbgfs_dp3_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_ssrx1_eq
 */
static ssize_t tusb546_dbgfs_ssrx1_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	tusb546_update_eq_val(LGE_TUSB_SSRX1_EQ, val);

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_ssrx1_ops = {
	.open = tusb546_dbgfs_open,
	.write = tusb546_dbgfs_ssrx1_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_ssrx2_eq
 */
static ssize_t tusb546_dbgfs_ssrx2_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	tusb546_update_eq_val(LGE_TUSB_SSRX2_EQ, val);

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_ssrx2_ops = {
	.open = tusb546_dbgfs_open,
	.write = tusb546_dbgfs_ssrx2_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_sstx_eq
 */
static ssize_t tusb546_dbgfs_sstx_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int buf_size;
	long val;
	int rc = 0;

	buf_size = (DEBUG_BUF_SIZE < count) ? DEBUG_BUF_SIZE : count;
	buf = kmalloc(sizeof(char) * buf_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf)) {
		pr_err("%s: Error allocating memory\n", __func__);
		return -ENOMEM;
	}

	rc = simple_write_to_buffer(buf, buf_size, ppos, ubuf, count);
	if (rc < 0)
		goto err;
	buf[rc] = '\0';

	rc = kstrtol(buf, 0, &val);
	if (rc < 0)
		goto err;

	tusb546_update_eq_val(LGE_TUSB_SSTX_EQ, val);

	rc = count;
err:
	kfree(buf);
	return count;
}

static const struct file_operations tusb546_dbgfs_sstx_ops = {
	.open = tusb546_dbgfs_open,
	.write = tusb546_dbgfs_sstx_write,
	.release = tusb546_dbgfs_release,
};

/*
 * tusb546_dbgfs_events
 */

/* Maximum debug message length */
#define DBG_DATA_MSG   64UL
/* Maximum number of messages */
#define DBG_DATA_MAX   2048UL

static struct {
	char (buf[DBG_DATA_MAX])[DBG_DATA_MSG];	/* buffer */
	unsigned idx;	/* index */
	unsigned tty;	/* print to console? */
	rwlock_t lck;	/* lock */
} dbg_tusb_data = {
	.idx = 0,
	.tty = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

static inline void __maybe_unused dbg_dec(unsigned *idx)
{
	*idx = (*idx - 1) % DBG_DATA_MAX;
}

static inline void dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) % DBG_DATA_MAX;
}

#define TIME_BUF_LEN  20
static char *get_timestamp(char *tbuf)
{
	unsigned long long t;
	unsigned long nanosec_rem;

	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000)/1000;
	scnprintf(tbuf, TIME_BUF_LEN, "[%5lu.%06lu] ", (unsigned long)t,
			nanosec_rem);
	return tbuf;
}

void tusb_dbg_print(const char *name, int status, const char *extra)
{
	unsigned long flags;
	char tbuf[TIME_BUF_LEN];

	write_lock_irqsave(&dbg_tusb_data.lck, flags);

	scnprintf(dbg_tusb_data.buf[dbg_tusb_data.idx], DBG_DATA_MSG,
			"%s\t? %-12.12s %4i ?\t%s\n",
			get_timestamp(tbuf), name, status, extra);

	dbg_inc(&dbg_tusb_data.idx);

	write_unlock_irqrestore(&dbg_tusb_data.lck, flags);

	if (dbg_tusb_data.tty != 0)
		pr_notice("%s\t? %-7.7s %4i ?\t%s\n",
				get_timestamp(tbuf), name, status, extra);
}

void tusb_dbg_event(const char *name, int status)
{
	tusb_dbg_print(name, status, "");
}

static ssize_t tusb546_dbgfs_events_store(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned tty;

	if (ubuf == NULL) {
		pr_err("[%s] EINVAL\n", __func__);
		goto done;
	}

	if (sscanf(ubuf, "%u", &tty) != 1 || tty > 1) {
		pr_err("<1|0>: enable|disable console log\n");
		goto done;
	}

	dbg_tusb_data.tty = tty;
	pr_info("tty = %u", dbg_tusb_data.tty);

done:
	return count;
}

static int tusb546_dbgfs_events_show(struct seq_file *s, void *unused)
{
	unsigned long flags;
	unsigned i;

	read_lock_irqsave(&dbg_tusb_data.lck, flags);

	i = dbg_tusb_data.idx;
	if (strnlen(dbg_tusb_data.buf[i], DBG_DATA_MSG))
		seq_printf(s, "%s\n", dbg_tusb_data.buf[i]);
	for (dbg_inc(&i); i != dbg_tusb_data.idx; dbg_inc(&i)) {
		if (!strnlen(dbg_tusb_data.buf[i], DBG_DATA_MSG))
			continue;
		seq_printf(s, "%s\n", dbg_tusb_data.buf[i]);
	}

	read_unlock_irqrestore(&dbg_tusb_data.lck, flags);

	return 0;
}

static int tusb546_dbgfs_events_open(struct inode *inode, struct file *f)
{
	return single_open(f, tusb546_dbgfs_events_show, inode->i_private);
}

static const struct file_operations tusb546_dbgfs_events_ops = {
	.open = tusb546_dbgfs_events_open,
	.read = seq_read,
	.write = tusb546_dbgfs_events_store,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * tusb546_debugfs_init
 */
static struct dentry *tusb546_dbgfs_dent;

int tusb546_debugfs_init(struct tusb546 *tusb)
{
	struct dentry *entry = NULL;

	tusb546_dbgfs_dent = debugfs_create_dir("tusb546", 0);
	if (IS_ERR(tusb546_dbgfs_dent))
		return -ENOMEM;

	entry = debugfs_create_file("power", 0600, tusb546_dbgfs_dent,
			tusb, &tusb546_dbgfs_power_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("status", 0444, tusb546_dbgfs_dent,
			tusb, &tusb546_dbgfs_status_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("dump", 0444, tusb546_dbgfs_dent,
			tusb, &tusb546_dbgfs_dump_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("reg", 0600, tusb546_dbgfs_dent,
			tusb, &tusb546_dbgfs_reg_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("val", 0600, tusb546_dbgfs_dent,
		       	tusb, &tusb546_dbgfs_val_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("mode", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_mode_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("events", 0444, tusb546_dbgfs_dent,
			tusb, &tusb546_dbgfs_events_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("dp0", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_dp0_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("dp1", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_dp1_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("dp2", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_dp2_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("dp3", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_dp3_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("ssrx1", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_ssrx1_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("ssrx2", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_ssrx2_ops);
	if (!entry)
		goto err;

	entry = debugfs_create_file("sstx", 0600, tusb546_dbgfs_dent,
				tusb, &tusb546_dbgfs_sstx_ops);
	if (!entry)
		goto err;

	return 0;
err:
	debugfs_remove_recursive(tusb546_dbgfs_dent);
	return -ENOMEM;
}
#else
int tusb546_debugfs_init(struct tusb546 *tusb) { return 0; }
#endif
