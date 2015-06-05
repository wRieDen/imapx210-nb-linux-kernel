/***************************************************************************** 
 * mp_mem.c
 * 
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Description: 
 * 	Main source file of IMAPX Memory Pool memory management. On IMAPX
 * 	platform, memory pool is designed for several hardware module use,
 * 	VDEC, GPS, ILA, DMA and SLV. But at most time of kernel running, 
 * 	these modules do not access memory pool memory. So we can use the
 * 	memory for other usage, accelarate net speed etc.
 * 	I design this management mainly or only for net speed optimize use,
 * 	and the net using tasklet to process and alloc this buffer. And it
 * 	is supposed to be an unreloadable operation, so, the mutex protect
 * 	is removed because of atomic feature of tasklet code.
 *
 * Author:
 *	Sololz <sololz.luo@gmail.com>.
 *      
 * Revision History: 
 * ­­­­­­­­­­­­­­­­­ 
 * 1.0  2011/01/14 Sololz
 * 	Create this file.
 ******************************************************************************/

#include "memalloc.h"
#include "mp_mem.h"

static mpmem_global_t mpmem_global = {
	.mplock = SPIN_LOCK_UNLOCKED,
	.mpmem_init = 0,
	.usz = 0,
};

/*
 * FUNCTION
 * mpmem_initialize()
 *
 * Initialize memory pool relate data structures and hardware.
 */
int mpmem_initialize(void)
{
	/* FIXME: Initialize memory pool global data structures. */
	/* spin_lock_init(&mpmem_global.mplock); */

	/* Here I suppse the memory pool has been initialized and set to be VDEC mode.
	 * So map the managed memory region to kernel space. */
	mpmem_global.mpmem_vstart = NULL;
	mpmem_global.mpmem_vstart = (void *)ioremap_nocache(MPMEM_ADDRESS_START, MPMEM_REGION_SIZE);
	if (unlikely(mpmem_global.mpmem_vstart == NULL)) {
		memalloc_error("Map memory managed memory error.\n");
		return -EFAULT;
	}
	memset(mpmem_global.mpmem_vstart, 0x00, MPMEM_REGION_SIZE);

	/* Create a origin memory node contains all memory. */
	mpmem_global.mpmem_head = NULL;
	mpmem_global.mpmem_head = (mpmem_node_t *)kmalloc(sizeof(mpmem_node_t), GFP_KERNEL);
	if (unlikely(mpmem_global.mpmem_head == NULL)) {
		memalloc_error("Allocate for origin memory pool node error.\n");
		return -ENOMEM;
	}
	memset(mpmem_global.mpmem_head, 0x00, sizeof(mpmem_node_t));
	mpmem_global.mpmem_head->pre = NULL;
	mpmem_global.mpmem_head->next = NULL;
	mpmem_global.mpmem_head->size = MPMEM_REGION_SIZE;
	mpmem_global.mpmem_head->phys = MPMEM_ADDRESS_START;
	mpmem_global.mpmem_head->virt = mpmem_global.mpmem_vstart;
	mpmem_global.mpmem_head->used = 0;

	mpmem_global.mpmem_init = 1;

	return 0;
}

/* 
 * FUNCTION
 * mpmem_release()
 *
 * Release memory pool relate data structure and hardware.
 */
void mpmem_release(void)
{
	mpmem_node_t *tmp = mpmem_global.mpmem_head;
	mpmem_node_t *cur = mpmem_global.mpmem_head;

	/* Free all nodes. */
	while (tmp != NULL) {
		cur = tmp->next;
		kfree(tmp);

		tmp = cur;
	}

	/* Unmap memory pool region. */
	if (likely(mpmem_global.mpmem_vstart != NULL)) {
		iounmap(mpmem_global.mpmem_vstart);
		mpmem_global.mpmem_vstart = NULL;
	}

	/* FIXME: Destroy lock. */

	mpmem_global.mpmem_init = 0;
}

/*
 * FUNCTION
 * mpmem_alloc()
 *
 * Allocate memory from memory pool.
 */
int mpmem_alloc(mpmem_alloc_t *mem)
{
	unsigned int size = 0;
	mpmem_node_t *tmp = NULL;
	mpmem_node_t *cur = NULL;

	if (unlikely(mem == NULL)) {
		memalloc_debug("Argement error.\n");
		return -EINVAL;
	} else if (unlikely(mpmem_global.mpmem_init == 0)) {
		memalloc_debug("Memory Pool not initialized.\n");
		return -EFAULT;
	}

	size = (mem->size + MPMEM_ADDRESS_ALIGN - 1) & (~(MPMEM_ADDRESS_ALIGN - 1));

	spin_lock(&mpmem_global.mplock);
	/* Find a suitable memory block. */
	cur = mpmem_global.mpmem_head;
	do {
		if (cur == NULL) {
			spin_unlock(&mpmem_global.mplock);
			memalloc_debug("No suitable memory block in memory pool, %d, %d.\n", size, mpmem_global.usz);
			return -ENOMEM;
		}

		if ((!cur->used) && (cur->size >= size)) {
			if (cur->size == size) {
				cur->used = 1;
				mem->phys = cur->phys;
				mem->virt = cur->virt;
			} else {
				/* Allocate a new mpmem node. */
				tmp = (mpmem_node_t *)kmalloc(sizeof(mpmem_node_t), GFP_ATOMIC);
				if (unlikely(tmp == NULL)) {
					spin_unlock(&mpmem_global.mplock);
					memalloc_error("kmalloc() for new mpmem node error.\n");
					return -EFAULT;
				}
				memset(tmp, 0x00, sizeof(mpmem_node_t));

				if (cur->next != NULL) {
					cur->next->pre = tmp;
				}
				tmp->next = cur->next;
				tmp->pre = cur;
				cur->next = tmp;

				tmp->used = 0;
				tmp->size = cur->size - size;
				tmp->phys = cur->phys + size;
				tmp->virt = (void *)((unsigned int)cur->virt + size);

				cur->used = 1;
				cur->size = size;

				mem->phys = cur->phys;
				mem->virt = cur->virt;
			}

			break;
		} else {
			cur = cur->next;
		}
	} while (1);

	mpmem_global.usz += size;
	spin_unlock(&mpmem_global.mplock);

	return 0;
}
EXPORT_SYMBOL(mpmem_alloc);

/*
 * FUNCTION
 * mpmem_free()
 *
 * Free allocated memory pool memory block.
 */
int mpmem_free(mpmem_alloc_t *mem)
{
	mpmem_node_t *tmp = NULL;
	mpmem_node_t *cur = NULL;

	if (unlikely(mem == NULL)) {
		memalloc_debug("Argument error.\n");
		return -EINVAL;
	} else if (unlikely((mem->phys < MPMEM_ADDRESS_START) || (mem->phys > MPMEM_ADDRESS_END))) {
		memalloc_debug("Memory address not in memory pool.\n");
		return -EINVAL;
	} else if (unlikely(mpmem_global.mpmem_init == 0)) {
		memalloc_debug("Memory Pool not initialized.\n");
		return -EFAULT;
	}

	spin_lock(&mpmem_global.mplock);
	cur = mpmem_global.mpmem_head;
	do {
		if (cur == NULL) {
			spin_unlock(&mpmem_global.mplock);
			memalloc_error("No such memory block in memory pool memory.\n");
			return -EINVAL;
		}

		if (cur->phys == mem->phys) {
			tmp = cur;
			if (tmp->used) {
				tmp->used = 0;
			} else {
				break;
			}
			mpmem_global.usz -= cur->size;

			/* Merge pre. */
			if (tmp->pre != NULL) {
				if (!tmp->pre->used) {
					cur = tmp->pre;

					cur->size += tmp->size;
					cur->next = tmp->next;
					if (cur->next != NULL) {
						cur->next->pre = cur;
					}
				}
			}

			/* Merge next. */
			if (cur->next != NULL) {
				if (!cur->next->used) {
					mpmem_node_t *trans = NULL;

					cur->size += cur->next->size;
					trans = cur->next;
					cur->next = cur->next->next;
					if (cur->next != NULL) {
						cur->next->pre = cur;
					}

					if (likely(trans != NULL)) {
						kfree(trans);
					}
				}
			}

			if (likely(cur != tmp)) {
				kfree(tmp);
			}

			break;
		}

		cur = cur->next;
	} while (1);
	spin_unlock(&mpmem_global.mplock);

	return 0;
}
EXPORT_SYMBOL(mpmem_free);

/*
 * FUNCTION
 * mpmem_virt_to_phys()
 *
 * Check input paramter virtual address is in mp memory or not, and return 
 * virt correspond phys.
 */
inline unsigned int mpmem_virt_to_phys(void *virt)
{
	if (likely((unsigned int)virt >= (unsigned int)mpmem_global.mpmem_vstart)) {
		unsigned int interval = (unsigned int)virt - (unsigned int)mpmem_global.mpmem_vstart;
		if (likely(interval < MPMEM_REGION_SIZE)) {
			return (MPMEM_ADDRESS_START + interval);
		}
	}

	return MPMEM_BAD_ADDRESS;
}
EXPORT_SYMBOL(mpmem_virt_to_phys);

#if 0
/*
 * FUNCTION
 * mpmem_origin_user_mark()
 *
 * This function is supposed to be called by origin user(VDEC...etc) driver to
 * mark correspond memory to be used status or release it.
 * TODO FIXME
 * This function is first designed to be a IMAPX Memory Pool management API.
 * But currently the management is not implemented and used...
 */
int mpmem_origin_user_mark(int users, int on)
{
	int ret = -EINVAL;

	spin_lock(&mpmem_global.mplock);
	if (users & MP_MEM_ORIGIN_VDEC) {
		mpmem_global.vdec_on = on;
		ret = 0;
	}
	if (users & MP_MEM_ORIGIN_GPS) {
		mpmem_global.gps_on = on;
		ret = 0;
	}
	if (users & MP_MEM_ORIGIN_ILA) {
		mpmem_global.ila_on = on;
		ret = 0;
	}
	if (users & MP_MEM_ORIGIN_DMA) {
		mpmem_global.dma_on = on;
		ret = 0;
	}
	if (users & MP_MEM_ORIGIN_SLV) {
		mpmem_global.slv_on = on;
		ret = 0;
	}
	spin_unlock(&mpmem_global.mplock);

	return ret;
}
EXPORT_SYMBOL(mpmem_origin_user_mark);
#endif
