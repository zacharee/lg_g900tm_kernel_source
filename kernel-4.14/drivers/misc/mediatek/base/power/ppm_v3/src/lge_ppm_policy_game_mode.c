/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "mtk_ppm_internal.h"

struct ppm_user_limit auto_game_limit[NR_PPM_CLUSTERS] = {
#if defined (CONFIG_MACH_MT6885)
	{ // L Cluster(500Mhz x 4)
		.min_freq_idx = 15,
		.max_freq_idx = 15,
		.min_core_num = 0,
		.max_core_num = 4,
	},
	{ // B Cluster(730Mhz x 4)
		.min_freq_idx = 15,
		.max_freq_idx = 15,
		.min_core_num = 0,
		.max_core_num = 4,
	}
#endif
};

static void ppm_auto_game_limit_update_limit_cb(void);
static void ppm_auto_game_limit_status_change_cb(bool enable);

/* other members will init by ppm_main */
static struct ppm_policy_data auto_game_limit_policy = {
	.name			= __stringify(PPM_POLICY_AUTO_GAME_LIMIT),
	.lock			= __MUTEX_INITIALIZER(auto_game_limit_policy.lock),
	.policy			= PPM_POLICY_AUTO_GAME_LIMIT,
	.priority		= PPM_POLICY_PRIO_HIGHEST,
	.update_limit_cb	= ppm_auto_game_limit_update_limit_cb,
	.status_change_cb	= ppm_auto_game_limit_status_change_cb,
};

struct ppm_userlimit_data auto_game_limit_data = {
	.is_freq_limited_by_user = false,
	.is_core_limited_by_user = false,
};


/* MUST in lock */
static bool ppm_auto_game_limit_is_policy_active(void)
{
	if (!auto_game_limit_data.is_freq_limited_by_user
			&& !auto_game_limit_data.is_core_limited_by_user)
		return false;
	else
		return true;
}

static void ppm_auto_game_limit_update_limit_cb(void)
{
	unsigned int i;
	struct ppm_policy_req *req = &auto_game_limit_policy.req;

	FUNC_ENTER(FUNC_LV_POLICY);

	if (auto_game_limit_data.is_freq_limited_by_user
			|| auto_game_limit_data.is_core_limited_by_user) {
		ppm_clear_policy_limit(&auto_game_limit_policy);

		for (i = 0; i < req->cluster_num; i++) {
			req->limit[i].min_cpu_core =
				(auto_game_limit_data.limit[i].min_core_num == -1)
				? req->limit[i].min_cpu_core
				: auto_game_limit_data.limit[i].min_core_num;
			req->limit[i].max_cpu_core =
				(auto_game_limit_data.limit[i].max_core_num == -1)
				? req->limit[i].max_cpu_core
				: auto_game_limit_data.limit[i].max_core_num;
			req->limit[i].min_cpufreq_idx =
				(auto_game_limit_data.limit[i].min_freq_idx == -1)
				? req->limit[i].min_cpufreq_idx
				: auto_game_limit_data.limit[i].min_freq_idx;
			req->limit[i].max_cpufreq_idx =
				(auto_game_limit_data.limit[i].max_freq_idx == -1)
				? req->limit[i].max_cpufreq_idx
				: auto_game_limit_data.limit[i].max_freq_idx;
		}
	}

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_auto_game_limit_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("auto_game_limit policy status changed to %d\n", enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

unsigned int mt_ppm_auto_game_limit(bool is_clear)
{
	int i = 0;

	ppm_lock(&auto_game_limit_policy.lock);

	if (!auto_game_limit_policy.is_enabled) {
		ppm_warn("auto_game_limit policy is not enabled!\n");
		ppm_unlock(&auto_game_limit_policy.lock);
		return -1;
	}

	if (is_clear) {
		auto_game_limit_data.is_freq_limited_by_user = false;
		auto_game_limit_data.is_core_limited_by_user = false;

		for_each_ppm_clusters(i) {
			auto_game_limit_data.limit[i].min_freq_idx = get_cluster_min_cpufreq_idx(i);
			auto_game_limit_data.limit[i].max_freq_idx = get_cluster_max_cpufreq_idx(i);

			auto_game_limit_data.limit[i].min_core_num = get_cluster_min_cpu_core(i);
			auto_game_limit_data.limit[i].max_core_num = get_cluster_max_cpu_core(i);
		}
	} else {
		auto_game_limit_data.is_freq_limited_by_user = true;
		auto_game_limit_data.is_core_limited_by_user = true;

		for_each_ppm_clusters(i) {
			auto_game_limit_data.limit[i].min_freq_idx = auto_game_limit[i].min_freq_idx;
			auto_game_limit_data.limit[i].max_freq_idx = auto_game_limit[i].max_freq_idx;

			auto_game_limit_data.limit[i].min_core_num = auto_game_limit[i].min_core_num;
			auto_game_limit_data.limit[i].max_core_num = auto_game_limit[i].max_core_num;
		}
	}

	auto_game_limit_policy.is_activated = ppm_auto_game_limit_is_policy_active();

	ppm_unlock(&auto_game_limit_policy.lock);
	mt_ppm_main();

	return 0;
}

static int ppm_auto_game_limit_proc_show(
	struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < auto_game_limit_policy.req.cluster_num; i++) {
		seq_printf(m, "cl%d: min_core_num = %d, max_core_num = %d\n",
			i, auto_game_limit_data.limit[i].min_core_num,
			auto_game_limit_data.limit[i].max_core_num);
	}

	return 0;
}

static ssize_t ppm_auto_game_limit_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enabled;
	bool is_clear = true;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &enabled)) {
		ppm_info("Request Auto Game Mode Set : %d\n", enabled);
	} else {
		ppm_err("Failed to parsing auto game mode\n");
		goto out;
	}

	if (enabled)
		is_clear = false;
	else
		is_clear = true;

	mt_ppm_auto_game_limit(is_clear);

out:
	free_page((unsigned long)buf);
	return count;
}
PROC_FOPS_RW(auto_game_limit);

static int __init ppm_auto_game_limit_policy_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(auto_game_limit),
	};

	FUNC_ENTER(FUNC_LV_POLICY);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
			policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n",
				__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	auto_game_limit_data.limit = kcalloc(ppm_main_info.cluster_num,
		sizeof(*auto_game_limit_data.limit), GFP_KERNEL);
	if (!auto_game_limit_data.limit) {
		ret = -ENOMEM;
		goto out;
	}

	/* init auto_game_limit_data */
	for_each_ppm_clusters(i) {
		auto_game_limit_data.limit[i].min_freq_idx = -1;
		auto_game_limit_data.limit[i].max_freq_idx = -1;
		auto_game_limit_data.limit[i].min_core_num = -1;
		auto_game_limit_data.limit[i].max_core_num = -1;
	}

	if (ppm_main_register_policy(&auto_game_limit_policy)) {
		ppm_err("@%s: auto_game_limit policy register failed\n", __func__);
		kfree(auto_game_limit_data.limit);
		ret = -EINVAL;
		goto out;
	}

	ppm_info("@%s: register %s done!\n", __func__, auto_game_limit_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_auto_game_limit_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	kfree(auto_game_limit_data.limit);

	ppm_main_unregister_policy(&auto_game_limit_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_auto_game_limit_policy_init);
module_exit(ppm_auto_game_limit_policy_exit);

