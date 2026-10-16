// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/version.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/qcedev.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include "qcedev_fe.h"
#include "qcedev_fe_virt.h"

#define QCE_FE_FIRST_MINOR 0
#define QCE_FE_MINOR_CNT   1
#define QCEDEV_MAX_BUFFERS      16
#define QCE_MM_HAB_ID  MM_GPCE_1
#define HAB_OPEN_WAIT_TIMEOUT_MS (300)
#define MAX_CEHW_REQ_TRANSFER_SIZE (128*32*1024)

static struct cdev cdev_qce_fe;
static dev_t dev_qce_fe;
static struct class *class_qce_fe;
static struct completion create_hab_channel_done;

struct qce_fe_drv_hab_handles   *drv_handles;

static int qce_fe_hab_open(uint32_t *handle);

/**
 * qcedev_fe_convert_cipher_req() - Convert qcedev cipher request to GPCE format
 * @ext_req: Input extended cipher request from userspace
 * @gpce_req: Output GPCE cipher request for backend
 *
 * This function transforms the qcedev_extended_cipher_req structure to
 * gpce_cipher_req format that the backend VM expects.
 */
static void qcedev_fe_convert_cipher_req(
		const struct qcedev_extended_cipher_req *ext_req,
		struct gpce_cipher_req *gpce_req)
{
	int i = 0;

	/* Copy buffer information */
	for (i = 0; i < ext_req->vbuf.entries; i++) {
		gpce_req->vbuf.src[i].vaddr = (u64)ext_req->vbuf.src[i].vaddr;
		gpce_req->vbuf.src[i].len = ext_req->vbuf.src[i].len;
		gpce_req->vbuf.dst[i].vaddr = (u64)ext_req->vbuf.dst[i].vaddr;
		gpce_req->vbuf.dst[i].len = ext_req->vbuf.dst[i].len;
	}

	/* Copy cipher parameters */
	gpce_req->entries = ext_req->vbuf.entries;
	gpce_req->data_len = ext_req->data_len;
	gpce_req->in_place_op = ext_req->in_place_op;
	gpce_req->encklen = ext_req->key.key_length;
	memcpy(gpce_req->iv, ext_req->iv, sizeof(gpce_req->iv));
	gpce_req->ivlen = ext_req->iv_len;
	gpce_req->iv_ctr_size = ext_req->iv_ctr_size;
	gpce_req->block_offset = (u8)ext_req->byte_offset;
	gpce_req->is_pattern_valid = ext_req->is_pattern_valid;
	gpce_req->is_copy_op = ext_req->is_copy_op;
	gpce_req->encrypt = ext_req->encrypt;
	memcpy(gpce_req->mac, ext_req->mac, sizeof(gpce_req->mac));
	gpce_req->mac_len = ext_req->mac_len;

	/* Handle key based on key type */
	if (ext_req->key.key_type == QCEDEV_KEY_TYPE_SOFTWARE_KEY) {
		memcpy(gpce_req->key, ext_req->key.software_key,
			sizeof(gpce_req->key));
		gpce_req->key_size = ext_req->key.key_length;
	} else if (ext_req->key.key_type == QCEDEV_KEY_TYPE_DRM_KEY_INDEX ||
		   ext_req->key.key_type == QCEDEV_KEY_TYPE_GP_KEY_INDEX) {
		gpce_req->key_index = ext_req->key.key_index;
	}

	/* Copy pattern information */
	gpce_req->pattern_info.patt_sz = ext_req->pattern_info.patt_sz;
	gpce_req->pattern_info.proc_data_sz = ext_req->pattern_info.proc_data_sz;
	gpce_req->pattern_info.patt_offset = ext_req->pattern_info.patt_offset;

	/* Copy algorithm and mode information */
	gpce_req->alg = ext_req->alg;
	gpce_req->mode = ext_req->mode;
	gpce_req->op = ext_req->op;
	gpce_req->err = ext_req->err;

	/* Set secure buffer flags based on operation type */
	if (ext_req->op == QCEDEV_OFFLOAD_HLOS_CPB) {
		/* Non-secure to secure: input from HLOS (non-secure), output to CPB (secure) */
		gpce_req->is_secure_in = 0;   /* false - input is non-secure */
		gpce_req->is_secure_out = 1;  /* true - output is secure */
	}
}

/**
 * qcedev_fe_process_cipher_rsp() - Process cipher response from backend
 * @ext_req: Extended cipher request to update with response data
 * @gpce_rsp: GPCE cipher response from backend
 * @hab_err: HAB communication error code (0 if successful)
 *
 * This function updates the extended cipher request with the response
 * from the backend, including updated IV and error status.
 */
static void qcedev_fe_process_cipher_rsp(
		struct qcedev_extended_cipher_req *ext_req,
		const struct gpce_cipher_rsp *gpce_rsp,
		int hab_err)
{
	if ( (gpce_rsp->status == 0) && (hab_err != 0) ) {
		/* HAB communication error */
		ext_req->err = QCEDEV_OFFLOAD_GENERIC_ERROR;
		pr_err("%s: HAB communication failed - %d\n", __func__, hab_err);
		return;
	}

	/* Validate ivlen before copying to prevent buffer overread/overwrite */
	if (gpce_rsp->ivlen > sizeof(ext_req->iv)) {
		pr_err("%s: invalid ivlen %u from backend\n", __func__, gpce_rsp->ivlen);
		ext_req->err = QCEDEV_OFFLOAD_GENERIC_ERROR;
		return;
	}

	/* Backend processed the request successfully */
	/* Copy updated IV and ivlen from response */
	ext_req->iv_len = gpce_rsp->ivlen;
	memcpy(ext_req->iv, gpce_rsp->iv, ext_req->iv_len);

	/* Map backend status to qcedev error code */
	if (gpce_rsp->status != 0) {
		ext_req->err = (enum qcedev_offload_err_enum)gpce_rsp->status;
		pr_err("%s: backend reported error status: %d\n",
			__func__, gpce_rsp->status);
	} else {
		ext_req->err = QCEDEV_OFFLOAD_NO_ERROR;
	}
}

/**
 * qcedev_ext_cipher_ioctl() - Handle extended cipher operation IOCTL
 * @handle: QCE device handle
 * @arg: User space argument pointer
 *
 * This function processes all entries in a single ioctl call with chunking
 * for large buffers, matching the behavior of the original qcedev driver.
 *
 * Return: 0 on success, negative error code on failure
 */
static int qcedev_ext_cipher_ioctl(struct qcedev_fe_handle *handle,
				   unsigned long arg)
{
	struct qcedev_extended_cipher_req *ext_cipher_req = NULL;
	struct qcedev_extended_cipher_req *temp_req = NULL;
	struct gpce_cipher_req gpce_req = {0};
	struct gpce_cipher_rsp gpce_rsp = {0};
	int err = 0;
	int i;
	size_t max_data_xfer = MAX_CEHW_REQ_TRANSFER_SIZE;

	/* Allocate memory for requests to avoid stack overflow */
	ext_cipher_req = kzalloc(sizeof(*ext_cipher_req), GFP_KERNEL);
	if (!ext_cipher_req) {
		pr_err("%s: failed to allocate ext_cipher_req\n", __func__);
		return -ENOMEM;
	}

	temp_req = kzalloc(sizeof(*temp_req), GFP_KERNEL);
	if (!temp_req) {
		pr_err("%s: failed to allocate temp_req\n", __func__);
		err = -ENOMEM;
		goto free_ext_req;
	}

	/* Copy request from userspace */
	if (copy_from_user(ext_cipher_req, (void __user *)arg,
			   sizeof(*ext_cipher_req))) {
		pr_err("%s: copy_from_user failed\n", __func__);
		err = -EFAULT;
		goto free_temp_req;
	}

	/* Validate request parameters */
	if (ext_cipher_req->vbuf.entries > QCEDEV_MAX_BUFFERS) {
		pr_err("%s: entries = %d exceeds max value\n",
			__func__, ext_cipher_req->vbuf.entries);
		err = -EINVAL;
		goto free_temp_req;
	}

	pr_info("qce_fe: Processing %d entries in single ioctl\n", ext_cipher_req->vbuf.entries);

	/* Process ALL entries in this ioctl (like original qcedev) */
	for (i = 0; i < ext_cipher_req->vbuf.entries; i++) {
		size_t pending_data_len = ext_cipher_req->vbuf.src[i].len;
		__u8 *user_src = ext_cipher_req->vbuf.src[i].vaddr;
		__u8 *user_dst = ext_cipher_req->vbuf.dst[i].vaddr;
		u8 byte_offset = (i == 0) ? ext_cipher_req->byte_offset : 0;

		if (byte_offset)
			max_data_xfer = MAX_CEHW_REQ_TRANSFER_SIZE - byte_offset;

		/* Process this entry in chunks if needed */
		while (pending_data_len > 0) {
			size_t transfer_len = (pending_data_len < max_data_xfer) ?
					      pending_data_len : max_data_xfer;

			/* Setup temp request for this chunk */
			memcpy(temp_req, ext_cipher_req, sizeof(*temp_req));
			temp_req->vbuf.entries = 1;
			temp_req->vbuf.src[0].vaddr = user_src;
			temp_req->vbuf.src[0].len = transfer_len;
			temp_req->vbuf.dst[0].vaddr = user_dst;
			temp_req->vbuf.dst[0].len = transfer_len;
			temp_req->data_len = transfer_len;
			temp_req->byte_offset = byte_offset;

			/* Convert request to backend format */
			qcedev_fe_convert_cipher_req(temp_req, &gpce_req);

			/* Send request to backend via HAB channel */
			err = qcedev_fe_send_cipher_req(&gpce_req, &gpce_rsp, drv_handles);

			/* Process response and update request structure */
			qcedev_fe_process_cipher_rsp(ext_cipher_req, &gpce_rsp, err);

			if (err || ext_cipher_req->err != QCEDEV_OFFLOAD_NO_ERROR) {
				pr_err("%s: Failed for entry %d, err=%d, req_err=%d\n",
					__func__, i, err, ext_cipher_req->err);
				goto copy_to_user;
			}

			/* Update for next chunk */
			pending_data_len -= transfer_len;
			user_src += transfer_len;
			user_dst += transfer_len;
			byte_offset = 0; /* Only first chunk has byte offset */
			max_data_xfer = MAX_CEHW_REQ_TRANSFER_SIZE;
		}
	}

copy_to_user:
	/* Copy response back to userspace */
	if (copy_to_user((void __user *)arg, ext_cipher_req,
			 sizeof(*ext_cipher_req))) {
		pr_err("%s: copy_to_user failed\n", __func__);
		err = -EFAULT;
	}

free_temp_req:
	kfree(temp_req);
free_ext_req:
	kfree(ext_cipher_req);

	return err;
}


static int qce_fe_open(struct inode *i, struct file *f)
{
	struct qcedev_fe_handle *handle;

	handle = kzalloc(sizeof(struct qcedev_fe_handle), GFP_KERNEL);
	if (handle == NULL)
		return -ENOMEM;
	f->private_data = handle;
	mutex_init(&handle->registeredbufs.lock);
	INIT_LIST_HEAD(&handle->registeredbufs.list);
	pr_info("%s : Done succesffuly\n", __func__);
	return 0;
}

static int qce_fe_close(struct inode *i, struct file *f)
{
	struct qcedev_fe_handle *handle;

	pr_info("%s: Driver close\n", __func__);
	handle =  f->private_data;

	if (qcedev_unmap_all_buffers(handle, drv_handles))
		pr_err("%s: failed to unmap all ion buffers\n", __func__);

	kfree(handle);
	f->private_data = NULL;
	return 0;
}

#if (KERNEL_VERSION(2, 6, 35) > LINUX_VERSION_CODE)
static int gce_fe_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long gce_fe_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
	int err = 0;
	struct qcedev_fe_handle *handle;

	handle =  f->private_data;

	/* Verify user arguments. */
	if (_IOC_TYPE(cmd) != QCEDEV_IOC_MAGIC) {
		err = -ENOTTY;
		goto exit_qcedev;
	}

	switch (cmd) {
	case QCEDEV_IOCTL_MAP_BUF_REQ:
		{
			unsigned long long vaddr = 0;
			struct qcedev_map_buf_req map_buf = { {0} };
			int i = 0;

			if (copy_from_user(&map_buf,
					(void __user *)arg, sizeof(map_buf))) {
				err = -EFAULT;
				goto exit_qcedev;
			}

			if (map_buf.num_fds > ARRAY_SIZE(map_buf.fd)) {
				pr_err("%s: err: num_fds = %d exceeds max value\n",
							__func__, map_buf.num_fds);
				err = -EINVAL;
				goto exit_qcedev;
			}

			for (i = 0; i < map_buf.num_fds; i++) {
				err = qcedev_check_and_map_buffer(handle,
						map_buf.fd[i],
						map_buf.fd_offset[i],
						map_buf.fd_size[i],
						&vaddr,
						drv_handles);
				if (err) {
					pr_err(
						"%s: err: failed to map fd(%d) - %d\n",
						__func__, map_buf.fd[i], err);
					goto exit_qcedev;
				}
				map_buf.buf_vaddr[i] = vaddr;
				pr_info("%s: info: vaddr = %llx\n",
					__func__, vaddr);
			}

			if (copy_to_user((void __user *)arg, &map_buf,
					sizeof(map_buf))) {
				err = -EFAULT;
				goto exit_qcedev;
			}
			break;
		}

	case QCEDEV_IOCTL_UNMAP_BUF_REQ:
		{
			struct qcedev_unmap_buf_req unmap_buf = { { 0 } };
			int i = 0;

			if (copy_from_user(&unmap_buf,
				(void __user *)arg, sizeof(unmap_buf))) {
				err = -EFAULT;
				goto exit_qcedev;
			}
			if (unmap_buf.num_fds > ARRAY_SIZE(unmap_buf.fd)) {
				pr_err("%s: err: num_fds = %d exceeds max value\n",
							__func__, unmap_buf.num_fds);
				err = -EINVAL;
				goto exit_qcedev;
			}

			for (i = 0; i < unmap_buf.num_fds; i++) {
				err = qcedev_check_and_unmap_buffer(handle,
						unmap_buf.fd[i],
						drv_handles);
				if (err) {
					pr_err(
						"%s: err: failed to unmap fd(%d) - %d\n",
						 __func__,
						unmap_buf.fd[i], err);
					goto exit_qcedev;
				}
			}
			break;
		}

	case QCEDEV_IOCTL_EXT_CIPHER_OP_REQ:
		err = qcedev_ext_cipher_ioctl(handle, arg);
		if (err)
			goto exit_qcedev;
		break;

	default:
		pr_err("QCE_FE: Failed. Invalid  IOCTL cmd  0x%x\n", cmd);
		err = -EINVAL;
	}
exit_qcedev:
	return err;
}


static const struct file_operations qce_fe_fops = {
	.owner = THIS_MODULE,
	.open = qce_fe_open,
	.release = qce_fe_close,
#if (KERNEL_VERSION(2, 6, 35) > LINUX_VERSION_CODE)
	.ioctl = gce_fe_ioctl
#else
	.unlocked_ioctl = gce_fe_ioctl
#endif
};

static int qce_fe_hab_open(uint32_t *handle)
{
	int ret;

	if (handle == NULL || *handle != 0) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}
	ret = habmm_socket_open(handle, QCE_MM_HAB_ID, 0, 0);
	if (ret) {
		pr_err("habmm_socket_open failed with ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static int qce_fe_create_hab_channel(void *pv)
{
	int ret = 0;
	int i = 0;
	drv_handles = kzalloc(sizeof(struct qce_fe_drv_hab_handles), GFP_KERNEL);
	if (drv_handles == NULL) {
		ret = -ENOMEM;
		goto wait_for_thread_stop;
	}
	spin_lock_init(&(drv_handles->handle_lock));

	/* open hab handles which will be used for communication with QCE backend */
	for (i = 0; i < HAB_HANDLE_NUM; i++) {
		ret = qce_fe_hab_open(&(drv_handles->qce_fe_hab_handles[i].handle));
		if (ret != 0) {
			pr_info("%s: qce_fe_hab_open failed , ret = %d\n", __func__, ret);
			goto wait_for_thread_stop;
		}
		drv_handles->qce_fe_hab_handles[i].in_use = false;
		if (i == 0)
			drv_handles->initialized = true;
	}
	pr_info("%s:create hab channels succeeded\n", __func__);

wait_for_thread_stop:
	complete(&create_hab_channel_done);
	while (!kthread_should_stop())
		schedule();

	return ret;
}

static int qce_fe_hab_close(uint32_t handle)
{
	int ret;

	if (handle == 0)
		return 0;

	ret = habmm_socket_close(handle);
	if (ret) {
		pr_err("habmm_socket_close failed with ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static void qce_fe_destroy_hab_channel(void)
{
	int i = 0;

	if (drv_handles == NULL)
		return;
	spin_lock(&(drv_handles->handle_lock));
	/* open hab handles which will be used for communication with QCE backend */
	for (i = 0; i < HAB_HANDLE_NUM; i++)
		qce_fe_hab_close(drv_handles->qce_fe_hab_handles[i].handle);
	spin_unlock(&(drv_handles->handle_lock));
	pr_info("%s: Close hab channel succeeded\n", __func__);
}

static int __init qce_fe_init(void)
{
	int ret;
	struct device *dev_ret;
	struct task_struct *create_channel_kthread_task;

	pr_info("%s:QCE FE driver init\n", __func__);
	/* Dynamically allocate device numbers */
	ret = alloc_chrdev_region(&dev_qce_fe, QCE_FE_FIRST_MINOR, QCE_FE_MINOR_CNT, "qce");
	if (ret < 0) {
		pr_err("%s: failed with error: %d\n", __func__, ret);
		return -EFAULT;
	}
	cdev_init(&cdev_qce_fe, &qce_fe_fops);
	ret = cdev_add(&cdev_qce_fe, dev_qce_fe, QCE_FE_MINOR_CNT);
	if (ret < 0) {
		pr_err("%s: cdev_add() failed with error: %d\n", __func__, ret);
		ret = -EFAULT;
		goto unregister_chrdev_region_error;
	}

#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
	class_qce_fe = class_create("qce");
#else
	class_qce_fe = class_create(THIS_MODULE, "qce");
#endif
	if (IS_ERR_OR_NULL(class_qce_fe)) {
		pr_err("%s: class_create() failed\n", __func__);
		ret = -EFAULT;
		goto cdev_del_error;
	}
	/* create a device and registers it with sysfs */
	dev_ret = device_create(class_qce_fe, NULL, dev_qce_fe, NULL, "qce");
	if (IS_ERR_OR_NULL(dev_ret)) {
		pr_err("%s: device_create() failed\n", __func__);
		ret = -EFAULT;
		goto class_destroy_error;
	}
	init_completion(&create_hab_channel_done);
	create_channel_kthread_task = kthread_run(qce_fe_create_hab_channel, NULL,
											 "thread_create_channel");
	if (IS_ERR(create_channel_kthread_task)) {
		pr_err("fail to create kthread to create hab channels\n");
		ret = -EFAULT;
		goto device_destroy_error;
	}
	wait_for_completion_interruptible_timeout(
		&create_hab_channel_done, msecs_to_jiffies(HAB_OPEN_WAIT_TIMEOUT_MS));

	/*Stop the create_channel_kthread_task and collect the results */
	ret = kthread_stop(create_channel_kthread_task);
	if (ret < 0) {
		pr_err("%s: timeout hit\n", __func__);
		if ((drv_handles != NULL) && (drv_handles->initialized)) {
			pr_info("%s:create hab channels partially succeeded\t"
					"performance might be affected\n", __func__);
			return 0;
		}
		pr_err("%s:create hab channels failed, unloading qce_fe\n", __func__);
		/*termporarily don't set ret value */
		//ret = -ETIME;
		ret = 0;
		goto device_destroy_error;
	} else {
		pr_info("%s:create hab channels succeeded\n", __func__);
	}
	return 0;
device_destroy_error:
	device_destroy(class_qce_fe, dev_qce_fe);

class_destroy_error:
	class_destroy(class_qce_fe);

cdev_del_error:
	cdev_del(&cdev_qce_fe);

unregister_chrdev_region_error:
	unregister_chrdev_region(dev_qce_fe, QCE_FE_MINOR_CNT);

	return ret;
}

static void __exit qce_fe_exit(void)
{
	pr_info("%s: Unloading qce fe driver.\n", __func__);
	qce_fe_destroy_hab_channel();
	device_destroy(class_qce_fe, dev_qce_fe);
	class_destroy(class_qce_fe);
	cdev_del(&cdev_qce_fe);
	unregister_chrdev_region(dev_qce_fe, QCE_FE_MINOR_CNT);
}

module_init(qce_fe_init);
module_exit(qce_fe_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("QCE FE");
MODULE_IMPORT_NS(DMA_BUF);
