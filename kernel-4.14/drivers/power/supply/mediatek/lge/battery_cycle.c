#define pr_fmt(fmt) "[CYCLE] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/syscalls.h>
#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif

//#define CYCLE_DEBUG 138

#define BATTERY_CYCLE_PATH "/mnt/vendor/persist-lg/battery/"
#define BATTERY_COUNT_FILE BATTERY_CYCLE_PATH "cycle_count"
#define BATTERY_DELTA_FILE BATTERY_CYCLE_PATH "cycle_delta"
#define BATTERY_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define BATTERY_FILE_LENGTH 20

#define BATTERY_CYCLE_DELTA_MAX 100

#ifdef CONFIG_LGE_PM_BATTERY_AGING_FACTOR
#define BATTERY_COUNT_BACKUP_FILE BATTERY_CYCLE_PATH "cycle_count_backup"
#define BATTERY_CHARGE_FULL_FILE BATTERY_CYCLE_PATH "charge_full"
#define BATTERY_CHARGE_FULL_BACKUP_FILE BATTERY_CYCLE_PATH "charge_full_backup"
#define BATTERY_AGING_FACTOR_FILE BATTERY_CYCLE_PATH "aging_factor"
#define BATTERY_REMOVE_CHECK_FILE BATTERY_CYCLE_PATH "cbc3_action"
#endif

static DEFINE_MUTEX(update_mutex);

static bool battery_cycle_initialized = false;
static bool battery_removed = false;

static int battery_cycle_count = 0;
static int battery_cycle_delta = 0;

static bool cycle_enabled(void)
{
	int boot_mode = get_boot_mode();

	switch (boot_mode) {
	case NORMAL_BOOT:
	case KERNEL_POWER_OFF_CHARGING_BOOT:
	case LOW_POWER_OFF_CHARGING_BOOT:
		return true;
	}

	return false;
}

static unsigned int cycle_read_from_fs(char *filename)
{
	mm_segment_t old_fs = get_fs();
	char temp_str[BATTERY_FILE_LENGTH];
	int fd = 0;
	int result = 0;
	unsigned int value_read = -EINVAL;

	set_fs(KERNEL_DS);

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		pr_err("open error %s (%d) \n", filename, fd);
		goto Error;
	}
	memset(temp_str, 0x00, sizeof(temp_str));
	result = sys_read(fd, temp_str, sizeof(temp_str));
	if (result < 0) {
		pr_err("read error %s (%d)\n", filename, result);
		goto Error;
	}
	result = kstrtouint(temp_str, 10, &value_read);
	if (result != 0) {
		pr_err("kstrtouint Error\n");
		goto Error;
	}

Error:
	sys_close(fd);
	set_fs(old_fs);
	return value_read;
}

static int cycle_write_to_fs(char *filename, unsigned int value)
{
	mm_segment_t old_fs = get_fs();
	char temp_str[BATTERY_FILE_LENGTH];
	int fd = 0;
	int result = 0;
	int value_write = -EINVAL;
	size_t size;

	set_fs(KERNEL_DS);
	fd = sys_open(filename, O_WRONLY | O_CREAT | O_TRUNC | S_IROTH, BATTERY_FILE_MODE);
	if (fd < 0) {
		pr_err("open error %s (%d)\n", filename, fd);
		goto Error;
	}

	memset(temp_str, 0x00, sizeof(temp_str));
	size = snprintf(temp_str, sizeof(temp_str), "%u\n", value);
	result = sys_write(fd, temp_str, size);

	if (result < 0) {
		pr_err("write error %s (%d) \n", filename, result);
		goto Error;
	}
	value_write = result;
	sys_fsync(fd);

Error:
	sys_close(fd);
	sys_chmod(filename, BATTERY_FILE_MODE);
	set_fs(old_fs);
	return value_write;
}

static void cycle_dump(void)
{
	pr_info("battery cycle = %d.%02d\n",
			battery_cycle_count, battery_cycle_delta);
}

int battery_cycle_get_count(void)
{
	return battery_cycle_count;
}
EXPORT_SYMBOL(battery_cycle_get_count);

int battery_cycle_set_count(unsigned int cycle)
{
	/* cycle should be increased */
	if (cycle <= battery_cycle_count)
		return 0;

	mutex_lock(&update_mutex);

	battery_cycle_count = cycle;

	cycle_write_to_fs(BATTERY_COUNT_FILE, battery_cycle_count);

	mutex_unlock(&update_mutex);

	pr_debug("cycle changed to %d\n", cycle);

	cycle_dump();

	return 0;
}
EXPORT_SYMBOL(battery_cycle_set_count);

void battery_cycle_update(int soc)
{
	static int pre_soc = -EINVAL;
	unsigned int count = 0;
	unsigned int delta = 0;

	if (!cycle_enabled())
		return;

	if (!battery_cycle_initialized)
		return;

	if (pre_soc == -EINVAL)
		pre_soc = soc;

	if (soc > pre_soc)
		delta = soc - pre_soc;
	pre_soc = soc;

#ifdef CYCLE_DEBUG
	delta += CYCLE_DEBUG;
#endif

	/* soc not increased */
	if (!delta)
		return;

	mutex_lock(&update_mutex);

	delta += battery_cycle_delta;
	count += (delta / BATTERY_CYCLE_DELTA_MAX);

	if (count) {
		battery_cycle_count += count;
		delta %= BATTERY_CYCLE_DELTA_MAX;

		cycle_write_to_fs(BATTERY_COUNT_FILE, battery_cycle_count);
	}

	battery_cycle_delta = delta;

	cycle_write_to_fs(BATTERY_DELTA_FILE, battery_cycle_delta);

	mutex_unlock(&update_mutex);

	cycle_dump();
}
EXPORT_SYMBOL(battery_cycle_update);

static void cycle_clear(void)
{
	mutex_lock(&update_mutex);

	pr_info("clear battery cycle\n");

	cycle_write_to_fs(BATTERY_COUNT_FILE, 0);
	cycle_write_to_fs(BATTERY_DELTA_FILE, 0);

	battery_cycle_count = 0;
	battery_cycle_delta = 0;

	mutex_unlock(&update_mutex);
}

void battery_cycle_set_battery_removed(void)
{
	if (!battery_cycle_initialized) {
		battery_removed = true;
		return;
	}

	pr_warn("battery removed\n");
	cycle_clear();
}
EXPORT_SYMBOL(battery_cycle_set_battery_removed);

void battery_cycle_initial(void)
{
	bool clear_cycle = false;
	int count, delta;

	if (!cycle_enabled())
		return;

	count = cycle_read_from_fs(BATTERY_COUNT_FILE);
	delta = cycle_read_from_fs(BATTERY_DELTA_FILE);

	if (count < 0) {
		pr_err("failed to read count\n");
		count = 0;
		clear_cycle = true;
	}
	if (delta < 0) {
		pr_err("failed to read delta\n");
		clear_cycle = true;
	}

	if (battery_removed) {
		pr_warn("battery removed\n");
		clear_cycle = true;
	}

	if (clear_cycle) {
		cycle_clear();

#ifdef CONFIG_LGE_PM_BATTERY_AGING_FACTOR
		cycle_write_to_fs(BATTERY_COUNT_BACKUP_FILE, count);
#endif

		count = 0;
		delta = 0;
	}

	mutex_lock(&update_mutex);

	battery_cycle_count = count;
	battery_cycle_delta = delta;

	mutex_unlock(&update_mutex);

	battery_cycle_initialized = true;

	pr_warn("battery cycle initialized\n");
	cycle_dump();
}
EXPORT_SYMBOL(battery_cycle_initial);


#ifdef CONFIG_LGE_PM_BATTERY_AGING_FACTOR
#define AGING_AVG_SIZE	2
#define AGING_CYCLE_CHECK	0
#define MIN_DROP_PERCENTAGE	10
#define LEARNED_CAPACITY_COUNT_CHECK	2
#define EMPTY	0

static unsigned int battery_aging_capacity = 0;
static int pre_aging_capacity = 0;
static int aging_capacity[AGING_AVG_SIZE];
static int avg_aging_capacity = 0;

static unsigned int battery_aging_factor = 0;
static int pre_aging_factor = 0;
static int aging_factor[AGING_AVG_SIZE];
static int avg_aging_factor = 0;

static int capacity_count_check = 0;
static int capacity_pre_check = 0;
static int index = 0;

static void aging_dump(void)
{
	pr_info("[AGING] learning_capacity: %d aging_factor: %d\n",
			battery_aging_capacity, battery_aging_factor);
}

int battery_aging_get_capacity(void)
{
	return battery_aging_capacity * 100;
}
EXPORT_SYMBOL(battery_aging_get_capacity);

int battery_aging_get_factor(void)
{
	return battery_aging_factor / 100;
}
EXPORT_SYMBOL(battery_aging_get_factor);

int battery_aging_get_factor_ten_multiple(void)
{
	unsigned int value = 100;

	if (!battery_aging_factor)
		return value;

	value = battery_aging_factor / 1000;
	value += 1;
	value *= 10;

	if (value >= 100)
		value = 100;

	return value;
}
EXPORT_SYMBOL(battery_aging_get_factor_ten_multiple);

int battery_aging_get_factor_tri_level(void)
{
	unsigned int value = 1;

	if (!battery_aging_factor)
		return value;

	value = battery_aging_factor / 1000;
	value *= 10;

	if (value >= 80)
		return 1;
	else if (value >= 50)
		return 2;
	else
		return 3;
}
EXPORT_SYMBOL(battery_aging_get_factor_tri_level);

void battery_aging_update_by_full(int capacity, int factor, int temp)
{
	int i = 0;
	int min_drop_value = 0;

	if (temp < 15 || temp >= 45) {
		pr_info("[AGING] Not Update for temperature(%d)\n", temp);
		return;
	}

	/* Not use for AGING_CYCKE_CHECK */
	if (battery_cycle_count < AGING_CYCLE_CHECK) {
		pr_info("[AGING] aging_init_check %d\n", battery_cycle_count);
		return;
	}

	if (capacity == capacity_pre_check) {
		return;
	} else {
		capacity_count_check++;
		capacity_pre_check = capacity;
	}

	if (capacity_count_check < LEARNED_CAPACITY_COUNT_CHECK) {
		pr_info("[AGING] capacity_count_init_check %d\n", capacity_count_check);
		return;
	}

	aging_capacity[index] = capacity;
	aging_factor[index] = factor;

	avg_aging_capacity += aging_capacity[index];
	avg_aging_factor += aging_factor[index];

	index ++;
	if (index >= AGING_AVG_SIZE)
		index = 0;

	if (aging_capacity[AGING_AVG_SIZE-1] == EMPTY) {
		pr_info("[AGING] EMPTY \n");
		return;
	}

	for (i = 0; i < AGING_AVG_SIZE; i++) {
		pr_info("[AGING] aging_capacity[%d]: %d  aging_factor[%d]: %d\n",
			i, aging_capacity[i], i, aging_factor[i]);
	}

	avg_aging_capacity /= AGING_AVG_SIZE;
	avg_aging_factor /= AGING_AVG_SIZE;

	pr_info("[AGING] avg_aging_capacity: %d avg_aging_factor = %d\n",
			avg_aging_capacity, avg_aging_factor);

	mutex_lock(&update_mutex);

	/* Capacity */
	battery_aging_capacity = avg_aging_capacity;

	if (pre_aging_capacity > 0) {
		min_drop_value = pre_aging_capacity * (100 - MIN_DROP_PERCENTAGE) / 100;
		if (battery_aging_capacity < min_drop_value)
			battery_aging_capacity = min_drop_value;
	}
	cycle_write_to_fs(BATTERY_CHARGE_FULL_FILE, battery_aging_capacity);

	pre_aging_capacity = battery_aging_capacity;

	/* Aging_factor */
	battery_aging_factor = avg_aging_factor;

	if (pre_aging_factor > 0) {
		min_drop_value = pre_aging_factor * (100 - MIN_DROP_PERCENTAGE) / 100;
		if (battery_aging_factor < min_drop_value)
			battery_aging_factor = min_drop_value;
	}

	/* update 1st aging_factor after over 150cycle */
	if (pre_aging_factor == 0) {
		min_drop_value = 10000 * (100 - MIN_DROP_PERCENTAGE) / 100;
			if (battery_aging_factor < min_drop_value)
				battery_aging_factor = min_drop_value;
	}

	cycle_write_to_fs(BATTERY_AGING_FACTOR_FILE, battery_aging_factor);

	pre_aging_factor = battery_aging_factor;

	mutex_unlock(&update_mutex);

	aging_dump();
}
EXPORT_SYMBOL(battery_aging_update_by_full);

static void aging_clear(void)
{
	mutex_lock(&update_mutex);

	pr_info("[AGING] clear battery aging\n");

	cycle_write_to_fs(BATTERY_CHARGE_FULL_FILE, 0);
	cycle_write_to_fs(BATTERY_AGING_FACTOR_FILE, 0);

	battery_aging_capacity = 0;
	battery_aging_factor = 0;

	mutex_unlock(&update_mutex);
}

void battery_aging_initial(void)
{
	bool clear_aging = false;
	int capacity ,factor;

	capacity = cycle_read_from_fs(BATTERY_CHARGE_FULL_FILE);
	factor = cycle_read_from_fs(BATTERY_AGING_FACTOR_FILE);

	if (capacity < 0) {
		pr_err("[AGING] failed to read count\n");
		clear_aging = true;
	}
	if (factor < 0) {
		pr_err("[AGING] failed to read delta\n");
		clear_aging = true;
	}

	if (battery_removed) {
		pr_info("[AGING] battery removed\n");
		cycle_write_to_fs(BATTERY_CHARGE_FULL_BACKUP_FILE, capacity);
		cycle_write_to_fs(BATTERY_REMOVE_CHECK_FILE, 0);
		clear_aging = true;
	}

	if (clear_aging) {
		aging_clear();

		capacity = 0;
		factor = 0;
	}

	mutex_lock(&update_mutex);

	battery_aging_capacity = capacity;
	battery_aging_factor = factor;

	mutex_unlock(&update_mutex);

	pr_warn("[AGING] battery aging initialized\n");
	aging_dump();
}
EXPORT_SYMBOL(battery_aging_initial);
#endif

