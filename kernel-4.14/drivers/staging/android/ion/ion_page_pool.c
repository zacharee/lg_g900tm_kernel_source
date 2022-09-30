/*
 * drivers/staging/android/ion/ion_mem_pool.c
 *
 * Copyright (C) 2011 Google, Inc.
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

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/sched/clock.h>
#include <linux/llist.h>
#include "ion_priv.h"

static unsigned long long last_alloc_ts;

static void *ion_page_pool_alloc_pages(struct ion_page_pool *pool)
{
	unsigned long long start, end;
	struct page *page;

	start = sched_clock();
	page = alloc_pages(pool->gfp_mask, pool->order);
	end = sched_clock();

	if ((end - start > 10000000ULL) &&
	    (end - last_alloc_ts > 1000000000ULL)) { /* unit is ns, 1s */
		IONMSG("warn: alloc pages order: %d time: %lld ns\n",
		       pool->order, end - start);
		show_free_areas(0, NULL);
		last_alloc_ts = end;
	}

	if (!page)
		return NULL;
	ion_pages_sync_for_device(g_ion_device->dev.this_device,
				  page, PAGE_SIZE << pool->order,
				  DMA_BIDIRECTIONAL);
	atomic64_add_return((1 << pool->order), &page_sz_cnt);
	return page;
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
				     struct page *page)
{
	__free_pages(page, pool->order);
	if (atomic64_sub_return((1 << pool->order), &page_sz_cnt) < 0) {
		IONMSG("underflow!, total_now[%ld]free[%lu]\n",
		       atomic64_read(&page_sz_cnt),
		       (unsigned long)(1 << pool->order));
		atomic64_set(&page_sz_cnt, 0);
	}
}

static int ion_page_pool_add(struct ion_page_pool *pool, struct page *page)
{
	if (PageHighMem(page)) {
		llist_add((struct llist_node *)&page->lru, &pool->high_items);
		atomic_inc(&pool->high_count);
	} else {
		llist_add((struct llist_node *)&page->lru, &pool->low_items);
		atomic_inc(&pool->low_count);
	}

	return 0;
}

static struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high)
{
	struct page *page = NULL;
	struct llist_node *ret;

	if (high) {
		BUG_ON(!atomic_read(&pool->high_count));
		ret = llist_del_first(&pool->high_items);
		if (ret)
			page = llist_entry((struct list_head *)ret, struct page, lru);
		atomic_dec(&pool->high_count);
	} else {
		BUG_ON(!atomic_read(&pool->low_count));
		ret = llist_del_first(&pool->low_items);
		if (ret)
			page = llist_entry((struct list_head *)ret, struct page, lru);
		atomic_dec(&pool->low_count);
	}

	return page;
}

struct page *ion_page_pool_alloc(struct ion_page_pool *pool)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	mutex_lock(&pool->mutex);
	if (atomic_read(&pool->high_count))
		page = ion_page_pool_remove(pool, true);
	else if (atomic_read(&pool->low_count))
		page = ion_page_pool_remove(pool, false);
	mutex_unlock(&pool->mutex);

	if (!page)
#ifdef CONFIG_MIGRATE_HIGHORDER
	{
		if (pool->order > 1 &&
				(global_zone_page_state(NR_FREE_HIGHORDER_PAGES)
				 < ((1 << pool->order) * 2)))
			return page;
#endif
		page = ion_page_pool_alloc_pages(pool);
#ifdef CONFIG_MIGRATE_HIGHORDER
	}
#endif

	return page;
}

void ion_page_pool_free(struct ion_page_pool *pool, struct page *page)
{
	int ret;

	if (pool->order != compound_order(page))
		IONMSG("free page = 0x%p, compound_order(page) = 0x%x",
		       page, compound_order(page));

	BUG_ON(pool->order != compound_order(page));

	ret = ion_page_pool_add(pool, page);
	if (ret)
		ion_page_pool_free_pages(pool, page);
}

static int ion_page_pool_total(struct ion_page_pool *pool, bool high)
{
	int count = atomic_read(&pool->low_count);

	if (high)
		count += atomic_read(&pool->high_count);

	return count << pool->order;
}

int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
			 int nr_to_scan)
{
	int freed = 0;
	bool high;

	if (current_is_kswapd())
		high = true;
	else
		high = !!(gfp_mask & __GFP_HIGHMEM);

	if (nr_to_scan == 0)
		return ion_page_pool_total(pool, high);

	while (freed < nr_to_scan) {
		struct page *page;

		mutex_lock(&pool->mutex);
		if (atomic_read(&pool->low_count)) {
			page = ion_page_pool_remove(pool, false);
		} else if (high && atomic_read(&pool->high_count)) {
			page = ion_page_pool_remove(pool, true);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		ion_page_pool_free_pages(pool, page);
		freed += (1 << pool->order);
	}

	return freed;
}

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order,
					   bool cached)
{
	struct ion_page_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool) {
		IONMSG("%s kmalloc failed pool is null.\n", __func__);
		return NULL;
	}
	atomic_set(&pool->high_count, 0);
	atomic_set(&pool->low_count, 0);
	init_llist_head(&pool->low_items);
	init_llist_head(&pool->high_items);
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	mutex_init(&pool->mutex);
	plist_node_init(&pool->list, order);
	if (cached)
		pool->cached = true;

	return pool;
}

void ion_page_pool_destroy(struct ion_page_pool *pool)
{
	kfree(pool);
}

static int __init ion_page_pool_init(void)
{
	return 0;
}
device_initcall(ion_page_pool_init);
