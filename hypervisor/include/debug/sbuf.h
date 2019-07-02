/*
 * SHARED BUFFER
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#ifndef SHARED_BUFFER_H
#define SHARED_BUFFER_H

/**
 * (sbuf) head + buf (store (ele_num - 1) elements at most)
 * buffer empty: tail == head
 * buffer full:  (tail + ele_size) % size == head
 *
 *	     Base of memory for elements
 *		|
 *		|
 * ----------------------------------------------------------------------
 * | struct shared_buf | raw data (ele_size)| ... | raw data (ele_size) |
 * ----------------------------------------------------------------------
 * |
 * |
 * struct shared_buf *buf
 */

#endif /* SHARED_BUFFER_H */
