// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/module.h>
#include <linux/version.h>
#include "synx_api.h"
#include "synx_hwfence.h"
#include "synx_private.h"
#include "synx_debugfs.h"
#include "synx_ioctl.h"

int synx_debug = SYNX_ERR | SYNX_WARN | SYNX_INFO;

struct synx_device *synx_dev;
#ifdef CONFIG_DEBUG_FS
extern const struct file_operations synx_test_fops;
#endif

struct synx_session *synx_internal_initialize(
	struct synx_initialization_params *params)
{
	return NULL;
}

int synx_internal_recover(enum synx_client_id id)
{
	return -SYNX_NOSUPPORT;
}

void synx_util_default_user_callback(u32 h_synx,
	int status, void *data)
{
	dprintk(SYNX_ERR, "unimplemented user callback function\n");
}

int synx_bind(struct synx_session *session,
	u32 h_synx,
	struct synx_external_desc_v2 external_sync)
{
	return -SYNX_NOSUPPORT;
}

static const struct file_operations synx_fops = {
	.owner = THIS_MODULE,
	.open  = synx_open,
	.release = synx_close,
	.unlocked_ioctl = synx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = synx_ioctl,
#endif
};

static int __init synx_init(void)
{
	int rc;

	synx_dev = kzalloc(sizeof(*synx_dev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(synx_dev)) {
		dprintk(SYNX_ERR, "synx_dev allocation failed\n");
		return -SYNX_NOMEM;
	}

	rc = alloc_chrdev_region(&synx_dev->dev, 0, 1, SYNX_DEVICE_NAME);
	if (rc < 0) {
		dprintk(SYNX_ERR, "alloc_chrdev_region failed, err=%d\n", rc);
		goto alloc_fail;
	}

	cdev_init(&synx_dev->cdev, &synx_fops);
	synx_dev->cdev.owner = THIS_MODULE;

	rc = cdev_add(&synx_dev->cdev, synx_dev->dev, 1);
	if (rc < 0) {
		dprintk(SYNX_ERR, "cdev_add failed, err=%d\n", rc);
		goto reg_fail;
	}

#if (KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE)
	synx_dev->class = class_create(SYNX_DEVICE_NAME);
#else
	synx_dev->class = class_create(THIS_MODULE, SYNX_DEVICE_NAME);
#endif
	if (IS_ERR(synx_dev->class)) {
		rc = PTR_ERR(synx_dev->class);
		dprintk(SYNX_ERR, "class_create failed, err=%d\n", rc);
		goto err_class_create;
	}

	if (IS_ERR(device_create(synx_dev->class, NULL, synx_dev->dev,
				 NULL, SYNX_DEVICE_NAME))) {
		rc = -EINVAL;
		dprintk(SYNX_ERR, "device_create failed, err=%d\n", rc);
		goto err_device_create;
	}

#ifdef CONFIG_DEBUG_FS
	synx_dev->debugfs_root = debugfs_create_dir("synx_debug", NULL);
	if (IS_ERR_OR_NULL(synx_dev->debugfs_root)) {
		dprintk(SYNX_ERR, "Failed to create debugfs for synx\n");
		goto err_debugfs_create;
	}
	if (!debugfs_create_file("synx_test_ioctl", 0644, synx_dev->debugfs_root, synx_dev,
			&synx_test_fops)) {
		dprintk(SYNX_ERR, "Failed to create debugfs test ioctl file for synx\n");
		debugfs_remove_recursive(synx_dev->debugfs_root);
		goto err_debugfs_create;
	}
	debugfs_create_u32("debug_level", 0644, synx_dev->debugfs_root, &synx_debug);
#endif /* CONFIG_DEBUG_FS */

	/* ignore error because Synx API is still functional even if an error is returned */
	rc = synx_hwfence_init_ops(&synx_hwfence_ops);
	if (rc)
		dprintk(SYNX_DBG, "hwfence is not supported through synx api, err=%d\n", rc);

	return 0;

#ifdef CONFIG_DEBUG_FS
err_debugfs_create:
	device_destroy(synx_dev->class, synx_dev->dev);
#endif /* CONFIG_DEBUG_FS */
err_device_create:
	class_destroy(synx_dev->class);
err_class_create:
	cdev_del(&synx_dev->cdev);
reg_fail:
	unregister_chrdev_region(synx_dev->dev, 1);
alloc_fail:
	kfree(synx_dev);
	synx_dev = NULL;
	return rc;
}

static void __exit synx_exit(void)
{
	if (!IS_ERR_OR_NULL(synx_dev)) {
#ifdef CONFIG_DEBUG_FS
		debugfs_remove_recursive(synx_dev->debugfs_root);
#endif /* CONFIG_DEBUG_FS */
		device_destroy(synx_dev->class, synx_dev->dev);
		class_destroy(synx_dev->class);
		cdev_del(&synx_dev->cdev);
		unregister_chrdev_region(synx_dev->dev, 1);
		kfree(synx_dev);
		synx_dev = NULL;
	}
}

module_init(synx_init);
module_exit(synx_exit);
MODULE_DESCRIPTION("Global Synx Driver");
MODULE_LICENSE("GPL");
