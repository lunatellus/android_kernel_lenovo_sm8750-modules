/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __QSEECOM_KERNEL_H_
#define __QSEECOM_KERNEL_H_

#include <linux/types.h>


#define QSEECOM_ALIGN_SIZE	0x40
#define QSEECOM_ALIGN_MASK	(QSEECOM_ALIGN_SIZE - 1)
#define QSEECOM_ALIGN(x)	\
	((x + QSEECOM_ALIGN_MASK) & (~QSEECOM_ALIGN_MASK))

/*
 * struct qseecom_handle -
 *      Handle to the qseecom device for kernel clients
 * @sbuf - shared buffer pointer
 * @sbbuf_len - shared buffer size
 */
struct qseecom_handle {
	void *dev; /* in/out */
	unsigned char *sbuf; /* in/out */
	uint32_t sbuf_len; /* in/out */
};

int qseecom_start_app(struct qseecom_handle **handle,
						char *app_name, uint32_t size);
int qseecom_shutdown_app(struct qseecom_handle **handle);
int qseecom_send_command(struct qseecom_handle *handle, void *send_buf,
			uint32_t sbuf_len, void *resp_buf, uint32_t rbuf_len);

int qseecom_set_bandwidth(struct qseecom_handle *handle, bool high);
int qseecom_process_listener_from_smcinvoke(uint32_t *result,
					u64 *response_type, unsigned int *data);

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
int qseecom_create_key_in_slot(uint8_t usage_code, uint8_t key_slot,
			       const uint8_t *key_id, const uint8_t *inhash32);
#endif
#endif /* __QSEECOM_KERNEL_H_ */
