// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/version.h>

#if (IS_ENABLED(CONFIG_QCOM_SI_CORE) && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE))

#define pr_fmt(fmt) "si-core-tests-smo: %s: " fmt, __func__

#include "si_core_test.h"

/* All macros are referenced from ITestMemManager.h */

#define ITESTMEMMANAGER_OP_ACCESS			0
#define ITESTMEMMANAGER_OP_ACCESSSCATTEREDMEMORYOBJECT	9

/* All interfaces are translated from ITestMemManager.h */

/*
 *
 * ITestMemManager_access(Object self, Object mo)
 * {
 *   ObjectArg a[1]={{{0,0}}};
 *   a[0].o = mo;
 *
 *   return Object_invoke(self, ITestMemManager_OP_access, a,
 *                        ObjectCounts_pack(0, 0, 1, 0));
 * }
 */

static int mem_test_obj_access_cma(struct si_object_invoke_ctx *oic,
				struct si_object *mem_test_obj, struct si_object *test_memobj)
{
	int ret, result;

	struct si_arg args[2] = { 0 };

	args[0].type = SI_AT_IO;
	args[0].o = test_memobj;

	args[1].type = SI_AT_END;

	/* Get reference for QTEE. */
	get_si_object(test_memobj);

	ret = si_object_do_invoke(oic, mem_test_obj, ITESTMEMMANAGER_OP_ACCESS, args, &result);
	if (ret || result) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	return 0;
}

/*
 *
 * ITestMemManager_accessScatteredMemoryObject(Object self, Object mo, uint8_t modify_val)
 * {
 *   ObjectArg a[2]={{{0,0}}};
 *   a[0].b = (ObjectBuf) { &modify_val, sizeof(uint8_t) };
 *   a[1].o = mo;
 *
 *   return Object_invoke(self, ITESTMEMMANAGER_OP_ACCESSSCATTEREDMEMORYOBJECT, a,
 *                        ObjectCounts_pack(1, 0, 1, 0));
 * }
 */

static int mem_test_obj_access_sg(struct si_object_invoke_ctx *oic,
				  struct si_object *mem_test_obj,
				  struct si_object *test_memobj, uint8_t modify)
{
	int ret, result;
	struct si_arg args[3] = { 0 };

	args[0].type = SI_AT_IB;
	args[0].b = (struct si_buffer) { {&modify}, sizeof(uint8_t) };

	args[1].type = SI_AT_IO;
	args[1].o = test_memobj;

	args[2].type = SI_AT_END;

	/* Get reference for QTEE. */
	get_si_object(test_memobj);

	ret = si_object_do_invoke(oic, mem_test_obj,
			ITESTMEMMANAGER_OP_ACCESSSCATTEREDMEMORYOBJECT, args, &result);
	if (ret || result) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	return 0;
}

static int mem_obj_test_sg_lend(struct si_object *mem_test_obj, struct si_object_invoke_ctx *oic)
{
	int ret;
	struct test_mem_obj_ctx *test_memobj_ctx;
	size_t i;
	void *vaddr;
	struct sg_table *sgt;
	unsigned int nents;

	/* Create a non-contiguous memory object to be lent to QTEE. */
	ret = test_mem_obj_create_scatter(&test_memobj_ctx, NUM_PAGES * PAGE_SIZE, DO_LEND);
	if (ret) {
		pr_err("Failed test_mem_obj_create_scatter.\n");
		return ret;
	}

	/* Write a pattern to the physically non-contiguous pages. */
	vaddr = test_memobj_ctx->vaddr;
	for (i = 0; i < NUM_PAGES; i++)
		memset(vaddr + (i * PAGE_SIZE), i, PAGE_SIZE);

	sgt = mem_object_to_sgt(test_memobj_ctx->object);
	/* dma_map_mem_object() could merge the sglist reducing nents, and so, we must
	 * store it here, so we can pass the same value to dma_unmap_mem_object() later.
	 */
	nents = sgt->nents;

	/* Create a DMA mapping for this kernel allocated memory and flush the cache line.
	 * This is required when lending cached memory to QTEE because QTEE creates a new
	 * secure mapping for the lent memory in it's page-table, and therefore uses a separate
	 * secure cache line which won't be shared with Linux.
	 */
	dma_map_mem_object(test_memobj_ctx->object, nents);
	dma_unmap_mem_object(test_memobj_ctx->object, nents);

	/* Immediately lend this memory to QTEE, kernel can no longer access it from this
	 * point onwards.
	 */
	ret = early_map_memory_obj(test_memobj_ctx->object);
	vaddr = NULL;
	if (ret) {
		pr_err("early_map_memory_obj failed (%d).\n", ret);
		goto out;
	}

	/* Send this memory object to QTEE and let it validate the input pattern written
	 * by the kernel.
	 */
	ret = mem_test_obj_access_sg(oic, mem_test_obj, test_memobj_ctx->object, 0);
	if (ret) {
		pr_err("mem_test_obj_access_sg failed (%d).\n", ret);
		goto out;
	}

	pr_info("MEMORY OBJECT SG LEND TEST CASE PASSED!\n");
out:
	put_si_object(test_memobj_ctx->object);
	return ret;
}

static int mem_obj_test_sg_share(struct si_object *mem_test_obj, struct si_object_invoke_ctx *oic)
{
	int ret;
	size_t i;
	struct test_mem_obj_ctx *test_memobj_ctx;
	void *vaddr;
	void *result_buf;

	/* Create a non-contiguous memory object to be shared with QTEE. */
	ret = test_mem_obj_create_scatter(&test_memobj_ctx, NUM_PAGES * PAGE_SIZE, DO_SHARE);
	if (ret) {
		pr_err("Failed test_mem_obj_create_scatter.\n");
		return ret;
	}

	/* Write a pattern to the physically non-contiguous pages. */
	vaddr = test_memobj_ctx->vaddr;
	for (i = 0; i < NUM_PAGES; i++)
		memset(vaddr + (i * PAGE_SIZE), i, PAGE_SIZE);

	/* Send this memory object to QTEE and let it validate and reverse the
	 * input pattern written by the kernel.
	 */
	ret = mem_test_obj_access_sg(oic, mem_test_obj, test_memobj_ctx->object, 1);
	if (ret) {
		pr_err("mem_test_obj_access_sg failed (%d).\n", ret);
		goto out;
	}

	result_buf = kzalloc(NUM_PAGES * PAGE_SIZE, GFP_KERNEL);
	if (!result_buf) {
		ret = -ENOMEM;
		goto out;
	}
	for (i = 0; i < NUM_PAGES; i++)
		memset(result_buf + (i * PAGE_SIZE), (NUM_PAGES - 1) - i, PAGE_SIZE);

	/* Check if QTEE correctly reversed the input pattern. */
	if (memcmp(vaddr, result_buf, NUM_PAGES * PAGE_SIZE)) {
		pr_err("Reversed pattern check failed!\n");
		ret = -EINVAL;
		kfree(result_buf);
		goto out;
	}

	kfree(result_buf);

	pr_info("MEMORY OBJECT SG SHARE TEST CASE PASSED!\n");
out:
	put_si_object(test_memobj_ctx->object);
	return ret;
}

int mem_obj_test_cma(struct si_object *mem_test_obj, struct si_object_invoke_ctx *oic)
{
	int ret;
	struct test_mem_obj_ctx *test_memobj_ctx;

	/* Create a contiguous memory object to be shared with QTEE. */
	ret = test_mem_obj_create_cma(&test_memobj_ctx, NUM_PAGES * PAGE_SIZE);
	if (ret) {
		pr_err("Failed create_assign_mem_obj.\n");
		return ret;
	}

	/* Update memory object as per QTEE's expectation. */
	*(uint64_t *)test_memobj_ctx->vaddr = PATTERN1;

	/* Share memory object with QTEE. */
	ret = mem_test_obj_access_cma(oic, mem_test_obj, test_memobj_ctx->object);
	if (ret != 0) {
		pr_err("mem_test_obj_access_cma failed (%d).\n", ret);
		goto out;
	}

	/* Validate memory object update by QTEE. */
	if (*(uint64_t *)test_memobj_ctx->vaddr != PATTERN2) {
		pr_err("pattern check failed.\n");
		ret = -EINVAL;
	}

out:
	put_si_object(test_memobj_ctx->object);
	return ret;
}

int mem_obj_test_sg(struct si_object *mem_test_obj, struct si_object_invoke_ctx *oic)
{
	int ret;

	ret = mem_obj_test_sg_lend(mem_test_obj, oic);
	if (ret) {
		pr_err("Failed mem_obj_test_sg_lend.\n");
		return ret;
	}

	ret = mem_obj_test_sg_share(mem_test_obj, oic);
	if (ret) {
		pr_err("Failed mem_obj_test_sg_share.\n");
		return ret;
	}

	return 0;
}

#endif /* CONFIG_QCOM_SI_CORE && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE) */
