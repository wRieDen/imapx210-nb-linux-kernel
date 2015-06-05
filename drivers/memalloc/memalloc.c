/*******************************************************************************
 * memalloc.c 
 * 
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Description:
 * 	Main file of memalloc char driver to allocate physical address for 
 * 	multi-media. This char driver contains a lot of annotation to 
 * 	explain how and why I designed it to be. And still, there are
 * 	problems and bugs with it, so I hope anyone change or add something
 * 	to it in the future, write your name and reason/declaration.
 *
 * Author:
 * 	Sololz <sololz.luo@gmail.com>.
 *      
 * Revision Version: 2.9
 *****************************************************************************/

#include "memalloc.h"
#include "lpmemalloc.h"

/* ############################################################################################# */

/* Global variables in memalloc driver. */
static memalloc_global_t m_global;

/* ############################################################################################# */

/* Process memory block list node. */
static int m_reset_mem(memalloc_inst_t *inst);
static int m_insert_mem(unsigned int paddr, unsigned int size, memalloc_inst_t *inst);
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
static int m_free_mem(unsigned int paddr, memalloc_inst_t *inst);
#endif
/* Control memory pool DMA power. */
static int m_enable_mp(void);
static int m_disable_mp(void);
/* Reserved memory management. */
static int rsv_alloc(unsigned int size, unsigned int *paddr, memalloc_inst_t *inst);
static int rsv_free(unsigned int paddr, memalloc_inst_t *inst);
/* Memory pool DMA copy for system call use. */
static int mpdma_memcpy(mpdma_param_t *mpdma);
/* Flush cache. */
static int cacheflush_usrvirt(void *virt, unsigned int size);
/* Get decoder of memory pool using status. */
#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
extern uint32_t get_decode_running_status(void);
#endif
/* Flush cache of all memory block nodes in current driver instance. */
/* XXX: Currently do not need it.
static void cacheflush_instance(memalloc_inst_t *inst);
*/

/* ############################################################################################# */

/*
 * FUNCTION
 * memalloc_open()
 *
 * System call open of memalloc, /dev/memalloc can be open 256 times.
 */
static int memalloc_open(struct inode *inode, struct file *file)
{
	memalloc_inst_t *inst = NULL;

	/* Allocate instance data structure first, avoid kmalloc be called at
	 * lock time. */
	inst = (memalloc_inst_t *)kmalloc(sizeof(memalloc_inst_t), GFP_KERNEL);
	if (unlikely(inst == NULL)) {
		memalloc_error("kmalloc() for instance error.\n");
		return -ENOMEM;
	}
	memset(inst, 0x00, sizeof(memalloc_inst_t));

	mutex_lock(&m_global.m_lock);
	if (unlikely(m_global.inst_count >= MEMALLOC_MAX_OPEN)) {
		mutex_unlock(&m_global.m_lock);

		memalloc_error("More than %d times have been openned, no more instances.\n",
				MEMALLOC_MAX_OPEN);
		kfree(inst);
		return -EPERM;
	}
	m_global.inst_count++;
	mutex_unlock(&m_global.m_lock);

	file->private_data = inst;
	memalloc_debug("Memalloc open OK.\n");

	return 0;
}

/*
 * FUNCTION
 * memalloc_release()
 *
 * All memory block should be released before memalloc_release. If you 
 * forget to, memalloc_release will check it and fix your mistake.
 */
static int memalloc_release(struct inode *inode, struct file *file)
{
	int icount = 0;
	int gcount = 0;
	memalloc_inst_t *inst = (memalloc_inst_t *)file->private_data;

	/* Check argument. */
	if (unlikely(inst == NULL)) {
		memalloc_error("Argument error, instance private data unexpected to be NULL.\n");
		return -EINVAL;
	}

	mutex_lock(&m_global.m_lock);
	/* Check share memory whether has been released. */
	for (icount = 0; icount < (sizeof(inst->shaddr) / sizeof(inst->shaddr[0])); icount++) {
		if (unlikely(inst->shaddr[icount] != 0)) {
			/* Find this share memory in global share memory library. */
			for (gcount = 0; gcount < MEMALLOC_MAX_ALLOC; gcount++) {
				if (m_global.shm[gcount].shaddr == inst->shaddr[icount]) {
					if (m_global.shm[gcount].dep_count > 0) {
						m_global.shm[gcount].dep_count--;
					} else {
						m_global.shm[gcount].dep_count = 0;
					}

					if (m_global.shm[gcount].dep_count == 0) {
						kfree((unsigned long *)phys_to_virt(m_global.shm[gcount].shaddr));
						memset(&(m_global.shm[gcount]), 0x00, sizeof(shm_param_t));
					}
				}
			}

			inst->shaddr[icount] = 0;
		}
	}

#if defined(CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT)
	/* Clear all mpool memory if user forgets to free them. */
	{
		int mpi = 0;
		mpmem_alloc_t mpmem;

		for (mpi = 0; mpi < (sizeof(inst->mpmem) / sizeof(inst->mpmem[0])); mpi++) {
			if (unlikely(inst->mpmem[mpi].phys != 0)) {
				mpmem.phys = inst->mpmem[mpi].phys;
				mpmem.size = inst->mpmem[mpi].size;
				mpmem_free(&mpmem);
				inst->mpmem[mpi].phys = 0;
				inst->mpmem[mpi].size = 0;
			}
		}
	}
#endif	/* CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT */

#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
	/* If there's no memalloc instance exists and decode is not running, turn off mpool power. */
	if ((inst->mpool_use == 1) && (m_global.mpool_refer > 0)) {
		m_global.mpool_refer--;
	}

	if ((m_global.mpool_refer == 0) && (get_decode_running_status() == 0)) {
		m_disable_mp();
	}
#endif

	/* Free memalloc instance structure. */
	if (unlikely(inst == NULL)) {
		/* Process nothing. */
		memalloc_debug("File private data already NULL.\n");
	} else {
		/* Check whether allocated memory release normally, it's a MUST step. */
		m_reset_mem(inst);

		kfree(inst);
	}
	m_global.inst_count--;
	mutex_unlock(&m_global.m_lock);

	memalloc_debug("Memalloc release OK.\n");
	return 0;
}

/*
 * FUNCTION
 * memalloc_ioctl()
 *
 * Don't forget to call free ioctl if you have called any allocate ops.
 */
static int memalloc_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int i = 0;
	int j = 0;
	void *vaddr = NULL;
	memalloc_inst_t *inst = NULL;

	inst = (memalloc_inst_t *)file->private_data;
	if (unlikely(inst == NULL)) {
		memalloc_error("IO control system call error file.\n");
		return -EFAULT;
	}

#if defined(CONFIG_EMULATE_ANDROID_PMEM_INTERFACES)
	switch (cmd) {
		/* This pmem ioctl is emulated for GPU copybit optimization use. */
		case PMEM_GET_PHYS:
		{
			struct pmem_region region;

			region.offset = 0;
			if (unlikely(copy_to_user((struct pmem_region *)arg,
						&region, sizeof(struct pmem_region)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}

			return 0;
		}

		case PMEM_CACHE_FLUSH:
		{
			struct pmem_region region;
			void *flush_virt = NULL;
			void *flush_virt_end = NULL;

			memalloc_debug("Get in flush.\n");

			if (unlikely(copy_from_user(&region, (struct pmem_region *)arg,
							sizeof(struct pmem_region)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}

			if ((region.offset < m_global.rsv_phys) ||
					(region.offset >= m_global.rsv_phys + m_global.rsv_size)) {
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
				rsv_mem_t *tmp_mem = m_global.rsv_head;

				/* 
				 * XXX
				 * If kmalloc support is enabled, in this case, the memory still can't be
				 * specified to be an invalid one. So I have to check in the global memory
				 * blocks to see whether this block is allocated by memalloc. And it's a
				 * very slow operation, in a word, do not enable kmalloc support, please@@.
				 */
				mutex_lock(&m_global.m_lock);
				/* Check whether correspond memory block is in global storage. */
				while (tmp_mem != NULL) {
					if (tmp_mem->phys == region.offset) {
						if (tmp_mem->size >= region.len) {
							break;
						} else {
							mutex_unlock(&m_global.m_lock);
							memalloc_error("Cache flush region error.\n");
							return -EINVAL;
						}
					}

					tmp_mem = tmp_mem->next;
				}
				if (tmp_mem == NULL) {
					mutex_unlock(&m_global.m_lock);
					memalloc_error("Error flush memory block.\n");
					return -EINVAL;
				}

				flush_virt = (void *)phys_to_virt(region.offset);
				dmac_flush_range(flush_virt, (void *)((unsigned int)flush_virt + region.len));
				mutex_unlock(&m_global.m_lock);

				return 0;
#endif
				memalloc_error("Invalid physical address, pmem offset correspond.\n");
				return -EINVAL;
			}

			/* RESERVED MEMORY REGION PROCESS. */
			if (region.offset - m_global.rsv_phys > m_global.rsv_size) {
				memalloc_error("Cache flush size invalid.\n");
				return -EINVAL;
			}
			mutex_lock(&m_global.m_lock);
			/* Flush current memory block first. */
			flush_virt = (void *)(((unsigned int)m_global.rsv_virt + region.offset - m_global.rsv_phys) & 
					(~(MEMALLOC_PAGE_SIZE - 1)));
			flush_virt_end = (void *)(((unsigned int)flush_virt + region.len + MEMALLOC_PAGE_SIZE - 1) & 
					(~(MEMALLOC_PAGE_SIZE - 1)));
			dmac_flush_range(flush_virt, flush_virt_end);
			mutex_unlock(&m_global.m_lock);

			memalloc_debug("Find memory block to flush.\n");

			return 0;
		}

		default:
			break;
	}
#endif	/* CONFIG_EMULATE_ANDROID_PMEM_INTERFACES */

	/* Check command. */
	if (unlikely(_IOC_TYPE(cmd) != MEMALLOC_MAGIC)) {
		memalloc_error("IO control command magic error.\n");
		return -ENOTTY;
	}
	if (unlikely(_IOC_NR(cmd) > MEMALLOC_MAX_CMD)) {
		memalloc_error("IO control command number error.\n");
		return -ENOTTY;
	}
	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (unlikely(!access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd)))) {
			memalloc_error("IO control command requires read access but buffer unwritable.\n");
			return -EFAULT;
		}
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (unlikely(!access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd)))) {
			memalloc_error("IO control command requires write access but buffer unreadable.\n");
			return -EFAULT;
		}
	}

	switch (cmd) {
		/* Reset current instance memory blocks. */
		case MEMALLOC_RESET:
			mutex_lock(&m_global.m_lock);
			m_reset_mem(inst);
			mutex_unlock(&m_global.m_lock);
			break;

		/*
		 * Memory alloc by kmalloc with flags GFP_DMA | GFP_ATOMIC
		 * the max size may be 8MB, 16MB, 32MB, you can change or
		 * get this in kernel configuration.
		 */
		case MEMALLOC_GET_BUF:
		{
			memalloc_param_t param;

			/* Copy user data first to avoid be called at lock time. */
			if (unlikely(copy_from_user(&param, (memalloc_param_t *)arg, sizeof(memalloc_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}
			/* Check parameters. */
			if (unlikely((param.size == 0) || (param.size > MEMALLOC_MAX_ALLOC_SIZE))) {
				memalloc_error("Get buffer parameters error.\n");
				return -EINVAL;
			}

			mutex_lock(&m_global.m_lock);
			if (unlikely(inst->alloc_count >= MEMALLOC_MAX_ALLOC)) {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("No more allocation allowed in this instance.\n");
				return -ENOMEM;
			}

			/*
			 * Allocation method: 
			 * Reserved memory is default alloc memory region, if any error occurs
			 * while allocating reserved memory, program will transfer to dynamic
			 * kernel memory resource alloc.
			 */
			if (unlikely(rsv_alloc(param.size, &(param.paddr), inst) != 0)) {
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
				mutex_unlock(&m_global.m_lock);
				memalloc_debug("Alloc reserved memory error.\n");

				/* 
				 * Here I use kmalloc() with flag GFP_KERNEL but not GFP_ATOMIC because I suppose
				 * some sleep here is reasonable. The disadvantage is it makes the code structure
				 * complicated considering the lock area. Code within lock area should be as fast
				 * as possible, best be atomic.
				 */
				vaddr = (void *)kmalloc(param.size, GFP_KERNEL);
				if (vaddr == NULL) {
					memalloc_error("kmalloc() for buffer failed!\n");
					return -ENOMEM;
				}

				/* Recheck and relock, it's very important!! */
				mutex_lock(&m_global.m_lock);
				if (unlikely(inst->alloc_count >= MEMALLOC_MAX_ALLOC)) {
					mutex_unlock(&m_global.m_lock);
					memalloc_error("No more allocation allowed in this instance.\n");
					kfree(vaddr);
					return -ENOMEM;
				}

				param.paddr = (unsigned int)virt_to_phys((unsigned long *)vaddr);
				/* Each instance has a memory allocation list, it's max management number is 256
				 * each time alloc ioctl called, file private data will update */
				m_insert_mem(param.paddr, param.size, inst);
#else
				mutex_unlock(&m_global.m_lock);
				memalloc_error("Alloc memory error.\n");

				return -ENOMEM;
#endif
			}
			inst->alloc_count++;
			mutex_unlock(&m_global.m_lock);

			if (unlikely(copy_to_user((memalloc_param_t *)arg, &param, sizeof(memalloc_param_t)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}

			break;
		}

		case MEMALLOC_FREE_BUF:
		{
			memalloc_param_t param;

			/* Copy first. */
			if (unlikely(copy_from_user(&param, (memalloc_param_t *)arg, sizeof(memalloc_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}

			mutex_lock(&m_global.m_lock);
			if (unlikely(inst->alloc_count <= 0)) {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("Nothing to be free.\n");
				return -ENOMEM;
			}

			/* 
			 * First find whether physical address correspond memory region is in 
			 * reserved memory or not. if yes correspond memory region will be released.
			 * If memory region is not allocate by reserved memory, I will call kfree
			 * to free memory region.
			 */
			if (unlikely(rsv_free(param.paddr, inst) != 0)) {
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
				/* memalloc_debug("rsv_free() error.\n"); */
				if (unlikely(m_free_mem(param.paddr, inst))) {
					mutex_unlock(&m_global.m_lock);
					return -EFAULT;
				}
#else
				mutex_unlock(&m_global.m_lock);

				memalloc_error("Free memory error.\n");
				return -EFAULT;
#endif
			}
			inst->alloc_count--;
			mutex_unlock(&m_global.m_lock);

			param.size = 0;
			if (unlikely(copy_to_user((memalloc_param_t *)arg, &param, sizeof(memalloc_param_t)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}

			break;
		}

		case MEMALLOC_SHM_GET:
		{
			shm_param_t shm;

			/* Copy first. */
			if (unlikely(copy_from_user(&shm, (shm_param_t *)arg, sizeof(shm_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}

			/* Share memory is global to all processes, so do be careful to use it and 
			 * correctly free it at end of program process. */
			mutex_lock(&m_global.m_lock);
			if (unlikely(m_global.sh_count == MEMALLOC_MAX_ALLOC)) {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("Shared memory limits reaches.\n");
				return -ENOMEM;
			}

			/* Find shared memory asigned by key whether exists. */
			for (i = 0; i < MEMALLOC_MAX_ALLOC; i++) {
				if (shm.key == m_global.shm[i].key) {
					shm.shaddr = m_global.shm[i].shaddr;
					if (shm.size > m_global.shm[i].size) {
						mutex_unlock(&m_global.m_lock);
						memalloc_error("Asigned memory size not large enough.\n");
						return -ENOMEM;
					}

					m_global.shm[i].dep_count++;

					/* Fill instance structure. */
					for (j = 0; j < MEMALLOC_MAX_ALLOC; j++) {
						if (inst->shaddr[j] == 0) {
							inst->shaddr[j] = shm.shaddr;
							break;
						}
					}

					break;
				}

				if (i == MEMALLOC_MAX_ALLOC - 1) {
					vaddr = (void *)kmalloc(shm.size, GFP_KERNEL);
					if (unlikely(vaddr == NULL)) {
						mutex_unlock(&m_global.m_lock);
						memalloc_error("kmalloc() for buffer failed!\n");
						return -ENOMEM;
					}
					memset(vaddr, 0x00, shm.size);

					shm.shaddr = (unsigned int)virt_to_phys((unsigned long *)vaddr);

					/* Fill global structure. */
					for (j = 0; j < MEMALLOC_MAX_ALLOC; j++) {
						if (m_global.shm[j].shaddr == 0) {
							m_global.shm[j].shaddr = shm.shaddr;
							m_global.shm[j].dep_count = 1;
							m_global.shm[j].size = shm.size;
							m_global.shm[j].key = shm.key;
							break;
						}
					}

					/* Fill instance structure. */
					for (j = 0; j < MEMALLOC_MAX_ALLOC; j++) {
						if (inst->shaddr[j] == 0) {
							inst->shaddr[j] = shm.shaddr;
							break;
						}
					}
				}
			}
			mutex_unlock(&m_global.m_lock);

			if (unlikely(copy_to_user((shm_param_t *)arg, &shm, sizeof(shm_param_t)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}

			break;
		}

		case MEMALLOC_SHM_FREE:
		{
			unsigned int key = 0;

			if (unlikely(copy_from_user(&key, (unsigned int *)arg, 4))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}

			mutex_lock(&m_global.m_lock);
			for (i = 0; i < MEMALLOC_MAX_ALLOC; i++) {
				if (key == m_global.shm[i].key) {
					break;
				}

				if (unlikely(i == MEMALLOC_MAX_ALLOC - 1)) {
					mutex_unlock(&m_global.m_lock);
					memalloc_error("Error free share memory.\n");
					return -EFAULT;
				}
			}

			if (m_global.shm[i].dep_count > 0) {
				m_global.shm[i].dep_count--;
			}

			/* Clear instance structure. */
			for (j = 0; j < MEMALLOC_MAX_ALLOC; j++) {
				if (inst->shaddr[j] == m_global.shm[i].shaddr) {
					inst->shaddr[j] = 0;
					break;
				}
			}

			if ((m_global.shm[i].shaddr != 0) && (m_global.shm[i].dep_count == 0)) {
				kfree((unsigned int *)phys_to_virt(m_global.shm[i].shaddr));
				/* Clear global structure. */
				memset(&(m_global.shm[i]), 0x00, sizeof(shm_param_t));
			}
			mutex_unlock(&m_global.m_lock);

			break;
		}

		case MEMALLOC_GET_MPDMA_REG:
			__put_user(MPDMA_BASE_REG_PA, (unsigned int *)arg);
			break;

		case MEMALLOC_FLUSH_RSV:
		{
#if 0	/* XXX: This API is dangerous! */
			/* 
			 * WARNING: DON'T CALL UNLESS YOU ARE SURE
			 * This ioctl command will flush all reserved memory, 
			 * don't call this if you are not sure what will happen.
			 */
			rsv_mem_t *tmp_cur = NULL;
			rsv_mem_t *tmp_pre = NULL;

			mutex_lock(&m_global.m_lock);
			if (likely(m_global.rsv_head != NULL)) {
				/* Head node's pre node pointer is supposed to be NULL. */
				tmp_cur = tmp_pre = m_global.rsv_head->next;

				do {
					/* Reach the end of reserved memory list. */
					if (tmp_cur == NULL) {
						break;
					}

					tmp_cur = tmp_cur->next;
					kfree(tmp_pre);
					tmp_pre = tmp_cur;
				} while (1);

				/* Physical and virtual address is not changed. */
				m_global.rsv_head->mark = 0;
				m_global.rsv_head->size = m_global.rsv_size;
				m_global.rsv_head->pre = NULL;
				m_global.rsv_head->next = NULL;
			}
			mutex_unlock(&m_global.m_lock);
#endif

			break;
		}

		case MEMALLOC_MPDMA_COPY:
		{
			mpdma_param_t mpdma;

			if (unlikely(copy_from_user(&mpdma, (mpdma_param_t *)arg, sizeof(mpdma_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}

			mutex_lock(&m_global.m_lock);
#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
			/* 
			 * FIXME:
			 * Check whether decode is using mpool, it's really not a good idea to 
			 * enable mpool power here, but for predesign, user space might be using
			 * mpool that kernel don't know. So I have to power mpool on when first 
			 * open or not turned on.
			 * Turn on mpool power is here, but turn off code is at release syscall.
			 */
			if (m_global.mpool_power == 0) {
				m_enable_mp();
			}

			if (inst->mpool_use == 0) {
				m_global.mpool_refer++;
				inst->mpool_use = 1;
			}
#endif

			if (unlikely(mpdma_memcpy(&mpdma) < 0)) {
				mutex_unlock(&m_global.m_lock);
				return -EFAULT;
			}
			mutex_unlock(&m_global.m_lock);

			break;
		}

		case MEMALLOC_SET_CACHED:
			mutex_lock(&m_global.m_lock);
			__get_user(inst->cached_mark, (unsigned int *)arg);
			mutex_unlock(&m_global.m_lock);
			break;
			
		case MEMALLOC_FLUSH_CACHE:
		{
			cacheflush_param_t cf;

			if (unlikely(copy_from_user(&cf, (cacheflush_param_t *)arg, sizeof(cacheflush_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}

			if (unlikely(cacheflush_usrvirt(cf.virt, cf.size))) {
				memalloc_error("Flush cache error.\n");
				return -EFAULT;
			}

			break;
		}

#if defined(CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT)
		case MEMALLOC_GET_MPMEM:
		{
			memalloc_param_t param;
			mpmem_alloc_t mpmem;
			int mpi = 0;

			if (unlikely(copy_from_user(&param, (memalloc_param_t *)arg, sizeof(memalloc_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}
			mpmem.size = param.size;
			mpmem.size = (param.size + MEMALLOC_PAGE_SIZE - 1) & (~(MEMALLOC_PAGE_SIZE - 1));

			mutex_lock(&m_global.m_lock);
			if (mpmem_alloc(&mpmem)) {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("Allocate mpool memory error.\n");
				return -ENOMEM;
			}

			/* Insert mpool memory physical address into file private data. */
			for (mpi = 0; mpi < (sizeof(inst->mpmem) / sizeof(inst->mpmem[0])); mpi++) {
				if (inst->mpmem[mpi].phys == 0) {
					/* Find idle storage in instance structure. */
					inst->mpmem[mpi].phys = mpmem.phys;
					inst->mpmem[mpi].size = mpmem.size;
					break;
				}
			}
			if (inst->mpmem[mpi].phys != mpmem.phys) {
				mutex_unlock(&m_global.m_lock);
				mpmem_free(&mpmem);
				memalloc_error("No more space in instance structure, should never print.\n");
				return -EFAULT;
			}
			mutex_unlock(&m_global.m_lock);

			param.paddr = mpmem.phys;
			/* 
			 * XXX: Maybe someone might have notice that if copy_to_user() runs error, I
			 * do not release correspond resources. I mean to do it because if copy_to_user()
			 * error, Linux kernel is supposed happening serious faults and might kernel
			 * panic. So, release code is not neccessary, I think.
			 */
			if (unlikely(copy_to_user((memalloc_param_t *)arg, &param, sizeof(memalloc_param_t)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}

			break;
		}

		case MEMALLOC_FREE_MPMEM:
		{
			memalloc_param_t param;
			mpmem_alloc_t mpmem;
			int mpi = 0;

			if (unlikely(copy_from_user(&param, (memalloc_param_t *)arg, sizeof(memalloc_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}
			mpmem.phys = param.paddr;
			mpmem.size = param.size;

			mutex_lock(&m_global.m_lock);
			/* First release storage in instance structure. */
			for (mpi = 0; mpi < (sizeof(inst->mpmem) / sizeof(inst->mpmem[0])); mpi++) {
				if (inst->mpmem[mpi].phys == mpmem.phys) {
					memset(&inst->mpmem[mpi], 0x00, sizeof(inst->mpmem[mpi]));
				}
			}
			/* Free memory block. */
			if (unlikely(mpmem_free(&mpmem))) {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("Free mpool memory error.\n");
				return -ENOMEM;
			}
			mutex_unlock(&m_global.m_lock);

			break;
		}
#endif	/* CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT */

		case MEMALLOC_GET_TOTAL_SIZE:
			__put_user(m_global.rsv_size, (uint32_t *)arg);
			break;

		case MEMALLOC_GET_LEFT_SIZE:
			__put_user(m_global.rsv_size - m_global.trace_memory, (uint32_t *)arg);
			break;

		case MEMALLOC_ALLOC_WITH_KEY:
		{
			rsv_mem_t *tmp_cur = NULL;
			awk_param_t awk;

			if (unlikely(copy_from_user(&awk, (awk_param_t *)arg, 
							sizeof(awk_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}

			mutex_lock(&m_global.m_lock);
			if (unlikely(m_global.shared_inst.alloc_count >= MEMALLOC_MAX_ALLOC)) {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("No more shared memory block to alloc.\n");
				return -EINVAL;
			}

			/* Find key. */
			tmp_cur = m_global.rsv_head;
			while (tmp_cur != NULL) {
				if (tmp_cur->key == awk.key) {
					break;
				}
				tmp_cur = tmp_cur->next;
			}
			if (tmp_cur == NULL) {	/* Does not exist, create a new one. */
				if (unlikely(rsv_alloc(awk.size, &awk.addr, 
								&m_global.shared_inst))) {
					mutex_unlock(&m_global.m_lock);
					memalloc_error("Allocate shared memalloc block error.\n");
					return -ENOMEM;
				}
				m_global.shared_inst.alloc_count++;
				/* Find phys. */
				tmp_cur = m_global.rsv_head;
				while (tmp_cur != NULL) {
					if (tmp_cur->phys == awk.addr) {
						break;
					}
					tmp_cur = tmp_cur->next;
				}
				if (unlikely(tmp_cur == NULL)) {
					mutex_unlock(&m_global.m_lock);
					memalloc_error("Shared memalloc blocks flow error.\n");
					return -EFAULT;
				}
				tmp_cur->key = awk.key;
				tmp_cur->ref = 1;
				awk.fresh = 1;
				memalloc_debug("Allocate a new shared block %x.\n", awk.key);
			} else {		/* Get existing one. */
				tmp_cur->ref++;
				awk.addr = tmp_cur->phys;
				awk.size = tmp_cur->size;
				awk.fresh = 0;
				memalloc_debug("Reuse a shared block %x.\n", awk.key);
			}
			mutex_unlock(&m_global.m_lock);

			if (unlikely(copy_to_user((awk_param_t *)arg,
							&awk, 
							sizeof(awk_param_t)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}
			break;
		}

		case MEMALLOC_FREE_WITH_KEY:
		{
			rsv_mem_t *tmp_cur = NULL;
			awk_param_t awk;

			if (unlikely(copy_from_user(&awk, (awk_param_t *)arg, 
							sizeof(awk_param_t)))) {
				memalloc_error("copy_from_user() error.\n");
				return -EFAULT;
			}

			mutex_lock(&m_global.m_lock);
			if (unlikely(m_global.shared_inst.alloc_count <= 0)) {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("Error shared block free.\n");
				return -EINVAL;
			}

			/* Find memory block. */
			tmp_cur = m_global.rsv_head;
			while (tmp_cur != NULL) {
				if (tmp_cur->key == awk.key) {
					break;
				}
				tmp_cur = tmp_cur->next;
			}
			if (likely(tmp_cur != NULL)) {
				tmp_cur->ref--;
				tmp_cur->ref = (tmp_cur->ref < 0) ? 0 : tmp_cur->ref;
				if (!tmp_cur->ref) {
					rsv_free(tmp_cur->phys, &m_global.shared_inst);
					m_global.shared_inst.alloc_count--;
				}
			} else {
				mutex_unlock(&m_global.m_lock);
				memalloc_error("Unexisting shared memalloc block.\n");
				return -EINVAL;
			}
			memalloc_alert("Shared block %x freed.\n", awk.key);
			mutex_unlock(&m_global.m_lock);

			/*
			if (unlikely(copy_to_user((awk_param_t *)arg,
							&awk, 
							sizeof(awk_param_t)))) {
				memalloc_error("copy_to_user() error.\n");
				return -EFAULT;
			}
			*/
			break;
		}

		default:
			memalloc_error("Unknown ioctl command.\n");
			return -EFAULT;
	}

	memalloc_debug("Memalloc ioctl OK.\n");

	return 0;
}

/*
 * FUNCTION
 * memalloc_mmap
 *
 * mmap system call to map physical address to user space virtual address.
 */
static int memalloc_mmap(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	memalloc_inst_t	*inst	= (memalloc_inst_t *)file->private_data;

	/* Set memory map access to be uncached if cached memory is not required. */
	if (!inst->cached_mark) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	}

	/* Do map. */
	if (unlikely(remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
					size, vma->vm_page_prot))) {
		memalloc_error("mmap for user space virtual address error.\n");
		return -EAGAIN;
	}

	return 0;
}

/* File operations structure basic for a char device driver. */
static struct file_operations memalloc_fops = {
	.owner = THIS_MODULE,
	.open = memalloc_open,
	.release = memalloc_release,
	.ioctl = memalloc_ioctl,
	.mmap = memalloc_mmap,
};

#if defined(CONFIG_IMAP_MEMALLOC_DRIVER_REGISTER_MISC_MODE)
/* 
 * Misc driver model structure, the reason to use misc to register memalloc driver
 * is because preversion method just use the device_create(), which is supposed to
 * the basic method, to do driver register and device node creation. The disadvantage
 * is that you must create a driver class and asign major and minor device number 
 * for it. So while the char device driver increasing, the major number of device 
 * will might be running out. The misc device driver use 10 major number, so it can
 * contains at most 256 different devices by asigning different minor from 0 to 256.
 * So there will still be a problem of number overflow, but much better than manual
 * asign causing conflicts.
 */
static struct miscdevice memalloc_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MEMALLOC_DEV_NAME,
	.fops = &memalloc_fops,
};
#endif

/*
 * FUNCTION
 * memalloc_probe()
 *
 * Register char device, alloc memory for mutex it's called by init function.
 */
static int memalloc_probe(struct platform_device *pdev)
{
	/* Initualize global variables. */
	memset(&m_global, 0x00, sizeof(memalloc_global_t));

#if defined(CONFIG_IMAP_MEMALLOC_DRIVER_REGISTER_CLASS_MODE)
	/*
	 * FIXME
	 * The origin code is still not removed because if any problem with misc
	 * driver model. I have met similar problems because of file system device
	 * node create bug. So if you got some device node generate problems, use
	 * this device_create().
	 */

	/* 0 refers to dynamic alloc major number. */
	m_global.major = register_chrdev(MEMALLOC_DEFAULT_MAJOR, "memalloc", &memalloc_fops);
	if (unlikely(m_global.major < 0)) {
		memalloc_error("Register char device for memalloc error.\n");
		return -EFAULT;
	}

	m_global.major = MEMALLOC_DEFAULT_MAJOR;
	m_global.minor = MEMALLOC_DEFAULT_MINOR;

	m_global.m_class = class_create(THIS_MODULE, "memalloc");
	if (unlikely(m_global.m_class == NULL)) {
		memalloc_error("Create char device driver class error.\n");
		return -EFAULT;
	}

	device_create(m_global.m_class, NULL, MKDEV(m_global.major, m_global.minor), NULL, "memalloc");
#elif defined(CONFIG_IMAP_MEMALLOC_DRIVER_REGISTER_MISC_MODE)
	if (unlikely(misc_register(&memalloc_miscdev))) {
		memalloc_error("Register misc memalloc driver error.\n");
		return -EFAULT;
	}
#else
#error "memalloc driver register mode not asigned!"
#endif

	/* Initialize globel memalloc lock. */
	mutex_init(&m_global.m_lock);

	/* Initialize mpdma lock. */
	mutex_init(&m_global.mpdma_lock);
	/* Map DMA registers. */
	m_global.mpdma_reg = ioremap_nocache(MPDMA_BASE_REG_PA, sizeof(mpdma_reg_t));
	if (unlikely(m_global.mpdma_reg == NULL)) {
		memalloc_error("ioremap_nocache() for memory pool dma register error.\n");
		return -ENOMEM;
	}
#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
#if defined(CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT)
	/* 
	 * If the memory pool memory management is enabled, the power should always be on 
	 * status. Because memory pool memory is mainly for net use, so frequently power 
	 * on and off memory pool hardware doesn't make sense. So just power on mpool power 
	 * at initialization.
	 */
	if (unlikely(m_enable_mp())) {
		memalloc_error("Fail to enable memory pool.\n");
		return -EFAULT;
	}
	m_global.mpool_refer = 1;
#else	/* CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT */
	/* Uboot might have powered on mp to do something but without turning off it. 
	 * so clean waris's ass here. lol. */
	m_disable_mp();
#endif	/* CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT */
#else
	/* Enable memory pool. */
	if (unlikely(m_enable_mp())) {
		memalloc_error("Fail to enable memory pool.\n");
		return -EFAULT;
	}
#endif

#if defined(CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT)
	/* Initialize memory pool management, this function must be called after than memory
	 * pool powed on and initialized on hardware. */
	if (unlikely(mpmem_initialize() != 0)) {
		memalloc_error("Fail to initialize memory pool management.\n");
		return -EFAULT;
	}
#endif

/* RESERVED MEMORY */
	/*
	 * Reserved memory start address is supposed to page size aligned. Reserved 
	 * memory is set in kernel start boot config, best size and start address 
	 * should be mutiple of 4KB(page size). Cuz if allocated buffer address is 
	 * not page size aligned, mmap might fail(must fail in Android bionic lib).
	 */
	{
#if defined(CONFIG_IMAP_MEMALLOC_MANUAL_RESERVE)
		m_global.rsv_phys = MEMALLOC_RSV_ADDR;
		m_global.rsv_size = MEMALLOC_RSV_SIZE;
		m_global.rsv_phys_end = m_global.rsv_phys + m_global.rsv_size;
#elif defined(CONFIG_IMAP_MEMALLOC_SYSTEM_RESERVE)
		m_global.rsv_phys = (unsigned int)imap_get_reservemem_paddr(RESERVEMEM_DEV_MEMALLOC);
		m_global.rsv_size = (unsigned int)imap_get_reservemem_size(RESERVEMEM_DEV_MEMALLOC);
		m_global.rsv_phys_end = m_global.rsv_phys + m_global.rsv_size;
		if (unlikely((m_global.rsv_phys == 0) || (m_global.rsv_size == 0))) {
			memalloc_error("memalloc reserved memory physical address or size is invalid.\n");
			return -EFAULT;
		}
#else
#error "Unknow memalloc reserved memory type!"
#endif
		/* 
		 * FIXME: 
		 * Maybe it's not neccessary to map reserved memory to kernel memory space 
		 * whole memory region is map to be nocache, cuz is for decode and encode, display
		 * use, hardware use physicall address only.
		 */
		m_global.rsv_virt = (unsigned int *)ioremap_cached(m_global.rsv_phys,
				m_global.rsv_size);
		if (unlikely(m_global.rsv_virt == NULL)) {
			memalloc_error("Remap reserved memory error.\n");
			return -ENOMEM;
		}
		m_global.rsv_virt_end = (unsigned int *)((unsigned int)m_global.rsv_virt +
				m_global.rsv_size);
		/* 
		 * Calculate interval between virtual address and physical address. Because
		 * kernel high memory virtual address is larger than 0xc0000000, and physical
		 * memory address is from 0x40000000, end address can't be larger than 0xc0000000
		 * on IMAPX200 platform. 
		 */
		m_global.rsv_interval = (unsigned int)m_global.rsv_virt - m_global.rsv_phys;
	}

	/*
	 * Initialize reserved memory list header, at start of system boot, there is
	 * just only one big memory region. This region is the size of reserved memory,
	 * and start address is the reserved memory, any following allocation will devide
	 * this origin block into many small regions. I only use one list to hold all 
	 * memory block, so all connected memory regions is address linear. It will be 
	 * very easy to merge consecutive idle block.
	 */
	{
		m_global.rsv_head = (rsv_mem_t *)kmalloc(sizeof(rsv_mem_t), GFP_KERNEL);
		if (unlikely(m_global.rsv_head == NULL)) {
			memalloc_error("Alloc structure memory for reserved list head error.\n");
			return -ENOMEM;
		}
		memset(m_global.rsv_head, 0x00, sizeof(rsv_mem_t));
		m_global.rsv_head->mark = 0;	/* Supposed to be idle block. */
		m_global.rsv_head->pre = NULL;
		m_global.rsv_head->next = NULL;
		m_global.rsv_head->phys = m_global.rsv_phys;
		m_global.rsv_head->size = m_global.rsv_size;
		m_global.rsv_head->virt = m_global.rsv_virt;
	}

/* PROCESS EXPORT RELATED VARIABLES */
	memset(&m_global.export_inst, 0x00, sizeof(memalloc_inst_t));
	m_global.export_mark = 1;	/* Mark export API usable. */
	memset(&m_global.shared_inst, 0x00, sizeof(memalloc_inst_t));

	return 0;
}

/*
 * FUNCTION
 * memalloc_remove()
 *
 * Platform driver model remove function interface.
 */
static int memalloc_remove(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < MEMALLOC_MAX_ALLOC; i++) {
		if (m_global.shm[i].shaddr != 0) {
			kfree((unsigned long *)phys_to_virt(m_global.shm[i].shaddr));
			memset(&(m_global.shm[i]), 0x00, sizeof(shm_param_t));
		}
	}

	/* Release memory pool management. */
#if defined(CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT)
	mpmem_release();
#endif

	/* Disable memory poll power. */
	m_disable_mp();
	if (likely(m_global.mpdma_reg != NULL)) {
		iounmap(m_global.mpdma_reg);
	}

/* RESERVED MEMORY */
	/* Recycle system resources, delete reserved memory list, and free all
	 * allocated memory. */
	{
		rsv_mem_t *tmp_cur = NULL;
		rsv_mem_t *tmp_pre = NULL;

		if (likely(m_global.rsv_head != NULL)) {
			/* Head node's pre node pointer is supposed to be NULL. */
			tmp_cur = tmp_pre = m_global.rsv_head->next;

			do {
				/* Reach the end of reserved memory list. */
				if (tmp_cur == NULL) {
					break;
				}

				tmp_cur = tmp_cur->next;
				kfree(tmp_pre);
				tmp_pre = tmp_cur;
			} while (1);

			if (likely(m_global.rsv_head->virt != NULL)) {
				iounmap((void *)(m_global.rsv_head->virt));
			}

			kfree(m_global.rsv_head);
		}

		m_global.rsv_phys = 0;
		m_global.rsv_size = 0;
		m_global.rsv_virt = NULL;
	}

	m_global.export_mark = 0;

#if defined(CONFIG_IMAP_MEMALLOC_DRIVER_REGISTER_CLASS_MODE)
	device_destroy(m_global.m_class, MKDEV(m_global.major, m_global.minor));
	class_destroy(m_global.m_class);
	unregister_chrdev(m_global.major, "memalloc");
#elif defined(CONFIG_IMAP_MEMALLOC_DRIVER_REGISTER_MISC_MODE)
	misc_deregister(&memalloc_miscdev);
#endif

	mutex_destroy(&m_global.m_lock);
	mutex_destroy(&m_global.mpdma_lock);

	return 0;
}

/*
 * FUNCTION
 * memalloc_suspend()
 *
 * Platform driver model suspend function interface.
 */
static int memalloc_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* Just disable memory pool. */
	m_disable_mp();

	return 0;
}

/*
 * FUNCTION
 * memalloc_resume()
 *
 * Platform driver model resume function interface.
 */
static int memalloc_resume(struct platform_device *pdev)
{
	/* Enable memory pool. */
	/* Here lock should not be called to acess global data, because there might be 
	 * dead lock while kernel resume. */
	if (m_global.inst_count > 0) {
		m_enable_mp();
	}

	return 0;
}

static struct platform_driver memalloc_driver = {
	.probe = memalloc_probe,
	.remove = memalloc_remove,
	.suspend = memalloc_suspend,
	.resume = memalloc_resume,
	.driver = {
		.owner = THIS_MODULE,
		.name = "memalloc",
	},
};

static int __init memalloc_init(void)
{
	if (unlikely(platform_driver_register(&memalloc_driver))) {
		memalloc_error("Register platform device error.\n");
		return -EPERM;
	}

	memalloc_debug("Memalloc initualize OK\n");

	return 0;
}

static void __exit memalloc_exit(void)
{
	platform_driver_unregister(&memalloc_driver);

	memalloc_debug("Memalloc exit OK.\n");
}

module_init(memalloc_init);
module_exit(memalloc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sololz of InfoTM");
MODULE_DESCRIPTION("Memory tool for Decode and Encode mainly");

/* ############################################################################################# */

/*  
 * FUNCTION
 * m_reset_mem()
 *
 * This function free all memory allocated in current instance.
 */
int m_reset_mem(memalloc_inst_t *inst)
{
	int i = 0;

	for (i = 0; i < (sizeof(inst->pmemblock) / sizeof(inst->pmemblock[0])); i++) {
		if (inst->pmemblock[i].phys != 0) {
			memalloc_debug("Memory alloced by /dev/memalloc didn't free normally, fix here.\n");
			/* Check whether this memory block is in reserved memory. */
			if (unlikely(rsv_free(inst->pmemblock[i].phys, inst) != 0)) {
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
				/* FIXME: This code based on reserved memory is set at end of physical memory. */
				if (likely((inst->pmemblock[i].phys < m_global.rsv_phys) ||
						(inst->pmemblock[i].phys > m_global.rsv_phys_end))) {
					kfree((unsigned int *)phys_to_virt(inst->pmemblock[i].phys));
				}
#else
				memalloc_debug("Memory block is in instance structure, but not in reserved memory.\n");
#endif
			}

			inst->pmemblock[i].phys = 0;
			inst->pmemblock[i].size = 0;
		}
	}

	return 0;
}

/*
 * FUNCTION
 * m_insert_mem()
 *
 * This function insert a new allocated memory block in to instance record.
 */
int m_insert_mem(unsigned int paddr, unsigned int size, memalloc_inst_t *inst)
{
	int i = 0;

	for (i = 0; i < MEMALLOC_MAX_ALLOC; i++) {
		if (inst->pmemblock[i].phys == 0) {
			inst->pmemblock[i].phys = paddr;
			inst->pmemblock[i].size = size;
			memalloc_debug("Memory block get inserted.\n");
			break;
		}
	}

	return 0;
}

#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
/*
 * FUNCTION
 * m_free_mem()
 *
 * This function free one memory block by its address,
 * I have to admit that it's not a good algorithm to locate.
 */
int m_free_mem(unsigned int paddr, memalloc_inst_t *inst)
{
	int i = 0;

	for (i = 0; i < MEMALLOC_MAX_ALLOC; i++) {
		if (inst->pmemblock[i].phys == paddr) {
			memalloc_debug("Memory block located to free.\n");
			if (likely(paddr < m_global.rsv_phys)) {
				kfree((unsigned long *)phys_to_virt(paddr));
			}

			inst->pmemblock[i].phys = 0;
			inst->pmemblock[i].size = 0;

			break;
		}

		if (unlikely(i == MEMALLOC_MAX_ALLOC - 1)) {
			memalloc_error("No such memory block.\n");
			return -ENOMEM;
		}
	}

	return 0;
}
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    MEMORY POOL HARDWARE INIT AND DEINIT */

/*
 * FUNCTION
 * m_enable_mp()
 *
 * Enable memory poll by control manager registers.
 * ATTENTION: This function must be called using lock to protect if 
 * communicate with decode driver to decide whether to initialize mp.
 */
int m_enable_mp(void)
{
	int i = 0;
	volatile unsigned int val = 0;
	mpdma_reg_t *reg = NULL;

	/* Set mempool power. */
	val = readl(rNPOW_CFG);
	val |= (1 << 2);
	writel(val, rNPOW_CFG);
	while (!(readl(rPOW_ST) & (1 << 2)));

	/* Reset mempool power. */
	val = readl(rMD_RST);
	val |= (1 << 2);
	writel(val, rMD_RST);
	for (i = 0; i < 0x1000; i++);

	/* Isolate mempool. */
	val = readl(rMD_ISO);
	val &= ~(1 << 2);
	writel(val, rMD_ISO);
	val = readl(rMD_RST);
	val &= ~(1 << 2);
	writel(val, rMD_RST);

	/* Reset mempool. */
	val = readl(rAHBP_RST);
	val |= (1 << 22);
	writel(val, rAHBP_RST);
	for (i = 0; i < 0x1000; i++);
	val = readl(rAHBP_RST);
	val &= ~(1 << 22);
	writel(val, rAHBP_RST);

	/* Enable mempool. */
	val = readl(rAHBP_EN);
	val |= (1 << 22);
	writel(val, rAHBP_EN);

	/* Set memory pool to decode mode. */
	val = readl(rMP_ACCESS_CFG);
	val |= 1;
	writel(val, rMP_ACCESS_CFG);

	/* Disable mpdma interrupt. */
	if (unlikely(m_global.mpdma_reg == NULL)) {
		return -1;
	}
	reg = (mpdma_reg_t *)m_global.mpdma_reg;
	reg->int_en = 0;

#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
	m_global.mpool_power = 1;
#endif

#if defined(CONFIG_IMAP_MMPOOL_TRACE_POWER)
	memalloc_alert("[KERN MEMALLOC MMPOOL POWER] mpool power is turned on.\n");
#endif

	return 0;
}

/*
 * FUNCTION
 * m_disable_mp()
 *
 * Disable memory poll by control magager registers.
 * ATTENTION: This function must be called using lock to protect if 
 * communicate with decode driver to decide whether to deinitialize mp.
 */
int m_disable_mp(void)
{
	volatile unsigned int val = 0;

	/* disable mempool */
	val = readl(rAHBP_EN);
	val &= ~(1 << 22);
	writel(val, rAHBP_EN);

	/* unisolate mempool */
	val = readl(rMD_ISO);
	val |= (1 << 2);
	writel(val, rMD_ISO);

	/* shut mempool power */
	val = readl(rNPOW_CFG);
	val &= ~(1 << 2);
	writel(val, rNPOW_CFG);
	while (readl(rPOW_ST) & (1 << 2));

#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
	m_global.mpool_power = 0;
#endif

#if defined(CONFIG_IMAP_MMPOOL_TRACE_POWER)
	memalloc_alert("[KERN MEMALLOC MMPOOL POWER] mpool power is turned off.\n");
#endif

	return 0;
}

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ RESERVED MEMORY FUNCTIONS */

/*
 * FUNCITON
 * rsv_alloc()
 *
 * HOWTO:
 * Here just use one memory region list to preserve memory block in
 * reserved memory area, and alloc method is low address first, and 
 * connected memory regions must be address sequence. This design will
 * be quite easy to guarantee that after free reserved memory region,
 * connected idle reserved memory region will be merged. But the disadvantage
 * of this method is that, if most of the allocated memory size is quite
 * so small, it will take a quite long time to find an suitable idle 
 * reserved memory block. O(n) time cost should be improved in the 
 * future.
 * Considering the reserved memory region is mainly used by media player
 * and display, most of the reserved memory is allocated at start of media
 * player start, and released at media player ends. So I set a current node
 * mark to mark for lastest allocation block. And next allocation will 
 * check from this pointer node and save alot of alloc time. 
 * This function allocate memory region in reserved memory, if success, 
 * will return 0, else a negative interger. This function not only do alloc
 * but also insert allocated memory region into instance structure.
 */
int rsv_alloc(unsigned int size, unsigned int *paddr, memalloc_inst_t *inst)
{
	rsv_mem_t *cur = NULL;
	rsv_mem_t *tmp = NULL;

	/* Size and paddr, inst validation need no check. */
	memalloc_debug("Get into reserved memory alloc.\n");

	/* 
	 * From current list next node start, find fits blank memory block, rsv_cur 
	 * is supposed to pointer to last allocate memory structure. Normally,
	 * next node of rsv_cur should be a large blank memory region.
	 */
	cur = m_global.rsv_head;
	do {
		if (cur == NULL) {
			memalloc_error("Can't find fit memory region in reserved memory.\n");
			return -ENOMEM;
		}

		if ((cur->mark == 0) && (cur->size >= size)) {
			/* Find suitable memory region. */
			if (cur->size == size) {
				cur->mark = 1;
				*paddr = cur->phys;
			} else {
				/* Current block is larger than required size, so devide current region. */
				tmp = (rsv_mem_t *)kmalloc(sizeof(rsv_mem_t), GFP_KERNEL);
				if (unlikely(tmp == NULL)) {
					memalloc_error("kmalloc() for reserved memory node structure error.\n");
					return -ENOMEM;
				}
				memset(tmp, 0x00, sizeof(rsv_mem_t));

				/* Update list structure. */
				/* Reset third node. */
				if (cur->next != NULL) {
					cur->next->pre = tmp;
				}
				tmp->next = cur->next;
				tmp->pre = cur;
				cur->next = tmp;

				/* Reset devided memory region size, and address. */
				tmp->mark = 0;
				tmp->size = cur->size - size;
				tmp->phys = cur->phys + size;
				tmp->virt = (unsigned int *)((unsigned int)(cur->virt) + size);

				cur->mark = 1;
				cur->size = size;

				*paddr = cur->phys;
			}

			/* Insert current memory block into instance structure. */
			m_insert_mem(*paddr, size, inst);

			m_global.trace_memory += size;
#ifdef CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE
			memalloc_alert("Alloc address 0x%08x, total memory usage %d.\n", *paddr, m_global.trace_memory);
#endif

			break;
		} else {
			cur = cur->next;
		}
	} while (1);

	memalloc_debug("Reserved memory alloc OK.\n");
	return 0;
}

/*
 * FUNCTION
 * rsv_free()
 *
 * First find correspond physical address in reserved memory, if memory
 * region found, it will be deleted from instance structure. Else, return
 * negative interger.
 */
int rsv_free(unsigned int paddr, memalloc_inst_t *inst)
{
	int i = 0;
	rsv_mem_t *cur = NULL;
	rsv_mem_t *tmp = NULL;

	/* Size and paddr, inst validation need no check. */
	memalloc_debug("Get into reserved memory free.\n");

	/*
	 * XXX: At first this part code is located after checking in global memory
	 * list. But suppose a situation like this, there are two threads/tasks 
	 * access reserved memory, tA & tB. First A allocates one memory and the
	 * address is aA, then tA free aA correspond memory. Then tB allocates
	 * a piece of memory and the address and size is just as the aA in tA.
	 * Then, tA unexpectly free aA again, in this case, the aA will be released
	 * that we do not want it to. So at free, the address must be checked 
	 * whether allocated by current fd, then do free.
	 */
	/* Clean instance structure. */
	for (i = 0; i < MEMALLOC_MAX_ALLOC; i++) {
		if (inst->pmemblock[i].phys == paddr) {
			memalloc_debug("Memory block located to free.\n");
			inst->pmemblock[i].phys = 0;
			inst->pmemblock[i].size = 0;
			break;
		}

		if (i == MEMALLOC_MAX_ALLOC - 1) {
			memalloc_error("No such memory block in instance structure.\n");
			return -ENOMEM;
		}
	}

	/* 
	 * Find correspond reserved memory region in reserved memory list, I have not
	 * found a better method to find the paddr in reserved memory list. Currently, 
	 * just find node from head.
	 */
	cur = m_global.rsv_head;
	do {
		if (cur == NULL) {
			memalloc_debug("No such reserved memory in reserved memory list.\n");
			return -EINVAL;
		}

		if (cur->phys == paddr) {
			tmp = cur;	/* Tmp records fit memory block node pointer. */
			/* If someone free one memory block more than once, there will
			 * be bug on memory usage trace. */
			if (tmp->mark) {
				tmp->mark = 0;	/* Set current memory block to be idle. */
			} else {
				memalloc_error("DON'T, current memory has been released.\n");
				break;
			}

			m_global.trace_memory -= cur->size;
#ifdef CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE
			memalloc_alert("Free address 0x%08x, total memory usage %d.\n", paddr, m_global.trace_memory);
#endif

			if (tmp->pre != NULL) {
				/* Merge block, save prenode. */
				if (tmp->pre->mark == 0) {
					cur = tmp->pre;

					cur->size = cur->size + tmp->size;
					cur->next = tmp->next;
					if (cur->next != NULL) {
						cur->next->pre = cur;
					}
				}
			}

			if (cur->next != NULL) {
				if (cur->next->mark == 0) {
					rsv_mem_t *trans = NULL;

					cur->size = cur->size + cur->next->size;
					trans = cur->next;
					cur->next = cur->next->next;
					if (cur->next != NULL) {
						cur->next->pre = cur;
					}

					if (trans != NULL) {
						kfree(trans);
					}
				}
			}

			if (cur != tmp) {
				kfree(tmp);
			}

			break;
		}

		cur = cur->next;
	} while (1);

	memalloc_debug("Reserved memory free OK.\n");

	return 0;
}

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ EXPORT RESERVED MEMROY API */

/*
 * FUNCTION
 * lpmem_alloc()
 *
 * This function is exported for kernel use to allocate physical linear 
 * memory. Mainly designed for GPU use, also any other driver can call 
 * this function to allocate physical linear memory in standardization.
 * Allocate memory block size best be page align.
 */
int lpmem_alloc(lpmem_t *mem)
{
	unsigned int size = 0;

	/* Check parameter. */
	if (unlikely(!m_global.export_mark)) {
		memalloc_error("Memory not prepared to be allocated.\n");
		return -EFAULT;
	} else if (unlikely(mem == NULL)) {
		memalloc_error("Input parameter error.\n");
		return -EINVAL;
	} else if (unlikely((mem->size <= 0) || (mem->size > MEMALLOC_MAX_ALLOC_SIZE))) {
		memalloc_error("Alloc size invalid, %d.\n", mem->size);
		return -EINVAL;
	}

	size = (mem->size + MEMALLOC_PAGE_SIZE - 1) & (~(MEMALLOC_PAGE_SIZE - 1));

	mutex_lock(&m_global.m_lock);
	if (unlikely(m_global.export_inst.alloc_count >= MEMALLOC_MAX_ALLOC)) {
		mutex_unlock(&m_global.m_lock);
		memalloc_error("Export instance reaches max alloc count.\n");
		return -EINVAL;
	}

	if (unlikely(rsv_alloc(size, &(mem->phys), &m_global.export_inst) != 0)) {
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
		mutex_unlock(&m_global.m_lock);

		mem->virt = (unsigned int *)kmalloc(size, GFP_KERNEL);
		if (unlikely(mem->virt == NULL)) {
			memalloc_error("Kernel export alloc function error.\n");
			return -ENOMEM;
		}
		mem->phys = (unsigned int)virt_to_phys(mem->virt);

		mutex_lock(&m_global.m_lock);
		if (unlikely(m_global.export_inst.alloc_count >= MEMALLOC_MAX_ALLOC)) {
			mutex_unlock(&m_global.m_lock);
			memalloc_error("Export instance reaches max alloc count.\n");
			return -EINVAL;
		}
		m_insert_mem(mem->phys, size, &m_global.export_inst);
#else
		mutex_unlock(&m_global.m_lock);
		memalloc_error("kernel export alloc function error\n");
		return -ENOMEM;
#endif
	}
	m_global.export_inst.alloc_count++;
	mutex_unlock(&m_global.m_lock);

	/* Set alloc memory kernel space virtual address. */
	if (likely((mem->phys >= m_global.rsv_phys) && (mem->phys < m_global.rsv_phys_end))) {
		mem->virt = (unsigned int *)((unsigned int)(m_global.rsv_virt) +
				(mem->phys - m_global.rsv_phys));
	}

	return 0;
}
EXPORT_SYMBOL(lpmem_alloc);

/*
 * FUNCTION
 * lpmem_free()
 *
 * This function is exported for kernel use to free physical linear
 * memory allocated by lpmem_alloc().
 */
int lpmem_free(lpmem_t *mem)
{
	/* Check parameter. */
	if (unlikely(!m_global.export_mark)) {
		memalloc_error("Memory not prepared to be released.\n");
		return -EFAULT;
	} else if (unlikely(m_global.export_inst.alloc_count <= 0)) {
		memalloc_error("Nothing to be released in instance.\n");
		return -EFAULT;
	} else if (unlikely(mem == NULL)) {
		memalloc_error("Input parameter error.\n");
		return -EINVAL;
	}

	mutex_lock(&m_global.m_lock);
	if (unlikely(rsv_free(mem->phys, &m_global.export_inst) != 0)) {
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
		if (unlikely(m_free_mem(mem->phys, &m_global.export_inst))) {
			mutex_unlock(&m_global.m_lock);
			memalloc_error("No such memory in export instance to be released.\n");
			return -EFAULT;
		}
#else
		mutex_unlock(&m_global.m_lock);
		memalloc_error("No such memory in export instance to be released.\n");
		return -EFAULT;
#endif
	}
	if (likely(m_global.export_inst.alloc_count > 0)) {
		m_global.export_inst.alloc_count--;
	}
	mutex_unlock(&m_global.m_lock);

	return 0;
}
EXPORT_SYMBOL(lpmem_free);

/*
 * FUNCTION
 * lpmem_get_rsv_info()
 *
 * Get reserved memory information.
 */
inline void lpmem_get_rsv_info(struct lpmem_rsv_info *info)
{
	if (unlikely(info == NULL)) {
		memalloc_error("*info* must not be NULL.\n");
		return;
	}

	info->interval = m_global.rsv_interval;
	info->phys = m_global.rsv_phys;
	info->phys_end = m_global.rsv_phys_end;
	info->virt = (void *)m_global.rsv_virt;
	info->virt_end = (void *)m_global.rsv_virt_end;
	info->size = m_global.rsv_size;
}
EXPORT_SYMBOL(lpmem_get_rsv_info);

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ MEMORY POOL COPY CODE PART */

/*
 * FUNCTION
 * mpdma_memcpy()
 *
 * Copy user space address memory using memory pool. Copy method must be 
 * done in kernel space, because of safety consideration and user space 
 * has no function to get virtual address correspond physical address.
 * And hardware need physical address. This function also support physical
 * address to physical address copy, but it's export for driver use.
 */
static int mpdma_memcpy(mpdma_param_t *mpdma)
{
	unsigned int size = 0;
	unsigned int *vsrc = NULL;
	unsigned int psrc = 0;
	unsigned int *vdst = NULL;
	unsigned int pdst = 0;
	unsigned int ref = 0;	/* Refer to source address by default, 0 - src, 1 - dst */
	mpdma_reg_t *mpreg = (mpdma_reg_t *)m_global.mpdma_reg;

	/* Check arguments. */
	if (unlikely(mpreg == NULL)) {
		memalloc_error("Memory pool hardware not prepared.\n");
		return -EINVAL;
	}
	if (unlikely(mpdma == NULL)) {
		memalloc_error("Input parameter error.\n");
		return -EINVAL;
	}
	size = mpdma->size;
	vsrc = mpdma->vsrc;
	psrc = mpdma->psrc;
	vdst = mpdma->vdst;
	pdst = mpdma->pdst;
	if (unlikely((size <= 0) || (size > MEMALLOC_MAX_ALLOC_SIZE))) {
		memalloc_error("Copy size is invalid, %d.\n", size);
		return -EINVAL;
	} else if (unlikely((vsrc == NULL) && (psrc <= IMAPX200_MEMORY_START))) {
		memalloc_error("Source memory address error.\n");
		return -EINVAL;
	} else if (unlikely((vdst == NULL) && (pdst <= IMAPX200_MEMORY_START))) {
		memalloc_error("Dest memory address error.\n");
		return -EINVAL;
	} else {
		/* Everything seems to be OK. */
	}

	/* Check source and dest address to decide refer type. */
	if ((psrc != 0) && (vdst != NULL) && (pdst == 0)) {
		ref = 1;
	} else {
		ref = 0;
	}

	memalloc_debug("[MEMALLOC] ref = %d.\n", ref);

/* START COPY. */
	/*
	 * Copy size is set to be 4KB each time, if start address is not 4KB,
	 * first unaligned memory region will be tranferred. Though both src
	 * and dst is physical address situation can be handled here, but not
	 * faster than user space code. memory copy situation gets here suposed
	 * to be that one of src and dst is virtual memory buffer. If either src
	 * and dst address are not multiple of 4KB, and srcaddr % 4KB doesn't
	 * equal dstaddr % 4KB, this copy method will get worst performance.
	 * cuz all memory copy is less than 4KB means that alway need 2 time
	 * copy operation to finish 4KB size transfer. Size align refers to 
	 * source address by default.
	 */
	do {
		unsigned int saddr = 0;		/* Source physical address set to dma hardware. */
		unsigned int daddr_0 = 0;	/* Dest physical address set to dma hardware. */
		unsigned int daddr_1 = 0;	/* Dest physical address set to dma hardware. */
		unsigned int cplen = 0;		/* Copy length in current loop. */
		unsigned int pgsize = 1024 * 4;	/* Arm platform page size is supposed to 4KB. */
		unsigned int pgnum = 0;		/* Return value of get_user_pages(), represents number of get pages. */
		struct page *spage = NULL;
		struct page *dpage_0 = NULL;
		struct page *dpage_1 = NULL;
		unsigned int cplen_0 = 0;	/* Dest address might be devide to 2 part to tranfer. */
		unsigned int cplen_1 = 0;
		unsigned int *line_addr = NULL;

		/* Get source memory address copy size. */
		if (ref == 0) {
			cplen = (psrc != 0) ? (pgsize - (psrc & (pgsize - 1))) :
				(pgsize - (((unsigned int)vsrc) & (pgsize - 1)));
		} else {
			cplen = (pdst != 0) ? (pgsize - (pdst & (pgsize - 1))) :
				(pgsize - (((unsigned int)vdst) & (pgsize - 1)));
		}

		memalloc_debug("[MEMALLOC] copy len %d.\n", cplen);

/* DO COPY. */
		if (size <= cplen) {
			cplen = size;
		}

		/* Get source physical address. */
		if (psrc != 0) {
			saddr = psrc;
		} else {
			/*
			 * Source virtual address doesn't need 2 address variable 
			 * to do depart address convertion and data transferring.
			 * Because if source virtual address is used to do tranfer,
			 * source is the reference size. get_user_pages() function
			 * must be used with mmap_sem held, both read and write
			 * held will be OK. Considering kernel code all use read
			 * held, so down_read() is used here.
			 */
			down_read(&(current->mm->mmap_sem));
			pgnum = get_user_pages(current, current->mm, (unsigned int)vsrc & (~(pgsize - 1)),
					1, 0, 0, &(spage), NULL);
			up_read(&(current->mm->mmap_sem));
			if (unlikely((pgnum != 1) || (spage == NULL))) {
				memalloc_error("get_user_pages() error.\n");
				spage = NULL;
				goto mpdma_copy_error;
			}

			/*
			 * FIXME TODO
			 * Here I use page_address to get kernel linear virtual address instead of 
			 * kmap cuz kmap is just a function pack of page_address. If page correspond
			 * physical address has not been mapped to kernel linear logical address 
			 * space or high memory virtual address space, kmap will map this page to 
			 * high memory. But I don't know how to process highmemory address to convert
			 * to physical address. So all pages is supposed that have been mapped to 
			 * kernel logical address. According to "understanding linux kernel" book, 
			 * high memory address start at (3GB + 896MB) place, so if platform memory
			 * is less than 896MB, following code will always work. Else some errors will
			 * happen. so any one know how to convert highmemory address(not by vmalloc)
			 * to physical address, please let me know.
			 */
			line_addr = (unsigned int *)page_address(spage);
			if (unlikely(line_addr == NULL)) {
				memalloc_error("Page address is NULL.\n");
				goto mpdma_copy_error;
			}
			line_addr = (unsigned int *)((unsigned int)line_addr +
					((unsigned int)vsrc & (pgsize - 1)));

			saddr = dma_map_single(NULL, (void *)line_addr, cplen, DMA_TO_DEVICE);
			if (unlikely(saddr < IMAPX200_MEMORY_START)) {
				memalloc_error("Get dma bus address error.\n");
				goto mpdma_copy_error;
			}
		}

		/* Get dest physical address. */
		if (pdst != 0) {
			daddr_0 = pdst;
			cplen_0 = cplen;
			cplen_1 = 0;
		} else {
			unsigned int dst_redu = pgsize - ((unsigned int)vdst & (pgsize - 1));

			/* Dest virtual address redundance size less than source virtual address
			 * redundance size, in this case, 2 part of dest to transfer all data. */

			/* Source refer. */
			if ((ref == 0) && (dst_redu < cplen)) {
				/* Get first part page. */
				down_read(&(current->mm->mmap_sem));
				pgnum = get_user_pages(current, current->mm, (unsigned int)vdst & (~(pgsize - 1)),
						1, 1, 0, &dpage_0, NULL);
				up_read(&(current->mm->mmap_sem));
				if (unlikely((pgnum != 1) || (dpage_0 == NULL))) {
					memalloc_error("get_user_pages() error.\n");
					dpage_0 = NULL;
					goto mpdma_copy_error;
				}

				line_addr = (unsigned int *)page_address(dpage_0);
				if (unlikely(line_addr == NULL)) {
					memalloc_error("Page address is NULL.\n");
					goto mpdma_copy_error;
				}
				line_addr = (unsigned int *)((unsigned int)line_addr +
						((unsigned int)vdst & (pgsize - 1)));

				cplen_0 = dst_redu;
				cplen_1 = cplen - cplen_0;

				daddr_0 = dma_map_single(NULL, (void *)line_addr, cplen_0, DMA_FROM_DEVICE);
				if (unlikely(daddr_0 < IMAPX200_MEMORY_START)) {
					memalloc_error("Get DMA bus address error.\n");
					goto mpdma_copy_error;
				}

				/* Get second part page. */
				down_read(&(current->mm->mmap_sem));
				pgnum = get_user_pages(current, current->mm, (unsigned int)vdst + cplen_0,
						1, 1, 0, &dpage_1, NULL);
				up_read(&(current->mm->mmap_sem));
				if (unlikely((pgnum != 1) || (dpage_1 == NULL))) {
					memalloc_error("get_user_pages() error.\n");
					dpage_1 = NULL;
					goto mpdma_copy_error;
				}
				line_addr = (unsigned int *)page_address(dpage_1);

				daddr_1 = dma_map_single(NULL, (void *)line_addr, cplen_1, DMA_FROM_DEVICE);
				if (unlikely(daddr_1 < IMAPX200_MEMORY_START)) {
					memalloc_error("Get dma bus address error.\n");
					goto mpdma_copy_error;
				}
			} else {
				/* In this case, dest virtual address is reference address. so only
				 * one part of page is used to do data transfer. */

				/*
				 * FIXME TODO ATTENTION
				 * The down_write() and up_write() is annotated, because I find some dead lock
				 * error while multi-thread programs. And I still don't know why we have to 
				 * down current task/process *mmap_sem*. Error is not found till now without
				 * lock *mmap_sem*. So I suppose the virt->phys DMA copy address convert will
				 * cause dead lock too. But I never get any error from virt->phys, so it's 
				 * remained there and supposed to be hidden trouble.
				 */
				/* down_write(&(current->mm->mmap_sem)); */
				pgnum = get_user_pages(current, current->mm, (unsigned int)vdst & (~(pgsize - 1)),
						1, 1, 0, &(dpage_0), NULL);
				/* up_write(&(current->mm->mmap_sem)); */
				if (unlikely((pgnum != 1) || (dpage_0 == NULL))) {
					memalloc_error("get_user_pages() error, %d.\n", pgnum);
					dpage_0 = NULL;
					goto mpdma_copy_error;
				}

				line_addr = (unsigned int *)page_address(dpage_0);
				if (unlikely(line_addr == NULL)) {
					memalloc_error("page_address() return NULL.\n");
					goto mpdma_copy_error;
				}
				line_addr = (unsigned int *)((unsigned int)line_addr +
						((unsigned int)vdst & (pgsize - 1)));

				daddr_0 = dma_map_single(NULL, (void *)line_addr, cplen, DMA_FROM_DEVICE);
				if (unlikely(daddr_0 < IMAPX200_MEMORY_START)) {
					memalloc_error("Get dest dma bus address error.\n");
					goto mpdma_copy_error;
				}

				cplen_0 = cplen;
				cplen_1 = 0;
			}
		}

/* START DMA COPY. */
		mutex_lock(&m_global.mpdma_lock);
		{
			/*
			 * DMA copy use 0x30000 internal address of memory pool, which is the 
			 * start address of memory in VDEC mode. Here the max copy size of 
			 * system memory is set to be page size/4KB.
			 */

			mpreg->ch0_maddr = 0x30000;
			mpreg->ch1_maddr = 0x30000;

			/* Use memory pool to move part 1 data. */
			/* First part must be transfered, cuz it's a must existance. */
			mpreg->ch0_saddr = saddr;
			mpreg->ch1_saddr = daddr_0;

			mpreg->dma_en = 0;
			mpreg->dma_en = 1;

			mpreg->ch1_ctrl = cplen_0 | 0x02000000;
			mpreg->ch0_ctrl = cplen_0 | 0x8c000000;

			while (mpreg->ch0_ctrl & 0x80000000);

			/* Use memory pool to move part 2 data. */
			/* Second part might not be existed. */
			if (cplen_1 > 0) {
				mpreg->ch0_saddr = saddr + cplen_0;
				mpreg->ch1_saddr = daddr_1;

				mpreg->dma_en = 0;
				mpreg->dma_en = 1;

				mpreg->ch1_ctrl = cplen_1 | 0x02000000;
				mpreg->ch0_ctrl = cplen_1 | 0x8c000000;

				while (mpreg->ch0_ctrl & 0x80000000);
			}
		}
		mutex_unlock(&m_global.mpdma_lock);

/* RELEASE. */
		if (likely(spage != NULL)) {
			SetPageDirty(spage);
			page_cache_release(spage);
			spage = NULL;
		}

		if (likely(dpage_0 != NULL)) {
			SetPageDirty(dpage_0);
			page_cache_release(dpage_0);
			dpage_0 = NULL;
		}

		if (dpage_1 != NULL) {
			SetPageDirty(dpage_1);
			page_cache_release(dpage_1);
			dpage_1 = NULL;
		}

/* LENGTH PROCESS */
		if (psrc != 0) {
			psrc += cplen;
		} else {
			vsrc = (unsigned int *)((unsigned int)vsrc + cplen);
		}

		if (pdst != 0) {
			pdst += cplen;
		} else {
			vdst = (unsigned int *)((unsigned int)vdst + cplen);
		}

		size -= cplen;

		if (size <= 0) {
			break;
		}

		continue;

 mpdma_copy_error:
		if (likely(spage != NULL)) {
			SetPageDirty(spage);
			page_cache_release(spage);
			spage = NULL;
		}

		if (likely(dpage_0 != NULL)) {
			SetPageDirty(dpage_0);
			page_cache_release(dpage_0);
			dpage_0 = NULL;
		}

		if (dpage_1 != NULL) {
			SetPageDirty(dpage_1);
			page_cache_release(dpage_1);
			dpage_1 = NULL;
		}

		return -EFAULT;
	} while (size > 0);

	return 0;
}

#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
/*
 * FUNCTION
 * power_on_mpool_in_memalloc()
 *
 * Export for decode driver to power on memory pool.
 */
int power_on_mpool_in_memalloc(void)
{
	int ret = 0;

	mutex_lock(&m_global.m_lock);
	if (m_global.mpool_power == 0) {
		ret = m_enable_mp();
	}
	mutex_unlock(&m_global.m_lock);

	return ret;
}
EXPORT_SYMBOL(power_on_mpool_in_memalloc);

/*
 * FUNCTION
 * power_off_mpool_in_memalloc()
 *
 * Export for decode driver to power of memory pool.
 */
int power_off_mpool_in_memalloc(void)
{
	int ret = 0;

	mutex_lock(&m_global.m_lock);
	if (m_global.mpool_refer == 0) {
		ret = m_disable_mp();
	}
	mutex_unlock(&m_global.m_lock);

	return ret;
}
EXPORT_SYMBOL(power_off_mpool_in_memalloc);
#endif

/* 
 * FUNCTION
 * cacheflush_usrvirt()
 *
 * Flush cached part of user space virtual memory.
 */
static int cacheflush_usrvirt(void *virt, unsigned int size)
{
	if (virt == NULL) {
		memalloc_error("*virt* is NULL.\n");
		return -EINVAL;
	}

	__cpuc_coherent_user_range((unsigned int)virt & (~(MEMALLOC_PAGE_SIZE - 1)),
			((unsigned int)virt + size + MEMALLOC_PAGE_SIZE - 1) & (~(MEMALLOC_PAGE_SIZE - 1)));

	return 0;
}

/*
 * FUNCTION
 * cacheflush_instance()
 *
 * Cacheflush all memory block nodes in driver instance.
 */
/*
static void cacheflush_instance(memalloc_inst_t *inst)
{
	int i = 0;
	void *flush_virt = NULL;

	for (i = 0; i < MEMALLOC_MAX_ALLOC; i++) {
		if (inst->pmemblock[i].phys != 0) {
			flush_virt = (void *)((unsigned int)m_global.rsv_virt +
					inst->pmemblock[i].phys - m_global.rsv_phys);
			dmac_flush_range(flush_virt, (void *)((unsigned int)flush_virt +
						inst->pmemblock[i].size));
		}
	}
}
*/
