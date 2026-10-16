/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DRIVERS_QCEDEV_FE_VIRT_H_
#define _DRIVERS_QCEDEV_FE_VIRT_H_

#include <linux/types.h>
#include <linux/dma-buf.h>
#include "qcedev_fe.h"

/* GPCE size definitions */
#define GPCE_MAX_BUFFERS           16
#define GPCE_MAX_IV_SIZE           32
#define GPCE_MAX_MAC_SIZE          32
#define GPCE_AES_KEY_256           32
#define GPCE_MAX_HWKM_KEY_SIZE     256

/* SMMU command structures */
struct msm_gpce_smmu_vm_map_cmd {
	int cmd_id;
	u32 export_id;
	u32 buf_size;
	void *mem_handle;
	u32 is_secure;
} __packed;

struct msm_gpce_smmu_vm_map_cmd_rsp {
	int status;
	u64 addr;
} __packed;

struct msm_gpce_smmu_vm_unmap_cmd {
	int cmd_id;
	u32 export_id;
	void *unused; // for alignment with the host
	u32 is_secure;
} __packed;

struct msm_gpce_smmu_vm_unmap_cmd_rsp {
	int status;
} __packed;

/* Buffer info structure for GPCE backend */
struct gpce_buf_info {
    union {
        u32 offset;
        u64 vaddr;
    };
	u32 len;
} __packed;

/* Virtual buffer info for GPCE backend */
struct gpce_vbuf_info {
	struct gpce_buf_info src[GPCE_MAX_BUFFERS];
	struct gpce_buf_info dst[GPCE_MAX_BUFFERS];
} __packed;

/* Pattern info for GPCE backend */
struct gpce_pattern_info {
	u8 patt_sz;
	u8 proc_data_sz;
	u8 patt_offset;
} __packed;

/* Cipher request structure matching gpce_decrypt_req_t from backend */
struct gpce_cipher_req {
	int cmd_id;
	struct gpce_vbuf_info vbuf;
	u32 entries;
	u32 data_len;
	u32 in_place_op;
	u32 encklen;
	u8 iv[GPCE_MAX_IV_SIZE];
	u32 ivlen;
	u32 iv_ctr_size;
	u32 byteoffset;
	u8 block_offset;
	u8 is_pattern_valid;
	u8 is_copy_op;
	u8 encrypt;
	u8 mac[GPCE_MAX_MAC_SIZE];
	u32 mac_len;
	u8 key[GPCE_AES_KEY_256];
	u32 key_size;
	u8 hwkm_key[GPCE_MAX_HWKM_KEY_SIZE];
	u32 hwkm_key_size;
	u32 gcm_byte_offset;
	u32 is_secure_in;
	u32 is_secure_out;
	struct gpce_buf_info aad;
	u32 aad_rpt;
	struct gpce_pattern_info pattern_info;
	u32 alg;   /* GPCE_CIPHER_ALG */
	u32 mode;  /* GPCE_CIPHER_MODE */
	u32 op;    /* GPCE_OPERATION_TYPE */
	u32 err;   /* GPCE_ERROR */
	u32 key_index;
} __packed;

/* Cipher response structure matching gpce_decrypt_rsp_t from backend */
struct gpce_cipher_rsp {
	int status;
	u8 iv[GPCE_MAX_IV_SIZE];  /* Updated IV after crypto operation */
	u32 ivlen;
} __packed;

/**
 * qcedev_fe_send_cipher_req() - Send cipher request to backend VM
 * @cipher_req: Pointer to cipher request structure
 * @cipher_rsp: Pointer to cipher response structure
 * @drv_handles: Pointer to HAB handles structure
 *
 * This function sends a cipher operation request to the backend VM via HAB
 * and waits for the response.
 *
 * Return: 0 on success, negative error code on failure
 */
int qcedev_fe_send_cipher_req(struct gpce_cipher_req *cipher_req,
		struct gpce_cipher_rsp *cipher_rsp,
		struct qce_fe_drv_hab_handles *drv_handles);

/* SMMU/Buffer management functions */
int qcedev_check_and_map_buffer(void *qce_hndl,
		int fd, unsigned int offset, unsigned int fd_size,
		unsigned long long *vaddr, struct qce_fe_drv_hab_handles *drv_handles);
int qcedev_check_and_unmap_buffer(void *handle, int fd,
		struct qce_fe_drv_hab_handles *drv_handles);
int qcedev_unmap_all_buffers(void *handle, struct qce_fe_drv_hab_handles *drv_handles);

#endif /* _DRIVERS_QCEDEV_FE_VIRT_H_ */
