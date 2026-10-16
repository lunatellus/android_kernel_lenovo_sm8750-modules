// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "cam_workq_util.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_mem_mgr_api.h"

#define WORKQ_ACQUIRE_LOCK(workq, flags) {\
	if ((workq)->in_irq) \
		spin_lock_irqsave(&(workq)->lock_bh, (flags)); \
	else \
		spin_lock_bh(&(workq)->lock_bh); \
}

#define WORKQ_RELEASE_LOCK(workq, flags) {\
	if ((workq)->in_irq) \
		spin_unlock_irqrestore(&(workq)->lock_bh, (flags)); \
	else	\
		spin_unlock_bh(&(workq)->lock_bh); \
}

struct cam_workq_task *cam_workq_get_task(
	struct cam_core_workq *workq)
{
	struct cam_workq_task *task = NULL;
	unsigned long flags = 0;

	if (!workq)
		return NULL;

	WORKQ_ACQUIRE_LOCK(workq, flags);
	if (list_empty(&workq->task.empty_head))
		goto end;

	task = list_first_entry(&workq->task.empty_head,
		struct cam_workq_task, entry);
	if (task) {
		atomic_sub(1, &workq->task.free_cnt);
		list_del_init(&task->entry);
	}

end:
	WORKQ_RELEASE_LOCK(workq, flags);

	return task;
}

static void cam_workq_put_task(struct cam_workq_task *task)
{
	struct cam_core_workq *workq =
		(struct cam_core_workq *)task->parent;
	unsigned long flags = 0;

	list_del_init(&task->entry);
	task->cancel = 0;
	task->process_cb = NULL;
	task->priv = NULL;
	WORKQ_ACQUIRE_LOCK(workq, flags);
	list_add_tail(&task->entry,
		&workq->task.empty_head);
	atomic_add(1, &workq->task.free_cnt);
	WORKQ_RELEASE_LOCK(workq, flags);
}

void *cam_workq_get_task_payload(struct cam_core_workq *workq,
	struct cam_workq_task *workq_task)
{
	void *payload;

	if (!workq) {
		CAM_ERR(CAM_WORKER, "Invalid workq");
		return NULL;
	}

	if (!workq_task) {
		CAM_ERR(CAM_WORKER, "Invalid workq task, worker name: %s",
			workq->workq_name);
		return NULL;
	}

	payload = workq_task->payload;
	if (!payload) {
		CAM_ERR(CAM_WORKER,
			"Invalid payload, bind payload should happen before get task payload, worker name: %s",
			workq->workq_name);
		cam_workq_put_task(workq_task);
	}

	return payload;
}

void cam_workq_flush(struct cam_core_workq *workq)
{
	if (!workq) {
		CAM_ERR(CAM_WORKER, "workq is null");
		return;
	}

	atomic_set(&workq->flush, 1);
	cancel_work_sync(&workq->work);
	atomic_set(&workq->flush, 0);
}

/**
 * cam_workq_process_task() - Process the enqueued task
 * @task: pointer to task workq thread shall process
 */
static int cam_workq_process_task(struct cam_workq_task *task)
{
	if (!task)
		return -EINVAL;

	if (task->process_cb)
		task->process_cb(task->priv, task->payload);
	else
		CAM_WARN(CAM_WORKER, "FATAL:no task handler registered for workq");

	return 0;
}

/**
 * cam_workq_process() - main loop handling
 * @w: workqueue task pointer
 */
void cam_workq_process(struct work_struct *w)
{
	struct cam_core_workq *workq = NULL;
	struct cam_workq_task         *task;
	int32_t                        i = CAM_WORKQ_TASK_PRIORITY_0;
	unsigned long                  flags = 0;
	ktime_t                        sched_start_time;
	void                          *cb = NULL;

	if (!w) {
		CAM_ERR(CAM_WORKER, "NULL task pointer can not schedule");
		return;
	}

	workq = (struct cam_core_workq *)
		container_of(w, struct cam_core_workq, work);

	while (i < CAM_WORKQ_TASK_PRIORITY_MAX) {
		WORKQ_ACQUIRE_LOCK(workq, flags);
		while (!list_empty(&workq->task.process_head[i])) {
			task = list_first_entry(&workq->task.process_head[i],
				struct cam_workq_task, entry);
			cb = (void *)task->process_cb;
			cam_common_util_thread_switch_delay_detect(
				workq->workq_name, "schedule", cb,
				task->task_scheduled_ts,
				CAM_WORKQ_SCHEDULE_TIME_THRESHOLD);
			sched_start_time = ktime_get_boottime();
			atomic_sub(1, &workq->task.pending_cnt);
			list_del_init(&task->entry);
			WORKQ_RELEASE_LOCK(workq, flags);
			if (!unlikely(atomic_read(&workq->flush)))
				cam_workq_process_task(task);
			/**
			 * When flush is set, we skip the client callback to avoid executing code
			 * during shutdown, but the task was already removed from process_head,
			 * so it must be returned to empty_head to keep the pool coherent.
			 * This ensures pending_cnt/free_cnt stay balanced and avoids
			 * transient leaks of tasks off any list.
			 */
			cam_workq_put_task(task);
			cam_common_util_thread_switch_delay_detect(
				workq->workq_name, "execution", cb,
				sched_start_time,
				CAM_WORKQ_SCHEDULE_TIME_THRESHOLD);
			CAM_DBG(CAM_WORKER, "processed task %pK free_cnt %d",
				task, atomic_read(&workq->task.free_cnt));
			WORKQ_ACQUIRE_LOCK(workq, flags);
		}
		WORKQ_RELEASE_LOCK(workq, flags);
		i++;
	}
}

int cam_workq_enqueue_task(struct cam_workq_task *task,
	void *priv, int32_t prio)
{
	int rc = 0;
	struct cam_core_workq *workq = NULL;
	unsigned long flags = 0;

	if (!task) {
		CAM_WARN(CAM_WORKER, "NULL task pointer can not schedule");
		return -EINVAL;
	}

	workq = (struct cam_core_workq *)task->parent;
	if (!workq) {
		CAM_DBG(CAM_WORKER, "NULL workq pointer suspect mem corruption");
		return -EINVAL;
	}

	if (task->cancel == 1 || atomic_read(&workq->flush)) {
		rc = 0;
		goto abort;
	}
	task->priv = priv;
	task->priority =
		(prio < CAM_WORKQ_TASK_PRIORITY_MAX && prio >= CAM_WORKQ_TASK_PRIORITY_0)
		? prio : CAM_WORKQ_TASK_PRIORITY_0;
	task->task_scheduled_ts = ktime_get_boottime();

	WORKQ_ACQUIRE_LOCK(workq, flags);
	if (!workq->job) {
		rc = -EINVAL;
		WORKQ_RELEASE_LOCK(workq, flags);
		goto abort;
	}

	list_add_tail(&task->entry,
		&workq->task.process_head[task->priority]);

	atomic_add(1, &workq->task.pending_cnt);
	CAM_DBG(CAM_WORKER, "enq task %pK pending_cnt %d",
		task, atomic_read(&workq->task.pending_cnt));

	queue_work(workq->job, &workq->work);
	WORKQ_RELEASE_LOCK(workq, flags);

	return rc;
abort:
	cam_workq_put_task(task);
	CAM_INFO(CAM_WORKER, "task aborted and queued back to pool");
	return rc;
}

int cam_workq_create(char *name, int32_t num_tasks, uint32_t max_active,
	struct cam_core_workq **workq, enum cam_workq_context in_irq,
	int flags, void (*func)(struct work_struct *w))
{
	int32_t i, wq_flags = 0;
	struct cam_workq_task *task;
	struct cam_core_workq *cam_workq = NULL;
	char buf[128] = "cam_workq-";

	if (!*workq) {
		cam_workq = CAM_MEM_ZALLOC(sizeof(struct cam_core_workq),
			GFP_KERNEL);
		if (cam_workq == NULL)
			return -ENOMEM;

		wq_flags |= WQ_UNBOUND;

		if (flags & CAM_WORKQ_FLAG_HIGH_PRIORITY)
			wq_flags |= WQ_HIGHPRI;

		if (flags & CAM_WORKER_FLAG_MEM_RECLAIM)
			wq_flags |= WQ_MEM_RECLAIM;

		if (flags & CAM_WORKER_FLAG_SYSFS)
			wq_flags |= WQ_SYSFS;

		strlcat(buf, name, sizeof(buf));
		CAM_DBG(CAM_WORKER, "create workque cam_workq-%s", name);
		if (flags & CAM_WORKQ_FLAG_SERIAL)
			cam_workq->job = alloc_ordered_workqueue(buf, wq_flags);
		else
			cam_workq->job = alloc_workqueue(buf, wq_flags, max_active);
		if (!cam_workq->job) {
			CAM_MEM_FREE(cam_workq);
			return -ENOMEM;
		}

		/* Workq attributes initialization */
		strscpy(cam_workq->workq_name, buf, sizeof(cam_workq->workq_name));
		INIT_WORK(&cam_workq->work, func);
		spin_lock_init(&cam_workq->lock_bh);
		CAM_DBG(CAM_WORKER, "LOCK_DBG workq %s lock %pK",
			name, &cam_workq->lock_bh);

		/* Task attributes initialization */
		atomic_set(&cam_workq->task.pending_cnt, 0);
		atomic_set(&cam_workq->task.free_cnt, 0);
		for (i = CAM_WORKQ_TASK_PRIORITY_0; i < CAM_WORKQ_TASK_PRIORITY_MAX; i++)
			INIT_LIST_HEAD(&cam_workq->task.process_head[i]);
		INIT_LIST_HEAD(&cam_workq->task.empty_head);
		atomic_set(&cam_workq->flush, 0);
		cam_workq->in_irq = in_irq;
		cam_workq->task.num_task = num_tasks;
		cam_workq->task.pool = CAM_MEM_ZALLOC_ARRAY(cam_workq->task.num_task,
				sizeof(struct cam_workq_task), GFP_KERNEL);
		if (!cam_workq->task.pool) {
			CAM_WARN(CAM_WORKER, "Insufficient memory %zu",
				sizeof(struct cam_workq_task) *
				cam_workq->task.num_task);
			CAM_MEM_FREE(cam_workq);
			return -ENOMEM;
		}

		for (i = 0; i < cam_workq->task.num_task; i++) {
			task = &cam_workq->task.pool[i];
			task->parent = (void *)cam_workq;
			/* Put all tasks in free pool */
			INIT_LIST_HEAD(&task->entry);
			cam_workq_put_task(task);
		}
		*workq = cam_workq;
		CAM_DBG(CAM_WORKER, "free tasks %d",
			atomic_read(&cam_workq->task.free_cnt));
	}

	return 0;
}

void cam_workq_destroy(struct cam_core_workq **cam_workq)
{
	unsigned long             flags = 0;
	struct workqueue_struct  *job   = NULL;
	struct cam_core_workq    *workq = NULL;
	void                     *pool  = NULL;
	int                       i;

	if (!cam_workq || !*cam_workq)
		return;

	workq = *cam_workq;
	CAM_DBG(CAM_WORKER, "destroy workque %s", workq->workq_name);
	WORKQ_ACQUIRE_LOCK(workq, flags);
	/* prevent any processing of callbacks */
	atomic_set(&workq->flush, 1);
	if (workq->job) {
		job = workq->job;
		workq->job = NULL;
	}
	pool = workq->task.pool;
	workq->task.pool = NULL;

	INIT_LIST_HEAD(&workq->task.empty_head);
	for (i = 0; i < CAM_WORKQ_TASK_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&workq->task.process_head[i]);
	*cam_workq = NULL;
	WORKQ_RELEASE_LOCK(workq, flags);

	if (job)
		destroy_workqueue(job);
	if (pool)
		CAM_MEM_FREE(pool);

	CAM_MEM_FREE(workq);
}
