/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _CAM_WORKER_WRAPPER_API_H_
#define _CAM_WORKER_WRAPPER_API_H_

#include "cam_irq_controller.h"

#define CAM_WORKER_FLAG_SERIAL          (1 << 1)
#define CAM_WORKER_FLAG_UNBOUND         (1 << 2)
#define CAM_WORKER_FLAG_MEM_RECLAIM     (1 << 3)
#define CAM_WORKER_FLAG_HIGHPRI         (1 << 4)
#define CAM_WORKER_FLAG_SYSFS           (1 << 5)

/* Threshold for scheduling delay in ms */
#define CAM_WORKER_SCHEDULE_TIME_THRESHOLD 5

/**
 * enum cam_worker_wrapper_usage_irq
 *
 * @codes: used from irq context or not
 */
enum cam_worker_wrapper_usage_irq {
	WORKER_USAGE_NON_IRQ,
	WORKER_USAGE_IRQ,
	WORKER_USAGE_MAX,
};

/**
 * enum cam_worker_wrapper_task_priority
 *
 * @codes: Task priorities, lower the number higher the priority
 */
enum cam_worker_wrapper_task_priority {
	WORKER_TASK_PRIORITY_0,
	WORKER_TASK_PRIORITY_1,
	WORKER_TASK_PRIORITY_MAX,
};

/**
 * enum cam_worker_wrapper_class_type
 *
 * @codes: to identify worker wrapper class in which type
 */
enum cam_worker_wrapper_class_type {
	WORKER_CLASS_INVALID,
	WORKER_CLASS_RT,
	WORKER_CLASS_NRT,
};

/**
 * struct cam_worker_wrapper_taskdata_args
 *
 * @brief:          Taskdata containing worker type and related
 *                  task data
 * @task_priority:  Worker Task priority
 * @task_data:      Taskdata for specific worker content
 */
struct cam_worker_wrapper_taskdata_args {
	enum cam_worker_wrapper_task_priority task_priority;
	void                                 *task_data;
};

/**
 * struct cam_worker_wrapper_init_args
 *
 * @brief:              Parameters for worker wrapper initialization
 * @name:               Name of the worker to be allocated,
 * @num_tasks:          Num_tasks to be allocated for worker
 * @max_active:         Number of maximum active works
 * @in_irq:             Set to one if worker might be used in irq context
 * @flag:               Bitwise OR of Flags for worker behavior.
 * @priv_data:          Priv data of hw mgr
 * @index:              Index of hw mgr
 * @worker_ctx_priv:    Wrapper worker ctx
 */
struct cam_worker_wrapper_init_args {
	char                                *name;
	int32_t                              num_tasks;
	uint32_t                             max_active;
	enum cam_worker_wrapper_usage_irq    in_irq;
	int                                  flag;
	void                                *priv_data;
	uint32_t                             index;
	void                               **worker_ctx_priv;
};

/**
 * struct cam_worker_wrapper_mini_dump
 *
 * @worker_scheduled_ts: Scheduled ts
 * task -
 * @pending_cnt:         Pending count of tasks left in worker
 * @free_cnt:            Free count of free/available tasks
 * @num_task:            Size of tasks pool
 */
struct cam_worker_wrapper_mini_dump {
	ktime_t       worker_scheduled_ts;
	/* task */
	struct {
		uint32_t  pending_cnt;
		uint32_t  free_cnt;
		uint32_t  num_task;
	} task;
};

/**
 * cam_worker_wrapper_init()
 *
 * @brief:                      Initialize a wrapper worker
 * @worker_init_args:           Worker wrapper initialization parameters
 * @worker_class_type:          Indicate RT/NRT wrapper worker class
 * @return:                     0-Success
 *                              Negative-Failure
 */
int cam_worker_wrapper_init(
	struct cam_worker_wrapper_init_args *worker_init_args,
	enum cam_worker_wrapper_class_type   worker_class_type);

/**
 * cam_worker_wrapper_deinit()
 *
 * @brief:              Destroy a wrapper worker
 * @worker_ctx_priv:    Wrapper worker ctx
 * @return:             Void
 */
void cam_worker_wrapper_deinit(
	void *worker_ctx_priv);

/**
 * cam_worker_wrapper_start()
 *
 * @brief:              Enable or disable worker schedule for task run
 * @worker_ctx_priv:    Wrapper worker ctx
 * @return:             0-Success
 *                      Negative-Failure
 */
int cam_worker_wrapper_start(
	void *worker_ctx_priv);

/**
 * cam_worker_wrapper_flush()
 *
 * @brief:              Flush the worker.
 * @worker_ctx_priv:    Wrapper worker ctx
 */
void cam_worker_wrapper_flush(
	void *worker_ctx_priv);

/**
 * cam_worker_wrapper_get()
 *
 * @brief:              Get task and key taskdata based on worker wrapper
 *                      ctx, update task info in taskdata and return.
 * @worker_ctx_priv:    Wrapper worker ctx
 * @taskdata:           Taskdata w.r.t. specific worker task.
 * @return:             0-Success
 *                      Negative-Failure
 */
int cam_worker_wrapper_get(
	void *worker_ctx_priv,
	void *taskdata);

/**
 * cam_worker_wrapper_put()
 *
 * @brief:              Release task and key taskdata based on worker wrapper
 *                      ctx, update task info in taskdata and return.
 * @worker_ctx_priv:    Wrapper worker ctx
 * @taskdata:           Taskdata w.r.t. specific worker task.
 * @return:             Void
 */
void cam_worker_wrapper_put(
	void *worker_ctx_priv,
	void *taskdata);

/**
 * cam_worker_wrapper_enqueue()
 *
 * @brief:                 Enqueue task within payload and process task
 * @worker_ctx_priv:       Wrapper worker ctx
 * @taskdata:              Taskdata w.r.t. specific worker task.
 * @handler_priv:          Private data from client side
 * @evt_payload_priv:      Worker task payload
 * @bottom_half_handler:   Callback function
 * @return:                0-Success
 *                         Negative-Failure
 */
int cam_worker_wrapper_enqueue(
	void                       *worker_ctx_priv,
	void                       *taskdata,
	void                       *handler_priv,
	void                       *evt_payload_priv,
	CAM_IRQ_HANDLER_BOTTOM_HALF bottom_half_handler);

/**
 * cam_worker_wrapper_get_task_payload()
 *
 * @brief:               Get payload based on wrapper worker and provide to taskdata
 * @taskdata:            Taskdata including task priority info
 * @return:              Void
 */
void *cam_worker_wrapper_get_task_payload(
	void                                    *worker_ctx_priv,
	struct cam_worker_wrapper_taskdata_args *taskdata);

/**
 * cam_worker_wrapper_payload_bind()
 *
 * @brief:               Get payload based on wrapper worker and provide to taskdata.
 * @worker_ctx_priv:     Wrapper worker ctx
 * @work_data:           Taskdata w.r.t. specific worker task
 * @work_data_idx:       Index of the taskdata passed from client
 * @return:              0-Success
 *                       Negative-Failure
 */
int cam_worker_wrapper_payload_bind(
	void *worker_ctx_priv,
	void *work_data,
	int   work_data_idx);

/**
 * cam_worker_wrapper_dump_info_cb()
 *
 * @brief:               Dump some worker data info to client
 * @worker_ctx_priv:     Wrapper worker ctx
 * @worker_dump_info:    Worker dump info details
 * @return:              Void
 */
void cam_worker_wrapper_dump_info_cb(
	void                                *worker_ctx_priv,
	struct cam_worker_wrapper_mini_dump *worker_dump_info);

/**
 * cam_worker_wrapper_prop_update_init()
 *
 * @brief:               Create debugfs and corresponding worker for thread property update
 * @return:              0-Success
 *                       Negative-Failure
 */
int cam_worker_wrapper_prop_update_init(void);

/**
 * cam_worker_wrapper_prop_update_deinit()
 *
 * @brief:               Remove debugfs and corresponding worker for thread property update
 * @return:              Void
 */
void cam_worker_wrapper_prop_update_deinit(void);

#endif
