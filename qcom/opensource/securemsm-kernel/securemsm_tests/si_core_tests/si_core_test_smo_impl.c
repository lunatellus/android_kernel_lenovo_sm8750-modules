// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/version.h>

#if (IS_ENABLED(CONFIG_QCOM_SI_CORE) && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE))

#define pr_fmt(fmt) "si-core-tests-smo-impl: %s: " fmt, __func__

#include <linux/vmalloc.h>

#include "si_core_test.h"

/* Cleanup struct test_mem_obj. */
static void test_mem_object_release_sg(void *private)
{
	struct test_mem_obj_ctx *test_memobj_ctx = (struct test_mem_obj_ctx *)private;

	if (test_memobj_ctx && test_memobj_ctx->vaddr)
		vunmap(test_memobj_ctx->vaddr);
	kfree(test_memobj_ctx);
}

static void test_mem_object_release_cma(void *private)
{
	struct test_mem_obj_ctx *test_memobj_ctx = (struct test_mem_obj_ctx *)private;

	if (test_memobj_ctx && test_memobj_ctx->sgt) {
		__free_pages(sg_page(test_memobj_ctx->sgt->sgl),
				get_order(test_memobj_ctx->size));
		sg_free_table(test_memobj_ctx->sgt);
		kfree(test_memobj_ctx->sgt);
	}
	kfree(test_memobj_ctx);
}

int test_mem_obj_create_scatter(struct test_mem_obj_ctx **test_memobj_ctx, size_t size,
				uint8_t do_lend)
{
	int rc = 0;
	struct sg_table *sgt;
	struct scatterlist *sglist;
	unsigned int nents;
	struct page **pages;
	struct scatterlist *sg;
	unsigned int total_pg_count = 0;
	unsigned int i, count = 0;
	struct page *base_page;
	unsigned int pages_in_entry;
	unsigned int k;

	*test_memobj_ctx = kzalloc(sizeof(struct test_mem_obj_ctx), GFP_KERNEL);
	if (!*test_memobj_ctx) {
		pr_err("Failed kzalloc test_memobj_ctx.\n");
		return -ENOMEM;
	}

	if (do_lend) {
		(*test_memobj_ctx)->object = init_si_mem_object_pages(&size, 0,
								SI_CORE_MEM_OBJ_LEND,
								test_mem_object_release_sg,
								*test_memobj_ctx);
	} else {
		(*test_memobj_ctx)->object = init_si_mem_object_pages(&size, 0,
								SI_CORE_MEM_OBJ_SHARE,
								test_mem_object_release_sg,
								*test_memobj_ctx);
	}

	if (!(*test_memobj_ctx)->object) {
		pr_err("init_si_mem_object_pages failed.\n");
		kfree(*test_memobj_ctx);
		*test_memobj_ctx = NULL;
		return -EINVAL;
	}

	sgt = mem_object_to_sgt((*test_memobj_ctx)->object);
	sglist = sgt->sgl;
	nents = sgt->nents;

	for_each_sg(sglist, sg, nents, i) {
		total_pg_count += sg_page_count(sg);
	}

	pages = kmalloc_array(total_pg_count, sizeof(struct page *),
			      GFP_KERNEL);
	if (!pages) {
		rc = -ENOMEM;
		goto err_kmalloc_array;
	}

	for_each_sg(sglist, sg, nents, i) {
		base_page = sg_page(sg);
		pages_in_entry = sg_page_count(sg);

		for (k = 0; k < pages_in_entry; k++)
			pages[count++] = base_page + k;
	}

	// Create the virtually contiguous mapping
	(*test_memobj_ctx)->vaddr = vmap(pages, total_pg_count, VM_MAP,
					 PAGE_KERNEL);
	kfree(pages);
	if (!(*test_memobj_ctx)->vaddr) {
		pr_err("vmap failed for pages\n");
		rc = -ENOMEM;
		goto err_kmalloc_array;
	}

	(*test_memobj_ctx)->size = size;
	return 0;

err_kmalloc_array:
	put_si_object((*test_memobj_ctx)->object);
	*test_memobj_ctx = NULL;
	return rc;
}

int test_mem_obj_create_cma(struct test_mem_obj_ctx **test_memobj_ctx, size_t size)
{
	int rc = 0;
	struct sg_table *sgt;
	struct page *page;
	unsigned int page_length;

	*test_memobj_ctx = kzalloc(sizeof(struct test_mem_obj_ctx), GFP_KERNEL);
	if (!*test_memobj_ctx) {
		pr_err("Failed kzalloc test_memobj_ctx.\n");
		return -ENOMEM;
	}

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		rc = -ENOMEM;
		goto err_sgt_kzalloc;
	}

	/* kzalloc() allocates contiguous memory so we need an sgt with
	 * just 1 entry.
	 */
	rc = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (rc)
		goto err_sg_alloc_table;

	page = alloc_pages(GFP_KERNEL, get_order(size));
	if (!page) {
		rc = -ENOMEM;
		goto err_page_alloc;
	}

	page_length = PAGE_SIZE << get_order(size);
	sg_set_page(sgt->sgl, page, page_length, 0);

	(*test_memobj_ctx)->object = init_si_mem_object_sg(sgt, 0, 0,
							   test_mem_object_release_cma,
							  *test_memobj_ctx);
	if (!(*test_memobj_ctx)->object) {
		pr_err("init_si_mem_object_sg failed.\n");
		rc = -EINVAL;
		goto err_init_si_mem_object;
	}

	(*test_memobj_ctx)->vaddr = sg_virt(sgt->sgl);
	(*test_memobj_ctx)->size = PAGE_ALIGN(size);
	(*test_memobj_ctx)->sgt = sgt;

	return 0;

err_init_si_mem_object:
	__free_pages(page, get_order(size));
err_page_alloc:
	sg_free_table(sgt);
err_sg_alloc_table:
	kfree(sgt);
err_sgt_kzalloc:
	kfree(*test_memobj_ctx);
	return rc;
}

#endif /* CONFIG_QCOM_SI_CORE && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE) */
