/**********************************************************************************
 * lpmemalloc.h
 *
 * This head file exports functions in memalloc.c for other drivers 
 * to allocate physical linear memory. First this is designed for
 * GPU use, and caller must include this head file to call correspond
 * allocate and free functions.
 *
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Sololz <sololz.luo@gmail.com>.
 *
 * 2010/07/21	Create this file to export memory allocation function
 * 		for other drivers use.
 **********************************************************************************/

#ifndef __LPMEMALLOC_H__
#define __LPMEMALLOC_H__

/* This structure is for allocation parameter. */
typedef struct {
	unsigned int phys;	/* Physical address. */
	unsigned int *virt;	/* Virtual address in kernel space use. */
	unsigned int size;
} lpmem_t;
/**
 * Function designed for kernel driver use, never export it to user space
 * application. If allocate success, 0 will be returned, else a negative
 * number will return.
 */
int lpmem_alloc(lpmem_t *mem);
int lpmem_free(lpmem_t *mem);

/**
 * Get reserved memory information.
 */
struct lpmem_rsv_info {
	unsigned int interval;
	unsigned int phys;
	unsigned int phys_end;
	void *virt;
	void *virt_end;
	unsigned int size;
};
inline void lpmem_get_rsv_info(struct lpmem_rsv_info *info);

#endif	/* __LPMEMALLOC_H__ */
