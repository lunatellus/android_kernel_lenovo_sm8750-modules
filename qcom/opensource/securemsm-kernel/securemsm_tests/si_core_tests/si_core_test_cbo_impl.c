// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/version.h>

#if (IS_ENABLED(CONFIG_QCOM_SI_CORE) && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE))

#define pr_fmt(fmt) "si-core-tests-cbo-impl: %s: " fmt, __func__

#include "si_core_test.h"

/* All macros are referenced from ITestCBack.h */

#define ITESTCALLABLE_OP_CALL			0
#define ITESTCALLABLE_OP_CALLWITHBUFFER		1
#define ITESTCALLABLE_OP_CALLADDINT		6
#define ITESTCALLABLE_OP_CALLGETMEMOBJECT	11

/* All callback handler are translated from CTestCallable.cpp */

static int test_cb_obj_call(struct si_object *object)
{
	struct test_cb_obj_ctx *test_cbobj_ctx = to_test_cb_obj_ctx(object);

	test_cbobj_ctx->counter++;

	return test_cbobj_ctx->ret_value;
}

static int test_cb_obj_call_with_buffer(struct si_object *object,
					const void *buf, size_t buf_len)
{
	struct test_cb_obj_ctx *test_cbobj_ctx = to_test_cb_obj_ctx(object);

	if (buf_len == test_cbobj_ctx->buf_len &&
	    (memcmp(buf, test_cbobj_ctx->buf, buf_len) == 0))
		return test_cbobj_ctx->ret_value;
	else
		return test_cbobj_ctx->ret_value_error;

}

static int test_cb_obj_call_add_int(struct si_object *object, uint32_t in_val1, uint32_t in_val2,
					uint32_t *out_val)
{
	struct test_cb_obj_ctx *test_cbobj_ctx = to_test_cb_obj_ctx(object);

	*out_val = in_val1 + in_val2;
	return test_cbobj_ctx->ret_value;
}

static int test_cb_obj_call_get_mem_object(struct si_object *object, struct si_object **mo)
{
	int ret;

	struct test_cb_obj_ctx *test_cbobj_ctx = to_test_cb_obj_ctx(object);
	struct test_mem_obj_ctx *test_memobj_ctx;

	ret = test_mem_obj_create_cma(&test_memobj_ctx, NUM_PAGES * PAGE_SIZE);
	if (ret) {
		pr_err("Failed create_assign_mem_obj.\n");
		return ret;
	}

	*(uint64_t *)test_memobj_ctx->vaddr = PATTERN1;
	*mo = test_memobj_ctx->object;
	return test_cbobj_ctx->ret_value;
}

/* Dispatcher is translated from TestCBack_invoke.h */
static int test_cb_obj_dispatch(unsigned int context_id, struct si_object *object,
			unsigned long op, struct si_arg args[])
{
	switch (OBJECT_OP_METHOD_ID(op)) {
	case ITESTCALLABLE_OP_CALL:
		if (args[0].type != SI_AT_END)
			break;
		return test_cb_obj_call(object);
	case ITESTCALLABLE_OP_CALLWITHBUFFER:
		if (args[0].type != SI_AT_IB || args[1].type != SI_AT_END)
			break;
		const void *buf = (const void *) args[0].b.addr;
		size_t buf_len = args[0].b.size / 1;

		return test_cb_obj_call_with_buffer(object, buf, buf_len);
	case ITESTCALLABLE_OP_CALLADDINT:
		if (args[0].type != SI_AT_IB || args[1].type != SI_AT_OB ||
		    args[2].type != SI_AT_END || args[0].b.size != 8 ||
		    args[1].b.size != 4)
			break;

		struct add_type {
			uint32_t m_in_val1;
			uint32_t m_in_val2;
		};
		struct add_type *i = (struct add_type *)args[0].b.addr;
		uint32_t *out_val_ptr = (uint32_t *) args[1].b.addr;

		return test_cb_obj_call_add_int(object, i->m_in_val1, i->m_in_val2,
						out_val_ptr);
	case ITESTCALLABLE_OP_CALLGETMEMOBJECT:
		if (args[0].type != SI_AT_OO || args[1].type != SI_AT_END)
			break;
		return test_cb_obj_call_get_mem_object(object, &args[0].o);
	}
	return -EINVAL;
}

/* Cleanup struct test_cb_obj. */
static void test_cb_obj_release(struct si_object *object)
{
	struct test_cb_obj_ctx *test_cbobj_ctx = to_test_cb_obj_ctx(object);

	kfree(test_cbobj_ctx);
}

static struct si_object_operations test_cb_obj_ops = {
	.release = test_cb_obj_release,
	.dispatch = test_cb_obj_dispatch,
};

/* Create callback object in kernel. */
int test_cb_obj_create(struct si_object **test_cbobj)
{
	struct test_cb_obj_ctx *test_cbobj_ctx;
	int ret;

	test_cbobj_ctx = kzalloc(sizeof(struct test_cb_obj_ctx), GFP_KERNEL);
	if (!test_cbobj_ctx)
		return -ENOMEM;

	ret = init_si_object_user(&test_cbobj_ctx->object, SI_OT_CB_OBJECT, &test_cb_obj_ops,
				 "test-cb-obj");
	if (ret) {
		pr_err("init_si_object_user failed (%d).\n", ret);
		kfree(test_cbobj_ctx);
		return ret;
	}

	*test_cbobj = &test_cbobj_ctx->object;

	return ret;
}

#endif /* CONFIG_QCOM_SI_CORE && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE) */
