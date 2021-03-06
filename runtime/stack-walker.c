/*
 * Copyright (c) 2009 Tomasz Grabiec
 *
 * This file is released under the 2-clause BSD license. Please refer to the
 * file LICENSE for details.
 */

#include "runtime/stack-walker.h"

#include "jit/compilation-unit.h"
#include "jit/exception.h"

#include "vm/stack-trace.h"
#include "vm/preload.h"
#include "vm/errors.h"
#include "vm/object.h"
#include "vm/class.h"

#include <string.h>

struct vm_object *native_vmstackwalker_getclasscontext(void)
{
	struct stack_trace_elem st_elem;
	struct compilation_unit *cu;
	struct vm_array *array;
	struct vm_object *res;
	int nr_to_skip;
	int depth;
	int idx;
	int i;

	init_stack_trace_elem_current(&st_elem);
	depth = get_java_stack_trace_depth(&st_elem);

	/* Skip call to VMStackWalker.getClassContext() */
	depth -= 1;
	stack_trace_elem_next_java(&st_elem);

	res = vm_object_alloc_array(vm_array_of_java_lang_Class, depth);
	if (!res)
		return throw_oom_error();

	nr_to_skip = 0;
	idx = 0;

	for (i = 0; i < depth; i++, stack_trace_elem_next_java(&st_elem)) {
		cu = jit_lookup_cu(st_elem.addr);
		if (!cu)
			error("no compilation_unit mapping for %lx", st_elem.addr);

		struct vm_class *vmc = cu->method->class;

		if (vmc == vm_java_lang_reflect_Method &&
		    !strcmp(cu->method->name, "invoke")) {
			/*
			 * We should skip all Method.invoke() frames
			 * here as stated in comment for
			 * VMStackWalker.getClassContext().
			 */
			nr_to_skip++;
			continue;
		}

		if (vm_class_ensure_init(vmc))
			return NULL;

		array_set_field_object(res, idx++, vmc->object);
	}

	array	= vm_object_to_array(res);

	array->array_length -= nr_to_skip;

	return res;
}
