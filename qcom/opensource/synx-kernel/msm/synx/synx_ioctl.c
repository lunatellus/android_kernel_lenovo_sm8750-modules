// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/sync_file.h>
#include "synx_api.h"
#include "synx_ioctl.h"
#include "synx_debugfs.h"
#include "synx_private.h"
#include "synx_util.h"

static int synx_create_sync_fd(struct dma_fence *fence)
{
	int fd;
	struct sync_file *sync_file;

	if (IS_ERR_OR_NULL(fence)) {
		dprintk(SYNX_ERR, "invalid fence\n");
		return -SYNX_INVALID;
	}

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return fd;

	sync_file = sync_file_create(fence);
	if (IS_ERR_OR_NULL(sync_file)) {
		dprintk(SYNX_ERR, "error creating sync file\n");
		goto err;
	}

	fd_install(fd, sync_file->file);
	return fd;

err:
	put_unused_fd(fd);
	return -SYNX_INVALID;
}

static int synx_handle_initialize(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session **session)
{
	struct synx_initialize_v2 init_info;
	struct synx_initialization_params params = {0};

	if (k_ioctl->size != sizeof(init_info))
		return -SYNX_INVALID;

	if (!IS_ERR_OR_NULL(*session)) {
		dprintk(SYNX_ERR, "Session is already initialized %pK\n", *session);
		return -SYNX_ALREADY;
	}

	if (copy_from_user(&init_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.id = init_info.id;
	params.flags = init_info.flags;
	params.name = init_info.name;

	(*session) = synx_initialize(&params);
	if (IS_ERR_OR_NULL(*session)) {
		dprintk(SYNX_ERR, "Failed to initialize session, err: %ld\n", PTR_ERR(*session));
		return -SYNX_INVALID;
	}

	return SYNX_SUCCESS;
}

static int synx_handle_initialize_v3(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session **session)
{
	struct synx_initialize_v3 init_info;
	struct synx_initialization_params params = {0};
	struct synx_queue_desc qdesc = {0};

	if (k_ioctl->size != sizeof(init_info))
		return -SYNX_INVALID;

	if (!IS_ERR_OR_NULL(*session)) {
		dprintk(SYNX_ERR, "Session is already initialized %pK\n", *session);
		return -SYNX_ALREADY;
	}

	if (copy_from_user(&init_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.id = init_info.id;
	params.flags = init_info.flags;
	params.name = init_info.name;
	params.ptr = &qdesc;

	if (init_info.qdesc.type >= SYNX_MEM_MAX)
		return -SYNX_INVALID;

	(*session) = synx_initialize(&params);
	if (IS_ERR_OR_NULL(*session)) {
		dprintk(SYNX_ERR, "Failed to initialize session, err: %ld\n", PTR_ERR(*session));
		return -SYNX_INVALID;
	}

	if (init_info.qdesc.type == SYNX_MEM_DEFAULT) {
		init_info.qdesc.size = qdesc.size;
		init_info.qdesc.base_offset = qdesc.base_offset;
		init_info.qdesc.wr_idx_offset = qdesc.wr_idx_offset;
	}

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&init_info,
			k_ioctl->size))
		return -EFAULT;

	return SYNX_SUCCESS;
}

static int synx_handle_create(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int result;
	int csl_fence;
	struct synx_create_v2 create_info;
	struct synx_create_params params = {0};

	if (k_ioctl->size != sizeof(create_info))
		return -SYNX_INVALID;

	if (copy_from_user(&create_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.h_synx = &create_info.synx_obj;
	params.name = create_info.name;
	params.flags = create_info.flags;
	if (create_info.flags & SYNX_CREATE_CSL_FENCE) {
		csl_fence = create_info.desc.id[0];
		params.fence = &csl_fence;
	}
	result = synx_create(session, &params);

	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
				&create_info,
				k_ioctl->size))
			return -EFAULT;

	return result;
}

static int synx_handle_getstatus(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_signal_v2 signal_info;

	if (k_ioctl->size != sizeof(signal_info))
		return -SYNX_INVALID;

	if (copy_from_user(&signal_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	signal_info.synx_state =
		synx_get_status(session, signal_info.synx_obj);

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&signal_info,
			k_ioctl->size))
		return -EFAULT;

	return SYNX_SUCCESS;
}

static int synx_handle_import(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_import_info import_info;
	struct synx_import_params params = {0};
	int result = SYNX_SUCCESS;

	if (k_ioctl->size != sizeof(import_info))
		return -SYNX_INVALID;

	if (copy_from_user(&import_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (import_info.flags & SYNX_IMPORT_DMA_FENCE) {
		if (import_info.desc.id[0] == 0) {
			dprintk(SYNX_ERR, "dma fd is not provided\n");
			return -SYNX_INVALID;
		}
		params.indv.fence =
			sync_file_get_fence(import_info.desc.id[0]);
		if (IS_ERR_OR_NULL(params.indv.fence)) {
			dprintk(SYNX_ERR,
				"Invalid fence passed %d\n",
				import_info.desc.id[0]);
			return -SYNX_INVALID;
		}
	} else if ((import_info.flags & SYNX_IMPORT_SYNX_FENCE) &&
		(import_info.synx_obj != 0)) {
		params.indv.fence = &import_info.synx_obj;
	}

	params.type = SYNX_IMPORT_INDV_PARAMS;
	params.indv.flags = import_info.flags;
	params.indv.new_h_synx = &import_info.new_synx_obj;

	if (synx_import(session, &params))
		result = -SYNX_INVALID;

	// Fence needs to be put irresepctive of import status
	if ((import_info.flags & SYNX_IMPORT_DMA_FENCE) &&
		(import_info.desc.id[0] != 0))
		dma_fence_put(params.indv.fence);

	if (result != SYNX_SUCCESS)
		return result;

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&import_info,
			k_ioctl->size))
		return -EFAULT;

	return result;
}

static int synx_handle_import_v2(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{

	struct synx_import_info_v2 import_info_v2;
	struct synx_import_params params = {0};
	int result = SYNX_SUCCESS;

	if (k_ioctl->size != sizeof(import_info_v2))
		return -SYNX_INVALID;

	if (copy_from_user(&import_info_v2,
		u64_to_user_ptr(k_ioctl->ioctl_ptr), k_ioctl->size))
		return -EFAULT;

	if (import_info_v2.flags & SYNX_IMPORT_DMA_FENCE) {
		if (import_info_v2.desc.id[0] == 0) {
			dprintk(SYNX_ERR, "dma fd is not provided\n");
			return -SYNX_INVALID;
		}
		params.indv_v2.fence =
			sync_file_get_fence(import_info_v2.desc.id[0]);
		if (IS_ERR_OR_NULL(params.indv_v2.fence)) {
			dprintk(SYNX_ERR,
				"Invalid fence passed %d\n",
				import_info_v2.desc.id[0]);
			return -SYNX_INVALID;
		}
	} else if ((import_info_v2.flags & SYNX_IMPORT_SYNX_FENCE) &&
		(import_info_v2.synx_obj != 0)) {
		params.indv_v2.fence = &import_info_v2.synx_obj;
	}

	params.type = SYNX_IMPORT_INDV_PARAMS_V2;
	params.indv_v2.flags = import_info_v2.flags;
	params.indv_v2.new_h_synx = &import_info_v2.new_synx_obj;
	params.indv_v2.security_key_hi = import_info_v2.security_key_hi;
	params.indv_v2.security_key_lo = import_info_v2.security_key_lo;
	params.indv_v2.client_data_hi = import_info_v2.client_data_hi;
	params.indv_v2.client_data_lo = import_info_v2.client_data_lo;

	if (synx_import(session, &params))
		result = -SYNX_INVALID;

	import_info_v2.security_key_hi = params.indv_v2.security_key_hi;
	import_info_v2.security_key_lo = params.indv_v2.security_key_lo;

	// Fence needs to be put irresepctive of import status
	if ((import_info_v2.flags & SYNX_IMPORT_DMA_FENCE) &&
		(import_info_v2.desc.id[0] != 0))
		dma_fence_put(params.indv_v2.fence);

	if (result != SYNX_SUCCESS)
		return result;

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&import_info_v2,
			k_ioctl->size))
		return -EFAULT;

	return result;
}

static int synx_handle_import_arr(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int rc = -SYNX_INVALID;
	u32 idx = 0;
	struct synx_import_arr_info arr_info;
	struct synx_import_info *arr;
	struct synx_import_params params = {0};

	if (k_ioctl->size != sizeof(arr_info))
		return -SYNX_INVALID;

	if (copy_from_user(&arr_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (arr_info.num_objs == 0 || arr_info.num_objs >= SYNX_MAX_OBJS) {
		dprintk(SYNX_ERR, "invalid num_objs %u\n", arr_info.num_objs);
		return -SYNX_INVALID;
	}

	arr = kcalloc(arr_info.num_objs,
				sizeof(*arr), GFP_KERNEL);
	if (IS_ERR_OR_NULL(arr))
		return -ENOMEM;

	if (copy_from_user(arr,
			u64_to_user_ptr(arr_info.list),
			sizeof(*arr) * arr_info.num_objs)) {
		rc = -EFAULT;
		goto fail;
	}

	while (idx < arr_info.num_objs) {
		params.type = SYNX_IMPORT_INDV_PARAMS;
		params.indv.new_h_synx = &arr[idx].new_synx_obj;
		params.indv.flags = arr[idx].flags;
		params.indv.fence = NULL;

		if (arr[idx].flags & SYNX_IMPORT_DMA_FENCE) {
			if (arr[idx].desc.id[0] == 0) {
				dprintk(SYNX_ERR, "dma fd is not provided at idx %u\n", idx);
				rc = -SYNX_INVALID;
				break;
			}
			params.indv.fence =
				sync_file_get_fence(arr[idx].desc.id[0]);
			if (IS_ERR_OR_NULL(params.indv.fence)) {
				dprintk(SYNX_ERR,
				"Invalid fence passed %u\n",
				arr[idx].desc.id[0]);
				rc = -SYNX_INVALID;
				break;
			}
		} else if ((arr[idx].flags & SYNX_IMPORT_SYNX_FENCE) &&
			(arr[idx].synx_obj != 0)) {
			params.indv.fence = &arr[idx].synx_obj;
		}

		rc = synx_import(session, &params);

		// Fence needs to be put irresepctive of import status
		if ((arr[idx].flags & SYNX_IMPORT_DMA_FENCE) &&
			arr[idx].desc.id[0] != 0)
			dma_fence_put(params.indv.fence);

		if (rc != SYNX_SUCCESS)
			break;
		idx++;
	}

	/* release allocated handles in case of failure */
	if (rc != SYNX_SUCCESS) {
		while (idx > 0)
			synx_release(session,
				arr[--idx].new_synx_obj);
	} else {
		if (copy_to_user(u64_to_user_ptr(arr_info.list),
			arr,
			sizeof(*arr) * arr_info.num_objs)) {
			rc = -EFAULT;
			while (idx > 0)
				synx_release(session,
					arr[--idx].new_synx_obj);
			goto fail;
		}
	}

fail:
	kfree(arr);
	return rc;
}

static int synx_handle_import_arr_v2(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int rc = -SYNX_INVALID;
	u32 idx = 0;
	struct synx_import_arr_info arr_info_v2;
	struct synx_import_info_v2 *arr_v2;
	struct synx_import_params params = {0};

	if (k_ioctl->size != sizeof(arr_info_v2))
		return -SYNX_INVALID;

	if (copy_from_user(&arr_info_v2,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (arr_info_v2.num_objs == 0 || arr_info_v2.num_objs >= SYNX_MAX_OBJS) {
		dprintk(SYNX_ERR, "invalid num_objs %u\n", arr_info_v2.num_objs);
		return -SYNX_INVALID;
	}

	arr_v2 = kcalloc(arr_info_v2.num_objs,
				sizeof(*arr_v2), GFP_KERNEL);
	if (IS_ERR_OR_NULL(arr_v2))
		return -ENOMEM;

	if (copy_from_user(arr_v2,
			u64_to_user_ptr(arr_info_v2.list),
			sizeof(*arr_v2) * arr_info_v2.num_objs)) {
		rc = -EFAULT;
		goto fail;
	}

	while (idx < arr_info_v2.num_objs) {
		params.type = SYNX_IMPORT_INDV_PARAMS_V2;
		params.indv_v2.new_h_synx = &arr_v2[idx].new_synx_obj;
		params.indv_v2.flags = arr_v2[idx].flags;
		params.indv_v2.security_key_hi = arr_v2[idx].security_key_hi;
		params.indv_v2.security_key_lo = arr_v2[idx].security_key_lo;
		params.indv_v2.client_data_hi = arr_v2[idx].client_data_hi;
		params.indv_v2.client_data_lo = arr_v2[idx].client_data_lo;
		params.indv_v2.fence = NULL;

		if (arr_v2[idx].flags & SYNX_IMPORT_DMA_FENCE) {
			if (arr_v2[idx].desc.id[0] == 0) {
				dprintk(SYNX_ERR, "dma fd is not provided at idx %u\n", idx);
				rc = -SYNX_INVALID;
				break;
			}
			params.indv_v2.fence =
				sync_file_get_fence(arr_v2[idx].desc.id[0]);
			if (IS_ERR_OR_NULL(params.indv_v2.fence)) {
				dprintk(SYNX_ERR,
				"Invalid fence passed %u\n",
				arr_v2[idx].desc.id[0]);
				rc = -SYNX_INVALID;
				break;
			}
		} else if ((arr_v2[idx].flags & SYNX_IMPORT_SYNX_FENCE) &&
			(arr_v2[idx].synx_obj != 0)) {
			params.indv_v2.fence = &arr_v2[idx].synx_obj;
		}

		rc = synx_import(session, &params);

		arr_v2[idx].security_key_hi = params.indv_v2.security_key_hi;
		arr_v2[idx].security_key_lo = params.indv_v2.security_key_lo;

		// Fence needs to be put irresepctive of import status
		if ((arr_v2[idx].flags & SYNX_IMPORT_DMA_FENCE) &&
			arr_v2[idx].desc.id[0] != 0)
			dma_fence_put(params.indv_v2.fence);

		if (rc != SYNX_SUCCESS)
			break;
		idx++;
	}

	/* release allocated handles in case of failure */
	if (rc != SYNX_SUCCESS) {
		while (idx > 0)
			synx_release(session,
				arr_v2[--idx].new_synx_obj);
	} else {
		if (copy_to_user(u64_to_user_ptr(arr_info_v2.list),
			arr_v2,
			sizeof(*arr_v2) * arr_info_v2.num_objs)) {
			rc = -EFAULT;
			while (idx > 0)
				synx_release(session,
					arr_v2[--idx].new_synx_obj);
			goto fail;
		}
	}

fail:
	kfree(arr_v2);
	return rc;
}

static int synx_handle_export(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	return -SYNX_INVALID;
}

static int synx_handle_signal(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_signal_v2 signal_info;

	if (k_ioctl->size != sizeof(signal_info))
		return -SYNX_INVALID;

	if (copy_from_user(&signal_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	return synx_signal(session, signal_info.synx_obj,
		signal_info.synx_state);
}

static int synx_handle_merge(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	u32 *h_synxs;
	int result;
	struct synx_merge_v2 merge_info;
	struct synx_merge_params params = {0};

	if (k_ioctl->size != sizeof(merge_info))
		return -SYNX_INVALID;

	if (copy_from_user(&merge_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (merge_info.num_objs >= SYNX_MAX_OBJS)
		return -SYNX_INVALID;

	h_synxs = kcalloc(merge_info.num_objs,
				sizeof(*h_synxs), GFP_KERNEL);
	if (IS_ERR_OR_NULL(h_synxs)) {
		dprintk(SYNX_ERR, "h_synxs allocation failed\n");
		return -ENOMEM;
	}

	if (copy_from_user(h_synxs,
			u64_to_user_ptr(merge_info.synx_objs),
			sizeof(u32) * merge_info.num_objs)) {
		kfree(h_synxs);
		return -EFAULT;
	}

	params.num_objs = merge_info.num_objs;
	params.h_synxs = h_synxs;
	params.flags = merge_info.flags;
	params.h_merged_obj = &merge_info.merged;

	result = synx_merge(session, &params);
	if (!result)
		if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
				&merge_info,
				k_ioctl->size)) {
			kfree(h_synxs);
			return -EFAULT;
	}

	kfree(h_synxs);
	return result;
}

static int synx_handle_merge_n(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	u32 *h_synxs;
	int result = 0;
	struct synx_merge_n_info merge_n_info;
	struct synx_merge_n_params params = {0};

	if (k_ioctl->size != sizeof(merge_n_info))
		return -SYNX_INVALID;

	if (copy_from_user(&merge_n_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (merge_n_info.type == SYNX_MERGE_INDV_PARAMS) {

		if (merge_n_info.indv.num_objs >= SYNX_MAX_OBJS)
			return -SYNX_INVALID;

		h_synxs = kcalloc(merge_n_info.indv.num_objs,
					sizeof(*h_synxs), GFP_KERNEL);
		if (IS_ERR_OR_NULL(h_synxs)) {
			dprintk(SYNX_ERR, "h_synxs allocation failed\n");
			return -ENOMEM;
		}

		if (copy_from_user(h_synxs,
			u64_to_user_ptr(merge_n_info.indv.synx_objs),
			sizeof(u32) * merge_n_info.indv.num_objs)) {
			kfree(h_synxs);
			return -EFAULT;
		}

		params.type = SYNX_MERGE_INDV_PARAMS;
		params.indv.num_objs = merge_n_info.indv.num_objs;
		params.indv.h_synxs = h_synxs;
		params.indv.flags = merge_n_info.indv.flags;
		params.indv.h_merged_obj = &merge_n_info.indv.merged;
		params.indv.security_key_hi = merge_n_info.indv.security_key_hi;
		params.indv.security_key_lo = merge_n_info.indv.security_key_lo;

		result = synx_merge_n(session, &params);
		if (!result)
			if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
					&merge_n_info,
					k_ioctl->size)) {
				kfree(h_synxs);
				return -EFAULT;
		}
	}

	kfree(h_synxs);
	return result;
}

static int synx_handle_wait(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_wait_v2 wait_info;

	if (k_ioctl->size != sizeof(wait_info))
		return -SYNX_INVALID;

	if (copy_from_user(&wait_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_wait(session,
		wait_info.synx_obj, wait_info.timeout_ms);

	return SYNX_SUCCESS;
}

static int synx_handle_async_wait(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int rc = 0;
	struct synx_userpayload_info_v2 user_data;
	struct synx_callback_params params = {0};

	if (k_ioctl->size != sizeof(user_data))
		return -SYNX_INVALID;

	if (copy_from_user(&user_data,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.h_synx = user_data.synx_obj;
	params.cb_func = synx_util_default_user_callback;
	params.userdata = (void *)user_data.payload[0];
	params.timeout_ms = user_data.payload[2];

	rc = synx_async_wait(session, &params);
	if (rc)
		dprintk(SYNX_ERR,
			"user cb registration failed for handle %d\n",
			user_data.synx_obj);

	return rc;
}

static int synx_handle_cancel_async_wait(
	struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int rc = 0;
	struct synx_userpayload_info_v2 user_data;
	struct synx_callback_params params = {0};

	if (k_ioctl->size != sizeof(user_data))
		return -SYNX_INVALID;

	if (copy_from_user(&user_data,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	params.h_synx = user_data.synx_obj;
	params.cb_func = synx_util_default_user_callback;
	params.userdata = (void *)user_data.payload[0];

	rc = synx_cancel_async_wait(session, &params);
	if (rc)
		dprintk(SYNX_ERR,
			"user cb deregistration failed for handle %d\n",
			user_data.synx_obj);

	return rc;
}

static int synx_handle_bind(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_bind_v2 synx_bind_info;

	if (k_ioctl->size != sizeof(synx_bind_info))
		return -SYNX_INVALID;

	if (copy_from_user(&synx_bind_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_bind(session,
		synx_bind_info.synx_obj,
		synx_bind_info.ext_sync_desc);

	return k_ioctl->result;
}

static int synx_handle_release(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_info release_info;

	if (k_ioctl->size != sizeof(release_info))
		return -SYNX_INVALID;

	if (copy_from_user(&release_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	return synx_release(session, release_info.synx_obj);
}

static int synx_handle_release_n(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	int result = SYNX_SUCCESS;
	struct synx_release_n_info arr_info = {0};
	struct synx_release_n_params params = {0};
	u32 idx = 0;
	struct synx_release_indv_info *arr = NULL;

	if (k_ioctl->size != sizeof(arr_info))
		return -SYNX_INVALID;

	if (copy_from_user(&arr_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	if (arr_info.type == SYNX_RELEASE_ARR_PARAMS) {
		if (arr_info.arr.num_objs >= SYNX_MAX_OBJS || arr_info.arr.num_objs == 0
			|| arr_info.arr.list == 0)
			return -SYNX_INVALID;

		arr = kcalloc(arr_info.arr.num_objs,
					sizeof(*arr), GFP_KERNEL);
		if (IS_ERR_OR_NULL(arr))
			return -ENOMEM;

		if (copy_from_user(arr,
				u64_to_user_ptr(arr_info.arr.list),
				sizeof(*arr) * arr_info.arr.num_objs)) {
			kfree(arr);
			return -EFAULT;
		}

		params.type = SYNX_RELEASE_ARR_PARAMS;
		params.arr.num_objs = arr_info.arr.num_objs;
		params.arr.list = kcalloc(params.arr.num_objs,
					sizeof(struct synx_release_indv_params), GFP_KERNEL);
		if (IS_ERR_OR_NULL(params.arr.list)) {
			kfree(arr);
			return -ENOMEM;
		}

		for (idx = 0; idx < params.arr.num_objs; idx++) {
			params.arr.list[idx].h_synx = arr[idx].synx_obj;
			params.arr.list[idx].result = -SYNX_INVALID;
		}

		result = synx_release_n(session, &params);
		if (result != SYNX_SUCCESS)
			dprintk(SYNX_ERR, "synx_release_n failed %d", result);

		for (idx = 0; idx < params.arr.num_objs; idx++) {
			arr[idx].status = params.arr.list[idx].result;
			dprintk(SYNX_DBG, "Handle: %u Release status: %d\n",
				params.arr.list[idx].h_synx, params.arr.list[idx].result);
		}

		if (copy_to_user(u64_to_user_ptr(arr_info.arr.list),
			arr,
			sizeof(*arr) * arr_info.arr.num_objs)) {
			result = -EFAULT;
			dprintk(SYNX_ERR, "Copy to user failed for batch release.");
		}

		kfree(arr);
		kfree(params.arr.list);
	} else if (arr_info.type == SYNX_RELEASE_INDV_PARAMS) {
		params.type = SYNX_RELEASE_INDV_PARAMS;
		params.indv.h_synx = arr_info.indv.synx_obj;
		params.indv.result = -SYNX_INVALID;

		result = synx_release_n(session, &params);
		if (result != SYNX_SUCCESS) {
			dprintk(SYNX_ERR,
				"synx_release_n failed %d for indv handle %u",
				result, params.indv.h_synx);
		} else {
			dprintk(SYNX_VERB,
				"synx_release_n success for indv handle %u",
				params.indv.h_synx);
		}
	} else {
		dprintk(SYNX_ERR, "Invalid type passed %d", arr_info.type);
		result = -SYNX_INVALID;
	}
	return result;
}

static int synx_handle_get_fence(struct synx_private_ioctl_arg *k_ioctl,
	struct synx_session *session)
{
	struct synx_fence_fd fence_fd;
	struct dma_fence *fence;

	if (k_ioctl->size != sizeof(fence_fd))
		return -SYNX_INVALID;

	if (copy_from_user(&fence_fd,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	fence = synx_get_fence(session, fence_fd.synx_obj);
	if (IS_ERR_OR_NULL(fence))
		return -SYNX_INVALID;
	fence_fd.fd = synx_create_sync_fd(fence);
	/*
	 * release additional reference taken in synx_get_fence.
	 * additional reference ensures the fence is valid and
	 * does not race with handle/fence release.
	 */
	dma_fence_put(fence);

	if (copy_to_user(u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&fence_fd, k_ioctl->size))
		return -EFAULT;

	return SYNX_SUCCESS;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int synx_handle_recover(struct synx_private_ioctl_arg *k_ioctl, struct synx_session *session)
{
	struct synx_recover_info recover_info;

	if (k_ioctl->size != sizeof(recover_info))
		return -SYNX_INVALID;

	if (copy_from_user(&recover_info,
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = synx_recover(session->type);

	return k_ioctl->result;
}
#endif /* CONFIG_DEBUG_FS */

long synx_ioctl(struct file *filep,
	unsigned int cmd,
	unsigned long arg)
{
	s32 rc = 0;
	struct synx_private_ioctl_arg k_ioctl;
	struct synx_session *session = filep->private_data;

	if (cmd != SYNX_PRIVATE_IOCTL_CMD) {
		dprintk(SYNX_ERR, "invalid ioctl cmd\n");
		return -ENOIOCTLCMD;
	}

	if (copy_from_user(&k_ioctl,
			(struct synx_private_ioctl_arg *)arg,
			sizeof(k_ioctl))) {
		dprintk(SYNX_ERR, "invalid ioctl args\n");
		return -EFAULT;
	}

	if (!k_ioctl.ioctl_ptr)
		return -SYNX_INVALID;

	if (IS_ERR_OR_NULL(session) && k_ioctl.id != SYNX_INITIALIZE) {
		dprintk(SYNX_ERR, "session is not initialized\n");
		return -SYNX_INVALID;
	}

	dprintk(SYNX_VERB, "Enter cmd %u from pid %d\n",
		k_ioctl.id, current->pid);

	switch (k_ioctl.id) {
	case SYNX_INITIALIZE:
		rc = synx_handle_initialize(&k_ioctl, &session);
		filep->private_data = session;
		break;
	case SYNX_INITIALIZE_V3:
		rc = synx_handle_initialize_v3(&k_ioctl, &session);
		filep->private_data = session;
		break;
	case SYNX_CREATE:
		rc = synx_handle_create(&k_ioctl, session);
		break;
	case SYNX_RELEASE:
		rc = synx_handle_release(&k_ioctl, session);
		break;
	case SYNX_RELEASE_N:
		rc = synx_handle_release_n(&k_ioctl, session);
		break;
	case SYNX_REGISTER_PAYLOAD:
		rc = synx_handle_async_wait(&k_ioctl,
				session);
		break;
	case SYNX_DEREGISTER_PAYLOAD:
		rc = synx_handle_cancel_async_wait(&k_ioctl,
				session);
		break;
	case SYNX_SIGNAL:
		rc = synx_handle_signal(&k_ioctl, session);
		break;
	case SYNX_MERGE:
		rc = synx_handle_merge(&k_ioctl, session);
		break;
	case SYNX_MERGE_N:
		rc = synx_handle_merge_n(&k_ioctl, session);
		break;
	case SYNX_WAIT:
		rc = synx_handle_wait(&k_ioctl, session);
		if (copy_to_user((void *)arg,
			&k_ioctl,
			sizeof(k_ioctl))) {
			dprintk(SYNX_ERR, "invalid ioctl args\n");
			rc = -EFAULT;
		}
		break;
	case SYNX_BIND:
		rc = synx_handle_bind(&k_ioctl, session);
		break;
	case SYNX_GETSTATUS:
		rc = synx_handle_getstatus(&k_ioctl, session);
		break;
	case SYNX_IMPORT:
		rc = synx_handle_import(&k_ioctl, session);
		break;
	case SYNX_IMPORT_V2:
		rc = synx_handle_import_v2(&k_ioctl, session);
		break;
	case SYNX_IMPORT_ARR:
		rc = synx_handle_import_arr(&k_ioctl, session);
		break;
	case SYNX_IMPORT_ARR_V2:
		rc = synx_handle_import_arr_v2(&k_ioctl, session);
		break;
	case SYNX_EXPORT:
		rc = synx_handle_export(&k_ioctl, session);
		break;
	case SYNX_GETFENCE_FD:
		rc = synx_handle_get_fence(&k_ioctl, session);
		break;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	case SYNX_RECOVER:
		rc = synx_handle_recover(&k_ioctl, session);
		break;
#endif /* CONFIG_DEBUG_FS */
	default:
		rc = -SYNX_INVALID;
	}

	dprintk(SYNX_VERB, "exit with status %d\n", rc);

	return rc;
}

int synx_open(struct inode *inode, struct file *filep)
{
	dprintk(SYNX_VERB, "Enter pid: %d\n", current->pid);
	filep->private_data = NULL;

	return 0;
}

int synx_close(struct inode *inode, struct file *filep)
{
	struct synx_session *session = filep->private_data;

	return synx_uninitialize(session);
}
