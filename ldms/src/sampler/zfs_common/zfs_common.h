
/* -*- c-basic-offset 8 -*- */
/* Copyright 2022 Lawrence Livermore National Security, LLC
 * See the top-level COPYING file for details.
 *
 * SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
 */

#ifndef ZFS_COMMON
#define ZFS_COMMON

#include <libzfs.h>

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) (sizeof(a) / sizeof(*a))
#endif

#define DEFAULT_ARRAY_LEN 32
#define MAX_ZFS_NAME_LEN  256

typedef enum {
	TOPVDEV_COUNT  = 0,
	LEAFVDEV_COUNT = 1,
	ALLVDEV_COUNT  = 2
} vdevcount_type;


typedef struct {
	vdevcount_type	counttype;
	uint_t		vdev_count;
} vdevs_count_t;


/* prototypes */

char * escape_string(char *s);

int get_vdevs_count(zpool_handle_t *zhp, void *data);

#endif
