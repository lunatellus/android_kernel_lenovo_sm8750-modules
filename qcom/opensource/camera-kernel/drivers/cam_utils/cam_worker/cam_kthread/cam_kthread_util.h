/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _CAM_KTHREAD_UTIL_H_
#define _CAM_KTHREAD_UTIL_H_

#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/init.h>
#include<linux/sched.h>
#include<linux/slab.h>
#include<linux/timer.h>
#include<linux/kthread.h>
#include<linux/cpumask.h>
#include<uapi/linux/sched/types.h>

/* Kthread scheduling latency threshold in ms */
#define CAM_KTHREAD_SCHEDULE_TIME_THRESHOLD   5

/* Kthread execution latency threshold in ms */
#define CAM_KTHREAD_EXE_TIME_THRESHOLD        10

/* Kthread priority update latency threshold in ms */
#define CAM_KTHREAD_PRIORITY_UPDATE_THRESHOLD 200

/* Task priorities, lower the number higher the priority */
enum cam_kthread_task_priority {
	CAM_KTHREAD_TASK_PRIORITY_0,
	CAM_KTHREAD_TASK_PRIORITY_1,
	CAM_KTHREAD_TASK_PRIORITY_MAX,
};

/* kthread will be used from irq context or not */
enum cam_kthread_context {
	CAM_KTHREAD_USAGE_NON_IRQ,
	CAM_KTHREAD_USAGE_IRQ,
	CAM_KTHREAD_USAGE_MAX,
};

/**
 * struct cam_kthread_task
 *
 * @priority   : Caller can assign priority to task based on type.
 * @payload    : Depending of user of task this payload type will change
 * @process_cb : Registered callback called by worker when task enqueued is
 *               ready for processing in worker thread context
 * @parent     : Worker parent is link which is enqueuing taks to this kthread
 * @entry      : List head of this list entry is worker's empty_head
 * @cancel     : If caller has got free task from pool but wants to abort
 *               or put back without using it
 * @priv       : When task is enqueued, caller can attach priv along which
 *               it will get in process callback
 */
struct cam_kthread_task {
	int32_t             priority;
	void               *payload;
	int32_t           (*process_cb)(void *priv, void *data);
	void               *parent;
	struct list_head    entry;
	uint8_t             cancel;
	void               *priv;
};

/**
 * struct cam_core_kthread
 *
 * @work                : Work token used by kthread
 * @job                 : Kthread worker
 * @lock_bh             : Lock for task structs
 * @mutex_lock          : Mutex lock for task structs
 * @in_irq              : Kthread context, non zero value if kthread is used in irq context
 * @worker_scheduled_ts : Enqueue time of worker
 * @flush_in_process    : Used to track if flush has been called on kthread
 * @worker_name         : Name of the worker
 * @worker_completion   : Completion info
 * task -
 * @pending_cnt         : Number of tasks left in queue
 * @free_cnt            : Number of free/available tasks
 * @process_head        : List of tasks enqueued to be executed
 * @empty_head          : List head of available task which can be used
 *                        or acquired in order to enqueue a task to worker
 * @pool                : Pool of tasks used for handling events in worker context
 * @num_task            : Size of tasks pool
 */
struct cam_core_kthread {
	struct kthread_work      work;
	struct kthread_worker   *job;
	spinlock_t               lock_bh;
	struct mutex             mutex_lock;
	enum cam_kthread_context in_irq;
	ktime_t                  worker_scheduled_ts;
	atomic_t                 flush_in_process;
	char                     worker_name[128];
	struct completion        worker_completion;

	/* task */
	struct {
		atomic_t                 pending_cnt;
		atomic_t                 free_cnt;

		struct list_head         process_head[CAM_KTHREAD_TASK_PRIORITY_MAX];
		struct list_head         empty_head;
		struct cam_kthread_task *pool;
		uint32_t                 num_task;
	} task;
};

/**
 * struct cam_kthread_data - Single node of information about a kthread worker
 *
 * @kthread_worker  : Kthread worker
 * @list            : List member used to append this node to a linked list
 */
struct cam_kthread_data {
	struct kthread_worker *kthread_worker;
	struct list_head       list;
};

/**
 * struct cam_kthread_info - Kthread scheduling information
 *
 * @policy               : Scheduling policy
 * @priority             : Scheduling priority
 * @nice                 : Nice value
 * @affinity             : Core affinity
 * @kthread_list_mutex   : Mutex for list operations
 * @is_list_initialized  : Bool to show if list is initialized
 * @kthread_list         : List of all created kthreads
 * @is_prop_valid        : Flag to indicate if properties are ready to be updated
 * @result               : Result of setting kthread properties
 */
struct cam_kthread_info {
	uint32_t            policy;
	uint32_t            priority;
	int32_t             nice;
	uint32_t            affinity;
	struct mutex        kthread_list_mutex;
	bool                is_list_initalized;
	struct list_head    kthread_list;
	bool                is_prop_valid;
	int32_t             result;
};

/**
 * cam_kthread_flush()
 *
 * @brief   : Flush the kthread queue. Function will sleep until any active task is complete
 * @kthread : Pointer to the kthread queue data struct
 */
void cam_kthread_flush(struct cam_core_kthread *kthread);

/**
 * cam_kthread_process()
 *
 * @brief : Main loop hanlding
 * @w     : Pointer to kthread task
 */
void cam_kthread_process(struct kthread_work *w);

/**
 * cam_kthread_get_task()
 *
 * @brief   : Return empty task pointer for use
 * @kthread : Kthread queue used for processing
 */
struct cam_kthread_task *cam_kthread_get_task(struct cam_core_kthread *kthread);

/**
 * cam_kthread_get_task_payload()
 *
 * @brief        : Get payload of the worker task
 * @kthread      : Pointer to kthread struct
 * @kthread_task : Kthread task used for processing
 */
void *cam_kthread_get_task_payload(struct cam_core_kthread *kthread,
	struct cam_kthread_task *kthread_task);

/**
 * cam_kthread_enqueue_task()
 *
 * @brief : Enqueue task in kthread queue
 * @task  : Kthread task to be used by worker
 * @priv  : Clients private data
 * @prio  : Task priority
 */
int cam_kthread_enqueue_task(struct cam_kthread_task *task, void *priv, int32_t prio);

/**
 * cam_kthread_create()
 *
 * @brief     : Create a kthread
 * @name      : Name of the kthread to be allocated, it is combination of session hdl
 *              and link handle
 * @num_tasks : Number of tasks to be allocated for kthread worker
 * @kthread   : Return pointer to the allocated kthread
 * @in_irq    : Indicate whether kthread will be handled during irq context or not
 */
int cam_kthread_create(char *name, int32_t num_tasks,
	struct cam_core_kthread **kthread, enum cam_kthread_context in_irq);

/**
 * cam_kthread_destroy()
 *
 * @brief       : Destroy kthread
 * @cam_kthread : Pointer to kthread data structure
 */
void cam_kthread_destroy(struct cam_core_kthread **cam_kthread);

/**
 * cam_kthread_property_update_init()
 *
 * @brief : Init debugfs and related kthread worker for kthread related property
 */
int cam_kthread_property_update_init(void);

/**
 * cam_kthread_property_update_deinit()
 *
 * @brief : Deinit debugfs and related kthread worker for kthread related property
 */
void cam_kthread_property_update_deinit(void);

#endif
