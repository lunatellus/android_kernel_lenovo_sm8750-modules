/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __SI_CORE_TEST_H__
#define __SI_CORE_TEST_H__

#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/qtee_shmbridge.h>
#include <linux/firmware/qcom/si_object.h>
#include <linux/firmware/qcom/si_core_xts.h>
#include <linux/version.h>

#define NUM_PAGES 8

#define PATTERN1 0xFEEDC0DEFACE0001ULL
#define PATTERN2 0xFEEDC0DEFACE0002ULL

#define DO_SHARE 0
#define DO_LEND  1

#define OBJECT_OP_METHOD_ID(op)    ((op) & (uint32_t)0x0000FFFFu)

struct test_mem_obj_ctx {
	struct si_object *object;
	void *vaddr;
	size_t size;
	struct sg_table *sgt;
};

struct test_cb_obj_ctx {
	struct si_object object;
	/* Use-case specific members*/
	uint32_t counter;
	void *buf;
	uint32_t buf_len;
	uint32_t ret_value;
	uint32_t ret_value_error;
	struct test_mem_obj_ctx *test_memobj_ctx;
};

enum ta_load_mem_type {
	MEM_BUFFER,
#if (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE)
	MEM_CMA_REGION,
	MEM_SCATTER_REGION,
#endif
};

static inline int sg_page_count(struct scatterlist *sg)
{
	return PAGE_ALIGN(sg->offset + sg->length) >> PAGE_SHIFT;
}

#define to_test_cb_obj_ctx(o) container_of((o), struct test_cb_obj_ctx, object)

int test_cb_obj_create(struct si_object **test_cbobj);

/* Create a non-contiguous memory object by using a scatter gather list with > 1 entries */
int test_mem_obj_create_scatter(struct test_mem_obj_ctx **test_memobj_ctx, size_t size,
				uint8_t do_lend);
/* Create a contiguous memory object by using a scatter gather list with 1 entry */
int test_mem_obj_create_cma(struct test_mem_obj_ctx **test_memobj_ctx, size_t size);

int cb_obj_test(struct si_object *cb_test_obj, struct si_object_invoke_ctx *oic);
int mem_obj_test_cma(struct si_object *mem_test_obj, struct si_object_invoke_ctx *oic);
int mem_obj_test_sg(struct si_object *mem_test_obj, struct si_object_invoke_ctx *oic);

#if IS_ENABLED(CONFIG_QSEECOM_COMPAT)
int si_core_kernel_test_compat_retry_send_cmd(void);
int si_core_kernel_test_compat_retry_shutdown(void);
#else
static int si_core_kernel_test_compat_retry_send_cmd(void)
{
	return 0;
}
static int si_core_kernel_test_compat_retry_shutdown(void)
{
	return 0;
}
#endif

#endif /* __SI_CORE_TEST_H__ */
