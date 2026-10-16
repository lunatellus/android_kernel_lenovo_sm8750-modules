/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _DRIVERS_CRYPTO_FE_PARSE_H_
#define _DRIVERS_CRYPTO_FE_PARSE_H_

#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/types.h>
#include <linux/habmm.h>

#define HAB_HANDLE_NUM 6

/* Command IDs for GPCE backend communication */
enum gpce_cmd_id {
	MSM_GPCE_SMMU_VM_CMD_MAP,
	MSM_GPCE_SMMU_VM_CMD_UNMAP,
	MSM_GPCE_CIPHER_CMD,
};

struct qcedev_fe_ion_buf_info {
	u64        smmu_device_address;
	unsigned long mapped_buf_size;
	int buf_ion_fd;
	u32 export_id;
	u32 is_secure;
	struct dma_buf *dma_buf;
};

struct qcedev_fe_reg_buf_info {
	struct list_head list;
	union {
		struct qcedev_fe_ion_buf_info ion_buf;
	};
	atomic_t ref_count;
};

struct qcedev_fe_buffer_list {
	struct list_head list;
	struct mutex lock;
};

struct qcedev_fe_handle {
	/* qcedev mapped buffer list */
	struct qcedev_fe_buffer_list registeredbufs;
};

struct qce_fe_hab_handle {
	uint32_t handle;
	bool in_use;
};

struct qce_fe_drv_hab_handles {
	struct qce_fe_hab_handle qce_fe_hab_handles[HAB_HANDLE_NUM];
	spinlock_t  handle_lock;
	bool  initialized;
};
#endif //_DRIVERS_CRYPTO_FE_PARSE_H_

