/*******************************************************************************
 * memalloc.h
 * 
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Description:
 *	Commmon head file for memalloc char driver
 *
 * Author:
 *	Sololz <sololz.luo@gmail.com>.
 *      
 * Revision History: 
 * ­­­­­­­­­­­­­­­­­ 
 * 1.1  2009/12/10 Sololz
 * 	Create this memalloc device driver, using dynamic allocation
 * 	method.
 * 1.2	2010/07/07 Sololz
 * 	Considering dynamic allocation will cause memory fragments, so
 * 	last we decide to reserve memroy for Android media player, and 
 * 	display. But this preversion functionality memalloc is not 
 * 	abandoned, if reserved memory is oom, dynamic allocation will
 * 	method will be used.
 * 1.3	2011/01/03 Sololz
 * 	Add support for complete shared memory support for Android.
 * 1.4	2011/08/04 Sololz
 * 	Add support for memory sharing.
 ******************************************************************************/

#if !defined(__MEMALLOC_H__)
#define __MEMALLOC_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#if defined(CONFIG_EMULATE_ANDROID_PMEM_INTERFACES)
#include <linux/android_pmem.h>
#endif

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>

#include <plat/mem_reserve.h>
#include <mach/imapx_base_reg.h>
#include <mach/imapx_sysmgr.h>

#include "mp_mem.h"

/* ############################################################################################# */

#define MEMALLOC_MAX_OPEN	(256)	/* Max instance. */
#define MEMALLOC_MAX_ALLOC	(2048)	/* Max allocation each open. */

#if defined(CONFIG_IMAP_MEMALLOC_DRIVER_REGISTER_CLASS_MODE)
/* Origin driver register mode for memalloc. */
#define MEMALLOC_DYNAMIC_MAJOR	(0)	/* 0 means dynamic alloc by default. */
#define MEMALLOC_DYNAMIC_MINOR	(255)
#define	MEMALLOC_DEFAULT_MAJOR	(111)
#define MEMALLOC_DEFAULT_MINOR	(111)
#endif

#define IMAPX200_MEMORY_START	(0x40000000)

#define MEMALLOC_DEV_NAME	"memalloc"
#define MEMALLOC_PAGE_SIZE	(4096)	/* Page size defined internal. */
#define MEMALLOC_MAX_ALLOC_SIZE	(8 * 1024 * 1024)	/* 8MB is quite so enough, normally less then 3MB */

/* RESERVED MEMORY. */
#if defined(CONFIG_IMAP_MEMALLOC_MANUAL_RESERVE)
#define MEMALLOC_MEM_START	IMAPX200_MEMORY_START	/* Using board provide by infotm, memory start address must be this. */
#define MEMALLOC_MEM_END	(MEMALLOC_MEM_START + (CONFIG_IMAP_MEMALLOC_TOTAL_SIZE * 1024 * 1024))
#define MEMALLOC_RSV_SIZE	(CONFIG_IMAP_MEMALLOC_RSV_SIZE * 1024 * 1024)	/* Reserved size to be fix. */
#define MEMALLOC_RSV_ADDR	(MEMALLOC_MEM_END - MEMALLOC_RSV_SIZE)
#endif

/* Debug macros include debug alert error. */
#ifdef CONFIG_IMAP_MEMALLOC_DEBUG
#define MEMALLOC_DEBUG(debug, ...)	\
	printk(KERN_DEBUG "%s line %d: " debug, __func__, __LINE__, ##__VA_ARGS__)
#else
#define MEMALLOC_DEBUG(debug, ...)  	do{}while(0)
#endif

#define MEMALLOC_ALERT(alert, ...)	\
	printk(KERN_ALERT "%s line %d: " alert, __func__, __LINE__, ##__VA_ARGS__)
#define MEMALLOC_ERROR(error, ...)	\
	printk(KERN_ERR "%s line %d: " error, __func__, __LINE__, ##__VA_ARGS__)

#define memalloc_debug(debug, ...)	MEMALLOC_DEBUG(debug, ##__VA_ARGS__)
#define memalloc_alert(alert, ...)	MEMALLOC_ALERT(alert, ##__VA_ARGS__)
#define memalloc_error(error, ...)	MEMALLOC_ERROR(error, ##__VA_ARGS__)

/* IO control commands. */
#define MEMALLOC_MAGIC  	'k'

#define MEMALLOC_GET_BUF        _IOWR(MEMALLOC_MAGIC, 1, unsigned long)
#define MEMALLOC_FREE_BUF       _IOW(MEMALLOC_MAGIC, 2, unsigned long)

/* Old version of shared memory process, deprecated to be used. */
/* Shared memory is strongly not recommended to be used. */
#define MEMALLOC_SHM_GET	_IOWR(MEMALLOC_MAGIC, 3, unsigned long)
#define MEMALLOC_SHM_FREE	_IOW(MEMALLOC_MAGIC, 4, unsigned long)

#define MEMALLOC_GET_MPDMA_REG	_IOWR(MEMALLOC_MAGIC, 5, unsigned long)
#define MEMALLOC_FLUSH_RSV	_IO(MEMALLOC_MAGIC, 6)

/* Use memory pool in kernel space. */
#define MEMALLOC_MPDMA_COPY	_IOWR(MEMALLOC_MAGIC, 7, unsigned long)
/* Lock memory pool hard ware resource. */
#define MEMALLOC_LOCK_MPDMA	_IO(MEMALLOC_MAGIC, 8)
/* Unlock memory pool hard ware resource. */
#define MEMALLOC_UNLOCK_MPDMA	_IO(MEMALLOC_MAGIC, 9)
#define MEMALLOC_GET_MPDMA_MARK	_IOWR(MEMALLOC_MAGIC, 10, unsigned long)

/* Cached memroy support. */
#define MEMALLOC_SET_CACHED	_IOW(MEMALLOC_MAGIC, 11, unsigned long)
#define MEMALLOC_FLUSH_CACHE	_IOWR(MEMALLOC_MAGIC, 12, unsigned long)

#if defined(CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT)
/* Access to memory pool memory. */
#define MEMALLOC_GET_MPMEM	_IOWR(MEMALLOC_MAGIC, 13, memalloc_param_t)
#define MEMALLOC_FREE_MPMEM	_IOW(MEMALLOC_MAGIC, 14, memalloc_param_t)
#endif

/* Debugging tool. */
#define MEMALLOC_RESET		_IO(MEMALLOC_MAGIC, 15)

/* Export this interface for user space to get reserved memory 
 * size information. */
#define MEMALLOC_GET_TOTAL_SIZE	_IOR(MEMALLOC_MAGIC, 16, unsigned int)
#define MEMALLOC_GET_LEFT_SIZE	_IOR(MEMALLOC_MAGIC, 17, unsigned int)

/* Memory block with key asigned, to support sharing in different 
 * processes/tasks. */
#define MEMALLOC_ALLOC_WITH_KEY	_IOWR(MEMALLOC_MAGIC, 18, awk_param_t)
#define MEMALLOC_FREE_WITH_KEY	_IOW(MEMALLOC_MAGIC, 19, awk_param_t)

#define MEMALLOC_MAX_CMD	(19)

/* ############################################################################################# */

typedef struct {
	unsigned int paddr;
	unsigned int size;
} memalloc_param_t;

typedef struct {
	/* 
	 * Record the number of open handle using this share memory part, 
	 * this count mark increases at each share memory get ioctl syscall.
	 * And decrease at each share momory free ioctl syscall. Also if 
	 * user forget to free this sharing memory, this driver should recycle
	 * it if not been using.
	 */
	unsigned int dep_count;
	unsigned int shaddr;
	unsigned int size;
	unsigned int key;
} shm_param_t;

/* Special shared memory param struct. */
typedef struct {
	uint32_t key;
	uint32_t size;
	uint32_t addr;
	uint32_t fresh;
} awk_param_t;

typedef struct {
	uint32_t phys;
	uint32_t size;
} pmemblock_t;
/* This structure for open instance. */
typedef struct {
#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
	uint32_t mpool_use;
#endif

	/* Stores physics address alloc by ioctl for each instance. */
        pmemblock_t pmemblock[MEMALLOC_MAX_ALLOC];
#if defined(CONFIG_IMAP_MPMEM_MANAGEMENT_SUPPORT)
	/* Records all mpool memory blocks. */
	pmemblock_t mpmem[MPMEM_REGION_SIZE / MEMALLOC_PAGE_SIZE];
#endif
	int alloc_count;

	/* FIXME:  */
	unsigned int shaddr[MEMALLOC_MAX_ALLOC];

	uint32_t cached_mark;
} memalloc_inst_t;

/*
 * Reserved memory list structure, every memory node start address
 * is the end address + 1 of pre node memory, current end memory 
 * address is the start address of next node memory start address - 1.
 * It means that all memory block in list is linear address, this 
 * design will be very easy for unused block merge. Considering this
 * memory alloc might use O(n) time to get correspond memory, TODO, 
 * to be optimized.
 */
typedef struct rsv_mem_struct {
	unsigned int key;		/* A key mark for sharing support. */
	unsigned int ref;		/* Share count. */
	unsigned int mark;		/* Mark for current memory block is used or not. */
	unsigned int phys;		/* Physical start address of current memory block. */
	unsigned int *virt;		/* Kernel space virtual start address of curretn memory block. */
	unsigned int size;		/* Current memory block size. */
	struct rsv_mem_struct *pre;	/* Pre memory node of current memory block. */
	struct rsv_mem_struct *next;	/* Next memory node of current memory block. */
} rsv_mem_t;

/* This structure stores global parameters. */
typedef struct {
	/* TODO FIXME:
	 * While debuging this mutex, I found that spin_lock() does not work. */
	struct mutex m_lock;
#if defined(CONFIG_IMAP_MEMALLOC_DRIVER_REGISTER_CLASS_MODE)
        int major;
        int minor;
	struct class *m_class;
#endif
	void __iomem *mpdma_reg;	/* Memory pool DMA registers start virtual address. */
	struct mutex mpdma_lock;
	rsv_mem_t *rsv_head;		/* Head of reserved memory list. */
	unsigned int rsv_phys;		/* Reserved memory start physical address. */
	unsigned int rsv_phys_end;	/* Reserved memory end physical address. */
	unsigned int rsv_size;		/* Reserved memory size. */
	unsigned int *rsv_virt;		/* Reserved memory start virtual address. */
	unsigned int *rsv_virt_end;	/* Reserved memory end virtual address. */
	unsigned int rsv_interval;	/* Interval of reserved memory virtual address and physical address. */
        int inst_count;
	unsigned int export_mark;	/* Mark for export function validation. */
	memalloc_inst_t export_inst;	/* Export instance of memalloc function interfaces to access memory. */
	memalloc_inst_t shared_inst;	/* Shared instance. */
	int sh_count;			/* For android share memory in different processes and threads. */
	shm_param_t shm[MEMALLOC_MAX_ALLOC];
	unsigned int trace_memory;	/* Trace memory usage. */
#if defined(CONFIG_IMAP_MMPOOL_MANAGEMENT)
	uint32_t mpool_power;
	uint32_t mpool_refer;
#endif
} memalloc_global_t;

/* ################################## MEMORY POOL ############################### */

/* MPDMA MACROS. */
#define MPDMA_IDLE		(0x00000000)
#define MPDMA_USED		(0x34857912)

/* Memory pool dma registers. */
typedef struct {
	volatile unsigned int ch0_maddr;
	volatile unsigned int ch1_maddr;
	volatile unsigned int ch2_maddr;
	volatile unsigned int ch3_maddr;
	volatile unsigned int dma_en;
	volatile unsigned int int_en;
	volatile unsigned int int_stat;
	volatile unsigned char resv[0x64];
	volatile unsigned int ch0_saddr;
	volatile unsigned int ch0_ctrl;
	volatile unsigned int ch1_saddr;
	volatile unsigned int ch1_ctrl;
	volatile unsigned int ch2_saddr;
	volatile unsigned int ch2_ctrl;
	volatile unsigned int ch3_saddr;
	volatile unsigned int ch3_ctrl;
} mpdma_reg_t;

/* Memory pool copy transfer structure between user space and 
 * kernel. */
typedef struct {
	unsigned int *vsrc;	/* Source virtual address in user space. */
	unsigned int psrc;	/* Source physical address. */
	unsigned int *vdst;	/* Dest virtual address in user space. */
	unsigned int pdst;	/* Dest physical address. */
	unsigned int size;	/* Copy size. */
} mpdma_param_t;

/* Memory cached flush param. */
typedef struct {
	unsigned int size;
	void *virt;
} cacheflush_param_t;

typedef struct mpmem_node_struct {
	struct mpmem_node_struct *pre;
	struct mpmem_node_struct *next;

	unsigned int size;
	unsigned int phys;
	void *virt;

	unsigned int used;
} mpmem_node_t;

typedef struct {
	spinlock_t mplock;
	unsigned int mpmem_init;
	/* TODO
	int vdec_on;
	int gps_on;
	int ila_on;
	int dma_on;
	int slv_on;
	*/

	void __iomem *mpmem_vstart;	/* Kernel space managed memory pool start virtual address mapped by ioremap(). */
	mpmem_node_t *mpmem_head;
	unsigned int usz;
} mpmem_global_t;

int mpmem_initialize(void);
void mpmem_release(void);

#endif	/* __MEMALLOC_H__ */
