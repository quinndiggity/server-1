/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2014, 2018, MariaDB Corporation.

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
@file include/buf0flu.h
The database buffer pool flush algorithm

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0flu_h
#define buf0flu_h

#include "univ.i"
#include "ut0byte.h"
#include "log0log.h"
#include "buf0types.h"

/** Flag indicating if the page_cleaner is in active state. */
extern bool buf_page_cleaner_is_active;

#ifdef UNIV_DEBUG

/** Value of MySQL global variable used to disable page cleaner. */
extern my_bool		innodb_page_cleaner_disabled_debug;

#endif /* UNIV_DEBUG */

/** Event to synchronise with the flushing. */
extern os_event_t	buf_flush_event;

class ut_stage_alter_t;

/** Handled page counters for a single flush */
struct flush_counters_t {
	ulint	flushed;	/*!< number of dirty pages flushed */
	ulint	evicted;	/*!< number of clean pages evicted */
	ulint	unzip_LRU_evicted;/*!< number of uncompressed page images
				evicted */
};

/** Remove a block from the flush list of modified blocks.
@param[in]	bpage	block to be removed from the flush list */
void buf_flush_remove(buf_page_t* bpage);

/*******************************************************************//**
Relocates a buffer control block on the flush_list.
Note that it is assumed that the contents of bpage has already been
copied to dpage. */
void
buf_flush_relocate_on_flush_list(
/*=============================*/
	buf_page_t*	bpage,	/*!< in/out: control block being moved */
	buf_page_t*	dpage);	/*!< in/out: destination block */
/** Update the flush system data structures when a write is completed.
@param[in,out]	bpage	flushed page
@param[in]	dblwr	whether the doublewrite buffer was used */
void buf_flush_write_complete(buf_page_t* bpage, bool dblwr);
/** Initialize a page for writing to the tablespace.
@param[in]	block		buffer block; NULL if bypassing the buffer pool
@param[in,out]	page		page frame
@param[in,out]	page_zip_	compressed page, or NULL if uncompressed
@param[in]	newest_lsn	newest modification LSN to the page */
void
buf_flush_init_for_writing(
	const buf_block_t*	block,
	byte*			page,
	void*			page_zip_,
	lsn_t			newest_lsn);

# if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: block and LRU list mutexes must be held upon entering this function, and
they will be released by this function after flushing. This is loosely based on
buf_flush_batch() and buf_flush_page().
@param[in,out]	block		buffer control block
@return whether the page was flushed and the mutex released */
bool buf_flush_page_try(buf_block_t* block)
	MY_ATTRIBUTE((warn_unused_result));
# endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */
/** Do flushing batch of a given type.
NOTE: The calling thread is not allowed to own any latches on pages!
@param[in]	type		flush type
@param[in]	min_n		wished minimum mumber of blocks flushed
(it is not guaranteed that the actual number is that big, though)
@param[in]	lsn_limit	in the case BUF_FLUSH_LIST all blocks whose
oldest_modification is smaller than this should be flushed (if their number
does not exceed min_n), otherwise ignored
@param[out]	n		the number of pages which were processed is
passed back to caller. Ignored if NULL
@retval true	if a batch was queued successfully.
@retval false	if another batch of same type was already running. */
bool
buf_flush_do_batch(
	buf_flush_t		type,
	ulint			min_n,
	lsn_t			lsn_limit,
	flush_counters_t*	n);

/** This utility flushes dirty blocks from the end of the flush list of all
buffer pool instances.
NOTE: The calling thread is not allowed to own any latches on pages!
@param[in]	min_n		wished minimum mumber of blocks flushed (it is
not guaranteed that the actual number is that big, though)
@param[in]	lsn_limit	in the case BUF_FLUSH_LIST all blocks whose
oldest_modification is smaller than this should be flushed (if their number
does not exceed min_n), otherwise ignored
@param[out]	n_processed	the number of pages which were processed is
passed back to caller. Ignored if NULL.
@return true if a batch was queued successfully for each buffer pool
instance. false if another batch of same type was already running in
at least one of the buffer pool instance */
bool
buf_flush_lists(
	ulint			min_n,
	lsn_t			lsn_limit,
	ulint*			n_processed);

/******************************************************************//**
This function picks up a single page from the tail of the LRU
list, flushes it (if it is dirty), removes it from page_hash and LRU
list and puts it on the free list. It is called from user threads when
they are unable to find a replaceable page at the tail of the LRU
list i.e.: when the background LRU flushing in the page_cleaner thread
is not fast enough to keep pace with the workload.
@return true if success. */
bool
buf_flush_single_page_from_LRU();

/** Wait until a flush batch ends.
@param[in]	type	BUF_FLUSH_LRU or BUF_FLUSH_LIST */
void buf_flush_wait_batch_end(buf_flush_t type);
/** Wait until a flush batch of the given lsn ends
@param[in]	new_oldest	target oldest_modified_lsn to wait for */
void buf_flush_wait_flushed(lsn_t new_oldest);
/********************************************************************//**
This function should be called at a mini-transaction commit, if a page was
modified in it. Puts the block to the list of modified blocks, if it not
already in it. */
UNIV_INLINE
void
buf_flush_note_modification(
/*========================*/
	buf_block_t*	block,		/*!< in: block which is modified */
	lsn_t		start_lsn,	/*!< in: start lsn of the first mtr in a
					set of mtr's */
	lsn_t		end_lsn,	/*!< in: end lsn of the last mtr in the
					set of mtr's */
	FlushObserver*	observer);	/*!< in: flush observer */

/********************************************************************//**
This function should be called when recovery has modified a buffer page. */
UNIV_INLINE
void
buf_flush_recv_note_modification(
/*=============================*/
	buf_block_t*	block,		/*!< in: block which is modified */
	lsn_t		start_lsn,	/*!< in: start lsn of the first mtr in a
					set of mtr's */
	lsn_t		end_lsn);	/*!< in: end lsn of the last mtr in the
					set of mtr's */
/********************************************************************//**
Returns TRUE if the file page block is immediately suitable for replacement,
i.e., transition FILE_PAGE => NOT_USED allowed.
@return TRUE if can replace immediately */
ibool
buf_flush_ready_for_replace(
/*========================*/
	buf_page_t*	bpage);	/*!< in: buffer control block, must be
				buf_page_in_file(bpage) and in the LRU list */

/** Initialize page_cleaner. */
void buf_flush_page_cleaner_init();

/** Wait for any possible LRU flushes to complete. */
void buf_flush_wait_LRU_batch_end();

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Validate the flush list. */
void buf_flush_validate();
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/********************************************************************//**
Initialize the red-black tree to speed up insertions into the flush_list
during recovery process. Should be called at the start of recovery
process before any page has been read/written. */
void
buf_flush_init_flush_rbt(void);
/*==========================*/

/********************************************************************//**
Frees up the red-black tree. */
void
buf_flush_free_flush_rbt(void);
/*==========================*/

/** Write a flushable page asynchronously from the buffer pool to a file.
NOTE: 1. in simulated aio we must call os_aio_simulated_wake_handler_threads
after we have posted a batch of writes! 2. buf_page_get_mutex(bpage) must be
held upon entering this function. The LRU list mutex must be held if flush_type
== BUF_FLUSH_SINGLE_PAGE. Both mutexes will be released by this function if it
returns true.
@param[in]	bpage		buffer control block
@param[in]	flush_type	type of flush
@param[in]	sync		true if sync IO request
@return whether the page was flushed */
bool buf_flush_page(buf_page_t* bpage, buf_flush_t flush_type, bool sync);

/** Check if the block is modified and ready for flushing.
@param[in]	bpage		buffer control block, must be buf_page_in_file()
@param[in]	flush_type	type of flush
@return true if can flush immediately */
bool
buf_flush_ready_for_flush(
/*======================*/
	buf_page_t*	bpage,	/*!< in: buffer control block, must be
				buf_page_in_file(bpage) */
	buf_flush_t	flush_type)/*!< in: type of flush */
	MY_ATTRIBUTE((warn_unused_result));

/** Determine the number of dirty pages in a tablespace.
@param[in]	id		tablespace identifier
@param[in,out]	observer	flush observer
@return number of dirty pages */
ulint buf_pool_get_dirty_pages_count(ulint id, FlushObserver* observer);

/** Synchronously flush dirty blocks.
NOTE: The calling thread is not allowed to hold any buffer page latches! */
void buf_flush_sync();

/** Request IO burst and wake page_cleaner up.
@param[in]	lsn_limit	upper limit of LSN to be flushed */
void buf_flush_request_force(lsn_t lsn_limit);

/** We use FlushObserver to track flushing of non-redo logged pages in bulk
create index(BtrBulk.cc).Since we disable redo logging during a index build,
we need to make sure that all dirty pages modifed by the index build are
flushed to disk before any redo logged operations go to the index. */

class FlushObserver
{
public:
	/** Constructor
	@param[in,out]	space		tablespace
	@param[in]	trx		trx instance
	@param[in]	stage		performance schema accounting object,
	used by ALTER TABLE. It is passed to log_preflush_pool_modified_pages()
	for accounting. */
	FlushObserver(fil_space_t* space, trx_t* trx, ut_stage_alter_t* stage)
		: m_space(space), m_trx(trx), m_stage(stage),
		  m_pending(0), m_interrupted(false) {}

#ifndef DBUG_OFF
	~FlushObserver();
#endif

	/** Check pages have been flushed and removed from the flush list.
	@return true if the pages were removed from the flush list */
	bool is_complete()
	{
		return is_interrupted() || !my_atomic_load32(&m_pending);
	}

	/** @return whether to flush only some pages of the tablespace */
	bool is_partial_flush() const { return m_stage != NULL; }

	/** @return whether the operation was interrupted */
	bool is_interrupted() const { return m_interrupted; }

	/** Interrupt observer not to wait. */
	void interrupted() { m_interrupted = true; }

	/** Check whether trx is interrupted
	@return true if trx is interrupted */
	bool check_interrupted();

	/** Flush dirty pages. */
	void flush();
	/** Notify observer of flushing a page. */
	void notify_flush()
	{
		my_atomic_add32(&m_pending, 1);
		if (m_stage) {
			stage_increment();
		}
	}
	/** Notify observer of removing a page from flush list. */
	void notify_remove()
	{
		my_atomic_add32(&m_pending, -1);
	}
private:
	/** Increment the stage */
	void stage_increment();

	/** Tablespace */
	fil_space_t*		m_space;

	/** Trx instance */
	trx_t* const		m_trx;

	/** Performance schema accounting object, used by ALTER TABLE.
	If not NULL, then stage->begin_phase_flush() will be called initially,
	specifying the number of pages to be attempted to be flushed and
	subsequently, stage->inc() will be called for each page we attempt to
	flush. */
	ut_stage_alter_t*	m_stage;

	/** Number of pending flush requests */
	int32			m_pending;

	/** whether the operation was interrupted */
	bool			m_interrupted;
};

/** Start a buffer flush batch for LRU or flush list
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST
@return	whether the flush batch was started (was not already running) */
bool buf_flush_start(buf_flush_t flush_type);
/** End a buffer flush batch.
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST */
void buf_flush_end(buf_flush_t flush_type);

#include "buf0flu.ic"

#endif
