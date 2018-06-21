/*****************************************************************************

Copyright (c) 2006, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2018, MariaDB Corporation.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/buf0buddy.h
Binary buddy allocator for compressed pages

Created December 2006 by Marko Makela
*******************************************************/

#ifndef buf0buddy_h
#define buf0buddy_h

#ifdef UNIV_MATERIALIZE
# undef UNIV_INLINE
# define UNIV_INLINE
#endif

#include "univ.i"
#include "buf0types.h"

/**
@param[in]	block size in bytes
@return index of buf_pool.zip_free[], or BUF_BUDDY_SIZES */
inline
ulint
buf_buddy_get_slot(ulint size)
{
	ulint	i;
	ulint	s;

	ut_ad(ut_is_2pow(size));
	ut_ad(size >= UNIV_ZIP_SIZE_MIN);
	ut_ad(size <= srv_page_size);

	for (i = 0, s = BUF_BUDDY_LOW; s < size; i++, s <<= 1) {
	}
	ut_ad(i <= BUF_BUDDY_SIZES);
	return i;
}

/** Allocate a ROW_FORMAT=COMPRESSED block.
@param[in]	i	index of buf_pool.zip_free[] or BUF_BUDDY_SIZES
@param[out]	lru	whether buf_pool.mutex was temporarily released
@return allocated block, never NULL */
byte* buf_buddy_alloc_low(ulint i, bool& lru) MY_ATTRIBUTE((malloc));

/** Allocate a ROW_FORMAT=COMPRESSED block.
The caller must not hold buf_pool.mutex nor buf_pool.zip_mutex nor any
block->mutex.
@param[in]	size		compressed page size
@param[out]	lru	whether buf_pool.mutex was temporarily released
@return allocated block, never NULL */
inline byte* buf_buddy_alloc(ulint size, bool& lru)
{
	return buf_buddy_alloc_low(buf_buddy_get_slot(size), lru);
}

/** Deallocate a block.
@param[in]	buf	block to be freed, must not be pointed to
			by the buffer pool
@param[in]	i	index of buf_pool.zip_free[], or BUF_BUDDY_SIZES */
void buf_buddy_free_low(void* buf, ulint i);

/** Deallocate a block.
@param[in]	buf	block to be freed, must not be pointed to
			by the buffer pool
@param[in]	size	block size in bytes */
inline void buf_buddy_free(void* buf, ulint size)
{
	buf_buddy_free_low(buf, buf_buddy_get_slot(size));
}

/** Try to reallocate a block.
@param[in]	buf		block to be reallocated, must be pointed
to by the buffer pool
@param[in]	size		block size, up to srv_page_size
@retval false	if failed because of no free blocks. */
bool buf_buddy_realloc(void* buf, ulint size);

/** Combine all pairs of free buddies. */
void buf_buddy_condense_free();

#endif /* buf0buddy_h */
