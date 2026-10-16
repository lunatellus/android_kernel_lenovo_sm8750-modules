// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/version.h>

#if (IS_ENABLED(CONFIG_QCOM_SI_CORE) && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE))

#define pr_fmt(fmt) "si-core-tests-cbo: %s: " fmt, __func__

#include "si_core_test.h"

/* All macros are referenced from ITestCBack.h */

#define ITESTCBACK_OP_CALL		2
#define ITESTCBACK_OP_CALLWITHBUFFER	3
#define ITESTCBACK_OP_CALLADDINT	8
#define ITESTCBACK_OP_CALLGETMEMOBJECT	13


/* All interfaces are translated from ITestCBack.h */

/*
 *
 * ITestCBack_call(Object self, Object callee_val)
 * {
 *   ObjectArg a[1]={{{0,0}}};
 *   a[0].o = callee_val;
 *
 *   return Object_invoke(self, ITestCBack_OP_call, a,
 *                        ObjectCounts_pack(0, 0, 1, 0));
 * }
 */

static int cb_obj_test_call(struct si_object_invoke_ctx *oic, struct si_object *cb_test_obj,
				struct si_object *test_cbobj)
{
	int ret, result;

	/*
	 * ObjectArg a[1]={{{0,0}}}
	 * =>
	 * struct si_arg args[2] = { 0 }
	 *
	 * The array size is increased by 1 to accommodate
	 * the SI_AT_END terminator
	 */
	struct si_arg args[2] = { 0 };

	/*
	 * a[0].o = callee_val
	 * =>
	 * args[0].type = SI_AT_IO;
	 * args[0].o = test_cbobj;
	 *
	 * Along with assigning the argument, its type must
	 * also be specified:
	 *   - Input buffer  : SI_AT_IB
	 *   - Output buffer : SI_AT_OB
	 *   - Input object  : SI_AT_IO
	 *   - Output object : SI_AT_OO
	 */
	args[0].type = SI_AT_IO;
	args[0].o = test_cbobj;

	/*
	 * The last element must always have type SI_AT_END
	 * to indicate the end of the argument list
	 */
	args[1].type = SI_AT_END;

	/*
	 * get_si_object must be called for all input objects.
	 * The reference obtained here will be consumed by QTEE.
	 */
	get_si_object(test_cbobj);

	/*
	 * Object_invoke(self, ITestCBack_OP_call, a,
	 *               ObjectCounts_pack(0, 0, 1, 0))
	 * =>
	 * si_object_do_invoke(oic, cb_test_obj,
	 *                     ITESTCBACK_OP_CALL, args, &result)
	 *
	 * Mapping:
	 *   - self  => cb_test_obj (invocation target object)
	 *   - a     => args (argument for invocation)
	 *   - ObjectCounts_pack(...) is encoded via
	 *     the 'type' field in struct si_arg
	 *
	 * result: return value from the target object
	 * ret: transport-level return val
	 */
	ret = si_object_do_invoke(oic, cb_test_obj, ITESTCBACK_OP_CALL, args, &result);
	if (ret) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	return result;
}

/*
 *
 * ITestCBack_callWithBuffer(Object self, const void *arg_ptr,
 *                           size_t arg_len, Object callee_val)
 * {
 *   ObjectArg a[2]={{{0,0}}};
 *   a[0].bi = (ObjectBufIn) { arg_ptr, arg_len * 1 };
 *   a[1].o = callee_val;
 *
 *   return Object_invoke(self, ITestCBack_OP_callWithBuffer, a,
 *                        ObjectCounts_pack(1, 0, 1, 0));
 * }
 */

static int cb_obj_test_call_with_buffer(struct si_object_invoke_ctx *oic,
	struct si_object *cb_test_obj, void *buf, size_t buf_len, struct si_object *test_cbobj)
{
	int ret, result;

	struct si_arg args[3] = { 0 };

	args[0].type = SI_AT_IB;
	args[0].b = (struct si_buffer) { {buf}, buf_len };

	args[1].type = SI_AT_IO;
	args[1].o = test_cbobj;

	args[2].type = SI_AT_END;

	/* Get reference for QTEE. */
	get_si_object(test_cbobj);

	ret = si_object_do_invoke(oic, cb_test_obj, ITESTCBACK_OP_CALLWITHBUFFER, args, &result);
	if (ret) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	return result;
}

/*
 *
 * ITestCBack_callAddInt(Object self, Object callee_val,
 *                uint32_t inVal1_val, uint32_t inVal2_val, uint32_t *outVal_ptr)
 * {
 *   ObjectArg a[3]={{{0,0}}};
 *   struct {
 *     uint32_t m_inVal1;
 *     uint32_t m_inVal2;
 *   } i;
 *   i.m_inVal1 = inVal1_val;
 *   i.m_inVal2 = inVal2_val;
 *   a[0].b = (ObjectBuf) { &i, 8 };
 *   a[1].b = (ObjectBuf) { outVal_ptr, sizeof(uint32_t) };
 *   a[2].o = callee_val;
 *
 *   return Object_invoke(self, ITestCBack_OP_callAddInt, a, ObjectCounts_pack(1, 1, 1, 0));
 * }
 */

static int cb_obj_test_call_add_int(struct si_object_invoke_ctx *oic,
			struct si_object *cb_test_obj, struct si_object *test_cbobj,
			uint32_t in_val1, uint32_t in_val2, uint32_t *out_val)
{
	int ret, result;

	struct si_arg args[4] = { 0 };

	/*
	 * Pack input/output values as defined by MinkIDL
	 * Note: Do not use sizeof(i) when setting the buffer length,
	 * as structure padding may cause a larger size and lead to
	 * OBJECT_ERROR_INVALID (result = 2).
	 */
	struct {
		uint32_t m_in_val1;
		uint32_t m_in_val2;
	} i;
	i.m_in_val1 = in_val1;
	i.m_in_val2 = in_val2;

	/*
	 * a[0].b = (ObjectBuf) { &i, 8 };
	 * =>
	 * args[0].type = SI_AT_IB;
	 * args[0].b = (struct si_buffer) { &i, 8 };
	 *
	 */
	args[0].type = SI_AT_IB;
	args[0].b = (struct si_buffer) { {&i}, 8 };

	args[1].type = SI_AT_OB;
	args[1].b = (struct si_buffer) { {out_val}, sizeof(uint32_t) };

	args[2].type = SI_AT_IO;
	args[2].o = test_cbobj;

	args[3].type = SI_AT_END;

	/* Get reference for QTEE. */
	get_si_object(test_cbobj);

	ret = si_object_do_invoke(oic, cb_test_obj, ITESTCBACK_OP_CALLADDINT, args, &result);
	if (ret) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	return result;
}

/*
 *
 * ITestCBack_callGetMemObject(Object self, Object callee_val)
 * {
 *   ObjectArg a[1]={{{0,0}}};
 *   a[0].o = callee_val;
 *
 *   return Object_invoke(self, ITestCBack_OP_callGetMemObject, a,
 *                        ObjectCounts_pack(0, 0, 1, 0));
 * }
 */

static int cb_obj_test_call_get_mem_object(struct si_object_invoke_ctx *oic,
	struct si_object *cb_test_obj, struct si_object *test_cbobj)
{
	int ret, result;

	struct si_arg args[2] = { 0 };

	args[0].type = SI_AT_IO;
	args[0].o = test_cbobj;

	args[1].type = SI_AT_END;

	/* Get reference for QTEE. */
	get_si_object(test_cbobj);

	ret = si_object_do_invoke(oic, cb_test_obj, ITESTCBACK_OP_CALLGETMEMOBJECT, args, &result);
	if (ret) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	return result;
}


int cb_obj_test(struct si_object *cb_test_obj, struct si_object_invoke_ctx *oic)
{

	int ret;
	struct si_object *test_cbobj;
	struct test_cb_obj_ctx *test_cbobj_ctx;
	uint8_t buf[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	uint32_t in_val1;
	uint32_t in_val2;
	uint32_t out_val;

	/* Create callback object. */
	ret = test_cb_obj_create(&test_cbobj);
	if (ret) {
		pr_err("failed test_cb_obj_create (%d).\n", ret);
		return ret;
	}

	/* Update callback object context. */
	test_cbobj_ctx = to_test_cb_obj_ctx(test_cbobj);
	test_cbobj_ctx->counter = 0;
	test_cbobj_ctx->ret_value = 0;
	test_cbobj_ctx->ret_value_error = 0x0AFAFAFA;
	test_cbobj_ctx->buf = buf;
	test_cbobj_ctx->buf_len = sizeof(buf);

	/* Setup add test variables. */
	in_val1 = 10;
	in_val2 = 5;
	out_val = 0;

	/* Increment counter in callback. */
	ret = cb_obj_test_call(oic, cb_test_obj, test_cbobj);
	if (ret != test_cbobj_ctx->ret_value || test_cbobj_ctx->counter != 1) {
		pr_err("cb_test_obj_call failed (%d).\n", ret);
		goto out;
	}

	/* Compare local and callback argument buffer. */
	ret = cb_obj_test_call_with_buffer(oic, cb_test_obj, test_cbobj_ctx->buf,
			test_cbobj_ctx->buf_len, test_cbobj);
	if (ret != test_cbobj_ctx->ret_value) {
		pr_err("cb_test_obj_call_with_buffer failed (%d).\n", ret);
		goto out;
	}

	/* Negative scenario for above test. */
	ret = cb_obj_test_call_with_buffer(oic, cb_test_obj, test_cbobj_ctx->buf,
			test_cbobj_ctx->buf_len - 1, test_cbobj);
	if (ret != test_cbobj_ctx->ret_value_error) {
		pr_err("cb_test_obj_call_with_buffer (invalid size) failed (%d).\n", ret);
		ret = -EINVAL;
		goto out;
	}

	/* Add two integers in callback. */
	ret = cb_obj_test_call_add_int(oic, cb_test_obj, test_cbobj,
			in_val1, in_val2, &out_val);
	if (ret != test_cbobj_ctx->ret_value) {
		pr_err("cb_obj_test_call_get_add_int (%d).\n", ret);
		goto out;
	}

	/* Validate add result by callback. */
	if (out_val != in_val1 + in_val2) {
		pr_err("add check failed.\n");
		ret = -EINVAL;
		goto out;
	}

	/* Share memory object in callback response. */
	ret = cb_obj_test_call_get_mem_object(oic, cb_test_obj, test_cbobj);
	if (ret != test_cbobj_ctx->ret_value) {
		pr_err("cb_test_obj_call_get_mem_object failed (%d).\n", ret);
		goto out;
	}

out:
	/* Destroy callback object */
	put_si_object(test_cbobj);
	return ret;
}

#endif /* CONFIG_QCOM_SI_CORE && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE) */
