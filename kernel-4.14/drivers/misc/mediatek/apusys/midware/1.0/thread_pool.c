/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/delay.h>

#include "apusys_cmn.h"
#include "thread_pool.h"
#include "cmd_parser.h"

struct thread_pool_inst {
	/* work thread used */
	struct task_struct *task;
	struct mutex mtx;

	/* list with thread pool mgr */
	struct list_head list; // link to thread_pool_mgr's thread_list
	uint8_t stop;

	void *sc;
	int idx;
	int status;
};

struct job_inst {
	void *sc; // should be struct apusys_subcmd
	void *dev_info;
	struct list_head list; // link to thread_pool_mgr's job_list
};

struct thread_pool_mgr {
	/* thread info */
	struct list_head thread_list;
	struct mutex mtx;

	uint32_t total;
	struct completion comp;

	routine_func func_ptr;

	/* for job queue */
	struct list_head job_list;
	struct mutex job_mtx;
};

static struct thread_pool_mgr g_pool_mgr;

static int tp_service_routine(void *arg)
{
	struct thread_pool_inst *inst = (struct thread_pool_inst *)arg;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct job_inst *job_arg = NULL;
	int ret = 0;

	if (inst == NULL) {
		LOG_ERR("invalid argument, thread end\n");
		return -EINVAL;
	}

	while (!kthread_should_stop() && !inst->stop) {
		ret = wait_for_completion_interruptible(&g_pool_mgr.comp);
		if (ret) {
			switch (ret) {
			case -ERESTARTSYS:
				LOG_ERR("restart...\n");
				/* TODO: error handle, retry? */
				msleep(50);
				break;
			default:
				LOG_ERR("thread interruptible(%d)\n", ret);
				/* TODO: error handle */
				break;
			}
			continue;
		}

		/* 1. get cmd from job_list */
		mutex_lock(&g_pool_mgr.job_mtx);

		/* query list to find mem in apusys user */
		job_arg = NULL;
		list_for_each_safe(list_ptr, tmp, &g_pool_mgr.job_list) {
			job_arg = list_entry(list_ptr, struct job_inst, list);
			list_del(&job_arg->list);
			break;
		}
		mutex_unlock(&g_pool_mgr.job_mtx);
		if (job_arg == NULL)
			continue;

		LOG_DEBUG("thread(%d) execute sc(%p)\n",
			inst->idx, job_arg->sc);

		/* 2. execute cmd */
		mutex_lock(&inst->mtx);
		inst->status = APUSYS_THREAD_STATUS_BUSY;
		/* execute cmd */
		inst->sc = job_arg->sc;
		ret = g_pool_mgr.func_ptr(job_arg->sc, job_arg->dev_info);
		if (ret) {
			LOG_ERR("process arg(%p/%d) fail\n",
				job_arg->sc, ret);
		}

		kfree(job_arg);
		inst->sc = NULL;
		inst->status = APUSYS_THREAD_STATUS_IDLE;
		mutex_unlock(&inst->mtx);
	}

	LOG_INFO("thread(%d) end\n", inst->idx);
	return 0;
}

void thread_pool_dump(void)
{
	struct thread_pool_inst *inst = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_subcmd *sc = NULL;
	struct apusys_cmd *cmd = NULL;
	int i = 0;

	mutex_lock(&g_pool_mgr.mtx);

	LOG_INFO("=====================================\n");
	LOG_INFO("| apusys thread pool status: total(%d)\n", g_pool_mgr.total);
	list_for_each_safe(list_ptr, tmp, &g_pool_mgr.thread_list) {
		inst = list_entry(list_ptr, struct thread_pool_inst, list);
		LOG_INFO("-------------------------------------\n");
		sc = (struct apusys_subcmd *)inst->sc;
		if (sc == NULL)
			continue;
		cmd = sc->par_cmd;

		LOG_INFO(" thread idx = %d\n", i);
		LOG_INFO(" status     = %d\n", inst->status);
		if (sc == NULL || inst->status == APUSYS_THREAD_STATUS_IDLE)
			continue;
		LOG_INFO(" cmd        = 0x%llx\n", cmd->cmd_id);
		LOG_INFO(" subcmd     = %d/%p\n", sc->idx, sc);
		LOG_INFO(" stop       = %d\n", inst->stop);
	}
	LOG_INFO("=====================================\n");

	mutex_unlock(&g_pool_mgr.mtx);

}

int thread_pool_trigger(void *sc, void *dev_info)
{
	struct job_inst *job_arg = NULL;

	/* check argument */
	if (sc == NULL)
		return -EINVAL;

	/* 1. add cmd to job queue */
	job_arg = kzalloc(sizeof(struct job_inst), GFP_KERNEL);
	if (job_arg == NULL)
		return -ENOMEM;

	job_arg->sc = sc;
	job_arg->dev_info = dev_info;
	LOG_DEBUG("add to thread pool's job queue(%p)\n", sc);
	mutex_lock(&g_pool_mgr.job_mtx);
	list_add_tail(&job_arg->list, &g_pool_mgr.job_list);
	mutex_unlock(&g_pool_mgr.job_mtx);

	/* 2. hint thread pool dispatch one thread to service */
	complete(&g_pool_mgr.comp);

	return 0;
}

int thread_pool_add_once(void)
{
	struct thread_pool_inst *inst = NULL;
	char name[32];

	inst = kzalloc(sizeof(struct thread_pool_inst), GFP_KERNEL);
	if (inst == NULL) {
		LOG_ERR("alloc thread pool fail\n");
		return -ENOMEM;
	}

	mutex_init(&inst->mtx);
	INIT_LIST_HEAD(&inst->list);

	memset(name, 0, sizeof(name));
	/* critical seesion */
	mutex_lock(&g_pool_mgr.mtx);
	snprintf(name, sizeof(name)-1, "apusys_worker%d", g_pool_mgr.total);
	inst->status = APUSYS_THREAD_STATUS_IDLE;
	inst->idx = g_pool_mgr.total;

	inst->task = kthread_run(tp_service_routine, inst, name);
	if (inst->task == NULL) {
		LOG_ERR("create kthread(%d) fail\n", g_pool_mgr.total);
		kfree(inst);
		mutex_unlock(&g_pool_mgr.mtx);
		return -ENOMEM;
	}

	g_pool_mgr.total++;

	/* add to global mgr to store */
	list_add_tail(&inst->list, &g_pool_mgr.thread_list);
	mutex_unlock(&g_pool_mgr.mtx);
	return 0;
}

int thread_pool_delete(int num)
{
	int i = 0;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct thread_pool_inst *inst = NULL;

	mutex_lock(&g_pool_mgr.mtx);

	list_for_each_safe(list_ptr, tmp, &g_pool_mgr.thread_list) {
		inst = list_entry(list_ptr, struct thread_pool_inst, list);
		/* delete memory */
		list_del(&inst->list);
		kfree(inst);
		i++;

		/* stop thread mechanism */
		if (i >= num)
			break;
	}

	mutex_unlock(&g_pool_mgr.mtx);

	LOG_INFO("delete %d thread from pool\n", i);

	return 0;
}

int thread_pool_init(routine_func func_ptr)
{
	if (func_ptr == NULL)
		return -EINVAL;

	/* clean mgr */
	memset(&g_pool_mgr, 0, sizeof(struct thread_pool_mgr));

	/* init all list and mtx */
	mutex_init(&g_pool_mgr.mtx);
	INIT_LIST_HEAD(&g_pool_mgr.thread_list);
	mutex_init(&g_pool_mgr.job_mtx);
	INIT_LIST_HEAD(&g_pool_mgr.job_list);
	init_completion(&g_pool_mgr.comp);

	/* assign callback func*/
	g_pool_mgr.func_ptr = func_ptr;

	return 0;
}

int thread_pool_destroy(void)
{
	/* TODO delete all thread */

	return 0;
}

