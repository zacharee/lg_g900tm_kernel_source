/* touch_ili7807q_mp.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: BSP-TOUCH@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/ctype.h>

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
#include <soc/qcom/lge/board_lge.h>
#endif
#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
#include <soc/mediatek/lge/board_lge.h>
#endif
#include <touch_core.h>
#include <touch_hwif.h>

#include "touch_ili7807q.h"
#include "touch_ili7807q_mp.h"

struct mp_test_items tItems[MP_TEST_ITEM] = {
	{.desp = "noise peak to peak(with panel)", .catalog = PEAK_TO_PEAK_TEST, .lcm = true},
	{.desp = "noise peak to peak(ic only)", .catalog = PEAK_TO_PEAK_TEST, .cmd = CMD_PEAK_TO_PEAK, .lcm = true},
	{.desp = "open test(integration)_sp", .catalog = OPEN_TEST, .lcm = true},
	{.desp = "raw data(no bk)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_NO_BK, .lcm = true},
	{.desp = "raw data(have bk)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_HAVE_BK, .lcm = true},
	{.desp = "calibration data(dac)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_DAC, .lcm = true},
	{.desp = "short test", .catalog = SHORT_TEST, .lcm = true},
	{.desp = "doze raw data", .catalog = MUTUAL_TEST, .lcm = true},
	{.desp = "doze peak to peak", .catalog = PEAK_TO_PEAK_TEST, .lcm = true},
	{.desp = "open test_c", .catalog = OPEN_TEST, .lcm = true},
	{.desp = "touch deltac", .catalog = MUTUAL_TEST, .lcm = true},
	/* LCM OFF */
	{.desp = "raw data(have bk) (lcm off)", .catalog = MUTUAL_TEST, .lcm = false},
	{.desp = "raw data(no bk) (lcm off)", .catalog = MUTUAL_TEST, .lcm = false},
	{.desp = "noise peak to peak(with panel) (lcm off)", .catalog = PEAK_TO_PEAK_TEST, .lcm = false},
	{.desp = "noise peak to peak(ic only) (lcm off)", .catalog = PEAK_TO_PEAK_TEST, .lcm = false},
	{.desp = "raw data_td (lcm off)", .catalog = MUTUAL_TEST, .lcm = false},
	{.desp = "peak to peak_td (lcm off)", .catalog = PEAK_TO_PEAK_TEST, .lcm = false},
};

int32_t *frame_buf = NULL;
int32_t *frame1_cbk700 = NULL, *frame1_cbk250 = NULL, *frame1_cbk200 = NULL;
int32_t *cap_dac = NULL, *cap_raw = NULL;
int g_ini_items = 0;
char csv_name[128] = {0};
char seq_item[MAX_SECTION_NUM][PARSER_MAX_KEY_NAME_LEN] = {{0}};

struct ini_file_data ini_info[PARSER_MAX_KEY_NUM];
struct core_mp_test_data *core_mp = NULL;

static void write_file(struct device *dev, char *data, bool write_time)
{
	int fd = 0;
	char *fname = NULL;
	char time_string[TIME_STR_LEN] = {0};
	struct timespec my_time = {0,};
	struct tm my_date = {0,};
	int boot_mode = 0;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);

	switch (boot_mode) {
		case TOUCH_NORMAL_BOOT:
			fname = "/data/vendor/touch/touch_self_test.txt";
			break;
		case TOUCH_MINIOS_AAT:
			fname = "/data/touch/touch_self_test.txt";
			break;
		case TOUCH_MINIOS_MFTS_FOLDER:
		case TOUCH_MINIOS_MFTS_FLAT:
		case TOUCH_MINIOS_MFTS_CURVED:
			fname = "/data/touch/touch_self_mfts.txt";
			break;
		default:
			TOUCH_I("%s : not support mode\n", __func__);
			break;
	}

	if (fname) {
		fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);
		sys_chmod(fname, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n", __func__);
		set_fs(old_fs);
		return;
	}

	if (fd >= 0) {
		if (write_time) {
			my_time = current_kernel_time();
			time_to_tm(my_time.tv_sec, sys_tz.tz_minuteswest * 60 * (-1), &my_date);
			snprintf(time_string, TIME_STR_LEN,
				"\n[%02d-%02d %02d:%02d:%02d.%03lu]\n",
				my_date.tm_mon + 1,
				my_date.tm_mday, my_date.tm_hour,
				my_date.tm_min, my_date.tm_sec,
				(unsigned long) my_time.tv_nsec / 1000000);
			sys_write(fd, time_string, strlen(time_string));
		}
		sys_write(fd, data, strlen(data));
		sys_close(fd);
	} else {
		TOUCH_I("File open failed\n");
	}
	set_fs(old_fs);
}

static void log_file_size_check(struct device *dev)
{
	char *fname = NULL;
	struct file *file;
	loff_t file_size = 0;
	int i = 0;
	char buf1[128] = {0};
	char buf2[128] = {0};
	mm_segment_t old_fs = get_fs();
	int ret = 0;
	int boot_mode = 0;

	set_fs(KERNEL_DS);

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
		fname = "/data/vendor/touch/touch_self_test.txt";
		break;
	case TOUCH_MINIOS_AAT:
		fname = "/data/touch/touch_self_test.txt";
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		fname = "/data/touch/touch_self_mfts.txt";
		break;
	default:
		TOUCH_I("%s : not support mode\n", __func__);
		break;
	}

	if (fname) {
		file = filp_open(fname, O_RDONLY, 0666);
		sys_chmod(fname, 0666);
	} else {
		TOUCH_E("%s : fname is NULL, can not open FILE\n",
				__func__);
		goto error;
	}

	if (IS_ERR(file)) {
		TOUCH_I("%s : ERR(%ld) Open file error [%s]\n",
				__func__, PTR_ERR(file), fname);
		goto error;
	}

	file_size = vfs_llseek(file, 0, SEEK_END);
	TOUCH_I("%s : [%s] file_size = %lld\n",
			__func__, fname, file_size);

	filp_close(file, 0);

	if (file_size > MAX_LOG_FILE_SIZE) {
		TOUCH_I("%s : [%s] file_size(%lld) > MAX_LOG_FILE_SIZE(%d)\n",
				__func__, fname, file_size, MAX_LOG_FILE_SIZE);

		for (i = MAX_LOG_FILE_COUNT - 1; i >= 0; i--) {
			if (i == 0)
				sprintf(buf1, "%s", fname);
			else
				sprintf(buf1, "%s.%d", fname, i);

			ret = sys_access(buf1, 0);

			if (ret == 0) {
				TOUCH_I("%s : file [%s] exist\n",
						__func__, buf1);

				if (i == (MAX_LOG_FILE_COUNT - 1)) {
					if (sys_unlink(buf1) < 0) {
						TOUCH_E("%s : failed to remove file [%s]\n",
								__func__, buf1);
						goto error;
					}

					TOUCH_I("%s : remove file [%s]\n",
							__func__, buf1);
				} else {
					sprintf(buf2, "%s.%d",
							fname,
							(i + 1));

					if (sys_rename(buf1, buf2) < 0) {
						TOUCH_E("%s : failed to rename file [%s] -> [%s]\n",
								__func__, buf1, buf2);
						goto error;
					}

					TOUCH_I("%s : rename file [%s] -> [%s]\n",
							__func__, buf1, buf2);
				}
			} else {
				TOUCH_I("%s : file [%s] does not exist (ret = %d)\n",
						__func__, buf1, ret);
			}
		}
	}

error:
	set_fs(old_fs);
	return;
}


static int katoi(char *string)
{
	int result = 0;
	unsigned int digit;
	int sign;

	if (*string == '-') {
		sign = 1;
		string += 1;
	} else {
		sign = 0;
		if (*string == '+') {
			string += 1;
		}
	}

	for (;; string += 1) {
		digit = *string - '0';
		if (digit > 9)
			break;
		result = (10 * result) + digit;
	}

	if (sign) {
		return -result;
	}
	return result;
}

static int isspace_t(int x)
{
	if (x == ' ' || x == '\t' || x == '\n' ||
			x == '\f' || x == '\b' || x == '\r')
		return 1;
	else
		return 0;
}

static void dump_benchmark_data(int32_t *max_ptr, int32_t *min_ptr)
{
	int i;

	if (MP_DBG_MSG) {
		TOUCH_I("Dump Benchmark Max\n");

		for (i = 0; i < core_mp->frame_len; i++) {
			TOUCH_I("%d, ", max_ptr[i]);
			if (i % core_mp->xch_len == core_mp->xch_len - 1)
				TOUCH_I("\n");
		}

		TOUCH_I("Dump Denchmark Min\n");

		for (i = 0; i < core_mp->frame_len; i++) {
			TOUCH_I("%d, ", min_ptr[i]);
			if (i % core_mp->xch_len == core_mp->xch_len - 1)
				TOUCH_I("\n");
		}
	}
}

static void dump_node_type_buffer(int32_t *node_ptr, u8 *name)
{
	int i;

	if (MP_DBG_MSG) {
		TOUCH_I("Dump NodeType\n");
		for (i = 0; i < core_mp->frame_len; i++) {
			TOUCH_I("%d, ", node_ptr[i]);
			if (i % core_mp->xch_len == core_mp->xch_len-1)
				TOUCH_I("\n");
		}
	}
}

static int parser_get_ini_key_value(char *section, char *key, char *value)
{
	int i = 0;
	int ret = -2;

	TOUCH_TRACE();

	for (i = 0; i < g_ini_items; i++) {
		if (ipio_strcmp(section, ini_info[i].section_name) != 0)
			continue;

		if (ipio_strcmp(key, ini_info[i].key_name) == 0) {
			memcpy(value, ini_info[i].key_value, ini_info[i].key_val_len);
			TOUCH_D(TRACE, "(key: %s, value:%s) => (ini key: %s, val: %s)\n",
					key,
					value,
					ini_info[i].key_name,
					ini_info[i].key_value);
			ret = 0;
			break;
		}
	}
	return ret;
}

static void parser_ini_nodetype(int32_t *type_ptr, char *desp, int frame_len)
{
	int i = 0, j = 0, index1 = 0, temp, count = 0;
	char str[512] = {0}, record = ',';

	TOUCH_TRACE();

	for (i = 0; i < g_ini_items; i++) {
		if ((strstr(ini_info[i].section_name, desp) <= 0) ||
			ipio_strcmp(ini_info[i].key_name, NODE_TYPE_KEY_NAME) != 0) {
			continue;
		}

		record = ',';
		for (j = 0, index1 = 0; j <= ini_info[i].key_val_len; j++) {
			if (ini_info[i].key_value[j] == ';' || j == ini_info[i].key_val_len) {
				if (record != '.') {
					memset(str, 0, sizeof(str));
					memcpy(str, &ini_info[i].key_value[index1], (j - index1));
					temp = katoi(str);

					/* Over boundary, end to calculate. */
					if (count >= frame_len) {
						TOUCH_E("count(%d) is larger than frame length, break\n", count);
						break;
					}
					type_ptr[count] = temp;
					count++;
				}
				record = ini_info[i].key_value[j];
				index1 = j + 1;
			}
		}
	}
}

static void parser_ini_benchmark(int32_t *max_ptr, int32_t *min_ptr, int8_t type, char *desp, int frame_len)
{
	int i = 0, j = 0, index1 = 0, temp, count = 0;
	char str[512] = {0}, record = ',';
	int32_t data[4];
	char benchmark_str[256] = {0};

	TOUCH_TRACE();
	/* format complete string from the name of section "_Benchmark_Data". */
	snprintf(benchmark_str, sizeof(benchmark_str), "%s%s%s", desp, "_", BENCHMARK_KEY_NAME);

	for (i = 0; i < g_ini_items; i++) {
		if ((ipio_strcmp(ini_info[i].section_name, benchmark_str) != 0) ||
			ipio_strcmp(ini_info[i].key_name, BENCHMARK_KEY_NAME) != 0)
			continue;
		record = ',';
		for (j = 0, index1 = 0; j <= ini_info[i].key_val_len; j++) {
			if (ini_info[i].key_value[j] == ',' || ini_info[i].key_value[j] == ';' ||
				ini_info[i].key_value[j] == '.' || j == ini_info[i].key_val_len) {

				if (record != '.') {
					memset(str, 0, sizeof(str));
					memcpy(str, &ini_info[i].key_value[index1], (j - index1));
					temp = katoi(str);
					data[(count % 4)] = temp;

					/* Over boundary, end to calculate. */
					if ((count / 4) >= frame_len) {
						TOUCH_E("count (%d) is larger than frame length, break\n", (count / 4));
						break;
					}

					if ((count % 4) == 3) {
						if (data[0] == 1) {
							if (type == VALUE) {
								max_ptr[count/4] = data[1] + data[2];
								min_ptr[count/4] = data[1] - data[3];
							} else {
								max_ptr[count/4] = data[1] + (data[1] * data[2]) / 100;
								min_ptr[count/4] = data[1] - (data[1] * data[3]) / 100;
							}
						} else {
							max_ptr[count/4] = INT_MAX;
							min_ptr[count/4] = INT_MIN;
						}
					}
					count++;
				}
				record = ini_info[i].key_value[j];
				index1 = j + 1;
			}
		}
	}
}

static int parser_get_tdf_value(char *str, int catalog)
{
	u32 i, ans, index = 0, flag = 0, count = 0, size;
	char s[10] = {0};

	if (!str) {
		TOUCH_E("String is null\n");
		return -1;
	}

	size = strlen(str);
	for (i = 0, count = 0; i < size; i++) {
		if (str[i] == '.') {
			flag = 1;
			continue;
		}
		s[index] = str[i];
		index++;
		if (flag)
			count++;
	}
	ans = katoi(s);

	/* Multiply by 100 to shift out of decimal point */
	if (catalog == SHORT_TEST) {
		if (count == 0)
			ans = ans * 100;
		else if (count == 1)
			ans = ans * 10;
	}

	return ans;
}

static int parser_get_u8_array(char *key, u8 *buf, u16 base, int len)
{
	char *s = key;
	char *pToken;
	int ret, conut = 0;
	long s_to_long = 0;

	if (strlen(s) == 0 || len <= 0) {
		TOUCH_E("Can't find any characters inside buffer\n");
		return -1;
	}

	/*
	 *	@base: The number base to use. The maximum supported base is 16. If base is
	 *	given as 0, then the base of the string is automatically detected with the
	 *	conventional semantics - If it begins with 0x the number will be parsed as a
	 *	hexadecimal (case insensitive), if it otherwise begins with 0, it will be
	 *	parsed as an octal number. Otherwise it will be parsed as a decimal.
	 */
	if (isspace_t((int)(unsigned char)*s) == 0) {
		while ((pToken = strsep(&s, ",")) != NULL) {
			ret = kstrtol(pToken, base, &s_to_long);
			if (ret == 0)
				buf[conut] = s_to_long;
			else
				TOUCH_E("convert string too long, ret = %d\n", ret);
			conut++;

			if (conut >= len)
				break;
		}
	}

	return conut;
}

static int parser_get_int_data(char *section, char *keyname, char *rv, int rv_len)
{
	int len = 0;
	char value[512] = { 0 };

	if (rv == NULL || section == NULL || keyname == NULL) {
		TOUCH_E("Parameters are invalid\n");
		return -EINVAL;
	}

	/* return a white-space string if get nothing */
	if (parser_get_ini_key_value(section, keyname, value) < 0) {
		snprintf(rv, rv_len, "%s", value);
		return 0;
	}

	len = snprintf(rv, rv_len, "%s", value);
	return len;
}

/* Count the number of each line and assign the content to tmp buffer */
static int parser_get_ini_phy_line(char *data, char *buffer, int maxlen)
{
	int i = 0;
	int j = 0;
	int iRetNum = -1;
	char ch1 = '\0';

	for (i = 0, j = 0; i < maxlen; j++) {
		ch1 = data[j];
		iRetNum = j + 1;
		if (ch1 == '\n' || ch1 == '\r') {/* line end */
			ch1 = data[j + 1];
			if (ch1 == '\n' || ch1 == '\r')
				iRetNum++;
			break;
		} else if (ch1 == 0x00) {
			//iRetNum = -1;
			break;	/* file end */
		}

		buffer[i] = ch1;
		i++;
	}

	buffer[i] = '\0';
	return iRetNum;
}

static char *parser_ini_str_trim_r(char *buf)
{
	int len, i;
	char *tmp = NULL;
	char x[512] = {0};
	char *y = NULL;
	char *empty = "";

	len = strlen(buf);

	if (len < sizeof(x)) {
		tmp = x;
		goto copy;
	}

	y = kzalloc(len, GFP_KERNEL);
	if (ERR_ALLOC_MEM(y)) {
		TOUCH_E("Failed to allocate tmp buf\n");
		return empty;
	}
	tmp = y;

copy:
	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}

	if (i < len)
		strncpy(tmp, (buf + i), (len - i));

	strncpy(buf, tmp, len);
	ipio_kfree((void **)&y);
	return buf;
}

static int parser_get_ini_phy_data(char *data, int fsize)
{
	int i, n = 0, ret = 0, banchmark_flag = 0, empty_section, nodetype_flag = 0;
	int offset = 0, isEqualSign = 0, scount = 0;
	char *ini_buf = NULL, *tmpSectionName = NULL;
	char M_CFG_SSL = '[';
	char M_CFG_SSR = ']';
/* char M_CFG_NIS = ':'; */
	char M_CFG_NTS = '#';
	char M_CFG_EQS = '=';

	TOUCH_TRACE();

	if (data == NULL) {
		TOUCH_E("INI data is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	ini_buf = kzalloc((PARSER_MAX_CFG_BUF + 1) * sizeof(char), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ini_buf)) {
		TOUCH_E("Failed to allocate ini_buf memory, %ld\n", PTR_ERR(ini_buf));
		ret = -ENOMEM;
		goto out;
	}

	tmpSectionName = kzalloc((PARSER_MAX_CFG_BUF + 1) * sizeof(char), GFP_KERNEL);
	if (ERR_ALLOC_MEM(tmpSectionName)) {
		TOUCH_E("Failed to allocate tmpSectionName memory, %ld\n", PTR_ERR(tmpSectionName));
		ret = -ENOMEM;
		goto out;
	}

	memset(seq_item, 0, MP_TEST_ITEM * PARSER_MAX_KEY_NAME_LEN * sizeof(char));

	while (true) {
		banchmark_flag = 0;
		empty_section = 0;
		nodetype_flag = 0;
		if (g_ini_items > PARSER_MAX_KEY_NUM) {
			TOUCH_E("MAX_KEY_NUM: Out of length\n");
			goto out;
		}

		if (offset >= fsize)
			goto out;/*over size*/

		n = parser_get_ini_phy_line(data + offset, ini_buf, PARSER_MAX_CFG_BUF);
		if (n < 0) {
			TOUCH_E("End of Line\n");
			goto out;
		}

		offset += n;

		n = strlen(parser_ini_str_trim_r(ini_buf));

		if (n == 0 || ini_buf[0] == M_CFG_NTS)
			continue;

		/* Get section names */
		if (n > 2 && ((ini_buf[0] == M_CFG_SSL && ini_buf[n - 1] != M_CFG_SSR))) {
			TOUCH_E("Bad Section: %s\n", ini_buf);
			ret = -EINVAL;
			goto out;
		} else {
			if (ini_buf[0] == M_CFG_SSL) {
				ini_info[g_ini_items].section_len = n - 2;
				if (ini_info[g_ini_items].section_len > PARSER_MAX_KEY_NAME_LEN) {
					TOUCH_E("MAX_KEY_NAME_LEN: Out Of Length\n");
					ret = INI_ERR_OUT_OF_LINE;
					goto out;
				}

				if (scount > MAX_SECTION_NUM) {
					TOUCH_E("seq_item is over than its define (%d), abort\n", scount);
					ret = INI_ERR_OUT_OF_LINE;
					goto out;
				}

				ini_buf[n - 1] = 0x00;
				strncpy((char *)tmpSectionName, ini_buf + 1, (PARSER_MAX_CFG_BUF + 1) * sizeof(char));
				banchmark_flag = 0;
				nodetype_flag = 0;
				strncpy(seq_item[scount], tmpSectionName, PARSER_MAX_KEY_NAME_LEN);
				TOUCH_D(TRACE, "Section Name: %s, Len: %d, offset = %d\n", seq_item[scount], n - 2, offset);
				scount++;
				continue;
			}
		}

		/* copy section's name without square brackets to its real buffer */
		strncpy(ini_info[g_ini_items].section_name, tmpSectionName, (PARSER_MAX_KEY_NAME_LEN * sizeof(char)));
		ini_info[g_ini_items].section_len = strlen(tmpSectionName);

		isEqualSign = 0;
		for (i = 0; i < n; i++) {
			if (ini_buf[i] == M_CFG_EQS) {
				isEqualSign = i;
				break;
			}
			if (ini_buf[i] == M_CFG_SSL || ini_buf[i] == M_CFG_SSR) {
				empty_section = 1;
				break;
			}
		}

		if (isEqualSign == 0) {
			if (empty_section)
				continue;

			if (strstr(ini_info[g_ini_items].section_name, BENCHMARK_KEY_NAME) > 0) {
				banchmark_flag = 1;
				isEqualSign = -1;
			} else if (strstr(ini_info[g_ini_items].section_name, NODE_TYPE_KEY_NAME) > 0) {
				nodetype_flag = 1;
				isEqualSign = -1;
			} else {
				continue;
			}
		}

		if (banchmark_flag) {
			ini_info[g_ini_items].key_name_len = strlen(BENCHMARK_KEY_NAME);
			strncpy(ini_info[g_ini_items].key_name, BENCHMARK_KEY_NAME, (PARSER_MAX_KEY_NAME_LEN * sizeof(char)));
			ini_info[g_ini_items].key_val_len = n;
		} else if (nodetype_flag) {
			ini_info[g_ini_items].key_name_len = strlen(NODE_TYPE_KEY_NAME);
			strncpy(ini_info[g_ini_items].key_name, NODE_TYPE_KEY_NAME, (PARSER_MAX_KEY_NAME_LEN * sizeof(char)));
			ini_info[g_ini_items].key_val_len = n;
		} else{
			ini_info[g_ini_items].key_name_len = isEqualSign;
			if (ini_info[g_ini_items].key_name_len > PARSER_MAX_KEY_NAME_LEN) {
				/* ret = CFG_ERR_OUT_OF_LEN; */
				TOUCH_E("MAX_KEY_NAME_LEN: Out Of Length\n");
				ret = INI_ERR_OUT_OF_LINE;
				goto out;
			}

			memcpy(ini_info[g_ini_items].key_name, ini_buf, ini_info[g_ini_items].key_name_len);
			ini_info[g_ini_items].key_val_len = n - isEqualSign - 1;
		}

		if (ini_info[g_ini_items].key_val_len > PARSER_MAX_KEY_VALUE_LEN) {
			TOUCH_E("MAX_KEY_VALUE_LEN: Out Of Length\n");
			ret = INI_ERR_OUT_OF_LINE;
			goto out;
		}

		memcpy(ini_info[g_ini_items].key_value, ini_buf + isEqualSign + 1, ini_info[g_ini_items].key_val_len);

		TOUCH_D(TRACE, "%s = %s\n", ini_info[g_ini_items].key_name,
			ini_info[g_ini_items].key_value);

		g_ini_items++;
	}
out:
	ipio_kfree((void **)&ini_buf);
	ipio_kfree((void **)&tmpSectionName);
	return ret;
}

static int mp_ini_parser(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int path_idx = 0;
	int i = 0;
	int mfts_mode = 0;
	u8 *tmp = NULL;
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
	struct ili7807q_data *d = to_ili7807q_data(dev);
//	const char *path[2] = {ts->dual_panel_spec[0], ts->dual_panel_spec_mfts[0]};
    const char *path[2] = {NULL,NULL};
#else
    const char *path[2] = {ts->panel_spec, ts->panel_spec_mfts};
#endif
	const struct firmware *ini = NULL;

	TOUCH_TRACE();
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
	switch(d->panel_type){
		case TP_DPT:
			path[0]=ts->dual_panel_spec[TP_DPT];
			path[1]=ts->dual_panel_spec_mfts[TP_DPT];
     		break;
		case TP_HLT:
		default:
			path[0]=ts->dual_panel_spec[TP_HLT];
			path[1]=ts->dual_panel_spec_mfts[TP_HLT];
            break;
	}
	TOUCH_I("limit spec path:normal(%s),mfts(%s)\n",path[0],path[1]);
#endif
	mfts_mode = touch_check_boot_mode(dev);
	if (mfts_mode >= TOUCH_MINIOS_MFTS_FOLDER && mfts_mode <= TOUCH_MINIOS_MFTS_CURVED)
		path_idx = 1;
	else
		path_idx = 0;

#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
	if (ts->dual_panel_spec[0] == NULL || ts->dual_panel_spec_mfts[0] == NULL) {
#else
	if (ts->panel_spec == NULL || ts->panel_spec_mfts == NULL) {
#endif

#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
       TOUCH_E("dual_panel_spec file name is null\n");
#else
       TOUCH_E("panel_spec file name is null\n");
#endif
		ret = -1;
		goto out;
	}

	TOUCH_I("touch_panel_spec file path = %s\n", path[path_idx]);

	/* Get ini file location */
	ret = request_firmware(&ini, path[path_idx], dev);
	if (ret) {
		TOUCH_E("fail to request_firmware inipath: %s (ret:%d)\n",
				path[path_idx], ret);
		ret = -EMP_INI;
		goto out;
	}

	if (ERR_ALLOC_MEM(ini->data)) {
		ret = -EMP_INI;
		TOUCH_E("ini->data is NULL\n");
		goto out;
	}
	TOUCH_I("firmware ini size:%zu, data: %p\n", ini->size, ini->data);

	g_ini_items = 0;

	/* Initialise ini strcture */
	for (i = 0; i < PARSER_MAX_KEY_NUM; i++) {
		memset(ini_info[i].section_name, 0, PARSER_MAX_KEY_NAME_LEN);
		memset(ini_info[i].key_name, 0, PARSER_MAX_KEY_NAME_LEN);
		memset(ini_info[i].key_value, 0, PARSER_MAX_KEY_VALUE_LEN);
		ini_info[i].section_len = 0;
		ini_info[i].key_name_len = 0;
		ini_info[i].key_val_len = 0;
	}

	/* Change data type from const to u8 in order for lower case. */
	tmp = vmalloc(ini->size + 1);
	if (ERR_ALLOC_MEM(tmp)) {
		TOUCH_E("Failed to allocate tmp memory, %ld\n", PTR_ERR(tmp));
		ret = -ENOMEM;
		goto out;
	}
	memcpy(tmp, (u8 *)ini->data, ini->size);

	for (i = 0; i < ini->size; i++)
		tmp[i] = tolower(tmp[i]);

	ret = parser_get_ini_phy_data(tmp, ini->size);
	if (ret < 0) {
		TOUCH_E("Failed to get ini's physical data, ret = %d\n", ret);
	}

	TOUCH_I("Parsing INI file done\n");

out:
	release_firmware(ini);
	ipio_vfree((void **)&tmp);
	return ret;
}

static void mp_print_csv_header(char *csv, int *csv_len, int *csv_line, int file_size)
{
	int i, seq, tmp_len = *csv_len, tmp_line = *csv_line;

	TOUCH_TRACE();

	/* header must has 19 line*/
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "==============================================================================\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "ILITek C-TP Utility %x : Driver Sensor Test\n", core_mp->chip_pid);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Confidentiality Notice:\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Any information of this tool is confidential and privileged.\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "@ ILI TECHNOLOGY CORP. All Rights Reserved.\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "==============================================================================\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Firmware Version ,0x%x\n", core_mp->fw_ver);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Panel information ,XCH=%d, YCH=%d\n", core_mp->xch_len, core_mp->ych_len);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "INI Release Version ,%s\n", core_mp->ini_date);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "INI Release Date ,%s\n", core_mp->ini_ver);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Test Item:\n");
	tmp_line++;

	for (seq = 0; seq < core_mp->run_num; seq++) {
		i = core_mp->run_index[seq];
		tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "	  ---%s\n", tItems[i].desp);
		tmp_line++;
	}

	while (tmp_line < 19) {
		tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "\n");
		tmp_line++;
	}

	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "==============================================================================\n");

	*csv_len = tmp_len;
	*csv_line = tmp_line;
}

static void mp_print_csv_tail(char *csv, int *csv_len, int file_size)
{
	int i, seq, tmp_len = *csv_len;

	TOUCH_TRACE();

	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "==============================================================================\n");
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Result_Summary			\n");

	for (seq = 0; seq < core_mp->run_num; seq++) {
		i = core_mp->run_index[seq];
		if (tItems[i].item_result == MP_DATA_PASS)
			tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "	  {%s}	   ,OK\n", tItems[i].desp);
		else
			tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "	  {%s}	   ,NG\n", tItems[i].desp);
	}

	*csv_len = tmp_len;
}

static void mp_print_csv_cdc_cmd(char *csv, int *csv_len, int index, int file_size)
{
	int i, slen = 0, tmp_len = *csv_len, size;
	char str[128] = {0};
	char *open_sp_cmd[] = {"open dac", "open raw1", "open raw2", "open raw3"};
	char *open_c_cmd[] = {"open cap1 dac", "open cap1 raw"};
	char *name = tItems[index].desp;

	TOUCH_TRACE();

	if (ipio_strcmp(name, "open test(integration)_sp") == 0) {
		size = ARRAY_SIZE(open_sp_cmd);
		for (i = 0; i < size; i++) {
			slen = parser_get_int_data("pv5_4 command", open_sp_cmd[i], str, sizeof(str));
			if (slen < 0)
				TOUCH_E("Failed to get CDC command %s from ini\n", open_sp_cmd[i]);
			else
				tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "%s = ,%s\n", open_sp_cmd[i], str);
		}
	} else if (ipio_strcmp(name, "open test_c") == 0) {
		size = ARRAY_SIZE(open_c_cmd);
		for (i = 0; i < size; i++) {
			slen = parser_get_int_data("pv5_4 command", open_c_cmd[i], str, sizeof(str));
			if (slen < 0)
				TOUCH_E("Failed to get CDC command %s from ini\n", open_sp_cmd[i]);
			else
				tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "%s = ,%s\n", open_c_cmd[i], str);
		}
	} else {
		slen = parser_get_int_data("pv5_4 command", name, str, sizeof(str));
		if (slen < 0)
			TOUCH_E("Failed to get CDC command %s from ini\n", name);
		else
			tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "CDC command = ,%s\n", str);
	}
	*csv_len = tmp_len;
}

static void mp_compare_cdc_show_result(int index, int32_t *tmp, char *csv,
				int *csv_len, int type, int32_t *max_ts,
				int32_t *min_ts, const char *desp, int file_zise)
{
	int x, y, tmp_len = *csv_len;
	int mp_result = MP_DATA_PASS;

	TOUCH_TRACE();

	if (ERR_ALLOC_MEM(tmp)) {
		TOUCH_E("The data of test item is null (%p)\n", tmp);
		mp_result = -EMP_INVAL;
		goto out;
	}

	/* print X raw only */
	for (x = 0; x < core_mp->xch_len; x++) {
		if (x == 0) {
			DUMP("\n %s ", desp);
			tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "\n	   %s ,", desp);
		}
		DUMP("  X_%d	,", (x+1));
		tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "	 X_%d  ,", (x+1));
	}

	DUMP("\n");
	tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "\n");

	for (y = 0; y < core_mp->ych_len; y++) {
		DUMP("  Y_%d	,", (y+1));
		tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "	 Y_%d  ,", (y+1));

		for (x = 0; x < core_mp->xch_len; x++) {
			int shift = y * core_mp->xch_len + x;

			/* In Short teset, we only identify if its value is low than min threshold. */
			if (tItems[index].catalog == SHORT_TEST) {
				if (tmp[shift] < min_ts[shift]) {
					DUMP(" #%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "#%7d,", tmp[shift]);
					mp_result = MP_DATA_FAIL;
				} else {
					DUMP(" %7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), " %7d, ", tmp[shift]);
				}
				continue;
			}

			if ((tmp[shift] <= max_ts[shift] && tmp[shift] >= min_ts[shift]) || (type != TYPE_JUGE)) {
				if ((tmp[shift] == INT_MAX || tmp[shift] == INT_MIN) && (type == TYPE_BENCHMARK)) {
					DUMP("%s", "BYPASS,");
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "BYPASS,");
				} else {
					DUMP(" %7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), " %7d, ", tmp[shift]);
				}
			} else {
				if (tmp[shift] > max_ts[shift]) {
					DUMP(" *%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "*%7d,", tmp[shift]);
				} else {
					DUMP(" #%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "#%7d,", tmp[shift]);
				}
				mp_result = MP_DATA_FAIL;
			}
		}
		DUMP("\n");
		tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "\n");
	}

out:
	if (type == TYPE_JUGE) {
		if (mp_result == MP_DATA_PASS) {
			TOUCH_I("\n Result : PASS\n");
			tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "Result : PASS\n");
		} else {
			TOUCH_I("\n Result : FAIL\n");
			tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "Result : FAIL\n");
		}
	}
	*csv_len = tmp_len;
}

#define ABS(a, b) ((a > b) ? (a - b) : (b - a))
#define ADDR(x, y) ((y * core_mp->xch_len) + (x))

int compare_charge(int32_t *charge_rate, int x, int y, int32_t *inNodeType,
		int Charge_AA, int Charge_Border, int Charge_Notch)
{
	int OpenThreadhold, tempY, tempX, ret, k;
	int sx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
	int sy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

	TOUCH_TRACE();

	ret = charge_rate[ADDR(x, y)];

	/*Setting Threadhold from node type	 */
	if (charge_rate[ADDR(x, y)] == 0)
		return ret;
	else if ((inNodeType[ADDR(x, y)] & AA_Area) == AA_Area)
		OpenThreadhold = Charge_AA;
	else if ((inNodeType[ADDR(x, y)] & Border_Area) == Border_Area)
		OpenThreadhold = Charge_Border;
	else if ((inNodeType[ADDR(x, y)] & Notch) == Notch)
		OpenThreadhold = Charge_Notch;
	else
		return ret;

	/* compare carge rate with 3*3 node */
	/* by pass => 1.no compare 2.corner 3.Skip_Micro 4.full open fail node */
	for (k = 0; k < 8; k++) {
		tempX = x + sx[k];
		tempY = y + sy[k];

		/*out of range */
		if ((tempX < 0) || (tempX >= core_mp->xch_len) || (tempY < 0) || (tempY >= core_mp->ych_len))
			continue;

		if ((inNodeType[ADDR(tempX, tempY)] == NO_COMPARE) || ((inNodeType[ADDR(tempX, tempY)] & Round_Corner) == Round_Corner) ||
		((inNodeType[ADDR(tempX, tempY)] & Skip_Micro) == Skip_Micro) || charge_rate[ADDR(tempX, tempY)] == 0)
			continue;

		if ((charge_rate[ADDR(tempX, tempY)] - charge_rate[ADDR(x, y)]) > OpenThreadhold)
			return OpenThreadhold;
	}
	return ret;
}

int full_open_rate_compare(int32_t *full_open, int32_t *cbk, int x, int y, int32_t inNodeType, int full_open_rate)
{
	int ret = true;

	TOUCH_TRACE();

	if ((inNodeType == NO_COMPARE) || ((inNodeType & Round_Corner) == Round_Corner))
		return true;

	if (full_open[ADDR(x, y)] < (cbk[ADDR(x, y)] * full_open_rate / 100))
		ret = false;

	return ret;
}

static int32_t open_sp_formula(int dac, int raw, int tvch, int tvcl)
{
	s32 ret = 0;
	u16 id = core_mp->chip_id;
	if (id == ILI9881_CHIP) {
		ret = ((dac * 10000 * 161 / 100) - (16384 / 2 - raw) * 20000 * 7 / 16384 * 36 / 10) / (tvch - tvcl) / 2;
	} else {
		ret = ((dac * 10000 * 131 / 100) - (16384 / 2 - raw) * 20000 * 7 / 16384 * 36 / 10) / (tvch - tvcl) / 2;
	}
	return ret;
}

static int32_t open_c_formula(int dac, int raw, int tvch, int gain)
{
	s32 inCap1Value = 0;

	u16 id = core_mp->chip_id;
	u8 type = core_mp->chip_type;

	char str[128] = {0};
	int inFoutRange = 16384;
	int inFoutRange_half = 8192;
	int inVadc_range = 36;
	int inVbk = 39;
	int inCbk_step = core_mp->cbk_step;/* from mp.ini */
	int inCint = core_mp->cint;/* from mp.ini */
	int inVdrv = tvch;/* from mp.ini */
	int inGain = gain;/*  from mp.ini */
	int inMagnification = 10;
	int inPartDac = 0;
	int inPartRaw = 0;
	int accuracy = 0;
	parser_get_int_data("open_c_formula", "accuracy", str, sizeof(str));
	accuracy = katoi(str);
	if ((inCbk_step == 0) || (inCint == 0)) {
		if (id == ILI9881_CHIP) {
			if ((type == ILI_N) || (type == ILI_O)){
				inCbk_step = 32;
				inCint = 70;
			}
		} else if (id == ILI9882_CHIP) {
			if (type == ILI_N) {
				inCbk_step = 42;
				inCint = 70;
			} else if (type == ILI_H) {
				inCbk_step = 42;
				inCint = 69;
			}
		} else if (id == ILI7807_CHIP){
			if (type == ILI_Q) {
				inCbk_step = 28;
				inCint= 70;
			} else if (type == ILI_S) {
				inCbk_step = 38;
				inCint= 66;
				inVbk = 42;
				inVdrv = inVdrv / 10;
				inGain = inGain / 10;

			} else if (type == ILI_V) {
				inCbk_step = 28;
				inCint= 70;
			}
		}
	}

	inPartDac = (dac * inCbk_step * inVbk / 2);
	inPartRaw = ((raw - inFoutRange_half) * inVadc_range * inCint * 10 / inFoutRange);

	if (accuracy) {
		inCap1Value = ((inPartDac + inPartRaw) * 10) / inVdrv / inMagnification / inGain;
	} else {
		inCap1Value = (inPartDac + inPartRaw) / inVdrv / inMagnification / inGain;
	}

	return inCap1Value;
}

void allnode_open_cdc_result(int index, int *buf, int *dac, int *raw)
{
	int i;
	char *desp = tItems[index].desp;

	TOUCH_TRACE();

	if (ipio_strcmp(desp, "open test(integration)_sp") == 0) {
		for (i = 0; i < core_mp->frame_len; i++)
			buf[i] = open_sp_formula(dac[i], raw[i], core_mp->op_tvch, core_mp->op_tvcl);
	} else if (ipio_strcmp(desp, "open test_c") == 0) {
		for (i = 0; i < core_mp->frame_len; i++)
			buf[i] = open_c_formula(dac[i], raw[i], core_mp->op_tvch - core_mp->op_tvcl, core_mp->op_gain);
	}
}

static int codeToOhm(int32_t Code, u16 *v_tdf, u16 *h_tdf)
{
	u16 id = core_mp->chip_id;
	u8 type = core_mp->chip_type;

	int inTVCH = core_mp->short_tvch;
	int inTVCL = core_mp->short_tvcl;
	int inVariation = core_mp->short_variation;
	int inTDF1 = 0;
	int inTDF2 = 0;
	int inCint = core_mp->short_cint;
	int inRinternal = core_mp->short_rinternal;
	s32 temp = 0;
//	int j = 0;

	if (core_mp->isLongV) {
		inTDF1 = *v_tdf;
		inTDF2 = *(v_tdf + 1);
	} else {
		inTDF1 = *h_tdf;
		inTDF2 = *(h_tdf + 1);
	}

	if (inVariation == 0) {
		inVariation = 100;
	}

	if ((inCint == 0) ||  (inRinternal == 0)) {
		if (id == ILI9881_CHIP) {
			if ((type == ILI_N) || (type == ILI_O)){
				inRinternal = 1915;
				inCint = 70;
			}
		} else if (id == ILI9882_CHIP) {
			if (type == ILI_N) {
				inRinternal = 1354;
				inCint = 70;
			} else if (type == ILI_H) {
				inRinternal = 1354;
				inCint = 69;
			}
		} else if (id == ILI7807_CHIP){
			if (type == ILI_Q) {
				inRinternal = 1500;
				inCint= 70;
			} else if (type == ILI_S) {
				inRinternal = 1500;
				inCint= 66;
				inTVCH = inTVCH/10;
				inTVCL = inTVCL/10;
			} else if (type == ILI_V) {
				inRinternal = 1500;
				inCint= 70;
			}
		}
	}
		if (Code == 0) {//Code[j] ?
			TOUCH_E("code is invalid\n");
		} else {
			temp = ((inTVCH - inTVCL) * inVariation * (inTDF1 - inTDF2) * (1 << 12) / (9 * Code * inCint)) * 1000;
			temp = (temp - inRinternal) / 1000;
		}

	/* Unit = M Ohm */
	return temp;
}

static int short_test(int index, int frame_index)
{
	int j = 0, ret = 0;
	u16 v_tdf[2] = {0};
	u16 h_tdf[2] = {0};
	u32 pid = core_mp->chip_pid >> 8;
	char str[32] = {0};
	TOUCH_TRACE();

	v_tdf[0] = tItems[index].v_tdf_1;
	v_tdf[1] = tItems[index].v_tdf_2;
	h_tdf[0] = tItems[index].h_tdf_1;
	h_tdf[1] = tItems[index].h_tdf_2;
	parser_get_int_data("short test", "tvch", str, sizeof(str));
	core_mp->short_tvch = katoi(str);

	parser_get_int_data("short test", "tvcl", str, sizeof(str));
	core_mp->short_tvcl = katoi(str);

	parser_get_int_data("short test", "variation", str, sizeof(str));
	core_mp->short_variation = katoi(str);

	parser_get_int_data("short test", "cint", str, sizeof(str));
	core_mp->short_cint = katoi(str);

	parser_get_int_data("short test", "rinternal", str, sizeof(str));
	core_mp->short_rinternal = katoi(str);

	if ((pid != 0x988117) && (pid != 0x988118) && (pid != 0x78071A) && (pid != 0x78071C)) {
		if ((core_mp->short_cint == 0) || (core_mp->short_rinternal == 0)) {
			TOUCH_E("Failed to get short parameter");
			return -1;
		}
	}

	/* Calculate code to ohm and save to tItems[index].buf */
	for (j = 0; j < core_mp->frame_len; j++)
		tItems[index].buf[frame_index * core_mp->frame_len + j] = codeToOhm(frame_buf[j], v_tdf, h_tdf);

	return ret;
}

static int mp_cdc_get_pv5_4_command(u8 *cmd, int len, int index)
{
	int slen = 0;
	char str[128] = {0};
	char *key = tItems[index].desp;

	slen = parser_get_int_data("pv5_4 command", key, str, sizeof(str));
	if (slen < 0)
		return -1;

	if (parser_get_u8_array(str, cmd, 16, len) < 0)
		return -1;

	return 0;
}

static int mp_cdc_init_cmd_common(u8 *cmd, int len, int index)
{
	return mp_cdc_get_pv5_4_command(cmd, len, index);
}

static int allnode_open_cdc_data(int mode, int *buf)
{
	int i = 0, ret = 0, len = 0;
	int inDACp = 0, inDACn = 0;
	int cdc_len = core_mp->cdc_len;
	u8 cmd[16] = {0};
	u8 check_sum = 0;
	u8 header = 0x0;
	u8 *ori = NULL;
	char str[128] = {0};
	char *key[] = {"open dac", "open raw1", "open raw2", "open raw3",
			"open cap1 dac", "open cap1 raw"};

	TOUCH_TRACE();

	/* Multipling by 2 is due to the 16 bit in each node */
	len = (core_mp->xch_len * core_mp->ych_len * 2) + 2;

	TOUCH_D(TRACE, "Read X/Y Channel length = %d, mode = %d\n", len, mode);

	if (len <= 2) {
		TOUCH_E("Length is invalid\n");
		ret = -EMP_INVAL;
		goto out;
	}

	/* CDC init. Read command from ini file */
	if (parser_get_int_data("pv5_4 command", key[mode], str, sizeof(str)) < 0) {
		TOUCH_E("Failed to parse PV54 command, ret = %d\n", ret);
		ret = -EMP_PARSE;
		goto out;
	}

	parser_get_u8_array(str, cmd, 16, sizeof(cmd));

	if (MP_DBG_MSG)
		ili7807q_dump_packet(cmd, 8, sizeof(cmd), 0, "Open SP command");

	/* NOTE: If TP driver is doing MP test and commanding 0xF1 to FW, we add a checksum
	 * to the last index and plus 1 with size. */
	check_sum = ili7807q_calc_data_checksum(&cmd[0], cdc_len - 1);
	cmd[cdc_len - 1] = check_sum;
	header = cmd[0];
	ret = ili7807q_reg_write(core_mp->dev, header, &cmd[1], cdc_len - 1);
	if (ret < 0) {
		TOUCH_E("Write CDC command failed\n");
		ret = -EMP_CMD;
		goto out;
	}

	/* Check busy */
	if (core_mp->busy_cdc == POLL_CHECK)
		ret = ili7807q_check_cdc_busy(core_mp->dev, 50, 50);
	else
		ret = ili7807q_check_int_status(core_mp->dev, false);

	if (ret < 0) {
		ret = -EMP_CHECK_BUY;
		goto out;
	}

	if (core_mp->core_ver < CORE_VER_1420) {
		/* Prepare to get cdc data */
		cmd[0] = CMD_READ_DATA_CTRL;
		cmd[1] = CMD_GET_CDC_DATA;

		if (ili7807q_reg_write(core_mp->dev, CMD_READ_DATA_CTRL, &cmd[1], 1)) {
			TOUCH_E("Write (0x%x, 0x%x) error\n", cmd[0], cmd[1]);
			ret = -EMP_CMD;
			goto out;
		}
		/* Waiting for FW to prepare cdc data */
		mdelay(1);

		if (ili7807q_reg_write(core_mp->dev, CMD_GET_CDC_DATA, cmd, 0)) {
			TOUCH_E("Write (0x%x) error\n", cmd[1]);
			ret = -EMP_CMD;
			goto out;
		}
		/* Waiting for FW to prepare cdc data */
		mdelay(1);
	}

	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ori)) {
		TOUCH_E("Failed to allocate ori, (%ld)\n", PTR_ERR(ori));
		ret = -EMP_NOMEM;
		goto out;
	}

	/* Get original frame(cdc) data */
	//if (ili7807q_reg_read(core_mp->dev, CMD_NONE, ori, len)) {
	 if(ili7807q_reg_read(core_mp->dev, CMD_READ_DIRECT, ori, len)){
		TOUCH_E("Read cdc data error, len = %d\n", len);
		ret = -EMP_GET_CDC;
		goto out;
	}

	if (MP_DBG_MSG)
		ili7807q_dump_packet(ori, 8, len, 0, "Open SP CDC original");

	/* Convert original data to the physical one in each node */
	for (i = 0; i < core_mp->frame_len; i++) {
		if ((mode == 0) || (mode == 4)) {
			/* DAC - P */
			if (((ori[(2 * i) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				inDACp = 0 - (int)(ori[(2 * i) + 1] & 0x7F);
			} else {
				inDACp = ori[(2 * i) + 1] & 0x7F;
			}

			/* DAC - N */
			if (((ori[(1 + (2 * i)) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				inDACn = 0 - (int)(ori[(1 + (2 * i)) + 1] & 0x7F);
			} else {
				inDACn = ori[(1 + (2 * i)) + 1] & 0x7F;
			}

			buf[i] = inDACp + inDACn;
		} else {
			/* H byte + L byte */
			int32_t tmp = (ori[(2 * i) + 1] << 8) + ori[(1 + (2 * i)) + 1];
			if ((tmp & 0x8000) == 0x8000)
				buf[i] = tmp - 65536;
			else
				buf[i] = tmp;

		}
	}

	if (MP_DBG_MSG)
		ili7807q_dump_packet(buf, 10, core_mp->frame_len,  core_mp->xch_len, "Open SP CDC combined");
out:
	ipio_kfree((void **)&ori);
	return ret;
}

static int allnode_mutual_cdc_data(int index)
{
	int i, ret = 0, len = 0;
	int inDACp = 0, inDACn = 0;
	int cdc_len = core_mp->cdc_len;
	u8 cmd[16] = {0};
	u8 *ori = NULL;
	u8 check_sum = 0;
	u8 header = 0x0;

	TOUCH_TRACE();

	/* Multipling by 2 is due to the 16 bit in each node */
	len = (core_mp->xch_len * core_mp->ych_len * 2) + 2;

	TOUCH_D(TRACE, "Read X/Y Channel length = %d\n", len);

	if (len <= 2) {
		TOUCH_E("Length is invalid\n");
		ret = -EMP_INVAL;
		goto out;
	}

	memset(cmd, 0xFF, sizeof(cmd));

	/* CDC init */
	if (mp_cdc_init_cmd_common(cmd, sizeof(cmd), index) < 0) {
		TOUCH_E("Failed to get cdc command\n");
		ret = -EMP_CMD;
		goto out;
	}

	if (MP_DBG_MSG)
		ili7807q_dump_packet(cmd, 8, cdc_len, 0, "Mutual CDC command");

	/* NOTE: If TP driver is doing MP test and commanding 0xF1 to FW, we add a checksum
	 * to the last index and plus 1 with size. */
	check_sum = ili7807q_calc_data_checksum(&cmd[0], cdc_len - 1);
	cmd[cdc_len - 1] = check_sum;
	header = cmd[0];
	ret = ili7807q_reg_write(core_mp->dev, header, &cmd[1], cdc_len - 1);
	if (ret < 0) {
		TOUCH_E("Write CDC command failed\n");
		ret = -EMP_CMD;
		goto out;
	}

	/* Check busy */
	if (core_mp->busy_cdc == POLL_CHECK)
		ret = ili7807q_check_cdc_busy(core_mp->dev, 50, 50);
	else
		ret = ili7807q_check_int_status(core_mp->dev, false);

	if (ret < 0) {
		ret = -EMP_CHECK_BUY;
		goto out;
	}

	if (core_mp->core_ver < CORE_VER_1420) {
		/* Prepare to get cdc data */
		cmd[0] = CMD_READ_DATA_CTRL;
		cmd[1] = CMD_GET_CDC_DATA;

		if (ili7807q_reg_write(core_mp->dev, CMD_READ_DATA_CTRL, &cmd[1], 1)) {
			TOUCH_E("Write (0x%x, 0x%x) error\n", cmd[0], cmd[1]);
			ret = -EMP_CMD;
			goto out;
		}
		/* Waiting for FW to prepare cdc data */
		mdelay(1);

		if (ili7807q_reg_write(core_mp->dev, CMD_GET_CDC_DATA, cmd, 0)) {
			TOUCH_E("Write (0x%x) error\n", cmd[1]);
			ret = -EMP_CMD;
			goto out;
		}
		/* Waiting for FW to prepare cdc data */
		mdelay(1);
	}

	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ori)) {
		TOUCH_E("Failed to allocate ori, (%ld)\n", PTR_ERR(ori));
		ret = -EMP_NOMEM;
		goto out;
	}

	/* Get original frame(cdc) data */
	//if (ili7807q_reg_read(core_mp->dev, CMD_NONE, ori, len)) {
	if (ili7807q_reg_read(core_mp->dev, CMD_READ_DIRECT, ori, len)) {
		TOUCH_E("Read cdc data error, len = %d\n", len);
		ret = -EMP_GET_CDC;
		goto out;
	}

	if (MP_DBG_MSG)
		ili7807q_dump_packet(ori, 8, len, 0, "Mutual CDC original");

	if (ERR_ALLOC_MEM(frame_buf)) {
		frame_buf = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(frame_buf)) {
			TOUCH_E("Failed to allocate FrameBuffer mem (%ld)\n", PTR_ERR(frame_buf));
			ret = -EMP_NOMEM;
			goto out;
		}
	} else {
		memset(frame_buf, 0x0, core_mp->frame_len);
	}

	/* Convert original data to the physical one in each node */
	for (i = 0; i < core_mp->frame_len; i++) {
		if (ipio_strcmp(tItems[index].desp, "calibration data(dac)") == 0) {
			/* DAC - P */
			if (((ori[(2 * i) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				inDACp = 0 - (int)(ori[(2 * i) + 1] & 0x7F);
			} else {
				inDACp = ori[(2 * i) + 1] & 0x7F;
			}

			/* DAC - N */
			if (((ori[(1 + (2 * i)) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				inDACn = 0 - (int)(ori[(1 + (2 * i)) + 1] & 0x7F);
			} else {
				inDACn = ori[(1 + (2 * i)) + 1] & 0x7F;
			}

			frame_buf[i] = (inDACp + inDACn) / 2;
		} else {
			/* H byte + L byte */
			int32_t tmp = (ori[(2 * i) + 1] << 8) + ori[(1 + (2 * i)) + 1];

			if ((tmp & 0x8000) == 0x8000)
				frame_buf[i] = tmp - 65536;
			else
				frame_buf[i] = tmp;

			if (ipio_strcmp(tItems[index].desp, "raw data(no bk)") == 0 ||
				ipio_strcmp(tItems[index].desp, "raw data(no bk) (lcm off)") == 0) {
					frame_buf[i] -= core_mp->no_bk_shift;
			}
		}
	}

	if (MP_DBG_MSG)
		ili7807q_dump_packet(frame_buf, 32, core_mp->frame_len, core_mp->xch_len, "Mutual CDC combined");
out:
	ipio_kfree((void **)&ori);
	return ret;
}

static void compare_MaxMin_result(int index, int32_t *data)
{
	int x, y;

	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			int shift = y * core_mp->xch_len;

			if (tItems[index].max_buf[shift + x] < data[shift + x])
				tItems[index].max_buf[shift + x] = data[shift + x];

			if (tItems[index].min_buf[shift + x] > data[shift + x])
				tItems[index].min_buf[shift + x] = data[shift + x];
		}
	}
}

static int create_mp_test_frame_buffer(int index, int frame_count)
{
	TOUCH_TRACE();

	TOUCH_D(TRACE, "Create MP frame buffers (index = %d), count = %d\n",
			index, frame_count);

	if (ERR_ALLOC_MEM(tItems[index].buf)) {
		tItems[index].buf = vmalloc(frame_count * core_mp->frame_len * sizeof(int32_t));
		if (ERR_ALLOC_MEM(tItems[index].buf)) {
			TOUCH_E("Failed to allocate buf mem\n");
			return -ENOMEM;
		}
	}

	if (ERR_ALLOC_MEM(tItems[index].result_buf)) {
		tItems[index].result_buf = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(tItems[index].result_buf)) {
			TOUCH_E("Failed to allocate result_buf mem\n");
			return -ENOMEM;
		}
	}

	if (ERR_ALLOC_MEM(tItems[index].max_buf)) {
		tItems[index].max_buf = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(tItems[index].max_buf)) {
			TOUCH_E("Failed to allocate max_buf mem\n");
			return -ENOMEM;
		}
	}

	if (ERR_ALLOC_MEM(tItems[index].min_buf)) {
		tItems[index].min_buf = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(tItems[index].min_buf)) {
			TOUCH_E("Failed to allocate min_buf mem\n");
			return -ENOMEM;
		}
	}

	if (tItems[index].spec_option == BENCHMARK) {
		if (ERR_ALLOC_MEM(tItems[index].bench_mark_max)) {
			tItems[index].bench_mark_max = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
			if (ERR_ALLOC_MEM(tItems[index].bench_mark_max)) {
				TOUCH_E("Failed to allocate bench_mark_max mem\n");
				return -ENOMEM;
			}
		}
		if (ERR_ALLOC_MEM(tItems[index].bench_mark_min)) {
			tItems[index].bench_mark_min = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
			if (ERR_ALLOC_MEM(tItems[index].bench_mark_min)) {
				TOUCH_E("Failed to allocate bench_mark_min mem\n");
				return -ENOMEM;
			}
		}
	}
	return 0;
}

static int mutual_test(int index)
{
	int i = 0, j = 0, x = 0, y = 0, ret = 0, get_frame_cont = 1;

	TOUCH_TRACE();

	TOUCH_D(TRACE, "index = %d, desp = %s, Frame Count = %d\n",
		index, tItems[index].desp, tItems[index].frame_count);

	/*
	 * We assume that users who are calling the test forget to config frame count
	 * as 1, so we just help them to set it up.
	 */
	if (tItems[index].frame_count <= 0) {
		TOUCH_E("Frame count is zero, which is at least set as 1\n");
		tItems[index].frame_count = 1;
	}

	ret = create_mp_test_frame_buffer(index, tItems[index].frame_count);
	if (ret < 0) {
		ret = -EMP_NOMEM;
		goto out;
	}

	/* Init Max/Min buffer */
	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			tItems[index].max_buf[y * core_mp->xch_len + x] = INT_MIN;
			tItems[index].min_buf[y * core_mp->xch_len + x] = INT_MAX;
		}
	}

	if (tItems[index].catalog != PEAK_TO_PEAK_TEST)
		get_frame_cont = tItems[index].frame_count;

	if (tItems[index].spec_option == BENCHMARK) {
		parser_ini_benchmark(tItems[index].bench_mark_max, tItems[index].bench_mark_min,
				tItems[index].type_option, tItems[index].desp, core_mp->frame_len);
		dump_benchmark_data(tItems[index].bench_mark_max, tItems[index].bench_mark_min);
	}

	for (i = 0; i < get_frame_cont; i++) {
		ret = allnode_mutual_cdc_data(index);
		if (ret < 0) {
			TOUCH_E("Failed to initialise CDC data, %d\n", ret);
			goto out;
		}
		switch (tItems[index].catalog) {
		case SHORT_TEST:
			short_test(index, i);
			break;
		default:
			for (j = 0; j < core_mp->frame_len; j++)
				tItems[index].buf[i * core_mp->frame_len + j] = frame_buf[j];
			break;
		}
		compare_MaxMin_result(index, &tItems[index].buf[i * core_mp->frame_len]);
	}

out:
	return ret;
}

static int open_sp_test(int index)
{
	struct mp_test_P540_open open[tItems[index].frame_count];
	int i = 0, x = 0, y = 0, ret = 0, addr = 0;
	int Charge_AA = 0, Charge_Border = 0, Charge_Notch = 0, full_open_rate = 0;
	char str[512] = {0};

	TOUCH_TRACE();

	TOUCH_D(TRACE, "index = %d, desp = %s, Frame Count = %d\n",
		index, tItems[index].desp, tItems[index].frame_count);

	/*
	 * We assume that users who are calling the test forget to config frame count
	 * as 1, so we just help them to set it up.
	 */
	if (tItems[index].frame_count <= 0) {
		TOUCH_E("Frame count is zero, which is at least set as 1\n");
		tItems[index].frame_count = 1;
	}

	ret = create_mp_test_frame_buffer(index, tItems[index].frame_count);
	if (ret < 0) {
		ret = -EMP_NOMEM;
		goto out;
	}

	if (ERR_ALLOC_MEM(frame1_cbk700)) {
		frame1_cbk700 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(frame1_cbk700)) {
			TOUCH_E("Failed to allocate frame1_cbk700 buffer\n");
			return -EMP_NOMEM;
		}
	} else {
		memset(frame1_cbk700, 0x0, core_mp->frame_len);
	}

	if (ERR_ALLOC_MEM(frame1_cbk250)) {
		frame1_cbk250 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(frame1_cbk250)) {
			TOUCH_E("Failed to allocate frame1_cbk250 buffer\n");
			ipio_kfree((void **)&frame1_cbk700);
			return -EMP_NOMEM;
		}
	} else {
		memset(frame1_cbk250, 0x0, core_mp->frame_len);
	}

	if (ERR_ALLOC_MEM(frame1_cbk200)) {
		frame1_cbk200 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(frame1_cbk200)) {
			TOUCH_E("Failed to allocate cbk buffer\n");
			ipio_kfree((void **)&frame1_cbk700);
			ipio_kfree((void **)&frame1_cbk250);
			return -EMP_NOMEM;
		}
	} else {
		memset(frame1_cbk200, 0x0, core_mp->frame_len);
	}

	tItems[index].node_type = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(tItems[index].node_type)) {
		TOUCH_E("Failed to allocate node_type FRAME buffer\n");
		return -EMP_NOMEM;
	}

	/* Init Max/Min buffer */
	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			tItems[index].max_buf[y * core_mp->xch_len + x] = INT_MIN;
			tItems[index].min_buf[y * core_mp->xch_len + x] = INT_MAX;
		}
	}

	if (tItems[index].spec_option == BENCHMARK) {
		parser_ini_benchmark(tItems[index].bench_mark_max, tItems[index].bench_mark_min,
				tItems[index].type_option, tItems[index].desp, core_mp->frame_len);
		dump_benchmark_data(tItems[index].bench_mark_max, tItems[index].bench_mark_min);
	}

	parser_ini_nodetype(tItems[index].node_type, NODE_TYPE_KEY_NAME, core_mp->frame_len);
	dump_node_type_buffer(tItems[index].node_type, "node type");

	ret = parser_get_int_data(tItems[index].desp, "charge_aa", str, sizeof(str));
	if (ret || ret == 0)
		Charge_AA = katoi(str);

	ret = parser_get_int_data(tItems[index].desp, "charge_border", str, sizeof(str));
	if (ret || ret == 0)
		Charge_Border = katoi(str);

	ret = parser_get_int_data(tItems[index].desp, "charge_notch", str, sizeof(str));
	if (ret || ret == 0)
		Charge_Notch = katoi(str);

	ret = parser_get_int_data(tItems[index].desp, "full open", str, sizeof(str));
	if (ret || ret == 0)
		full_open_rate = katoi(str);

	ret = parser_get_int_data(tItems[index].desp, "tvch", str, sizeof(str));
	if (ret || ret == 0)
		core_mp->op_tvch = katoi(str);

	ret = parser_get_int_data(tItems[index].desp, "tvcl", str, sizeof(str));
	if (ret || ret == 0)
		core_mp->op_tvcl = katoi(str);

	if (ret < 0) {
		TOUCH_E("Failed to get parameters from ini file\n");
		ret = -EMP_PARSE;
		goto out;
	}

	TOUCH_D(TRACE, "open_sp_test: frame_cont %d, AA %d, Border %d, Notch %d, full_open_rate %d\n",
			tItems[index].frame_count, Charge_AA, Charge_Border, Charge_Notch, full_open_rate);

	for (i = 0; i < tItems[index].frame_count; i++) {
		open[i].tdf_700 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].tdf_250 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].tdf_200 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].cbk_700 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].cbk_250 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].cbk_200 = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].charg_rate = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].full_Open = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].dac = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	}

	for (i = 0; i < tItems[index].frame_count; i++) {
		ret = allnode_open_cdc_data(0, open[i].dac);
		if (ret < 0) {
			TOUCH_E("Failed to get Open SP DAC data, %d\n", ret);
			goto out;
		}
		ret = allnode_open_cdc_data(1, open[i].tdf_700);
		if (ret < 0) {
			TOUCH_E("Failed to get Open SP Raw1 data, %d\n", ret);
			goto out;
		}
		ret = allnode_open_cdc_data(2, open[i].tdf_250);
		if (ret < 0) {
			TOUCH_E("Failed to get Open SP Raw2 data, %d\n", ret);
			goto out;
		}
		ret = allnode_open_cdc_data(3, open[i].tdf_200);
		if (ret < 0) {
			TOUCH_E("Failed to get Open SP Raw3 data, %d\n", ret);
			goto out;
		}
		allnode_open_cdc_result(index, open[i].cbk_700, open[i].dac, open[i].tdf_700);
		allnode_open_cdc_result(index, open[i].cbk_250, open[i].dac, open[i].tdf_250);
		allnode_open_cdc_result(index, open[i].cbk_200, open[i].dac, open[i].tdf_200);

		addr = 0;

		/* record fist frame for debug */
		if (i == 0) {
			memcpy(frame1_cbk700, open[i].cbk_700, core_mp->frame_len * sizeof(int32_t));
			memcpy(frame1_cbk250, open[i].cbk_250, core_mp->frame_len * sizeof(int32_t));
			memcpy(frame1_cbk200, open[i].cbk_200, core_mp->frame_len * sizeof(int32_t));
		}

		if (MP_DBG_MSG) {
			ili7807q_dump_packet(open[i].cbk_700, 10, core_mp->frame_len, core_mp->xch_len, "cbk 700");
			ili7807q_dump_packet(open[i].cbk_250, 10, core_mp->frame_len, core_mp->xch_len, "cbk 250");
			ili7807q_dump_packet(open[i].cbk_200, 10, core_mp->frame_len, core_mp->xch_len, "cbk 200");
		}

		for (y = 0; y < core_mp->ych_len; y++) {
			for (x = 0; x < core_mp->xch_len; x++) {
				open[i].charg_rate[addr] = open[i].cbk_250[addr] * 100 / open[i].cbk_700[addr];
				open[i].full_Open[addr] = open[i].cbk_700[addr] - open[i].cbk_200[addr];
				addr++;
			}
		}

		if (MP_DBG_MSG) {
			ili7807q_dump_packet(open[i].charg_rate, 10, core_mp->frame_len, core_mp->xch_len, "origin charge rate");
			ili7807q_dump_packet(open[i].full_Open, 10, core_mp->frame_len, core_mp->xch_len, "origin full open");
		}

		addr = 0;
		for (y = 0; y < core_mp->ych_len; y++) {
			for (x = 0; x < core_mp->xch_len; x++) {
				if (full_open_rate_compare(open[i].full_Open, open[i].cbk_700, x, y, tItems[index].node_type[addr], full_open_rate) == false) {
					tItems[index].buf[(i * core_mp->frame_len) + addr] = 0;
					open[i].charg_rate[addr] = 0;
				}
				addr++;
			}
		}

		if (MP_DBG_MSG)
			ili7807q_dump_packet(&tItems[index].buf[(i * core_mp->frame_len)], 10, core_mp->frame_len, core_mp->xch_len, "after full_open_rate_compare");

		addr = 0;
		for (y = 0; y < core_mp->ych_len; y++) {
			for (x = 0; x < core_mp->xch_len; x++) {
				tItems[index].buf[(i * core_mp->frame_len) + addr] = compare_charge(open[i].charg_rate, x, y, tItems[index].node_type, Charge_AA, Charge_Border, Charge_Notch);
				addr++;
			}
		}

		if (MP_DBG_MSG)
			ili7807q_dump_packet(&tItems[index].buf[(i * core_mp->frame_len)], 10, core_mp->frame_len, core_mp->xch_len, "after compare charge rate");

		compare_MaxMin_result(index, &tItems[index].buf[(i * core_mp->frame_len)]);
	}

out:
	ipio_kfree((void **)&tItems[index].node_type);

	for (i = 0; i < tItems[index].frame_count; i++) {
		ipio_kfree((void **)&open[i].tdf_700);
		ipio_kfree((void **)&open[i].tdf_250);
		ipio_kfree((void **)&open[i].tdf_200);
		ipio_kfree((void **)&open[i].cbk_700);
		ipio_kfree((void **)&open[i].cbk_250);
		ipio_kfree((void **)&open[i].cbk_200);
		ipio_kfree((void **)&open[i].charg_rate);
		ipio_kfree((void **)&open[i].full_Open);
		ipio_kfree((void **)&open[i].dac);
	}
	return ret;
}

static int open_c_test(int index)
{
	struct mp_test_open_c open[tItems[index].frame_count];
	int i = 0, x = 0, y = 0, ret = 0, addr = 0;
	char str[512] = {0};
	u32 pid = core_mp->chip_pid >> 8;
	TOUCH_TRACE();

	if (tItems[index].frame_count <= 0) {
		TOUCH_E("Frame count is zero, which is at least set as 1\n");
		tItems[index].frame_count = 1;
	}

	if (create_mp_test_frame_buffer(index, tItems[index].frame_count) < 0) {
		ret = -EMP_NOMEM;
		goto out;
	}

	if (cap_dac == NULL) {
		cap_dac = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(cap_dac)) {
			TOUCH_E("Failed to allocate cap_dac buffer\n");
			return -EMP_NOMEM;
		}
	} else {
		memset(cap_dac, 0x0, core_mp->frame_len);
	}

	if (cap_raw == NULL) {
		cap_raw = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		if (ERR_ALLOC_MEM(cap_raw)) {
			TOUCH_E("Failed to allocate cap_raw buffer\n");
			ipio_kfree((void **)&cap_dac);
			return -EMP_NOMEM;
		}
	} else {
		memset(cap_raw, 0x0, core_mp->frame_len);
	}

	/* Init Max/Min buffer */
	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			tItems[index].max_buf[y * core_mp->xch_len + x] = INT_MIN;
			tItems[index].min_buf[y * core_mp->xch_len + x] = INT_MAX;
		}
	}

	if (tItems[index].spec_option == BENCHMARK) {
		parser_ini_benchmark(tItems[index].bench_mark_max, tItems[index].bench_mark_min,
				tItems[index].type_option, tItems[index].desp, core_mp->frame_len);
		dump_benchmark_data(tItems[index].bench_mark_max, tItems[index].bench_mark_min);
	}

	ret = parser_get_int_data(tItems[index].desp, "gain", str, sizeof(str));
	if (ret || ret == 0)
		core_mp->op_gain = katoi(str);

	ret = parser_get_int_data(tItems[index].desp, "tvch", str, sizeof(str));
	if (ret || ret == 0)
		core_mp->op_tvch = katoi(str);

	ret = parser_get_int_data(tItems[index].desp, "tvcl", str, sizeof(str));
	if (ret || ret == 0)
		core_mp->op_tvcl = katoi(str);

	if (ret < 0) {
		TOUCH_E("Failed to get parameters from ini file\n");
		ret = -EMP_PARSE;
		goto out;
	}
	parser_get_int_data(tItems[index].desp, "cbk_step", str, sizeof(str));
	core_mp->cbk_step = katoi(str);

	parser_get_int_data(tItems[index].desp, "cint", str, sizeof(str));
	core_mp->cint = katoi(str);
	if ((pid != 0x988117) && (pid != 0x988118) && (pid != 0x78071A) && (pid != 0x78071C)) {
		if ((core_mp->cbk_step == 0) || (core_mp->cint == 0)) {
			TOUCH_E("Failed to get open parameter\n");
			ret = -EMP_PARSE;
			goto out;
		}
	}
	TOUCH_D(TRACE, "open_test_c: frame_cont = %d, gain = %d, tvch = %d, tvcl = %d\n",
		tItems[index].frame_count, core_mp->op_gain, core_mp->op_tvch, core_mp->op_tvcl);

	for (i = 0; i < tItems[index].frame_count; i++) {
		open[i].cap_dac = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].cap_raw = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
		open[i].dcl_cap = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	}

	for (i = 0; i < tItems[index].frame_count; i++) {
		ret = allnode_open_cdc_data(4, open[i].cap_dac);
		if (ret < 0) {
			TOUCH_E("Failed to get Open CAP DAC data, %d\n", ret);
			goto out;
		}
		ret = allnode_open_cdc_data(5, open[i].cap_raw);
		if (ret < 0) {
			TOUCH_E("Failed to get Open CAP RAW data, %d\n", ret);
			goto out;
		}

		allnode_open_cdc_result(index, open[i].dcl_cap, open[i].cap_dac, open[i].cap_raw);

		/* record fist frame for debug */
		if (i == 0) {
			memcpy(cap_dac, open[i].cap_dac, core_mp->frame_len * sizeof(int32_t));
			memcpy(cap_raw, open[i].cap_raw, core_mp->frame_len * sizeof(int32_t));
		}

		if (MP_DBG_MSG)
			ili7807q_dump_packet(open[i].dcl_cap, 10, core_mp->frame_len, core_mp->xch_len, "DCL_Cap");

		addr = 0;
		for (y = 0; y < core_mp->ych_len; y++) {
			for (x = 0; x < core_mp->xch_len; x++) {
				tItems[index].buf[(i * core_mp->frame_len) + addr] = open[i].dcl_cap[addr];
				addr++;
			}
		}
		compare_MaxMin_result(index, &tItems[index].buf[i * core_mp->frame_len]);
	}

out:
	for (i = 0; i < tItems[index].frame_count; i++) {
		ipio_kfree((void **)&open[i].cap_dac);
		ipio_kfree((void **)&open[i].cap_raw);
		ipio_kfree((void **)&open[i].dcl_cap);
	}
	return ret;
}

static int mp_get_timing_info(void)
{
	int slen = 0;
	char str[256] = {0};
	u8 info[64] = {0};
	char *key = "timing_info_raw";

	TOUCH_TRACE();

	core_mp->isLongV = 0;

	slen = parser_get_int_data("pv5_4 command", key, str, sizeof(str));
	if (slen < 0)
		return -1;

	if (parser_get_u8_array(str, info, 16, slen) < 0)
		return -1;

	core_mp->isLongV = info[6];

	TOUCH_I("DDI Mode = %s\n", (core_mp->isLongV ? "Long V" : "Long H"));
	return 0;
}

static int mp_test_data_sort_average(int32_t *oringin_data, int index, int32_t *avg_result)
{
	int i, j, k, x, y, len = 5, size, ret = 0;
	int32_t u32temp;
	int u32up_frame, u32down_frame;
	int32_t *u32sum_raw_data;
	int32_t *u32data_buff;

	TOUCH_TRACE();

	if (tItems[index].frame_count <= 1)
		return 0;

	if (ERR_ALLOC_MEM(oringin_data)) {
		TOUCH_E("Input wrong address\n");
		return -ENOMEM;
	}

	u32data_buff = kcalloc(core_mp->frame_len * tItems[index].frame_count, sizeof(int32_t), GFP_KERNEL);
	u32sum_raw_data = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(u32sum_raw_data) || (ERR_ALLOC_MEM(u32data_buff))) {
		TOUCH_E("Failed to allocate u32sum_raw_data FRAME buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	size = core_mp->frame_len * tItems[index].frame_count;
	for (i = 0; i < size; i++)
		u32data_buff[i] = oringin_data[i];

	u32up_frame = tItems[index].frame_count * tItems[index].highest_percentage / 100;
	u32down_frame = tItems[index].frame_count * tItems[index].lowest_percentage / 100;
	TOUCH_D(TRACE, "Up=%d, Down=%d -%s\n", u32up_frame, u32down_frame, tItems[index].desp);

	if (MP_DBG_MSG) {
		TOUCH_I("\n[Show Original frist%d and last%d node data]\n", len, len);
		for (i = 0; i < core_mp->frame_len; i++) {
			for (j = 0 ; j < tItems[index].frame_count ; j++) {
				if ((i < len) || (i >= (core_mp->frame_len-len)))
					TOUCH_I("%d,", u32data_buff[j * core_mp->frame_len + i]);
			}
			if ((i < len) || (i >= (core_mp->frame_len-len)))
				TOUCH_I("\n");
		}
	}

	for (i = 0; i < core_mp->frame_len; i++) {
		for (j = 0; j < tItems[index].frame_count-1; j++) {
			for (k = 0; k < (tItems[index].frame_count-1-j); k++) {
				x = i+k*core_mp->frame_len;
				y = i+(k+1)*core_mp->frame_len;
				if (*(u32data_buff+x) > *(u32data_buff+y)) {
					u32temp = *(u32data_buff+x);
					*(u32data_buff+x) = *(u32data_buff+y);
					*(u32data_buff+y) = u32temp;
				}
			}
		}
	}

	if (MP_DBG_MSG) {
		TOUCH_I("\n[After sorting frist%d and last%d node data]\n", len, len);
		for (i = 0; i < core_mp->frame_len; i++) {
			for (j = u32down_frame; j < tItems[index].frame_count - u32up_frame; j++) {
				if ((i < len) || (i >= (core_mp->frame_len - len)))
					TOUCH_I("%d,", u32data_buff[i + j * core_mp->frame_len]);
			}
			if ((i < len) || (i >= (core_mp->frame_len-len)))
				TOUCH_I("\n");
		}
	}

	for (i = 0 ; i < core_mp->frame_len ; i++) {
		u32sum_raw_data[i] = 0;
		for (j = u32down_frame; j < tItems[index].frame_count - u32up_frame; j++)
			u32sum_raw_data[i] += u32data_buff[i + j * core_mp->frame_len];

		avg_result[i] = u32sum_raw_data[i] / (tItems[index].frame_count - u32down_frame - u32up_frame);
	}

	if (MP_DBG_MSG) {
		TOUCH_I("\n[Average result frist%d and last%d node data]\n", len, len);
		for (i = 0; i < core_mp->frame_len; i++) {
			if ((i < len) || (i >= (core_mp->frame_len-len)))
				TOUCH_I("%d,", avg_result[i]);
		}
		if ((i < len) || (i >= (core_mp->frame_len-len)))
			TOUCH_I("\n");
	}

out:
	ipio_kfree((void **)&u32data_buff);
	ipio_kfree((void **)&u32sum_raw_data);
	return ret;
}

static void mp_compare_cdc_result(int index, int32_t *tmp, int32_t *max_ts, int32_t *min_ts, int *result)
{
	int i;

	TOUCH_TRACE();

	if (ERR_ALLOC_MEM(tmp)) {
		TOUCH_E("The data of test item is null (%p)\n", tmp);
		*result = MP_DATA_FAIL;
		return;
	}

	if (tItems[index].catalog == SHORT_TEST) {
		for (i = 0; i < core_mp->frame_len; i++) {
			if (tmp[i] < min_ts[i]) {
				*result = MP_DATA_FAIL;
				return;
			}
		}
	} else {
		for (i = 0; i < core_mp->frame_len; i++) {
			if (tmp[i] > max_ts[i] || tmp[i] < min_ts[i]) {
				*result = MP_DATA_FAIL;
				return;
			}
		}
	}
}

static int mp_comp_result_before_retry(int index)
{
	int i, ret = 0, test_result = MP_DATA_PASS;
	int32_t *max_threshold = NULL, *min_threshold = NULL;

	TOUCH_TRACE();

	max_threshold = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(max_threshold)) {
		TOUCH_E("Failed to allocate threshold FRAME buffer\n");
		test_result = MP_DATA_FAIL;
		ret = -EMP_NOMEM;
		goto out;
	}

	min_threshold = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(min_threshold)) {
		TOUCH_E("Failed to allocate threshold FRAME buffer\n");
		test_result = MP_DATA_FAIL;
		ret = -EMP_NOMEM;
		goto out;
	}

	/* Show general test result as below */
	if (ERR_ALLOC_MEM(tItems[index].buf) || ERR_ALLOC_MEM(tItems[index].max_buf) ||
			ERR_ALLOC_MEM(tItems[index].min_buf) || ERR_ALLOC_MEM(tItems[index].result_buf)) {
		TOUCH_E("This test item (%s) has no data inside its buffer\n", tItems[index].desp);
		test_result = MP_DATA_FAIL;
		goto out;
	}

	if (tItems[index].spec_option == BENCHMARK) {
		for (i = 0; i < core_mp->frame_len; i++) {
			max_threshold[i] = tItems[index].bench_mark_max[i];
			min_threshold[i] = tItems[index].bench_mark_min[i];
		}
	} else {
		for (i = 0; i < core_mp->frame_len; i++) {
			max_threshold[i] = tItems[index].max;
			min_threshold[i] = tItems[index].min;
		}
	}

	/* general result */
	if (tItems[index].trimmed_mean && tItems[index].catalog != PEAK_TO_PEAK_TEST) {
		mp_test_data_sort_average(tItems[index].buf, index, tItems[index].result_buf);
		mp_compare_cdc_result(index, tItems[index].result_buf, max_threshold, min_threshold, &test_result);
	} else {
		mp_compare_cdc_result(index, tItems[index].max_buf, max_threshold, min_threshold, &test_result);
		mp_compare_cdc_result(index, tItems[index].min_buf, max_threshold, min_threshold, &test_result);
	}

out:
	ipio_kfree((void **)&max_threshold);
	ipio_kfree((void **)&min_threshold);
	tItems[index].item_result = test_result;
	return ret;
}

static void mp_do_retry(int index, int count)
{
	TOUCH_TRACE();

	if (count == 0) {
		TOUCH_I("Finish retry action\n");
		return;
	}

	TOUCH_I("retry = %d, item = %s\n", count, tItems[index].desp);

	tItems[index].do_test(index);

	if (mp_comp_result_before_retry(index) < 0)
		return mp_do_retry(index, count - 1);
}

static int mp_show_result(struct device *dev)
{
	int ret = MP_DATA_PASS, seq;
	int i = 0, j, csv_len = 0, line_count = 0, get_frame_cont = 1;
	int32_t *max_threshold = NULL, *min_threshold = NULL;
	char *csv = NULL;
	struct ili7807q_data *d = to_ili7807q_data(dev);

	TOUCH_TRACE();

	csv = vmalloc(CSV_FILE_SIZE);
	if (ERR_ALLOC_MEM(csv)) {
		TOUCH_E("Failed to allocate CSV mem\n");
		ret = -EMP_NOMEM;
		goto out;
	}

	max_threshold = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	min_threshold = kcalloc(core_mp->frame_len, sizeof(int32_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(max_threshold) || ERR_ALLOC_MEM(min_threshold)) {
		TOUCH_E("Failed to allocate threshold FRAME buffer\n");
		ret = -EMP_NOMEM;
		goto out;
	}

	/* file create , time log */
	if (d->lcd_mode == LCD_MODE_U3) {
		write_file(dev, "\nShow_sd Test Start", false);
	} else if (d->lcd_mode == LCD_MODE_U0) {
		write_file(dev, "\nShow_lpwg_sd Test Start", false);
	}
	write_file(dev, "\n", true);

	if (parser_get_ini_key_value("pv5_4 command", "date", core_mp->ini_date) < 0)
		memcpy(core_mp->ini_date, "Unknown", strlen("Unknown"));
	if (parser_get_ini_key_value("pv5_4 command", "version", core_mp->ini_ver) < 0)
		memcpy(core_mp->ini_ver, "Unknown", strlen("Unknown"));

	mp_print_csv_header(csv, &csv_len, &line_count, CSV_FILE_SIZE);

	for (seq = 0; seq < core_mp->run_num; seq++) {
		i = core_mp->run_index[seq];
		get_frame_cont = 1;

		if (tItems[i].item_result == MP_DATA_PASS) {
			TOUCH_I("[%s],OK \n", tItems[i].desp);
			csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "\n[%s],OK\n", tItems[i].desp);
		} else {
			TOUCH_I("[%s],NG \n", tItems[i].desp);
			csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "\n[%s],NG\n", tItems[i].desp);
		}

		mp_print_csv_cdc_cmd(csv, &csv_len, i, CSV_FILE_SIZE);

		TOUCH_I("Frame count = %d\n", tItems[i].frame_count);
		csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "Frame count = %d\n", tItems[i].frame_count);

		if (tItems[i].trimmed_mean && tItems[i].catalog != PEAK_TO_PEAK_TEST) {
			TOUCH_I("lowest percentage = %d\n", tItems[i].lowest_percentage);
			csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "lowest percentage = %d\n", tItems[i].lowest_percentage);

			TOUCH_I("highest percentage = %d\n", tItems[i].highest_percentage);
			csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "highest percentage = %d\n", tItems[i].highest_percentage);
		}

		/* Show result of benchmark max and min */
		if (tItems[i].spec_option == BENCHMARK) {
			for (j = 0; j < core_mp->frame_len; j++) {
				max_threshold[j] = tItems[i].bench_mark_max[j];
				min_threshold[j] = tItems[i].bench_mark_min[j];
			}
			mp_compare_cdc_show_result(i, tItems[i].bench_mark_max, csv, &csv_len, TYPE_BENCHMARK, max_threshold, min_threshold, "Max_Bench", CSV_FILE_SIZE);
			mp_compare_cdc_show_result(i, tItems[i].bench_mark_min, csv, &csv_len, TYPE_BENCHMARK, max_threshold, min_threshold, "Min_Bench", CSV_FILE_SIZE);
		} else {
			for (j = 0; j < core_mp->frame_len; j++) {
				max_threshold[j] = tItems[i].max;
				min_threshold[j] = tItems[i].min;
			}

			TOUCH_I("Max = %d\n", tItems[i].max);
			csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "Max = %d\n", tItems[i].max);

			TOUCH_I("Min = %d\n", tItems[i].min);
			csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "Min = %d\n", tItems[i].min);
		}

		if (ipio_strcmp(tItems[i].desp, "open test(integration)_sp") == 0) {
			mp_compare_cdc_show_result(i, frame1_cbk700, csv, &csv_len, TYPE_NO_JUGE, max_threshold, min_threshold, "frame1 cbk700", CSV_FILE_SIZE);
			mp_compare_cdc_show_result(i, frame1_cbk250, csv, &csv_len, TYPE_NO_JUGE, max_threshold, min_threshold, "frame1 cbk250", CSV_FILE_SIZE);
			mp_compare_cdc_show_result(i, frame1_cbk200, csv, &csv_len, TYPE_NO_JUGE, max_threshold, min_threshold, "frame1 cbk200", CSV_FILE_SIZE);
		}

		if (ipio_strcmp(tItems[i].desp, "open test_c") == 0) {
			mp_compare_cdc_show_result(i, cap_dac, csv, &csv_len, TYPE_NO_JUGE, max_threshold, min_threshold, "CAP_DAC", CSV_FILE_SIZE);
			mp_compare_cdc_show_result(i, cap_raw, csv, &csv_len, TYPE_NO_JUGE, max_threshold, min_threshold, "CAP_RAW", CSV_FILE_SIZE);
		}

		if (ERR_ALLOC_MEM(tItems[i].buf) || ERR_ALLOC_MEM(tItems[i].max_buf) || ERR_ALLOC_MEM(tItems[i].min_buf)) {
			TOUCH_E("This test item (%s) has no data inside its buffer\n", tItems[i].desp);
			continue;
		}

		/* Show general test result as below */
		if (tItems[i].trimmed_mean && tItems[i].catalog != PEAK_TO_PEAK_TEST) {
			mp_compare_cdc_show_result(i, tItems[i].result_buf, csv, &csv_len, TYPE_JUGE, max_threshold, min_threshold, "Mean result", CSV_FILE_SIZE);
		} else {
			mp_compare_cdc_show_result(i, tItems[i].max_buf, csv, &csv_len, TYPE_JUGE, max_threshold, min_threshold, "Max Hold", CSV_FILE_SIZE);
			mp_compare_cdc_show_result(i, tItems[i].min_buf, csv, &csv_len, TYPE_JUGE, max_threshold, min_threshold, "Min Hold", CSV_FILE_SIZE);
		}
		if (tItems[i].catalog != PEAK_TO_PEAK_TEST)
			get_frame_cont = tItems[i].frame_count;

		/* result of each frame */
		for (j = 0; j < get_frame_cont; j++) {
			char frame_name[128] = {0};
			snprintf(frame_name, (CSV_FILE_SIZE - csv_len), "Frame %d", (j+1));
			mp_compare_cdc_show_result(i, &tItems[i].buf[(j*core_mp->frame_len)], csv, &csv_len, TYPE_NO_JUGE, max_threshold, min_threshold, frame_name, CSV_FILE_SIZE);
		}
	}

	mp_print_csv_tail(csv, &csv_len, CSV_FILE_SIZE);

	write_file(dev, csv, false);

out:
	ipio_vfree((void **)&csv);
	ipio_kfree((void **)&max_threshold);
	ipio_kfree((void **)&min_threshold);
	return ret;
}

static int mp_init_item(struct device *dev)
{
	int i = 0, ret = 0;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_fw_info *fw_info = &d->ic_info.fw_info;
	struct ili7807q_protocol_info *protocol_info = &d->ic_info.protocol_info;
	struct ili7807q_panel_info *panel_info = &d->tp_info.panel_info;
	struct ili7807q_core_info *core_info = &d->ic_info.core_info;
	struct ili7807q_chip_info *chip_info = &d->ic_info.chip_info;

	TOUCH_TRACE();

	/* In case that core_mp didn't be freed successful in previous. */
	if (!ERR_ALLOC_MEM(core_mp))
		ipio_kfree((void **)&core_mp);

	core_mp = kzalloc(sizeof(*core_mp), GFP_KERNEL);
	if (ERR_ALLOC_MEM(core_mp)) {
		TOUCH_E("Failed to allocate core_mp mem\n");
		return -EMP_NOMEM;
	}

	core_mp->dev = dev;

	core_mp->chip_pid = chip_info->pid;
	core_mp->chip_id = chip_info->id;
	core_mp->chip_type = chip_info->type;
	core_mp->chip_ver = chip_info->ver;
	core_mp->fw_ver = COM_4BYTE(fw_info->core, fw_info->customer_code, fw_info->major, fw_info->minor);
	core_mp->protocol_ver = COM_3BYTE(protocol_info->major, protocol_info->mid, protocol_info->minor);
	core_mp->core_ver = COM_3BYTE(core_info->code_base, core_info->minor, core_info->revision_major);
	core_mp->cdc_len = 15 + 1;

	core_mp->no_bk_shift = 8192;
	core_mp->xch_len = panel_info->nXChannelNum;
	core_mp->ych_len = panel_info->nYChannelNum;
	core_mp->frame_len = core_mp->xch_len * core_mp->ych_len;

	core_mp->tdf = 240;
	core_mp->busy_cdc = INT_CHECK;
	core_mp->retry = false;
	core_mp->lost_benchmark = false;

	TOUCH_I("============== TP & Panel info ================\n");
	TOUCH_I("CHIP = 0x%x\n", core_mp->chip_pid);
	TOUCH_I("Firmware version = %x\n", core_mp->fw_ver);
	TOUCH_I("Protocol version = %x\n", core_mp->protocol_ver);
	TOUCH_I("Core version = %x\n", core_mp->core_ver);
	TOUCH_I("Read CDC Length = %d\n", core_mp->cdc_len);
	TOUCH_I("X length = %d, Y length = %d\n", core_mp->xch_len, core_mp->ych_len);
	TOUCH_I("Frame length = %d\n", core_mp->frame_len);
	TOUCH_I("Check busy method = %s\n", (core_mp->busy_cdc ? "Polling" : "Interrupt"));
	TOUCH_I("===============================================\n");

	if (core_mp->xch_len <= 0 || core_mp->xch_len <= 0) {
		TOUCH_E("Invalid frame length (%d, %d)\n", core_mp->xch_len, core_mp->xch_len );
		ret = -EMP_INVAL;
		goto out;
	}

	for (i = 0; i < MP_TEST_ITEM; i++) {
		tItems[i].spec_option = 0;
		tItems[i].type_option = 0;
		tItems[i].run = false;
		tItems[i].max = 0;
		tItems[i].max_res = MP_DATA_FAIL;
		tItems[i].item_result = MP_DATA_PASS;
		tItems[i].min = 0;
		tItems[i].min_res = MP_DATA_FAIL;
		tItems[i].frame_count = 0;
		tItems[i].trimmed_mean = 0;
		tItems[i].lowest_percentage = 0;
		tItems[i].highest_percentage = 0;
		tItems[i].v_tdf_1 = 0;
		tItems[i].v_tdf_2 = 0;
		tItems[i].h_tdf_1 = 0;
		tItems[i].h_tdf_2 = 0;
		tItems[i].goldenmode = 0;
		tItems[i].result_buf = NULL;
		tItems[i].buf = NULL;
		tItems[i].max_buf = NULL;
		tItems[i].min_buf = NULL;
		tItems[i].bench_mark_max = NULL;
		tItems[i].bench_mark_min = NULL;
		tItems[i].node_type = NULL;

		switch (tItems[i].catalog) {
		case OPEN_TEST:
			if (ipio_strcmp(tItems[i].desp, "open test(integration)_sp") == 0)
				tItems[i].do_test = open_sp_test;
			else if (ipio_strcmp(tItems[i].desp, "open test_c") == 0)
				tItems[i].do_test = open_c_test;
			else
				tItems[i].do_test = mutual_test;
			break;
		case MUTUAL_TEST:
		case SHORT_TEST:
		case PEAK_TO_PEAK_TEST:
			tItems[i].do_test = mutual_test;
			break;
		default:
			tItems[i].do_test = mutual_test;
			break;
		}

		tItems[i].result = kmalloc(16, GFP_KERNEL);
		snprintf(tItems[i].result, 16, "%s", "FAIL");
	}

out:
	return ret;
}

static void mp_test_run(int index)
{
	int i = index;
	char str[512] = {0};

	TOUCH_TRACE();

	/* Get parameters from ini */
	parser_get_int_data(tItems[i].desp, "spec option", str, sizeof(str));
	tItems[i].spec_option = katoi(str);
	parser_get_int_data(tItems[i].desp, "type option", str, sizeof(str));
	tItems[i].type_option = katoi(str);
	parser_get_int_data(tItems[i].desp, "frame count", str, sizeof(str));
	tItems[i].frame_count = katoi(str);
	parser_get_int_data(tItems[i].desp, "trimmed mean", str, sizeof(str));
	tItems[i].trimmed_mean = katoi(str);
	parser_get_int_data(tItems[i].desp, "lowest percentage", str, sizeof(str));
	tItems[i].lowest_percentage = katoi(str);
	parser_get_int_data(tItems[i].desp, "highest percentage", str, sizeof(str));
	tItems[i].highest_percentage = katoi(str);
	parser_get_int_data(tItems[i].desp, "goldenmode", str, sizeof(str));
	tItems[i].goldenmode = katoi(str);

	if (tItems[i].goldenmode && (tItems[i].spec_option != tItems[i].goldenmode))
		core_mp->lost_benchmark = true;

	/* Get TDF value from ini */
	if (tItems[i].catalog == SHORT_TEST) {
		parser_get_int_data(tItems[i].desp, "v_tdf_1", str, sizeof(str));
		tItems[i].v_tdf_1 = parser_get_tdf_value(str, tItems[i].catalog);
		parser_get_int_data(tItems[i].desp, "v_tdf_2", str, sizeof(str));
		tItems[i].v_tdf_2 = parser_get_tdf_value(str, tItems[i].catalog);
		parser_get_int_data(tItems[i].desp, "h_tdf_1", str, sizeof(str));
		tItems[i].h_tdf_1 = parser_get_tdf_value(str, tItems[i].catalog);
		parser_get_int_data(tItems[i].desp, "h_tdf_2", str, sizeof(str));
		tItems[i].h_tdf_2 = parser_get_tdf_value(str, tItems[i].catalog);
	} else {
		parser_get_int_data(tItems[i].desp, "v_tdf", str, sizeof(str));
		tItems[i].v_tdf_1 = parser_get_tdf_value(str, tItems[i].catalog);
		parser_get_int_data(tItems[i].desp, "h_tdf", str, sizeof(str));
		tItems[i].h_tdf_1 = parser_get_tdf_value(str, tItems[i].catalog);
	}

	/* Get threshold from ini structure in parser */
	parser_get_int_data(tItems[i].desp, "max", str, sizeof(str));
	tItems[i].max = katoi(str);
	parser_get_int_data(tItems[i].desp, "min", str, sizeof(str));
	tItems[i].min = katoi(str);

	TOUCH_D(TRACE, "%s: run = %d, max = %d, min = %d, frame_count = %d\n", tItems[i].desp,
		tItems[i].run, tItems[i].max, tItems[i].min, tItems[i].frame_count);

	TOUCH_D(TRACE, "v_tdf_1 = %d, v_tdf_2 = %d, h_tdf_1 = %d, h_tdf_2 = %d", tItems[i].v_tdf_1,
			tItems[i].v_tdf_2, tItems[i].h_tdf_1, tItems[i].h_tdf_2);

	TOUCH_I("Run MP Test Item : %s\n", tItems[i].desp);
	tItems[i].do_test(i);

	/* Check result before do retry (if enabled)  */
	if (mp_comp_result_before_retry(i) < 0) {
		if (core_mp->retry) {
			TOUCH_E("MP failed, doing retry\n");
			mp_do_retry(i, RETRY_COUNT);
		}
	}
}

static void mp_test_free(void)
{
	int i;

	TOUCH_TRACE();

	TOUCH_I("Free all allocated mem for MP\n");

	for (i = 0; i < MP_TEST_ITEM; i++) {
		tItems[i].run = false;
		tItems[i].max_res = MP_DATA_FAIL;
		tItems[i].min_res = MP_DATA_FAIL;
		tItems[i].item_result = MP_DATA_PASS;

		if (tItems[i].spec_option == BENCHMARK) {
			ipio_kfree((void **)&tItems[i].bench_mark_max);
			ipio_kfree((void **)&tItems[i].bench_mark_min);
		}
		ipio_kfree((void **)&tItems[i].node_type);
		ipio_kfree((void **)&tItems[i].result);
		ipio_kfree((void **)&tItems[i].result_buf);
		ipio_kfree((void **)&tItems[i].max_buf);
		ipio_kfree((void **)&tItems[i].min_buf);
		ipio_vfree((void **)&tItems[i].buf);
	}

	ipio_kfree((void **)&cap_raw);
	ipio_kfree((void **)&cap_dac);
	ipio_kfree((void **)&frame1_cbk700);
	ipio_kfree((void **)&frame1_cbk250);
	ipio_kfree((void **)&frame1_cbk200);
	ipio_kfree((void **)&frame_buf);
	ipio_kfree((void **)&core_mp);
}

static int mp_write_ret_to_terminal(char *buf, bool lcm_on)
{
	int i, seq, len = 0;
	/* LCD ON TEST ITEM */
	int rawdata_ret = MP_DATA_FAIL;
	int rawdata_doze_ret = MP_DATA_FAIL;
	int jitter_doze_ret = MP_DATA_FAIL;
	int jitter_ret = MP_DATA_FAIL;
	int open_ret = MP_DATA_FAIL;
	int short_ret = MP_DATA_FAIL;
	int calibration_ret = MP_DATA_FAIL;
	/* LCD OFF TEST ITEM */
	int lpwg_rawdata_ret = MP_DATA_FAIL;
	int lpwg_jitter_ret = MP_DATA_FAIL;
	int lpwg_jitter_td_ret = MP_DATA_FAIL;
	int lpwg_rawdata_td_ret = MP_DATA_FAIL;

	TOUCH_TRACE();

	if (ERR_ALLOC_MEM(buf)) {
		len += snprintf(buf + len, PAGE_SIZE - len, "\nbuf is null\n");
		TOUCH_E("buf is null\n");
		return -ENOMEM;
	}

	for (seq = 0; seq < core_mp->run_num; seq++) {
		i = core_mp->run_index[seq];
		if (tItems[i].item_result == MP_DATA_PASS) {
			//if (ipio_strcmp(tItems[i].desp, "raw data(no bk)") == 0)
			if ((ipio_strcmp(tItems[i].desp, "raw data(no bk)") == 0) |(ipio_strcmp(tItems[i].desp, "raw data(have bk)") == 0))
				rawdata_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "doze raw data") == 0)
				rawdata_doze_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "doze peak to peak") == 0)
				jitter_doze_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "calibration data(dac)") == 0)
				calibration_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "noise peak to peak(with panel)") == 0)
				jitter_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "open test_c") == 0)
				open_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "short test") == 0)
				short_ret = MP_DATA_PASS;
			//if (ipio_strcmp(tItems[i].desp, "raw data(no bk) (lcm off)") == 0)
			if ((ipio_strcmp(tItems[i].desp, "raw data(no bk) (lcm off)") == 0)||(ipio_strcmp(tItems[i].desp, "raw data(have bk) (lcm off)") == 0))
				lpwg_rawdata_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "noise peak to peak(with panel) (lcm off)") == 0)
				lpwg_jitter_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "peak to peak_td (lcm off)") == 0)
				lpwg_jitter_td_ret = MP_DATA_PASS;
			if (ipio_strcmp(tItems[i].desp, "raw data_td (lcm off)") == 0)
				lpwg_rawdata_td_ret = MP_DATA_PASS;
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "\n=========== RESULT ===========\n");
	TOUCH_I("=========== RESULT ===========\n");

	if (lcm_on) {
        if (rawdata_ret == MP_DATA_PASS && jitter_ret == MP_DATA_PASS
			&& rawdata_doze_ret == MP_DATA_PASS && jitter_doze_ret ==MP_DATA_PASS) {
			len += snprintf(buf + len, PAGE_SIZE - len, "Raw Data : Pass\n");
			TOUCH_I("Raw Data : Pass\n");
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len, "Raw Data : Fail (raw:%d/jitter:%d/raw_doze:%d/jitter_doze:%d)\n",
					(rawdata_ret == MP_DATA_FAIL) ? 0 : 1, (jitter_ret == MP_DATA_FAIL)? 0 : 1,
					(rawdata_doze_ret == MP_DATA_FAIL) ? 0 : 1, (jitter_doze_ret == MP_DATA_FAIL)? 0 : 1);
			TOUCH_I("Raw Data : Fail (raw:%d/jitter:%d/raw_doze:%d/jitter_doze:%d)\n",
					(rawdata_ret == MP_DATA_FAIL) ? 0 : 1, (jitter_ret == MP_DATA_FAIL)? 0 : 1,
					(rawdata_doze_ret == MP_DATA_FAIL) ? 0 : 1, (jitter_doze_ret == MP_DATA_FAIL)? 0 : 1);
		}
		if (open_ret == MP_DATA_PASS && short_ret == MP_DATA_PASS && calibration_ret == MP_DATA_PASS) {
			len += snprintf(buf + len, PAGE_SIZE - len, "Channel Status : Pass\n");
			TOUCH_I("Channel Status : Pass\n");
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len,"Channel Status : Fail (open:%d/short:%d/calibration:%d)\n",
				(open_ret == MP_DATA_FAIL) ? 0 : 1, (short_ret == MP_DATA_FAIL) ? 0 : 1,
				(calibration_ret == MP_DATA_FAIL) ? 0 : 1);
			TOUCH_I("Channel Status : Fail (open:%d/short:%d/calibration:%d)\n",
				(open_ret == MP_DATA_FAIL) ? 0 : 1, (short_ret == MP_DATA_FAIL) ? 0 : 1,
				(calibration_ret == MP_DATA_FAIL) ? 0 : 1);
		}
	} else {
		if (lpwg_rawdata_ret == MP_DATA_PASS && lpwg_jitter_ret == MP_DATA_PASS
				&& lpwg_rawdata_td_ret == MP_DATA_PASS && lpwg_jitter_td_ret == MP_DATA_PASS) {
			len += snprintf(buf + len, PAGE_SIZE - len, "LPWG RawData : Pass\n");
			TOUCH_I("LPWG RawData : Pass\n");
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len, "LPWG RawData : Fail (lpwg_raw:%d/lpwg_jitter:%d/td_raw:%d/td_jitter:%d)\n",
					(lpwg_rawdata_ret == MP_DATA_FAIL)? 0 : 1, (lpwg_jitter_ret == MP_DATA_FAIL)? 0 : 1,
					(lpwg_rawdata_td_ret == MP_DATA_FAIL)? 0 : 1, (lpwg_jitter_td_ret == MP_DATA_FAIL)? 0 : 1);
			TOUCH_I("LPWG RawData : Fail (lpwg_raw:%d/lpwg_jitter:%d/td_raw:%d/td_jitter:%d)\n",
					(lpwg_rawdata_ret == MP_DATA_FAIL)? 0 : 1, (lpwg_jitter_ret == MP_DATA_FAIL)? 0 : 1,
					(lpwg_rawdata_td_ret == MP_DATA_FAIL)? 0 : 1, (lpwg_jitter_td_ret == MP_DATA_FAIL)? 0 : 1);
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "==============================\n");
	TOUCH_I("==============================\n");

	return len;
}

static int mp_sort_item(bool lcm_on)
{
	int i, j;
	char str[128] = {0};

	TOUCH_TRACE();

	core_mp->run_num = 0;
	memset(core_mp->run_index, 0x0, MP_TEST_ITEM);

	for (i = 0; i < MAX_SECTION_NUM; i++) {
		for (j = 0; j < MP_TEST_ITEM; j++) {
			//TOUCH_E("seq_item[%d] = %s, tItems[%d].desp = %s\n", i, seq_item[i], j, tItems[j].desp);
			if (ipio_strcmp(seq_item[i], tItems[j].desp) != 0)
				continue;

			parser_get_int_data(tItems[j].desp, "enable", str, sizeof(str));
			tItems[j].run = katoi(str);
			if (tItems[j].run != 1 || tItems[j].lcm != lcm_on)
				continue;

			if (core_mp->run_num > MP_TEST_ITEM) {
				TOUCH_E("Test item(%d) is invaild, abort\n", core_mp->run_num);
				return -EINVAL;
			}

			core_mp->run_index[core_mp->run_num] = j;
			core_mp->run_num++;
		}
	}
	return 0;
}

int ili7807q_int_action_ctrl(struct device *dev, bool keep_status)
{
	int ret = 0;
	u8 data[2] = {0};

	data[0] = CONTROL_INT_ACTION;
	data[1] = keep_status ? 1 : 0; // 0x1 : INT keep High utill data recived, 0x0 : INT send a pulse

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write int action ctrl cmd error\n");

	TOUCH_I("%s - %s\n", __func__, data[1] ? "keep high" : "pulse");

	return ret;
}

static ssize_t show_sd(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int i, ret = 0;

	TOUCH_TRACE();

	TOUCH_I("Show sd Test Start!\n");

	if (atomic_read(&ts->state.core) == CORE_UPGRADE) {
		TOUCH_E("FW is upgrading, stop running mp test\n");
		return -1;
	}

	/* LCD mode check */
	if (d->lcd_mode != LCD_MODE_U3) {
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
			"LCD mode is not U3. Test Result : Fail\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	/* Switch to test mode */
	if (ili7807q_switch_fw_mode(dev, FIRMWARE_TEST_MODE)) {
		TOUCH_E("Failed to switch test mode\n");
		goto out;
	}

	if (d->int_action)
		ili7807q_int_action_ctrl(dev, true);

	if (mp_init_item(dev) < 0) {
		TOUCH_E("core_mp failed to initialize\n");
		goto out;
	}
	//TOUCH_I("ini_info size = %d/%d\n", sizeof(ini_info), sizeof(struct ini_file_data) * PARSER_MAX_KEY_NUM);
/*
	ini_info = (struct ini_file_data *)vmalloc(sizeof(struct ini_file_data) * PARSER_MAX_KEY_NUM);
	if (ERR_ALLOC_MEM(ini_info)) {
		TOUCH_E("Failed to malloc ini_info\n");
		ret = -EMP_NOMEM;
		goto out;
	}
*/
	/* Parse ini file */
	if (mp_ini_parser(dev)) {
		TOUCH_E("Failed to parse ini file\n");
		goto out;
	}

	/* Read timing info from ini file */
	if (mp_get_timing_info() < 0) {
		TOUCH_E("Failed to get timing info from ini\n");
		ret = -EMP_TIMING_INFO;
		goto out;
	}

	/* Sort test item by ini file */
	if (mp_sort_item(true) < 0) {
		TOUCH_E("Failed to sort test item\n");
		ret = -EMP_INI;
		goto out;
	}

	/* Run MP test */
	for (i = 0; i < core_mp->run_num; i++)
		mp_test_run(core_mp->run_index[i]);

	/* Write final test result into a file */
	mp_show_result(dev);

	/* Write final test result into cmd terminal */
	ret = mp_write_ret_to_terminal(buf, true);

out:
	mp_test_free();
	//ipio_vfree((void **)&ini_info);

	if (d->int_action)
		ili7807q_int_action_ctrl(dev, false);

	/* Reset IC to get back to demo mode.  */
	//ili7807q_reset_ctrl(dev, HW_RESET_SYNC);
	/* Switch to demo mode */
	if (ili7807q_switch_fw_mode(dev, FIRMWARE_DEMO_MODE)) {
		TOUCH_E("Failed to switch demo mode\n");
	}
	else {
		ts->driver->init(dev);
	}

	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);

	write_file(dev, "Show_sd Test End\n", true);
	log_file_size_check(dev);
	TOUCH_I("Show sd Test End!\n");
	return ret;
}

static ssize_t show_lpwg_sd(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int i, ret = 0;

	TOUCH_TRACE();

	TOUCH_I("Show lpwg_sd Test Start\n");

	if (atomic_read(&ts->state.core) == CORE_UPGRADE) {
		TOUCH_E("FW is upgrading, stop running mp test\n");
		return -1;
	}

	/* LCD mode check */
	if (d->lcd_mode != LCD_MODE_U0) {
		ret = snprintf(buf + ret, PAGE_SIZE - ret,
			"LCD mode is not U3. Test Result : Fail\n");
		return ret;
	}

	mutex_lock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	/* Switch to test mode */
	if (ili7807q_switch_fw_mode(dev, FIRMWARE_TEST_MODE)) {
		TOUCH_E("Failed to switch test mode\n");
		goto out;
	}

	if (d->int_action)
		ili7807q_int_action_ctrl(dev, true);

	if (mp_init_item(dev) < 0) {
		TOUCH_E("core_mp failed to initialize\n");
		goto out;
	}
/*
	ini_info = (struct ini_file_data *)vmalloc(sizeof(struct ini_file_data) * PARSER_MAX_KEY_NUM);
	if (ERR_ALLOC_MEM(ini_info)) {
		TOUCH_E("Failed to malloc ini_info\n");
		ret = -EMP_NOMEM;
		goto out;
	}
*/
	/* Parse ini file */
	if (mp_ini_parser(dev)) {
		TOUCH_E("Failed to parse ini file\n");
		goto out;
	}

	/* Read timing info from ini file */
	if (mp_get_timing_info() < 0) {
		TOUCH_E("Failed to get timing info from ini\n");
		ret = -EMP_TIMING_INFO;
		goto out;
	}

	/* Sort test item by ini file */
	if (mp_sort_item(false) < 0) {
		TOUCH_E("Failed to sort test item\n");
		ret = -EMP_INI;
		goto out;
	}

	/* Run MP test */
	for (i = 0; i < core_mp->run_num; i++)
		mp_test_run(core_mp->run_index[i]);

	/* Write final test result into a file */
	mp_show_result(dev);

	/* Write final test result into cmd terminal */
	ret = mp_write_ret_to_terminal(buf, false);

out:
	mp_test_free();
	//ipio_vfree((void **)&ini_info);
	if (d->int_action)
		ili7807q_int_action_ctrl(dev, false);

	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);

	write_file(dev, "Show lpwg_sd Test End\n", true);
	log_file_size_check(dev);
	TOUCH_I("Show lpwg_sd Test End\n");
	TOUCH_I("Need to turn on the lcd after lpwg_sd test (ILITEK Limitation)\n");
	return ret;
}

static ssize_t show_delta(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_debug_info *debug_info = &d->debug_info;
	struct ili7807q_panel_info *panel_info = &d->tp_info.panel_info;
	int ret = 0, try = 10;
	int log_ret = 0;
	int mutual_data_size = 0;
	int16_t *delta = NULL;
	int row = 0, col = 0,  index = 0;
	int i, x, y;
	int read_length = 0;
	u8 tmp = 0x00;
	u8 check_sum = 0;

	TOUCH_TRACE();
	TOUCH_I("Show Delta Data\n");

	mutex_lock(&ts->lock);

	row = panel_info->nYChannelNum;
	col = panel_info->nXChannelNum;
	read_length = 4 + 2 * row * col + 1 ;

	delta = kcalloc(row * col, sizeof(int16_t), GFP_KERNEL);
	if(ERR_ALLOC_MEM(delta)) {
		TOUCH_E("Failed to allocate delta mem\n");
		goto out;
	}

retry:

	if (try < 0)
		goto out;

	memset(delta, 0, row * col * sizeof(int16_t));

	ret = ili7807q_reg_write(dev, CMD_READ_DATA_CTRL, &tmp, 1);

	tmp = 0x01; //Set signal once data mode
	ret = ili7807q_reg_write(dev, 0xB7, &tmp, 1);
	if (ret) {
		TOUCH_E("Failed to write preparation command, %d\n", ret);
		goto out;
	}

	mdelay(120);

	/* read debug packet header */
	//ret = ili7807q_reg_read(dev, CMD_NONE, debug_info->data, read_length);
	ret = ili7807q_reg_read(dev, CMD_READ_DIRECT, debug_info->data, read_length);

	tmp = 0x03; //switch to normal mode
	ret = ili7807q_reg_write(dev, 0xB7, &tmp, 1);
	if (ret) {
		TOUCH_E("Failed to write preparation command, %d\n", ret);
		goto out;
	}

	/* Do retry if it couldn't get the correct header of delta data */
	if (debug_info->data[0] != 0xB7 && try != 0) {
		TOUCH_E("It's incorrect header (0x%x) of delta, do retry (%d)\n", debug_info->data[0], try);
		try--;
		goto retry;
	}

	//Compare checksum
	check_sum = ili7807q_calc_data_checksum(debug_info->data, (read_length - 1));
	if (check_sum != debug_info->data[read_length -1]) {
		TOUCH_E("Packet check sum Error, check_sum = %d, abs_data = %d\n",
				check_sum, debug_info->data[read_length -1]);
		try--;
		goto retry;
	}

	/* Decode mutual/self delta */
	mutual_data_size = (row * col * 2);

	for (i = 4, index = 0; index < row * col; i += 2, index++) {
		delta[index] = (debug_info->data[i] << 8) + debug_info->data[i + 1];
	}

	ret = snprintf(buf, PAGE_SIZE, "======== Deltadata ========\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "Header 0x%x ,Type %d, Length %d\n",debug_info->data[0], debug_info->data[3],
									((debug_info->data[2] << 8) + debug_info->data[1]));

	// print X raw only
	for (y = 0; y < row; y++) {
		char log_buf[LOG_BUF_SIZE]= {0,};
		log_ret = 0;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", (y+1));
		log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "[%2d] ", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%5d", delta[shift]);
			log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "%5d", delta[shift]);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		TOUCH_I("%s\n", log_buf);
	}


out:
	if (delta != NULL)
		kfree(delta);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_rawdata(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_debug_info *debug_info = &d->debug_info;
	struct ili7807q_panel_info *panel_info = &d->tp_info.panel_info;
	int ret = 0, try = 10;
	int log_ret = 0;
	int mutual_data_size = 0;
	int16_t *rawdata = NULL;
	int row = 0, col = 0,  index = 0;
	int i, x, y;
	int read_length = 0;
	u8 tmp = 0x0;
	u8 check_sum = 0;

	TOUCH_TRACE();
	TOUCH_I("Show RawData\n");

	mutex_lock(&ts->lock);

	row = panel_info->nYChannelNum;
	col = panel_info->nXChannelNum;
	read_length = 4 + 2 * row * col + 1 ;

	rawdata = kcalloc(row * col, sizeof(int16_t), GFP_KERNEL);
	if(ERR_ALLOC_MEM(rawdata)) {
		TOUCH_E("Failed to allocate rawdata mem\n");
		goto out;
	}

retry:

	if (try < 0)
		goto out;

	memset(rawdata, 0, row * col * sizeof(int16_t));

	ret = ili7807q_reg_write(dev, CMD_READ_DATA_CTRL, &tmp, 1);

	tmp = 0x02; //set raw once data mode
	ret = ili7807q_reg_write(dev, 0xB7, &tmp, 1);
	if (ret) {
		TOUCH_E("Failed to write preparation command, %d\n", ret);
		goto out;
	}
	mdelay(120);

	/* read debug packet header */
	//ret = ili7807q_reg_read(dev, CMD_NONE, debug_info->data, read_length);
	ret = ili7807q_reg_read(dev, CMD_READ_DIRECT, debug_info->data, read_length);

	tmp = 0x03;//switch to normal mode
	ret = ili7807q_reg_write(dev, 0xB7, &tmp, 1);
	if (ret) {
		TOUCH_E("Failed to write preparation command, %d\n", ret);
		goto out;
	}

	/* Do retry if it couldn't get the correct header of raw data */
	if (debug_info->data[0] != 0xB7 && try != 0) {
		TOUCH_E("It's incorrect header (0x%x) of rawdata, do retry (%d)\n", debug_info->data[0], try);
		ili7807q_dump_packet(debug_info->data, 10, read_length, col, "Header");
		try--;
		goto retry;
	}

	//Compare checksum
	check_sum = ili7807q_calc_data_checksum(debug_info->data, (read_length - 1));
	if (check_sum != debug_info->data[read_length -1]) {
		TOUCH_E("Packet check sum Error, check_sum = %d, abs_data = %d\n",
				check_sum, debug_info->data[read_length -1]);
		ili7807q_dump_packet(debug_info->data, 10, read_length, col, "raw");
		try--;
		goto retry;
	}

	/* Decode mutual/self rawdata */
	mutual_data_size = (row * col * 2);

	for (i = 4, index = 0; index < row * col; i += 2, index++) {
		rawdata[index] = (debug_info->data[i] << 8) + debug_info->data[i + 1];
	}

	ret = snprintf(buf, PAGE_SIZE, "======== Rawdata ========\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "Header 0x%x ,Type %d, Length %d\n",debug_info->data[0], debug_info->data[3],
									((debug_info->data[2] << 8) + debug_info->data[1]));

	// print X raw only
	for (y = 0; y < row; y++) {
		char log_buf[LOG_BUF_SIZE]= {0,};
		log_ret = 0;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", (y+1));
		log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "[%2d] ", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%5d", rawdata[shift]);
			log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "%5d", rawdata[shift]);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		TOUCH_I("%s\n", log_buf);
	}


out:
	if (rawdata != NULL)
		kfree(rawdata);

	mutex_unlock(&ts->lock);

	return ret;
}

#define ABS(a, b) ((a > b) ? (a - b) : (b - a))

static ssize_t show_noise(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_debug_info *debug_info = &d->debug_info;
	struct ili7807q_panel_info *panel_info = &d->tp_info.panel_info;
	int ret = 0, try = 10;
	int log_ret = 0;
	int16_t *p2p = NULL, *min = NULL, *max = NULL;
	int row = 0, col = 0,  index = 0;
	int i, x, y, j;
	int read_length = 0;
	int frame_cnt = 50, rawtemp;
	u8 cmd = 0x0;
	u8 check_sum = 0;

	TOUCH_TRACE();
	TOUCH_I("Show Noise(peak to peak)\n");

	mutex_lock(&ts->lock);

	row = panel_info->nYChannelNum;
	col = panel_info->nXChannelNum;
	read_length = 4 + 2 * row * col + 1 ;


	max = kcalloc(row * col, sizeof(int16_t), GFP_KERNEL);
	if(ERR_ALLOC_MEM(max)) {
		TOUCH_E("Failed to allocate max mem\n");
		goto out;
	}

	min = kcalloc(row * col, sizeof(int16_t), GFP_KERNEL);
	if(ERR_ALLOC_MEM(min)) {
		TOUCH_E("Failed to allocate min mem\n");
		goto out;
	}

	p2p = kcalloc(row * col, sizeof(int16_t), GFP_KERNEL);
	if(ERR_ALLOC_MEM(p2p)) {
		TOUCH_E("Failed to allocate p2p mem\n");
		goto out;
	}

	memset(max, INT_MIN, row * col * sizeof(int16_t));
	memset(min, INT_MAX, row * col * sizeof(int16_t));
	memset(p2p, 0, row * col * sizeof(int16_t));

	ret = ili7807q_reg_write(dev, CMD_READ_DATA_CTRL, &cmd, 1);

	for (i = 0; i < frame_cnt; i++) {
		try = 10;

retry:
		if (try < 0)
			goto out;

		cmd = 0x02; //set raw once data mode
		ret = ili7807q_reg_write(dev, 0xB7, &cmd, 1);
		if (ret) {
			TOUCH_E("Failed to write preparation command, %d\n", ret);
			goto out;
		}
		if (i == 0)
			mdelay(120);
		else
			mdelay(30);

		/* read debug packet header */
		//ret = ili7807q_reg_read(dev, CMD_NONE, debug_info->data, read_length);
		ret = ili7807q_reg_read(dev, CMD_READ_DIRECT, debug_info->data, read_length);

		/* Do retry if it couldn't get the correct header of raw data */
		if (debug_info->data[0] != 0xB7 && try != 0) {
			TOUCH_E("It's incorrect header (0x%x) of p2p, do retry (%d)\n", debug_info->data[0], try);
			ili7807q_dump_packet(debug_info->data, 10, read_length, col, "Header");
			try--;
			goto retry;
		}

		//Compare checksum
		check_sum = ili7807q_calc_data_checksum(debug_info->data, (read_length - 1));
		if (check_sum != debug_info->data[read_length -1]) {
			TOUCH_E("Packet check sum Error, check_sum = %d, abs_data = %d\n",
					check_sum, debug_info->data[read_length -1]);
			ili7807q_dump_packet(debug_info->data, 10, read_length, col, "raw");
			try--;
			goto retry;
		}

		for (j = 4, index = 0; index < row * col; j += 2, index++) {
			rawtemp = (debug_info->data[j] << 8) + debug_info->data[j + 1];
			if (i == 0) {
				max[index] = rawtemp;
				min[index] = rawtemp;
			} else {
				if (max[index] < rawtemp)
					max[index] = rawtemp;

				if (min[index] > rawtemp)
					min[index] = rawtemp;
			}
			p2p[index] = rawtemp;
			ili7807q_dump_packet(p2p, 16, row * col, col, "p2p raw");
		}
	}

	for (index = 0; index < row * col; index++) {
		p2p[index] = max[index] - min[index];
		ili7807q_dump_packet(max, 16, row * col, col, "max raw");
		ili7807q_dump_packet(min, 16, row * col, col, "min raw");
		ili7807q_dump_packet(p2p, 16, row * col, col, "p2p raw");
	}

	cmd = 0x03;//switch to normal mode
	ret = ili7807q_reg_write(dev, 0xB7, &cmd, 1);
	if (ret) {
		TOUCH_E("Failed to write preparation command, %d\n", ret);
		goto out;
	}

	ret = snprintf(buf, PAGE_SIZE, "======== peak to peak ========\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "frame = %d\n", frame_cnt);

	// print X raw only
	for (y = 0; y < row; y++) {
		char log_buf[LOG_BUF_SIZE]= {0,};
		log_ret = 0;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", (y+1));
		log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "[%2d] ", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%5d", p2p[shift]);
			log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "%5d", p2p[shift]);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		TOUCH_I("%s\n", log_buf);
	}

out:
	if (try < 0) {
		ret = snprintf(buf, PAGE_SIZE, "======== peak to peak ========\n");
		ret += snprintf(buf + ret, PAGE_SIZE, "Result Fail (get peak to peak error)\n");
	}

	if (max != NULL)
		kfree(max);

	if (min != NULL)
		kfree(min);

	if (p2p != NULL)
		kfree(p2p);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_jitter(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_debug_info *debug_info = &d->debug_info;
	struct ili7807q_panel_info *panel_info = &d->tp_info.panel_info;
	int ret = 0, try = 10;
	int log_ret = 0;
	int16_t *jitter = NULL;
	int16_t *rawdata = NULL;
	int row = 0, col = 0,  index = 0;
	int i, x, y, j;
	int read_length = 0;
	int frame_cnt = 50, rawtemp, jittertemp;
	u8 cmd = 0x0;
	u8 check_sum = 0;

	TOUCH_TRACE();
	TOUCH_I("Show Jitter\n");

	mutex_lock(&ts->lock);

	row = panel_info->nYChannelNum;
	col = panel_info->nXChannelNum;
	read_length = 4 + 2 * row * col + 1 ;


	jitter = kcalloc(row * col, sizeof(int16_t), GFP_KERNEL);
	if(ERR_ALLOC_MEM(jitter)) {
		TOUCH_E("Failed to allocate jitter mem\n");
		goto out;
	}

	rawdata = kcalloc(row * col, sizeof(int16_t), GFP_KERNEL);
	if(ERR_ALLOC_MEM(rawdata)) {
		TOUCH_E("Failed to allocate rawdata mem\n");
		goto out;
	}

	memset(jitter, 0, row * col * sizeof(int16_t));
	memset(rawdata, 0, row * col * sizeof(int16_t));

	ret = ili7807q_reg_write(dev, CMD_READ_DATA_CTRL, &cmd, 1);

	for (i = 0; i < frame_cnt; i++) {
		try = 10;

retry:
		if (try < 0)
			goto out;

		cmd = 0x02; //set raw once data mode
		ret = ili7807q_reg_write(dev, 0xB7, &cmd, 1);
		if (ret) {
			TOUCH_E("Failed to write preparation command, %d\n", ret);
			goto out;
		}
		if (i == 0)
			mdelay(120);
		else
			mdelay(30);

		/* read debug packet header */
		//ret = ili7807q_reg_read(dev, CMD_NONE, debug_info->data, read_length);
		ret = ili7807q_reg_read(dev, CMD_READ_DIRECT, debug_info->data, read_length);

		/* Do retry if it couldn't get the correct header of raw data */
		if (debug_info->data[0] != 0xB7 && try != 0) {
			TOUCH_E("It's incorrect header (0x%x) of rawdata, do retry (%d)\n", debug_info->data[0], try);
			ili7807q_dump_packet(debug_info->data, 10, read_length, col, "Header");
			try--;
			goto retry;
		}

		//Compare checksum
		check_sum = ili7807q_calc_data_checksum(debug_info->data, (read_length - 1));
		if (check_sum != debug_info->data[read_length -1]) {
			TOUCH_E("Packet check sum Error, check_sum = %d, abs_data = %d\n",
					check_sum, debug_info->data[read_length -1]);
			ili7807q_dump_packet(debug_info->data, 10, read_length, col, "raw");
			try--;
			goto retry;
		}

		for (j = 4, index = 0; index < row * col; j += 2, index++) {
			rawtemp = (debug_info->data[j] << 8) + debug_info->data[j + 1];

			if (i == 0) {
				rawdata[index] = rawtemp;
			} else {
				jittertemp = ABS(rawdata[index], rawtemp);
				rawdata[index] = rawtemp;
				if (jitter[index] < jittertemp)
					jitter[index] = jittertemp;
			}
		}
		ili7807q_dump_packet(rawdata, 16, row * col, col, "jitter raw");
	}

	cmd = 0x03;//switch to normal mode
	ret = ili7807q_reg_write(dev, 0xB7, &cmd, 1);
	if (ret) {
		TOUCH_E("Failed to write preparation command, %d\n", ret);
		goto out;
	}

	ret = snprintf(buf, PAGE_SIZE, "======== jitter ========\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "frame = %d\n", frame_cnt);
	// print X raw only
	for (y = 0; y < row; y++) {
		char log_buf[LOG_BUF_SIZE]= {0,};
		log_ret = 0;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%2d] ", (y+1));
		log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "[%2d] ", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%5d", jitter[shift]);
			log_ret += snprintf(log_buf + log_ret, sizeof(log_buf) - log_ret, "%5d", jitter[shift]);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		TOUCH_I("%s\n", log_buf);
	}

out:
	if (try < 0) {
		ret = snprintf(buf, PAGE_SIZE, "======== jitter ========\n");
		ret += snprintf(buf + ret, PAGE_SIZE, "Result Fail (get jitter error)\n");
	}

	if (jitter != NULL)
		kfree(jitter);

	if (rawdata != NULL)
		kfree(rawdata);

	mutex_unlock(&ts->lock);

	return ret;
}

static TOUCH_ATTR(sd, show_sd, NULL);
static TOUCH_ATTR(delta, show_delta, NULL);
// static TOUCH_ATTR(fdata, show_fdata, NULL);
static TOUCH_ATTR(rawdata, show_rawdata, NULL);
static TOUCH_ATTR(noise, show_noise, NULL);
static TOUCH_ATTR(jitter, show_jitter, NULL);
static TOUCH_ATTR(lpwg_sd, show_lpwg_sd, NULL);
// static TOUCH_ATTR(label, show_labeldata, NULL);
// static TOUCH_ATTR(debug, show_debug, NULL);
// static TOUCH_ATTR(base, show_base, NULL);


static struct attribute *mp_attribute_list[] = {
	&touch_attr_sd.attr,
	&touch_attr_delta.attr,
	&touch_attr_noise.attr,
	&touch_attr_jitter.attr,
	// &touch_attr_fdata.attr,
	&touch_attr_rawdata.attr,
	&touch_attr_lpwg_sd.attr,
	// &touch_attr_label.attr,
	// &touch_attr_debug.attr,
	// &touch_attr_base.attr,
	NULL,
};

static const struct attribute_group mp_attribute_group = {
	.attrs = mp_attribute_list,
};

int ili7807q_mp_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &mp_attribute_group);

	if (ret < 0)
		TOUCH_E("failed to create sysfs\n");

	return ret;
}
