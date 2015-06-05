/***************************************************************************** 
 * mp_mem.h
 * 
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Description: 
 * 	Head file of IMAPX Memory Pool management support.
 *
 * Author:
 *	Sololz <sololz.luo@gmail.com>.
 *      
 * Revision History: 
 * ­­­­­­­­­­­­­­­­­ 
 * 1.0  2011/01/14 Sololz
 * 	Create this file.
 ******************************************************************************/

#if !defined(__MP_MEM_H__)
#define __MP_MEM_H__

/* TODO */
#if 0
/**
 * Set origin hardware modules memory pool memory using status. It's supposed
 * to be called only by origin designed modules(VDEC...etc) drivers.
 */
#define MP_MEM_ORIGIN_VDEC	(0x00000001)
#define MP_MEM_ORIGIN_GPS	(0x00000002)
#define MP_MEM_ORIGIN_ILA	(0x00000004)
#define MP_MEM_ORIGIN_DMA	(0x00000008)
#define MP_MEM_ORIGIN_SLV	(0x00000010)
int mpmem_origin_user_mark(int users, int on);
#endif

/*
 * The Memory Pool valid used size is set to be 60KB, because the memory pool is
 * set to VDEC mode, and 9170 video decoder IC will use 6 blocks of all 10 of
 * memory pool. The memory pool mode should always be set to be VDEC mode, because
 * video decode is always needed. The last 4 blocks of memory pool has been used 
 * by DMA copy to accelerate memory copy speed. And the DMA copy mode use at most
 * 4KB to do data transfer between system memory and memory poll. So the max left
 * for memory pool management is 60KB.
 * DMA copy use the first page size memory region of memory pool in VDEC mode, 
 * which is from 0x3fff0000 to 0x3fff1000, so the management start address is 
 * 0x3fff1000.
 * And the alignment is set to be 8bytes because in VDEC mode the memory pool
 * is 64bit address accessable.
 * FIXME ATTENTION
 * If anyone wants to access memory pool, he must not access it directly. If any
 * new functionalities need to be add to memory pool, the should be added here.
 */
#define MPMEM_REGION_SIZE	(60 * 1024)
#define MPMEM_ADDRESS_START	(0x3fff1000)
#define MPMEM_ADDRESS_END	(0x3fff1000 + MPMEM_REGION_SIZE)
#define MPMEM_ADDRESS_ALIGN	(8)
#define MPMEM_BAD_ADDRESS	(0xffffffff)

typedef struct {
	unsigned int size;
	unsigned int phys;
	void *virt;
} mpmem_alloc_t;
int mpmem_alloc(mpmem_alloc_t *mem);
int mpmem_free(mpmem_alloc_t *mem);

inline unsigned int mpmem_virt_to_phys(void *virt);

#endif	/* __MP_MEM_H__ */

