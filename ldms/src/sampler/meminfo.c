/* -*- c-basic-offset: 8 -*-
 * Copyright (c) 2011 Open Grid Computing, Inc. All rights reserved.
 * Copyright (c) 2011 Sandia Corporation. All rights reserved.
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Export of this program may require a license from the United States
 * Government.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of Sandia nor the names of any contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *      Neither the name of Open Grid Computing nor the names of any
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *      Modified source versions must be plainly marked as such, and
 *      must not be misrepresented as being the original software.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \file meminfo.c
 * \brief /proc/meminfo data provider
 */
#define _GNU_SOURCE
#include <inttypes.h>
#include <unistd.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include "ldms.h"
#include "ldmsd.h"

#define PROC_FILE "/proc/meminfo"

static char *procfile = PROC_FILE;
static ldms_set_t set;
static FILE *mf = 0;
static ldmsd_msg_log_f msglog;
static uint64_t comp_id;
static ldms_schema_t schema;

static int create_metric_set(const char *path)
{
	int rc, i;
	uint64_t metric_value;
	char *s;
	char lbuf[256];
	char metric_name[128];

	mf = fopen(procfile, "r");
	if (!mf)
		return ENOENT;

	schema = ldms_create_schema("meminfo");
	if (!schema)
		return ENOMEM;

	/*
	 * Process the file to define all the metrics.
	 */
	int metric_no = 0;
	fseek(mf, 0, SEEK_SET);
	do {
		s = fgets(lbuf, sizeof(lbuf), mf);
		if (!s)
			break;

		rc = sscanf(lbuf, "%s %" PRIu64,
			    metric_name, &metric_value);
		if (rc < 2)
			break;

		/* Strip the colon from metric name if present */
		i = strlen(metric_name);
		if (i && metric_name[i-1] == ':')
			metric_name[i-1] = '\0';

		rc = ldms_add_metric(schema, metric_name, LDMS_V_U64);
		if (rc < 0) {
			rc = ENOMEM;
			goto err;
		}
	} while (s);

	rc = ldms_create_set(path, schema, &set);
	if (rc)
		goto err;

	for (rc = 0; rc < ldms_get_metric_count(schema); rc++)
		ldms_set_midx_udata(set, rc, comp_id);

	return 0;
 err:
	ldms_destroy_schema(schema);
	return rc;
}

/**
 * \brief Configuration
 *
 * config name=meminfo component_id=<comp_id> set=<setname>
 *     comp_id     The component id value.
 *     setname     The set name.
 */
static int config(struct attr_value_list *kwl, struct attr_value_list *avl)
{
	char *value;

	value = av_value(avl, "component_id");
	if (value)
		comp_id = strtoull(value, NULL, 0);

	value = av_value(avl, "set");
	if (value)
		create_metric_set(value);

	return 0;
}

static ldms_set_t get_set()
{
	return set;
}

static int sample(void)
{
	int rc;
	int metric_no;
	char *s;
	char lbuf[256];
	char metric_name[128];
	union ldms_value v;

	if (!set) {
		msglog("meminfo: plugin not initialized\n");
		return EINVAL;
	}
	ldms_begin_transaction(set);

	metric_no = 0;
	fseek(mf, 0, SEEK_SET);
	do {
		struct ldms_metric m;
		s = fgets(lbuf, sizeof(lbuf), mf);
		if (!s)
			break;
		rc = sscanf(lbuf, "%s %"PRIu64, metric_name, &v.v_u64);
		if (rc != 2 && rc != 3) {
			rc = EINVAL;
			goto out;
		}

		ldms_set_midx(set, metric_no, &v);
		metric_no++;
	} while (s);
 out:
	ldms_end_transaction(set);
	return 0;
}

static void term(void)
{
	if (mf)
		fclose(mf);
	if (schema)
		ldms_destroy_schema(schema);
	schema = NULL;
	if (set)
		ldms_destroy_set(set);
	set = NULL;
}

static const char *usage(void)
{
	return  "config name=meminfo component_id=<comp_id> set=<setname>\n"
		"    comp_id     The component id value.\n"
		"    setname     The set name.\n";
}

static struct ldmsd_sampler meminfo_plugin = {
	.base = {
		.name = "meminfo",
		.term = term,
		.config = config,
		.usage = usage,
	},
	.get_set = get_set,
	.sample = sample,
};

struct ldmsd_plugin *get_plugin(ldmsd_msg_log_f pf)
{
	msglog = pf;
	return &meminfo_plugin.base;
}
