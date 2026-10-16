/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _CAM_WORKER_WRAPPER_H_
#define _CAM_WORKER_WRAPPER_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/timer.h>

#include "cam_workq_util.h"
#include "cam_tasklet_util.h"
#include "cam_kthread_util.h"
#include "cam_worker_wrapper_api.h"

/**
 * enum cam_worker_wrapper_type
 * @codes: To identify worker wrapper in which type
 */
enum cam_worker_wrapper_type {
	WORKER_TYPE_INVALID,
	WORKER_TYPE_WORKQ,
	WORKER_TYPE_TASKLET,
	WORKER_TYPE_KTHREAD,
};

/**
 * struct cam_workq_create_args
 * @brief:      Parameters to create a workqueue
 * @name:       Name of the workque to be allocated, it is the combination
 *              of session handle and link handle
 * @num_tasks:  Number of tasks to be allocated for workq
 * @max_active: Number of maximum active works in the workq
 * @in_irq:     Set to one if workq might be used in irq context
 * @flag:       Bitwise OR of Flags for workq behavior.
 *              e.g. CAM_REG_MGR_WORKQ_HIGH_PRIORITY | CAM_REQ_MGR_WORKQ_SERIAL
 */
struct cam_workq_create_args {
	char                  *name;
	int32_t                num_tasks;
	uint32_t               max_active;
	enum cam_workq_context in_irq;
	int                    flag;
};

/**
 * struct cam_tasklet_create_args
 * @brief:      Parameters to create a tasklet
 * @priv_data:  Private data that will be passed to the handler
 * @idx:        Index of tasklet used to identify
 */
struct cam_tasklet_create_args {
	void     *priv_data;
	uint32_t  idx;
};

/**
 * struct cam_kthread_create_args
 * @brief:     Parameters to create a kthread worker
 * @name:      Name of the kthread to be alloced
 * @num_task:  Number of tasks to be allocated for kthread
 * @in_irq:    Indicate it works at IRQ context or not
 */
struct cam_kthread_create_args {
	char                    *name;
	int32_t                  num_tasks;
	enum cam_kthread_context in_irq;
};

/**
 * struct cam_worker_wrapper_ctx
 * @worker_type:  To identify which type of wrapper worker
 * @u:            Union of workq or tasklet
 * @workq:        To identify worker wrapper task in workq type
 * @tasklet:      To identify worker wrapper task in tasklet type
 * @kthread:      To identify worker wrapper task in kthread type
 */
struct cam_worker_wrapper_ctx {
	enum cam_worker_wrapper_type  worker_type;
	union {
		struct cam_core_workq   *workq;
		void                    *tasklet;
		struct cam_core_kthread *kthread;
	} u;
};

/**
 * struct cam_worker_wrapper_create_args
 * @brief:               Parameters to create a wrapper worker
 * @worker_ctx:          Wrapper worker ctx
 * @workq_create_para:   Parameters to create workq within worker
 * @tasklet_create_para: Parameters to create tasklet within worker
 * @kthread_create_para: Parameters to create kthread within worker
 */
struct cam_worker_wrapper_create_args {
	struct cam_worker_wrapper_ctx *worker_ctx;
	union {
		struct cam_workq_create_args   workq_create_para;
		struct cam_tasklet_create_args tasklet_create_para;
		struct cam_kthread_create_args kthread_create_para;
	} u;
};

#endif
