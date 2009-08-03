/*
 * Copyright (c) 2009 Tomasz Grabiec
 *
 * This file is released under the GPL version 2 with the following
 * clarification and special exception:
 *
 *     Linking this library statically or dynamically with other modules is
 *     making a combined work based on this library. Thus, the terms and
 *     conditions of the GNU General Public License cover the whole
 *     combination.
 *
 *     As a special exception, the copyright holders of this library give you
 *     permission to link this library with independent modules to produce an
 *     executable, regardless of the license terms of these independent
 *     modules, and to copy and distribute the resulting executable under terms
 *     of your choice, provided that you also meet, for each linked independent
 *     module, the terms and conditions of the license of that module. An
 *     independent module is a module which is not derived from or based on
 *     this library. If you modify this library, you may extend this exception
 *     to your version of the library, but you are not obligated to do so. If
 *     you do not wish to do so, delete this exception statement from your
 *     version.
 *
 * Please refer to the file LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jit/compiler.h"

#include "lib/string.h"

#include "vm/die.h"
#include "vm/thread.h"
#include "vm/trace.h"

static pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;

static __thread struct string *trace_buffer = NULL;

static void ensure_trace_buffer(void)
{
	if (trace_buffer)
		return;

	trace_buffer = alloc_str();
	if (!trace_buffer)
		error("out of memory");
}

int trace_printf(const char *fmt, ...)
{
	int err;
	va_list args;

	ensure_trace_buffer();

	va_start(args, fmt);
	err = str_vappend(trace_buffer, fmt, args);
	va_end(args);
	return err;
}

void trace_flush(void) {
	struct vm_thread *self;
	char *thread_name;
	char *strtok_ptr;
	char *line;

	ensure_trace_buffer();

	self = vm_thread_self();
	if (self)
		thread_name = vm_thread_get_name(self);
	else
		thread_name = "unknown";

	pthread_mutex_lock(&trace_mutex);

	line = strtok_r(trace_buffer->value, "\n", &strtok_ptr);
	while (line) {
		fprintf(stderr, "[%s] %s\n", thread_name, line);

		line = strtok_r(NULL, "\n", &strtok_ptr);
	}

	pthread_mutex_unlock(&trace_mutex);

	if (self)
		free(thread_name);

	trace_buffer->length = 0;
}
