// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "si-core-tests: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/qtee_shmbridge.h>
#if IS_ENABLED(CONFIG_QCOM_SI_CORE)
#include <linux/firmware/qcom/si_object.h>
#include <linux/firmware.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include "si_core_test.h"

// Function prototypes
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static long device_ioctl(struct file *, unsigned int, unsigned long);
static ssize_t device_write(struct file *file, const char __user *buf,
							size_t count, loff_t *offset);

// Define IOCTL commands
#define IOCTL_RUN_TEST _IOW(1, 0, int)

static struct class *driver_class;
static dev_t si_core_device_no;
static struct cdev si_core_cdev;
struct device *si_core_dev;

// File operations structure
static const struct file_operations fops = {
	.open = device_open,
	.release = device_release,
	.unlocked_ioctl = device_ioctl,
	.write = device_write
};

// Device open function
static int device_open(struct inode *inode, struct file *file)
{
	pr_info("Device opened\n");
	return 0;
}

// Device release function
static int device_release(struct inode *inode, struct file *file)
{
	pr_info("Device closed\n");
	return 0;
}

enum si_core_kernel_test_case {
	SI_CORE_KERNEL_TEST_GET_SERVICE = 1,
	SI_CORE_KERNEL_TEST_LOAD_APP,
	SI_CORE_KERNEL_TEST_END,
};

struct heap_info {
	u32 total_size;
	u32 used_size;
	u32 free_size;
	u32 overhead_size;
	u32 wasted_size;
	u32 largest_free_block_size;
};

static struct si_object_invoke_ctx oic;

#if (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE)

static int get_test_obj(struct si_object_invoke_ctx *oic, struct si_object *app_obj,
		u32 uid_val, struct si_object **test_obj)
{
	int ret, result;
	struct si_arg args[3] = { 0 };

	args[0].b = (struct si_buffer) { {&uid_val}, sizeof(uid_val) };
	args[0].type = SI_AT_IB;
	args[1].type = SI_AT_OO;
	args[2].type = SI_AT_END;

	/* IOpener_open is 0. */
	ret = si_object_do_invoke(oic, app_obj, 0, args, &result);
	if (ret || result) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	if (args[1].o != NULL_SI_OBJECT)
		*test_obj = args[1].o;

	return 0;
}

#endif

static int query_heap_info(struct si_object_invoke_ctx *oic, struct si_object *service,
							struct heap_info *heap_info)
{
	int ret, result;
	struct si_arg args[2] = { 0 };

	args[0].b = (struct si_buffer) { {heap_info}, sizeof(*heap_info) };
	args[0].type = SI_AT_OB;
	args[1].type = SI_AT_END;

	/* IDiagnostics_OP_queryHeapInfo is 0. */
	ret = si_object_do_invoke(oic, service, 0, args, &result);
	if (ret || result) {
		pr_err("failed with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}

	return 0;
}

#if (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE)

static void release_ta_scatter_mem(void *priv)
{
	struct test_mem_obj_ctx *memobj_ctx = (struct test_mem_obj_ctx *)priv;

	if (memobj_ctx && memobj_ctx->vaddr)
		vunmap(memobj_ctx->vaddr);
	kfree(memobj_ctx);
}

static struct si_object *load_ta_scatter_mem(void *ta_file, ssize_t length)
{
	struct test_mem_obj_ctx *memobj_ctx;
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
	void *buffer;

	memobj_ctx = kzalloc(sizeof(*memobj_ctx), GFP_KERNEL);
	if (!memobj_ctx)
		return NULL;

	memobj_ctx->object = init_si_mem_object_pages(&length, 0,
						      SI_CORE_MEM_OBJ_SHARE,
						      release_ta_scatter_mem,
						      memobj_ctx);
	if (!memobj_ctx->object) {
		pr_err("init_si_mem_object_pages() failed!\n");
		kfree(memobj_ctx);
		goto err_init_mo;
	}

	sgt = mem_object_to_sgt(memobj_ctx->object);
	sglist = sgt->sgl;
	nents = sgt->nents;

	for_each_sg(sglist, sg, nents, i) {
		total_pg_count += sg_page_count(sg);
	}

	pages = kmalloc_array(total_pg_count, sizeof(struct page *),
			      GFP_KERNEL);
	if (!pages)
		goto err_kmalloc_array;

	for_each_sg(sglist, sg, nents, i) {
		base_page = sg_page(sg);
		pages_in_entry = sg_page_count(sg);

		for (k = 0; k < pages_in_entry; k++)
			pages[count++] = base_page + k;
	}

	// Create the virtually contiguous mapping
	buffer = vmap(pages, total_pg_count, VM_MAP, PAGE_KERNEL);
	kfree(pages);
	if (!buffer) {
		pr_err("vmap failed for pages\n");
		goto err_kmalloc_array;
	}

	// Copy the TA binary in this scattered memory
	memcpy(buffer, ta_file, length);

	memobj_ctx->vaddr = buffer;
	memobj_ctx->size = length;

	return memobj_ctx->object;

err_kmalloc_array:
	put_si_object(memobj_ctx->object);
err_init_mo:
	return NULL;
}

static void release_ta_cma_mem(void *priv)
{
	struct test_mem_obj_ctx *memobj_ctx = (struct test_mem_obj_ctx *)priv;

	if (memobj_ctx && memobj_ctx->sgt) {
		__free_pages(sg_page(memobj_ctx->sgt->sgl),
				get_order(memobj_ctx->size));
		sg_free_table(memobj_ctx->sgt);
		kfree(memobj_ctx->sgt);
	}
	kfree(memobj_ctx);
}

static struct si_object *load_ta_cma_mem(void *ta_file, ssize_t length)
{
	int ret;
	struct sg_table *sgt;
	struct page *compound_page;
	unsigned int page_length;
	void *buffer;
	struct test_mem_obj_ctx *memobj_ctx;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	/* alloc_pages() allocates contiguous memory so we need an sgt with
	 * just 1 entry.
	 */
	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret) {
		pr_err("sg_alloc_table() failed! ret: %d\n", ret);
		goto err_sg_alloc;
	}

	compound_page = alloc_pages(GFP_KERNEL, get_order(length));
	if (!compound_page) {
		pr_err("alloc_pages() failed!\n");
		goto err_alloc_pages;
	}

	page_length = PAGE_SIZE << get_order(length);
	pr_info("Allocated pages of order %d and size 0x%x\n", get_order(length),
		page_length);

	sg_set_page(sgt->sgl, compound_page, page_length, 0);
	buffer = sg_virt(sgt->sgl);

	// Copy the TA binary in this CMA memory
	memcpy(buffer, ta_file, length);

	memobj_ctx = kzalloc(sizeof(*memobj_ctx), GFP_KERNEL);
	if (!memobj_ctx)
		goto err_ctx_alloc;

	memobj_ctx->vaddr = buffer;
	memobj_ctx->size = page_length;
	memobj_ctx->sgt = sgt;

	memobj_ctx->object = init_si_mem_object_sg(sgt, 0, 0, release_ta_cma_mem,
						   memobj_ctx);
	if (!memobj_ctx->object) {
		pr_err("init_si_mem_object_sg() failed!\n");
		goto err_init_mo;
	}

	return memobj_ctx->object;

err_init_mo:
	kfree(memobj_ctx);
err_ctx_alloc:
	__free_pages(compound_page, get_order(length));
err_alloc_pages:
	sg_free_table(sgt);
err_sg_alloc:
	kfree(sgt);

	return NULL;
}

#endif

static int load_app(struct si_object *app_loader, void *file, int len,
						struct si_object **app_controller,
						struct si_object **app_obj,
						struct si_object_invoke_ctx *oic,
						enum ta_load_mem_type from_region)
{
	int ret = -1;
	int result;
	void *buffer;
	ssize_t length;
	struct si_object *memobj = NULL;
	unsigned long app_loader_op;

	length = (ssize_t)len;
	pr_info("page aligned length of TA image 0x%lx\n", length);

	struct si_arg args[3] = { 0 };

	switch (from_region) {
#if (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE)
	case MEM_SCATTER_REGION:

		memobj = load_ta_scatter_mem(file, length);
		if (!memobj)
			return -EINVAL;

		get_si_object(memobj);

		args[0].type = SI_AT_IO;
		args[0].o = memobj;
		args[1].type = SI_AT_OO;
		args[2].type = SI_AT_END;

		/* IAppLoader_OP_loadFromRegion is 1. */
		app_loader_op = 1;
		break;
	case MEM_CMA_REGION:

		memobj = load_ta_cma_mem(file, length);
		if (!memobj)
			return -EINVAL;

		get_si_object(memobj);

		args[0].type = SI_AT_IO;
		args[0].o = memobj;
		args[1].type = SI_AT_OO;
		args[2].type = SI_AT_END;

		/* IAppLoader_OP_loadFromRegion is 1. */
		app_loader_op = 1;
		break;
#endif
	case MEM_BUFFER:

		buffer = file;
		args[0].type = SI_AT_IB;
		args[0].b = (struct si_buffer) { {buffer}, length };
		args[1].type = SI_AT_OO;
		args[2].type = SI_AT_END;

		/* IAppLoader_OP_loadFromBuffer is 0. */
		app_loader_op = 0;
		break;
	default:
		return -EINVAL;
	}

	ret = si_object_do_invoke(oic, app_loader, app_loader_op, args, &result);
	if (ret || result) {
		pr_err("failed IAppLoader_OP_loadFrom%s with result %d(ret = %d).\n",
				from_region ? "Region" : "Buffer", result, ret);
		if (from_region > 0)
			put_si_object(memobj);

		return -EINVAL;
	}
	*app_controller = args[1].o;

	if (from_region > 0)
		put_si_object(memobj);

	struct si_arg app_ctr_args[2] = { 0 };

	app_ctr_args[0].type = SI_AT_OO;
	app_ctr_args[1].type = SI_AT_END;

	/* IAppController_OP_getAppObject is 2. */
	ret = si_object_do_invoke(oic, *app_controller, 2, app_ctr_args, &result);
	if (ret || result) {
		put_si_object(*app_controller);
		pr_err("failed appController with result %d(ret = %d).\n", result, ret);
		return -EINVAL;
	}
	*app_obj = app_ctr_args[0].o;

	return ret;
}

static const char *string_from_hash(const uint8_t *in_hash, size_t in_hash_len)
{
	static char out_string[(2 * 32) + 1];
	size_t index = 0;

	for (size_t i = 0; i < in_hash_len; i++)
		index += scnprintf(out_string + index, sizeof(out_string) - index,
					"%02X", in_hash[i]);

	return out_string;
}

static int send_command(struct si_object *app_obj, struct si_object_invoke_ctx *oic)
{
	int ret, result;
	struct si_arg args[4] = { 0 };
	char string_to_hash[] = "String to hash";
	const char *print_hash_string;
	int hash_sha = 1;
	void *id_ptr = &hash_sha;
	uint8_t digest[32] = {0};

	args[0].type = SI_AT_IB;
	args[0].b = (struct si_buffer) { {string_to_hash}, sizeof(string_to_hash)-1 };
	args[1].type = SI_AT_IB;
	args[1].b = (struct si_buffer) { {id_ptr}, sizeof(hash_sha) };
	args[2].type = SI_AT_OB;
	args[2].b = (struct si_buffer) { {digest}, sizeof(digest) };
	args[3].type = SI_AT_END;

	// ISMCIExampleApp_computeHash
	ret = si_object_do_invoke(oic, app_obj, 1, args, &result);
	if (ret || result) {
		pr_err("failed ISMCIExampleApp_computeHash with result %d(ret = %d).\n",
				result, ret);
		return -EINVAL;
	}

	print_hash_string = string_from_hash(digest, sizeof(digest));
	pr_info("Hash String: %s\n", print_hash_string);
	return result;
}

static int si_core_get_service_test(struct si_object_invoke_ctx *oic)
{
	int ret;
	struct si_object *client_env, *service;
	struct heap_info heap_info;

	ret = si_core_get_client_env(oic, &client_env);
	if (ret) {
		pr_err("si_core_get_client_env failed (%d).\n", ret);
		return ret;
	}

	/* CDiagnostics_UID is 143. */
	ret = si_core_client_env_open(oic, client_env, 143, &service);
	if (ret) {
		pr_err("si_core_client_env_open failed (%d).\n", ret);
		put_si_object(client_env);
		return ret;
	}

	ret = query_heap_info(oic, service, &heap_info);
	if (ret) {
		pr_err("query_heap_info failed (%d).\n", ret);
		goto out;
	}

	pr_info("TEST SUCCESS.\n");

out:
	put_si_object(service);
	put_si_object(client_env);
	return ret;
}

static int si_core_kernel_test_load_app(struct si_object_invoke_ctx *oic, void *file, int len,
					enum ta_load_mem_type from_region)
{
	int ret;

	struct si_object *client_env, *app_loader, *app_controller, *app_obj;

	ret = si_core_get_client_env(oic, &client_env);
	if (ret) {
		pr_err("si_core_get_client_env failed (%d).\n", ret);
		return ret;
	}

	/* CAppLoader_UID is 3. */
	ret = si_core_client_env_open(oic, client_env, 3, &app_loader);
	if (ret) {
		pr_err("si_core_client_env_open failed (%d).\n", ret);
		goto out_client;
	}

	ret = load_app(app_loader, file, len, &app_controller, &app_obj, oic, from_region);
	if (ret) {
		pr_err("App loading failed (%d).\n", ret);
		goto out;
	}

	ret = send_command(app_obj, oic);
	if (!ret)
		pr_info("LOADING APP TEST SUCCESS.\n");
	else
		pr_err("Sending command failed (%d).\n", ret);

	put_si_object(app_obj);
	put_si_object(app_controller);

out:
	put_si_object(app_loader);
out_client:
	put_si_object(client_env);
	return ret;
}

#if (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE)

static int si_core_kernel_test_cbo_smo(struct si_object_invoke_ctx *oic, void *file, int len)
{
	int ret;
	enum ta_load_mem_type from_region = MEM_CMA_REGION;

#if IS_ENABLED(CONFIG_QCOM_SI_CORE_MEM_FFA)
	from_region = MEM_SCATTER_REGION;
#endif

	struct si_object *client_env, *app_loader, *app_controller, *app_obj,
			*cb_test_obj, *mem_test_obj;

	ret = si_core_get_client_env(oic, &client_env);
	if (ret) {
		pr_err("si_core_get_client_env failed (%d).\n", ret);
		return ret;
	}

	/* CAppLoader_UID is 3. */
	ret = si_core_client_env_open(oic, client_env, 3, &app_loader);
	if (ret) {
		pr_err("si_core_client_env_open failed (%d).\n", ret);
		goto out_client;
	}

	ret = load_app(app_loader, file, len, &app_controller, &app_obj, oic, from_region);
	if (ret) {
		pr_err("App loading failed (%d).\n", ret);
		goto out_app_loader;
	}

	/* CTzEcoTestApp_TestCBack_UID is 2 */
	ret = get_test_obj(oic, app_obj, 2, &cb_test_obj);
	if (ret != 0) {
		pr_err("get_test_obj failed (%d).\n", ret);
		goto out;
	}

	ret = cb_obj_test(cb_test_obj, oic);
	if (!ret)
		pr_info("CALLBACK OBJECT TEST SUCCESS.\n");
	else
		pr_err("Callback object test failed (%d).\n", ret);

	put_si_object(cb_test_obj);

	/* CTzEcoTestApp_TestMemManager_UID is 5 */
	ret = get_test_obj(oic, app_obj, 5, &mem_test_obj);
	if (ret != 0) {
		pr_err("get_test_obj failed (%d).\n", ret);
		goto out;
	}

	ret = mem_obj_test_cma(mem_test_obj, oic);
	if (!ret)
		pr_info("MEMORY OBJECT CMA TEST SUCCESS.\n");
	else
		pr_err("mem_obj_test_cma failed (%d).\n", ret);

#if IS_ENABLED(CONFIG_QCOM_SI_CORE_MEM_FFA)
	ret = mem_obj_test_sg(mem_test_obj, oic);
	if (!ret)
		pr_info("MEMORY OBJECT SG TEST SUCCESS.\n");
	else
		pr_err("mem_obj_test_sg failed (%d).\n", ret);
#endif

	put_si_object(mem_test_obj);

out:
	put_si_object(app_obj);
	put_si_object(app_controller);

out_app_loader:
	put_si_object(app_loader);
out_client:
	put_si_object(client_env);
	return ret;
}

#endif

static int create_firmware_buffer(char **firmware_buffer, size_t *buffer_size,
			const char *app, bool align)
{
	const struct firmware *fw_entry;
	char fw_name[30] = "\0";
	int ret = 0;

	snprintf(fw_name, sizeof(fw_name), "%s.mbn", app);
	ret = firmware_request_nowarn(&fw_entry, fw_name, si_core_dev);
	if (ret) {
		pr_err("Firmware request load %s failed, ret:%d\n", fw_name, ret);
		return ret;
	}

	*buffer_size = align ? ALIGN(fw_entry->size, PAGE_SIZE) : fw_entry->size;
	*firmware_buffer = kzalloc(*buffer_size, GFP_KERNEL);
	if (!*firmware_buffer) {
		pr_err("Failed kzalloc kernel_buffer.\n");
		release_firmware(fw_entry);
		return -ENOMEM;
	}
	memcpy(*firmware_buffer, fw_entry->data, fw_entry->size);
	release_firmware(fw_entry);

	return ret;
}

struct ioctl_arguments {
	u32 test_num;
	u64 file;
	u32 len;
};

static ssize_t device_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	char *firmware_buffer;
	size_t buffer_size;
	bool success = true;

	const char *smcinvokeapp = "smcinvoke_example_ta64";
#if (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE)
	const char *tzecotestapp = "tzecotestapp";
#endif
	int rc = 0;

	pr_info("Running test case 1: Direct Path\n");
	if (si_core_get_service_test(&oic) != 0) {
		pr_err("SI_CORE_KERNEL_TEST_GET_SERVICE failed.\n");
		success = false;
	} else {
		pr_info("SI_CORE_KERNEL_TEST_GET_SERVICE succeed.\n");
	}

	pr_info("Running test case 2: Loading TA (from buffer)/Sending command\n");
	rc = create_firmware_buffer(&firmware_buffer, &buffer_size, smcinvokeapp, false);
	if (rc) {
		pr_err("Failed create_firmware_buffer for %s.mbn.\n", smcinvokeapp);
		return rc;
	}

	if (si_core_kernel_test_load_app(&oic, firmware_buffer, buffer_size, MEM_BUFFER) != 0) {
		pr_err("SI_CORE_KERNEL_TEST_LOAD_APP failed.\n");
		success = false;
	} else {
		pr_info("SI_CORE_KERNEL_TEST_LOAD_APP succeed.\n");
	}

	kfree(firmware_buffer);

#if (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE)

	pr_info("Running test case 3: Loading TA (from region)/Sending command\n");
	rc = create_firmware_buffer(&firmware_buffer, &buffer_size, smcinvokeapp, true);
	if (rc) {
		pr_err("Failed create_firmware_buffer for %s.mbn.\n", smcinvokeapp);
		return rc;
	}

	if (si_core_kernel_test_load_app(&oic, firmware_buffer, buffer_size, MEM_CMA_REGION) != 0) {
		pr_err("SI_CORE_KERNEL_TEST_LOAD_APP_FROM_REGION failed.\n");
		success = false;
	} else {
		pr_info("SI_CORE_KERNEL_TEST_LOAD_APP_FROM_REGION succeed.\n");
	}

	kfree(firmware_buffer);

	pr_info("Running test case 4: Callback object/Memory object\n");
	rc = create_firmware_buffer(&firmware_buffer, &buffer_size, tzecotestapp, true);
	if (rc) {
		pr_err("Failed create_firmware_buffer for %s.mbn.\n", tzecotestapp);
		return rc;
	}

	if (si_core_kernel_test_cbo_smo(&oic, firmware_buffer, buffer_size) != 0) {
		pr_err("SI_CORE_KERNEL_TEST_CBO_SMO failed.\n");
		success = false;
	} else {
		pr_info("SI_CORE_KERNEL_TEST_CBO_SMO succeed.\n");
	}

	kfree(firmware_buffer);

	pr_info("Running test case 5: timed retry forwarder for send command\n");
	if (si_core_kernel_test_compat_retry_send_cmd() != 0) {
		pr_err("SI_CORE_KERNEL_TEST_COMPAT_RETRY_SEND_COMMAND failed.\n");
		success = false;
	} else {
		pr_info("SI_CORE_KERNEL_TEST_COMPAT_RETRY_SEND_COMMAND succeed.\n");
	}

	pr_info("Running test case 6: timed retry forwarder for shutdown\n");
	if (si_core_kernel_test_compat_retry_shutdown() != 0) {
		pr_err("SI_CORE_KERNEL_TEST_COMPAT_RETRY_SHUTDOWN failed.\n");
		success = false;
	} else {
		pr_info("SI_CORE_KERNEL_TEST_COMPAT_RETRY_SHUTDOWN succeed.\n");
	}

#endif

	if (success)
		pr_info("SI_CORE_KERNEL_TEST all test cases succeed.\n");

	return count;
}

// IOCTL function
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int test_case;
	struct ioctl_arguments msg;
	void __user *user_addr;
	char *kernel_buffer;

	switch (cmd) {
	case IOCTL_RUN_TEST:
		// Run the specified test case
		if (copy_from_user(&msg, (struct ioctl_arguments __user *)arg,
				sizeof(struct ioctl_arguments))) {
			pr_err("copy_from_user failed\n");
			return -EACCES;
		}
		test_case = (int)(msg.test_num);
		pr_info("test case selected: %d\n", test_case);
		switch (test_case) {
		case SI_CORE_KERNEL_TEST_GET_SERVICE:
			pr_info("Running test case 1: Direct Path\n");
			if (si_core_get_service_test(&oic) != 0)
				pr_err("SI_CORE_KERNEL_TEST_GET_SERVICE failed.\n");
			else
				pr_info("SI_CORE_KERNEL_TEST_GET_SERVICE succeed.\n");
			break;
		case SI_CORE_KERNEL_TEST_LOAD_APP:
			pr_info("Running test case 2: Loading TA/Sending command\n");

			kernel_buffer = kmalloc(msg.len, GFP_KERNEL);
			user_addr = u64_to_user_ptr(msg.file);
			if (copy_from_user(kernel_buffer, user_addr, msg.len)) {
				kfree(kernel_buffer);
				pr_err("copy_from_user failed\n");
				break;
			}

			if (si_core_kernel_test_load_app(&oic, kernel_buffer, msg.len, false) != 0)
				pr_err("SI_CORE_KERNEL_TEST_LOAD_APP failed.\n");
			else
				pr_info("SI_CORE_KERNEL_TEST_LOAD_APP succeed.\n");
			break;
		default:
			pr_err("Invalid test case\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int __init si_core_test_mod_init(void)
{
	int err;

	err = alloc_chrdev_region(&si_core_device_no, 0, 1, "si_core_test_client");
	if (err < 0)
		return err;

	driver_class = class_create("si_core_test_client");

	if (IS_ERR(driver_class)) {
		pr_err("class_create failed.\n");
		return -1;
	}

	cdev_init(&si_core_cdev, &fops);
	si_core_cdev.owner = THIS_MODULE;

	err = cdev_add(&si_core_cdev, MKDEV(MAJOR(si_core_device_no), 0), 1);
	if (err < 0) {
		pr_err("class_create failed.\n");
		return err;
	}

	si_core_dev = device_create(driver_class, NULL, si_core_device_no, NULL,
					"si_core_test_client");
	if (IS_ERR(si_core_dev)) {
		err = PTR_ERR(si_core_dev);
		pr_err("device_create failed %d.\n", err);
		return err;
	}

	return 0;
}

static void __exit si_core_test_mod_deinit(void)
{
	device_destroy(driver_class, si_core_device_no);
	cdev_del(&si_core_cdev);
	class_destroy(driver_class);
	unregister_chrdev_region(si_core_device_no, 1);
}

#else

static int __init si_core_test_mod_init(void)
{
	pr_info("CONFIG_QCOM_SI_CORE not enabled - skipping test\n");
	return 0;
}

static void __exit si_core_test_mod_deinit(void)
{
}

#endif /* CONFIG_QCOM_SI_CORE */
module_init(si_core_test_mod_init);
module_exit(si_core_test_mod_deinit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SI-CORE Test Kernel Driver.");
