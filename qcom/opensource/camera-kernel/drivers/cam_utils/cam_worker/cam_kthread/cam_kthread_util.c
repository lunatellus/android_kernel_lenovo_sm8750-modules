// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/debugfs.h>
#include "cam_kthread_util.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_mem_mgr_api.h"

/* Global struct to bookkeep all alive kthreads */
static struct cam_kthread_info  g_cam_kthread_info;

/* Global worker for kthread prop set */
static struct cam_core_kthread *g_prop_update_worker;

static struct dentry           *dbgfileptr;

#define KTHREAD_ACQUIRE_LOCK(worker_kthread, flags) {                   \
	if ((worker_kthread)->in_irq)                                   \
		spin_lock_irqsave(&(worker_kthread)->lock_bh, (flags)); \
	else                                                            \
		mutex_lock(&(worker_kthread)->mutex_lock);              \
}

#define KTHREAD_RELEASE_LOCK(worker_kthread, flags) {                        \
	if ((worker_kthread)->in_irq)                                        \
		spin_unlock_irqrestore(&(worker_kthread)->lock_bh, (flags)); \
	else	                                                             \
		mutex_unlock(&(worker_kthread)->mutex_lock);                 \
}

static inline char *__cam_kthread_sched_policy_to_name(uint32_t policy)
{
	switch (policy) {
	case SCHED_NORMAL:
		return "SCHED_NORMAL";
	case SCHED_FIFO:
		return "SCHED_FIFO";
	case SCHED_BATCH:
		return "SCHED_BATCH";
	case SCHED_RR:
		return "SCHED_RR";
	default:
		return "Invalid";
	}
}

static void cam_kthread_put_task_unlocked(struct cam_kthread_task *task)
{
	struct cam_core_kthread *kthread =
		(struct cam_core_kthread *)task->parent;

	task->cancel = 0;
	task->process_cb = NULL;
	task->priv = NULL;

	list_add_tail(&task->entry, &kthread->task.empty_head);
	atomic_add(1, &kthread->task.free_cnt);
}

static void cam_kthread_put_task(struct cam_kthread_task *task)
{
	struct cam_core_kthread *kthread = (struct cam_core_kthread *)task->parent;
	unsigned long            flags = 0;

	KTHREAD_ACQUIRE_LOCK(kthread, flags);
	cam_kthread_put_task_unlocked(task);
	KTHREAD_RELEASE_LOCK(kthread, flags);

	CAM_DBG(CAM_WORKER, "PUT task kthread %s, free_cnt %d",
		kthread->worker_name, atomic_read(&kthread->task.free_cnt));
}

inline void cam_kthread_flush(struct cam_core_kthread *kthread)
{
	int                      i;
	unsigned long            flags = 0;
	struct cam_kthread_task *task;

	if (!kthread) {
		CAM_ERR(CAM_WORKER, "kthread is null");
		return;
	}

	KTHREAD_ACQUIRE_LOCK(kthread, flags);
	atomic_set(&kthread->flush_in_process, 1);
	KTHREAD_RELEASE_LOCK(kthread, flags);

	kthread_flush_worker(kthread->job);

	KTHREAD_ACQUIRE_LOCK(kthread, flags);
	atomic_set(&kthread->task.pending_cnt, 0);
	atomic_set(&kthread->task.free_cnt, 0);
	INIT_LIST_HEAD(&kthread->task.empty_head);

	for (i = CAM_KTHREAD_TASK_PRIORITY_0; i < CAM_KTHREAD_TASK_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&kthread->task.process_head[i]);

	for (i = 0; i < kthread->task.num_task; i++) {
		task = &kthread->task.pool[i];
		task->parent = (void *)kthread;
		/* Put all tasks in free pool */
		INIT_LIST_HEAD(&task->entry);
		cam_kthread_put_task_unlocked(task);
	}

	atomic_set(&kthread->flush_in_process, 0);
	KTHREAD_RELEASE_LOCK(kthread, flags);
}

static void cam_kthread_process_task(struct cam_kthread_task *task)
{
	task->process_cb(task->priv, task->payload);
}

void cam_kthread_process(struct kthread_work *w)
{
	struct cam_core_kthread *worker_kthread;
	struct cam_kthread_task *task;
	int32_t                  i = CAM_KTHREAD_TASK_PRIORITY_0;
	unsigned long            flags = 0;
	ktime_t                  exec_start_time;
	void                    *cb;

	if (!w) {
		CAM_ERR(CAM_WORKER, "NULL task pointer for kthread work");
		return;
	}

	worker_kthread = (struct cam_core_kthread *)
		container_of(w, struct cam_core_kthread, work);

	while (i < CAM_KTHREAD_TASK_PRIORITY_MAX) {
		KTHREAD_ACQUIRE_LOCK(worker_kthread, flags);
		while (!list_empty(&worker_kthread->task.process_head[i])) {
			task = list_first_entry(&worker_kthread->task.process_head[i],
				struct cam_kthread_task, entry);
			cb = (void *)task->process_cb;
			if (!cb) {
				CAM_ERR(CAM_WORKER,
					"FATAL: no task handler registered for task, worker name: %s",
					worker_kthread->worker_name);
				KTHREAD_RELEASE_LOCK(worker_kthread, flags);
				return;
			}

			cam_common_util_thread_switch_delay_detect(
				worker_kthread->worker_name, "kthread schedule",
				cb, worker_kthread->worker_scheduled_ts,
				CAM_KTHREAD_SCHEDULE_TIME_THRESHOLD);

			exec_start_time = ktime_get_boottime();
			atomic_sub(1, &worker_kthread->task.pending_cnt);
			list_del_init(&task->entry);
			KTHREAD_RELEASE_LOCK(worker_kthread, flags);
			if (unlikely(atomic_read(&worker_kthread->flush_in_process)))
				CAM_WARN(CAM_WORKER,
				"Kthread process called during flush, worker name: %s",
				worker_kthread->worker_name);
			cam_kthread_process_task(task);

			cam_kthread_put_task(task);
			cam_common_util_thread_switch_delay_detect(
				worker_kthread->worker_name, "kthread execution",
				cb, exec_start_time, CAM_KTHREAD_EXE_TIME_THRESHOLD);
			CAM_DBG(CAM_WORKER, "Processed task %pK, free_cnt %d, worker name: %s",
				task, atomic_read(&worker_kthread->task.free_cnt),
				worker_kthread->worker_name);
			KTHREAD_ACQUIRE_LOCK(worker_kthread, flags);
		}
		KTHREAD_RELEASE_LOCK(worker_kthread, flags);
		i++;
	}
}

struct cam_kthread_task *cam_kthread_get_task(struct cam_core_kthread *kthread)
{
	struct cam_kthread_task *task = NULL;
	unsigned long            flags = 0;

	if (!kthread)
		return NULL;

	KTHREAD_ACQUIRE_LOCK(kthread, flags);
	if (list_empty(&kthread->task.empty_head))
		goto end;

	task = list_first_entry(&kthread->task.empty_head,
		struct cam_kthread_task, entry);
	if (task) {
		atomic_sub(1, &kthread->task.free_cnt);
		list_del_init(&task->entry);
	}

end:
	KTHREAD_RELEASE_LOCK(kthread, flags);

	CAM_DBG(CAM_WORKER, "GET task kthread %s, free_cnt %d",
		kthread->worker_name, atomic_read(&kthread->task.free_cnt));

	return task;
}

void *cam_kthread_get_task_payload(struct cam_core_kthread *kthread,
	struct cam_kthread_task *kthread_task)
{
	void *payload;

	if (!kthread) {
		CAM_ERR(CAM_WORKER, "Invalid kthread");
		return NULL;
	}

	if (!kthread_task) {
		CAM_ERR(CAM_WORKER, "Invalid kthread task, worker name: %s",
			kthread->worker_name);
		return NULL;
	}

	payload = kthread_task->payload;
	if (!payload) {
		CAM_ERR(CAM_WORKER,
			"Invalid payload, bind payload should happen before get task payload, worker name: %s",
			kthread->worker_name);
		cam_kthread_put_task(kthread_task);
	}

	return payload;
}

int cam_kthread_enqueue_task(struct cam_kthread_task *task,
	void *priv, int32_t prio)
{
	int                      rc = 0;
	struct cam_core_kthread *kthread;
	unsigned long            flags = 0;

	if (!task) {
		CAM_WARN(CAM_WORKER, "Invalid task pointer, can not schedule");
		return -EINVAL;
	}

	kthread = (struct cam_core_kthread *)task->parent;
	if (!kthread) {
		CAM_DBG(CAM_WORKER, "Invalid kthread pointer, suspect mem corruption");
		return -EINVAL;
	}

	task->priv = priv;
	task->priority = (prio < CAM_KTHREAD_TASK_PRIORITY_MAX &&
		prio >= CAM_KTHREAD_TASK_PRIORITY_0)
		? prio : CAM_KTHREAD_TASK_PRIORITY_0;

	kthread->worker_scheduled_ts = ktime_get_boottime();
	KTHREAD_ACQUIRE_LOCK(kthread, flags);
	if (!kthread->job) {
		rc = -EINVAL;
		KTHREAD_RELEASE_LOCK(kthread, flags);
		goto abort;
	}

	if (task->cancel == 1 || atomic_read(&kthread->flush_in_process)) {
		rc = 0;
		CAM_WARN(CAM_WORKER, "Enqueue ignored due to flush in progress, worker name: %s",
			kthread->worker_name);
		KTHREAD_RELEASE_LOCK(kthread, flags);
		goto abort;
	}

	list_add_tail(&task->entry, &kthread->task.process_head[task->priority]);
	atomic_add(1, &kthread->task.pending_cnt);
	CAM_DBG(CAM_WORKER, "Enqueue kthread task %pK, pending_cnt %d, worker name: %s",
		task, atomic_read(&kthread->task.pending_cnt),
		kthread->worker_name);

	kthread_queue_work(kthread->job, &kthread->work);
	KTHREAD_RELEASE_LOCK(kthread, flags);
	return rc;

abort:
	cam_kthread_put_task(task);
	CAM_INFO(CAM_WORKER, "Task aborted and queued back to pool, worker name: %s",
		kthread->worker_name);
	return rc;
}

static inline int cam_kthread_property_update(struct cam_kthread_data *kthread_data)
{
	struct sched_attr sched_attr = {0};
	struct cpumask    cpu_affinity;
	int               i = 0, temp, rc = 0;

	/* Set kthread priority and sched policy */
	if (g_cam_kthread_info.policy != SCHED_NORMAL)
		sched_attr.sched_priority = g_cam_kthread_info.priority;
	else
		sched_attr.sched_nice = g_cam_kthread_info.nice;

	sched_attr.sched_policy = g_cam_kthread_info.policy;
	CAM_DBG(CAM_WORKER,
		"Update kthread property, priority: %d, nice: %d, policy: %s",
		sched_attr.sched_priority, sched_attr.sched_nice,
		__cam_kthread_sched_policy_to_name(sched_attr.sched_policy));

	rc = sched_setattr(kthread_data->kthread_worker->task, &sched_attr);
	if (rc) {
		CAM_ERR(CAM_WORKER,
			"Failed to update kthread property, priority: %d, policy: %s, nice: %d, return: %d",
			sched_attr.sched_priority,
			__cam_kthread_sched_policy_to_name(sched_attr.sched_policy),
			sched_attr.sched_nice, rc);
		return rc;
	}

	/* Set kthread CPU affinity */
	if (g_cam_kthread_info.affinity) {
		temp = g_cam_kthread_info.affinity;
		while (temp) {
			if (temp & 0x1)
				cpumask_set_cpu(i, &cpu_affinity);
			temp >>= 1;
			i++;
		}
		kthread_bind_mask(kthread_data->kthread_worker->task, &cpu_affinity);
	}

	return rc;
}

static inline int cam_set_kthread_prop_internal(void *priv, void *data)
{
	struct cam_core_kthread *cam_kthread;
	struct cam_kthread_data *kthread_data;
	int                      rc = 0;

	g_cam_kthread_info.result = -1;

	/*
	 * If set property is sent from userspace or debugfs, such priv should
	 * be false, and set all alive kthread to that specific thread property
	 */
	if (priv) {
		cam_kthread = (struct cam_core_kthread *)priv;
		mutex_lock(&g_cam_kthread_info.kthread_list_mutex);
		list_for_each_entry(kthread_data, &g_cam_kthread_info.kthread_list, list) {
			if (kthread_data->kthread_worker == cam_kthread->job) {
				rc = cam_kthread_property_update(kthread_data);
				if (rc) {
					mutex_unlock(&g_cam_kthread_info.kthread_list_mutex);
					CAM_ERR(CAM_WORKER,
						"Failed to set kthread properties, worker name: %s",
						cam_kthread->worker_name);
					g_cam_kthread_info.result = rc;
					complete(&cam_kthread->worker_completion);
					return rc;
				}

				break;
			}
		}
		mutex_unlock(&g_cam_kthread_info.kthread_list_mutex);

		g_cam_kthread_info.result = 0;
		complete(&cam_kthread->worker_completion);
	} else {
		mutex_lock(&g_cam_kthread_info.kthread_list_mutex);
		list_for_each_entry(kthread_data, &g_cam_kthread_info.kthread_list, list) {
			rc = cam_kthread_property_update(kthread_data);
			if (rc) {
				mutex_unlock(&g_cam_kthread_info.kthread_list_mutex);
				CAM_ERR(CAM_WORKER, "Failed to set properties");
				g_cam_kthread_info.result = rc;
				complete(&g_prop_update_worker->worker_completion);
				return rc;
			}
		}
		mutex_unlock(&g_cam_kthread_info.kthread_list_mutex);

		g_cam_kthread_info.result = 0;
		complete(&g_prop_update_worker->worker_completion);
	}

	return rc;
}

int cam_kthread_create(char *name, int32_t num_tasks,
	struct cam_core_kthread **kthread, enum cam_kthread_context in_irq)
{
	struct cam_kthread_task *task;
	struct cam_core_kthread *cam_kthread;
	char                     buf[128] = "crm_kt-";
	int                      i, rc = 0;
	struct cam_kthread_data *kthread_data;
	unsigned long            rem_jiffies;

	if (!kthread || *kthread) {
		CAM_ERR(CAM_WORKER, "Invalid kthread input");
		return -EINVAL;
	}

	if (in_irq >= CAM_KTHREAD_USAGE_MAX || num_tasks <= 0) {
		CAM_ERR(CAM_WORKER, "Invalid kthread arguments, in_irq: %d, num_tasks: %d",
			in_irq, num_tasks);
		return -EINVAL;
	}

	cam_kthread = CAM_MEM_ZALLOC(sizeof(struct cam_core_kthread), GFP_KERNEL);
	if (!cam_kthread) {
		CAM_ERR(CAM_WORKER, "Failed at allocating kthread worker");
		return -ENOMEM;
	}

	strlcat(buf, name, sizeof(buf));
	CAM_DBG(CAM_WORKER, "Create kthread %s, num_task: %d, kthread context: %d",
		buf, num_tasks, in_irq);

	cam_kthread->job = kthread_create_worker(0, buf);
	if (IS_ERR(cam_kthread->job)) {
		rc = PTR_ERR(cam_kthread->job);
		CAM_ERR(CAM_WORKER, "Kthread job creation failed, err code: %d", rc);
		goto free_cam_kthread;
	}

	/* Kthread attributes initialization*/
	strscpy(cam_kthread->worker_name, buf, sizeof(cam_kthread->worker_name));
	kthread_init_work(&cam_kthread->work, cam_kthread_process);

	/* Kthread lock initialization for various purposes */
	if (in_irq)
		spin_lock_init(&cam_kthread->lock_bh);
	else
		mutex_init(&cam_kthread->mutex_lock);

	/* Task attributes initialization */
	atomic_set(&cam_kthread->task.pending_cnt, 0);
	atomic_set(&cam_kthread->task.free_cnt, 0);
	for (i = CAM_KTHREAD_TASK_PRIORITY_0; i < CAM_KTHREAD_TASK_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&cam_kthread->task.process_head[i]);
	INIT_LIST_HEAD(&cam_kthread->task.empty_head);
	atomic_set(&cam_kthread->flush_in_process, 0);

	cam_kthread->in_irq = in_irq;
	cam_kthread->task.num_task = num_tasks;
	cam_kthread->task.pool = CAM_MEM_ZALLOC_ARRAY(cam_kthread->task.num_task,
		sizeof(struct cam_kthread_task), GFP_KERNEL);
	if (!cam_kthread->task.pool) {
		CAM_ERR(CAM_WORKER,
			"Failed at allocation memory for kthread task pool, worker name: %s",
			cam_kthread->worker_name);
		rc = -ENOMEM;
		goto destroy_kthread_worker;
	}

	for (i = 0; i < cam_kthread->task.num_task; i++) {
		task = &cam_kthread->task.pool[i];
		task->parent = (void *)cam_kthread;

		/* Put all tasks in free pool */
		INIT_LIST_HEAD(&task->entry);
		cam_kthread_put_task(task);
	}

	*kthread = cam_kthread;

	kthread_data = CAM_MEM_ZALLOC(sizeof(struct cam_kthread_data), GFP_KERNEL);
	if (!kthread_data) {
		CAM_ERR(CAM_WORKER, "Failed at allocating memory for kthread data");
		rc = -ENOMEM;
		goto free_kthread_task_pool;
	}

	kthread_data->kthread_worker = cam_kthread->job;

	if (!g_cam_kthread_info.is_list_initalized) {
		INIT_LIST_HEAD(&g_cam_kthread_info.kthread_list);
		mutex_init(&g_cam_kthread_info.kthread_list_mutex);
		g_cam_kthread_info.is_list_initalized = true;
	}

	mutex_lock(&g_cam_kthread_info.kthread_list_mutex);
	INIT_LIST_HEAD(&kthread_data->list);
	list_add_tail(&kthread_data->list, &g_cam_kthread_info.kthread_list);
	mutex_unlock(&g_cam_kthread_info.kthread_list_mutex);

	/*
	 * is_prop_valid needs to be set from userspace or debugfs or
	 * pre-set from source code. Make sure after manually setting
	 * policy/priority/etc..., set is_prop_valid to true as well
	 */
	if (g_cam_kthread_info.is_prop_valid) {
		init_completion(&cam_kthread->worker_completion);

		task = cam_kthread_get_task(cam_kthread);
		if (IS_ERR_OR_NULL(task)) {
			CAM_ERR(CAM_WORKER, "No empty task = %d", PTR_ERR(task));
			rc = -EINVAL;
			goto free_kthread_data;
		}

		task->process_cb = &cam_set_kthread_prop_internal;
		rc = cam_kthread_enqueue_task(task, cam_kthread, CAM_KTHREAD_TASK_PRIORITY_0);
		if (rc) {
			CAM_ERR(CAM_WORKER, "Could not enqueue task, worker name: %s, rc: %d",
				cam_kthread->worker_name, rc);
			goto put_kthread_task;
		}

		rem_jiffies = cam_common_wait_for_completion_timeout(
			&cam_kthread->worker_completion,
			msecs_to_jiffies(CAM_KTHREAD_PRIORITY_UPDATE_THRESHOLD));
		if (!rem_jiffies) {
			CAM_ERR(CAM_WORKER,
				"Setting kthread properties timed out, worker_name: %s",
				cam_kthread->worker_name);
			rc = -ETIME;
			goto put_kthread_task;
		}

		if (g_cam_kthread_info.result)
			CAM_WARN(CAM_WORKER,
				"Failed to set properties, worker name: %s, result: %d",
				cam_kthread->worker_name, g_cam_kthread_info.result);
	}

	CAM_INFO(CAM_WORKER,
		"Kthread creation succeed, kthread name: %s, num_tasks: %d, irq context: %u, is_prop_valid: %s, policy: %s, priority: %u, nice: %d, affinity: 0x%x",
		cam_kthread->worker_name, num_tasks, in_irq,
		CAM_BOOL_TO_YESNO(g_cam_kthread_info.is_prop_valid),
		__cam_kthread_sched_policy_to_name(g_cam_kthread_info.policy),
		g_cam_kthread_info.priority,
		g_cam_kthread_info.nice,
		g_cam_kthread_info.affinity);
	return rc;

put_kthread_task:
	cam_kthread_put_task(task);
free_kthread_data:
	mutex_lock(&g_cam_kthread_info.kthread_list_mutex);
	list_del_init(&kthread_data->list);
	mutex_unlock(&g_cam_kthread_info.kthread_list_mutex);
	CAM_MEM_FREE(kthread_data);
free_kthread_task_pool:
	CAM_MEM_FREE(cam_kthread->task.pool);
	cam_kthread->task.pool = NULL;
destroy_kthread_worker:
	kthread_destroy_worker(cam_kthread->job);
free_cam_kthread:
	CAM_MEM_FREE(cam_kthread);
	return rc;
}

void cam_kthread_destroy(struct cam_core_kthread **cam_kthread)
{
	unsigned long            flags = 0;
	struct cam_core_kthread *kthread;
	struct cam_kthread_data *kthread_data, *tmp_kthread_data;
	struct kthread_worker   *job = NULL;
	int                      i;

	if (!cam_kthread || !*cam_kthread) {
		CAM_ERR(CAM_WORKER, "Invalid argument for kthread destroy");
		return;
	}

	kthread = *cam_kthread;
	CAM_DBG(CAM_WORKER, "Destroy kthread %s", kthread->worker_name);

	KTHREAD_ACQUIRE_LOCK(kthread, flags);
	/* Prevent any processing of callbacks */
	atomic_set(&kthread->flush_in_process, 1);
	if (kthread->job) {
		job = kthread->job;
		kthread->job = NULL;
		KTHREAD_RELEASE_LOCK(kthread, flags);
		kthread_destroy_worker(job);
		KTHREAD_ACQUIRE_LOCK(kthread, flags);
	}

	CAM_MEM_FREE(kthread->task.pool);

	/* Leave lists in stable state after freeing pool */
	INIT_LIST_HEAD(&kthread->task.empty_head);
	for (i = 0; i < CAM_KTHREAD_TASK_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&kthread->task.process_head[i]);
	*cam_kthread = NULL;
	KTHREAD_RELEASE_LOCK(kthread, flags);
	if (!kthread->in_irq)
		mutex_destroy(&kthread->mutex_lock);

	if (job) {
		mutex_lock(&g_cam_kthread_info.kthread_list_mutex);
		list_for_each_entry_safe(kthread_data, tmp_kthread_data,
			&g_cam_kthread_info.kthread_list, list) {
			if (kthread_data->kthread_worker == job) {
				list_del_init(&kthread_data->list);
				CAM_MEM_FREE(kthread_data);
				break;
			}
		}
		mutex_unlock(&g_cam_kthread_info.kthread_list_mutex);
	}
	CAM_MEM_FREE(kthread);
}

static int cam_kthread_update_thread_property_dbg(struct inode *inode, struct file *flip)
{
	int                      rc;
	struct cam_kthread_task *task;
	unsigned long            rem_jiffies;

	if (!g_cam_kthread_info.is_prop_valid) {
		CAM_INFO(CAM_WORKER,
			"Property validity bit is not set from userspace or debugfs");
		return 0;
	}

	init_completion(&g_prop_update_worker->worker_completion);

	task = cam_kthread_get_task(g_prop_update_worker);
	if (IS_ERR_OR_NULL(task)) {
		CAM_ERR(CAM_WORKER, "No empty task = %d in prop update worker", PTR_ERR(task));
		return -ENOENT;
	}

	task->process_cb = &cam_set_kthread_prop_internal;
	rc = cam_kthread_enqueue_task(task, NULL, CAM_KTHREAD_TASK_PRIORITY_0);
	if (rc) {
		CAM_ERR(CAM_WORKER, "Could not enqueue task, rc: %d", rc);
		return rc;
	}

	rem_jiffies = cam_common_wait_for_completion_timeout(
		&g_prop_update_worker->worker_completion,
		msecs_to_jiffies(CAM_KTHREAD_PRIORITY_UPDATE_THRESHOLD));
	if (!rem_jiffies) {
		CAM_ERR(CAM_WORKER, "Setting kthread properties timed out!");
		return -ETIMEDOUT;
	}

	if (g_cam_kthread_info.result)
		CAM_ERR(CAM_WORKER, "Failed to set kthread properties, result: %d",
			g_cam_kthread_info.result);

	return g_cam_kthread_info.result;
}

static ssize_t cam_kthread_set_thread_property_dbg(struct file *file, const char __user *ubuf,
	size_t size, loff_t *loff_t)
{
	char *delimiter1, *delimiter2, *delimiter3, *delimiter4;
	char  input_buf[64] = {'\0'};

	if (size >= 64)
		return -EINVAL;

	if (copy_from_user(input_buf, ubuf, size))
		return -EFAULT;

	input_buf[size] = '\0';

	/* User should input under format "<policy>_<proirty>_<nice>_<affinity>_0/1" */
	delimiter1 = strnchr(input_buf, size, '_');
	if (!delimiter1)
		goto end;

	delimiter2 = strnchr(delimiter1 + 1, size, '_');
	if (!delimiter2)
		goto end;

	delimiter3 = strnchr(delimiter2 + 1, size, '_');
	if (!delimiter3)
		goto end;

	delimiter4 = strnchr(delimiter3 + 1, size, '_');
	if (!delimiter4)
		goto end;

	*delimiter1 = '\0';
	*delimiter2 = '\0';
	*delimiter3 = '\0';
	*delimiter4 = '\0';

	if (kstrtou32(input_buf, 0, &g_cam_kthread_info.policy))
		goto end;

	if (kstrtou32(delimiter1 + 1, 0, &g_cam_kthread_info.priority))
		goto end;

	if (kstrtos32(delimiter2 + 1, 0, &g_cam_kthread_info.nice))
		goto end;

	if (kstrtou32(delimiter3 + 1, 0, &g_cam_kthread_info.affinity))
		goto end;

	if (kstrtobool(delimiter4 + 1, &g_cam_kthread_info.is_prop_valid))
		goto end;

	return size;
end:
	CAM_INFO(CAM_WORKER, "Failed at setting debugging thread property");
	return -EINVAL;
}

static ssize_t cam_kthread_get_thread_property_dbg(struct file *file, char __user *ubuf,
	size_t size, loff_t *loff_t)
{
	char display_string[256];
	int  len = 0;

	len += scnprintf(display_string + len, (256 - len),
		"\n****** Kernel Thread Property *****\n\n");

	len += scnprintf(display_string + len, (256 - len),
		"Policy: %s, ", __cam_kthread_sched_policy_to_name(g_cam_kthread_info.policy));

	len += scnprintf(display_string + len, (256 - len),
		"Priority: %u, ", g_cam_kthread_info.priority);

	len += scnprintf(display_string + len, (256 - len),
		"Nice: %d, ", g_cam_kthread_info.nice);

	len += scnprintf(display_string + len, (256 - len),
		"Affinity: 0x%x, ", g_cam_kthread_info.affinity);

	len += scnprintf(display_string + len, (256 - len),
		"Property valid: %s\n",
		CAM_BOOL_TO_YESNO(g_cam_kthread_info.is_prop_valid));

	scnprintf(display_string + len, (256 - len),
		"\n***********************************\n");
	return simple_read_from_buffer(ubuf, size, loff_t, display_string, strlen(display_string));
}

static const struct file_operations cam_kthread_debug_thread_property = {
	.owner = THIS_MODULE,
	.release = cam_kthread_update_thread_property_dbg,
	.read = cam_kthread_get_thread_property_dbg,
	.write = cam_kthread_set_thread_property_dbg,
};

int cam_kthread_property_update_init(void)
{
	int rc;

	/*
	 * 1. Create separate kthread to side load prop update from userspace/debugfs
	 * 2. Create debugfs to tune thread property if debugfs is available
	 */
	rc = cam_kthread_create("cam-prop-update", 2, &g_prop_update_worker,
		CAM_KTHREAD_USAGE_IRQ);
	if (rc) {
		CAM_ERR(CAM_WORKER,
			"Kthread creation failed for thread property update worker, rc: %d",
			rc);
		return rc;
	}

	if (!cam_debugfs_available())
		return 0;

	rc = cam_debugfs_create_subdir("kthread_worker", &dbgfileptr);
	if (rc) {
		CAM_ERR(CAM_WORKER, "DebugFS could not create directory for worker subdir, rc: %d",
			rc);
		cam_kthread_destroy(&g_prop_update_worker);
		g_prop_update_worker = NULL;
		return -ENOENT;
	}

	debugfs_create_file("cam_kthread_debug_thread_property", 0644, dbgfileptr,
		NULL, &cam_kthread_debug_thread_property);

	return rc;
}

void cam_kthread_property_update_deinit(void)
{
	cam_kthread_destroy(&g_prop_update_worker);
	g_prop_update_worker = NULL;

	if (!cam_debugfs_available())
		return;

	debugfs_remove_recursive(dbgfileptr);
	dbgfileptr = NULL;
}
