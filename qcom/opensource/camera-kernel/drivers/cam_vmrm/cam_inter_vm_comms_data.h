/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __CAM_INTER_VM_COMMS_DATA_H__
#define __CAM_INTER_VM_COMMS_DATA_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/workqueue.h>

#define CAM_INTER_VM_COMMS_WORKER_NAME        "cam_inter_vm_comms_worker"
#define CAM_INTER_VM_COMMS_MAX_PENDING_WORKS  5
#define CAM_INTER_VM_COMMS_NUM_TASK           100

/**
 * @brief cam_inter_vm_comms_handle
 *
 * @param comms_lock           : Mutex to synchronize the operations on the handle.
 * @param msg_buffer           : Buffer to hold the incoming messages.
 * @param msg_size             : The highest size of the messages exchanged.
 * @param message_cb           : Callback function to handle incoming messages.
 * @param is_comms_established : Indicates whether the two way comms has been established.
 * @param is_comms_terminated  : Indicates whether the communication has been terminated.
 * @param comms_protocol_data  : Data specific to the underlying communication protocol.
 * @param is_server_vm         : Indicates whether this handle corresponds to PVM.
 * @param worker_ctx           : Worker to process incoming messages.
 */
struct cam_inter_vm_comms_handle {
	struct mutex              comms_lock;
	void                     *msg_buffer;
	size_t                    msg_size;
	handle_message_cb         message_cb;
	bool                      is_comms_established;
	bool                      is_comms_terminated;
	void                     *comms_protocol_data;
	bool                      is_server_vm;
	void                     *worker_ctx;
};

#endif /*__CAM_INTER_VM_COMMS_DATA_H__*/
