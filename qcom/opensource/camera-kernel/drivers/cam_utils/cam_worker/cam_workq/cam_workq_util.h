/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CAM_WORKQ_UTIL_H_
#define _CAM_WORKQ_UTIL_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/timer.h>

/* Threshold for scheduling delay in ms */
#define CAM_WORKQ_SCHEDULE_TIME_THRESHOLD   5

/* Threshold for execution delay in ms */
#define CAM_WORKQ_EXE_TIME_THRESHOLD        10

/* Flag to create a high priority workq */
#define CAM_WORKQ_FLAG_HIGH_PRIORITY             (1 << 0)

/*
 * This flag ensures only one task from a given
 * workq will execute at any given point on any
 * given CPU.
 */
#define CAM_WORKQ_FLAG_SERIAL                    (1 << 1)

/* Task priorities, lower the number higher the priority*/
enum cam_workq_task_priority {
	CAM_WORKQ_TASK_PRIORITY_0,
	CAM_WORKQ_TASK_PRIORITY_1,
	CAM_WORKQ_TASK_PRIORITY_MAX,
};

/* workqueue will be used from irq context or not */
enum cam_workq_context {
	CAM_WORKQ_USAGE_NON_IRQ,
	CAM_WORKQ_USAGE_IRQ,
	CAM_WORKQ_USAGE_MAX,
};

/** struct cam_workq_task
 * @priority         : caller can assign priority to task based on type.
 * @payload          : depending of user of task this payload type will change
 * @process_cb       : registered callback called by workq when task enqueued is
 *                     ready for processing in workq thread context
 * @parent           : workq's parent is link which is enqqueing taks to this workq
 * @entry            : list head of this list entry is worker's empty_head
 * @cancel           : if caller has got free task from pool but wants to abort
 *                     or put back without using it
 * @priv             : when task is enqueuer caller can attach priv along which
 *                     it will get in process callback
 * @ret              : return value in future to use for blocking calls
 * @task_scheduled_ts: enqueue time of task
 */
struct cam_workq_task {
	int32_t                    priority;
	int32_t                    ret;
	void                      *payload;
	int32_t                  (*process_cb)(void *priv, void *data);
	void                      *parent;
	struct list_head           entry;
	uint8_t                    cancel;
	void                      *priv;
	ktime_t                    task_scheduled_ts;
};

/** struct cam_core_workq
 * @work              : work token used by workqueue
 * @job               : workqueue internal job struct
 * @lock_bh           : lock for task structs
 * @in_irq            : set true if workque can be used in irq context
 * @flush             : used to track if flush has been called on workqueue
 * @work_q_name       : name of the workq
 * @workq_scheduled_ts: enqueue time of workq
 * task -
 * @pending_cnt       : Number of tasks left in queue
 * @free_cnt          : Number of free/available tasks
 * @process_head      : List of tasks enqueued to be executed
 * @empty_head        : list  head of available taska which can be used
 *                      or acquired in order to enqueue a task to workq
 * @pool              : pool of tasks used for handling events in workq context
 * @num_task          : size of tasks pool
 */
struct cam_core_workq {
	struct work_struct         work;
	struct workqueue_struct   *job;
	spinlock_t                 lock_bh;
	uint32_t                   in_irq;
	ktime_t                    workq_scheduled_ts;
	atomic_t                   flush;
	char                       workq_name[128];

	/* tasks */
	struct {
		atomic_t               pending_cnt;
		atomic_t               free_cnt;

		struct list_head       process_head[CAM_WORKQ_TASK_PRIORITY_MAX];
		struct list_head       empty_head;
		struct cam_workq_task *pool;
		uint32_t               num_task;
	} task;
};

/**
 * cam_workq_process() - main loop handling
 * @w: workqueue task pointer
 */
void cam_workq_process(struct work_struct *w);

/**
 * cam_workq_create()
 * @brief      : create a workqueue
 * @name       : Name of the workque to be allocated, it is combination
 *               of session handle and link handle
 * @num_task   : Num_tasks to be allocated for workq
 * @max_active : Number of maximum ative works
 * @workq      : Double pointer worker
 * @in_irq     : Set to one if workq might be used in irq context
 * @flags      : Bitwise OR of Flags for workq behavior.
 *               e.g. CAM_WORKQ_FLAG_HIGH_PRIORITY | CAM_WORKQ_FLAG_SERIAL
 * @func       : function pointer for cam_workq_process wrapper function
 * This function will allocate and create workqueue and pass
 * the workq pointer to caller.
 */
int cam_workq_create(char *name, int32_t num_tasks, uint32_t max_active,
	struct cam_core_workq **workq, enum cam_workq_context in_irq,
	int flags, void (*func)(struct work_struct *w));

/**
 * cam_workq_destroy()
 * @brief: destroy workqueue
 * @workq: pointer to worker data struct
 * this function will destroy workqueue and clean up resources
 * associated with worker such as tasks.
 */
void cam_workq_destroy(struct cam_core_workq **workq);

/**
 * cam_workq_enqueue_task()
 * @brief: Enqueue task in worker queue
 * @task : task to be processed by worker
 * @priv : clients private data
 * @prio : task priority
 * process callback func
 */
int cam_workq_enqueue_task(struct cam_workq_task *task,
	void *priv, int32_t prio);

/**
 * cam_workq_get_task()
 * @brief: Returns empty task pointer for use
 * @workq: workque used for processing
 */
struct cam_workq_task *cam_workq_get_task(
	struct cam_core_workq *workq);

/**
 * cam_workq_get_task_payload()
 *
 * @brief      : Get payload of the worker task
 * @workq      : Pointer to workq struct
 * @workq_task : Workq task used for processing
 */
void *cam_workq_get_task_payload(struct cam_core_workq *workq,
	struct cam_workq_task *workq_task);

/**
 * cam_workq_flush()
 *
 * @brief: Flushes the work queue. Function will sleep until any active task is complete.
 * @workq: pointer to worker data struct
 */
void cam_workq_flush(struct cam_core_workq *workq);

#endif
