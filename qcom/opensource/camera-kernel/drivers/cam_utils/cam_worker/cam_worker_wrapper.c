// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "cam_irq_controller.h"
#include "cam_worker_wrapper_api.h"
#include "cam_worker_wrapper.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_mem_mgr_api.h"

#define CONFIG_RT_MAP_WORKER_TYPE                                           \
	(IS_ENABLED(CONFIG_RT_MAP_WORKER_WORKQ) ? WORKER_TYPE_WORKQ :       \
	IS_ENABLED(CONFIG_RT_MAP_WORKER_KTHREAD) ? WORKER_TYPE_KTHREAD :    \
						WORKER_TYPE_TASKLET)

#define CONFIG_NRT_MAP_WORKER_TYPE                                          \
	(IS_ENABLED(CONFIG_NRT_MAP_WORKER_KTHREAD) ? WORKER_TYPE_KTHREAD :  \
						WORKER_TYPE_WORKQ)

#define IS_KTHREAD_IN_USE                                                   \
	(IS_ENABLED(CONFIG_NRT_MAP_WORKER_KTHREAD) ? true :                 \
	IS_ENABLED(CONFIG_RT_MAP_WORKER_KTHREAD) ? true :                   \
						false)

static int cam_worker_wrapper_create(
	struct cam_worker_wrapper_create_args *init_args,
	enum cam_worker_wrapper_type           worker_type)
{
	int rc = 0;

	if (!init_args) {
		CAM_ERR(CAM_WORKER, "NULL for worker create args");
		return -EINVAL;
	}

	if (!init_args->worker_ctx) {
		CAM_ERR(CAM_WORKER, "NULL for worker ctx in create args");
		return -EINVAL;
	}

	CAM_DBG(CAM_WORKER, "Worker Wrapper Create process, worker type: %d",
		worker_type);
	switch (worker_type) {
	case WORKER_TYPE_WORKQ:
		init_args->worker_ctx->worker_type = worker_type;
		rc = cam_workq_create(init_args->u.workq_create_para.name,
			init_args->u.workq_create_para.num_tasks,
			init_args->u.workq_create_para.max_active,
			&init_args->worker_ctx->u.workq,
			init_args->u.workq_create_para.in_irq,
			init_args->u.workq_create_para.flag,
			cam_workq_process);
		if (rc < 0)
			CAM_ERR(CAM_WORKER, "Workq Create failed, worker name: %s",
				init_args->u.workq_create_para.name);

		break;
	case WORKER_TYPE_TASKLET:
		init_args->worker_ctx->worker_type = worker_type;
		rc = cam_tasklet_init(&init_args->worker_ctx->u.tasklet,
			init_args->u.tasklet_create_para.priv_data,
			init_args->u.tasklet_create_para.idx);

		if (rc < 0)
			CAM_ERR(CAM_WORKER, "Tasklet Init failed");

		break;
	case WORKER_TYPE_KTHREAD:
		init_args->worker_ctx->worker_type = worker_type;
		rc = cam_kthread_create(init_args->u.kthread_create_para.name,
			init_args->u.kthread_create_para.num_tasks,
			&init_args->worker_ctx->u.kthread,
			init_args->u.kthread_create_para.in_irq);
		if (rc)
			CAM_ERR(CAM_WORKER, "Kthread creation failed, worker name: %s",
				init_args->u.kthread_create_para.name);

		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid wrapper worker type: %d when worker create",
			worker_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

int cam_worker_wrapper_init(
	struct cam_worker_wrapper_init_args *worker_init,
	enum cam_worker_wrapper_class_type   worker_class_type)
{
	struct cam_worker_wrapper_init_args *worker_wrapper_init_para = worker_init;
	struct cam_worker_wrapper_create_args worker_create_args = {0};
	struct cam_worker_wrapper_ctx *worker_ctx_temp;
	enum cam_worker_wrapper_type worker_type = WORKER_TYPE_INVALID;
	int rc;

	if (!worker_wrapper_init_para) {
		CAM_ERR(CAM_WORKER, "NULL for worker init parameters");
		return -EINVAL;
	}

	CAM_DBG(CAM_WORKER, "Worker Wrapper Init, class type: %d, worker name: %s",
		worker_class_type, worker_wrapper_init_para->name);

	if (worker_class_type == WORKER_CLASS_RT) {
		worker_type = CONFIG_RT_MAP_WORKER_TYPE;
	} else if (worker_class_type == WORKER_CLASS_NRT) {
		worker_type = CONFIG_NRT_MAP_WORKER_TYPE;
	} else {
		CAM_ERR(CAM_WORKER, "Invalid Worker Class Type: %d, worker name: %s",
			worker_class_type, worker_wrapper_init_para->name);
		return -EINVAL;
	}

	worker_ctx_temp = CAM_MEM_ZALLOC(sizeof(struct cam_worker_wrapper_ctx), GFP_KERNEL);
	if (!worker_ctx_temp) {
		CAM_ERR(CAM_WORKER, "Failed to allocated worker wrapper, worker name: %s",
			worker_wrapper_init_para->name);
		return -ENOMEM;
	}

	worker_create_args.worker_ctx = worker_ctx_temp;
	if (worker_type == WORKER_TYPE_WORKQ) {
		/* Prepare parameters for worker wrapper type is WORKQ */
		worker_create_args.u.workq_create_para.name =
			worker_wrapper_init_para->name;
		worker_create_args.u.workq_create_para.num_tasks =
			worker_wrapper_init_para->num_tasks;
		worker_create_args.u.workq_create_para.in_irq =
			(enum cam_workq_context) worker_wrapper_init_para->in_irq;
		worker_create_args.u.workq_create_para.flag =
			worker_wrapper_init_para->flag;
	} else if (worker_type == WORKER_TYPE_TASKLET) {
		/* Prepare parameters for worker wrapper type is TASKLET */
		worker_create_args.u.tasklet_create_para.idx =
			worker_wrapper_init_para->index;
		worker_create_args.u.tasklet_create_para.priv_data =
			worker_wrapper_init_para->priv_data;
	} else if (worker_type == WORKER_TYPE_KTHREAD) {
		/* Prepare parameters for worker wrapper type is KTHREAD*/
		worker_create_args.u.kthread_create_para.name = worker_wrapper_init_para->name;
		worker_create_args.u.kthread_create_para.num_tasks =
			worker_wrapper_init_para->num_tasks;
		worker_create_args.u.kthread_create_para.in_irq =
			(enum cam_kthread_context) worker_wrapper_init_para->in_irq;
	}

	rc = cam_worker_wrapper_create(&worker_create_args, worker_type);
	if (rc < 0) {
		CAM_ERR(CAM_WORKER, "Worker Create Failed, type: %d, worker name: %s",
			worker_type, worker_wrapper_init_para->name);
		CAM_MEM_FREE(worker_ctx_temp);
		return rc;
	}

	*(worker_wrapper_init_para->worker_ctx_priv) = worker_ctx_temp;
	return rc;
}

static void cam_worker_wrapper_destroy(
	struct cam_worker_wrapper_ctx *worker_ctx)
{
	CAM_DBG(CAM_WORKER, "Worker Wrapper Destroy process: %d",
		worker_ctx->worker_type);
	switch (worker_ctx->worker_type) {
	case WORKER_TYPE_WORKQ:
		cam_workq_destroy(&worker_ctx->u.workq);
		break;
	case WORKER_TYPE_TASKLET:
		cam_tasklet_deinit(&worker_ctx->u.tasklet);
		break;
	case WORKER_TYPE_KTHREAD:
		cam_kthread_destroy(&worker_ctx->u.kthread);
		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid worker type: %d when worker destroy",
			worker_ctx->worker_type);
		break;
	}
	worker_ctx->worker_type = WORKER_TYPE_INVALID;
}

void cam_worker_wrapper_deinit(void *worker_ctx_priv)
{
	struct cam_worker_wrapper_ctx *worker_ctx = worker_ctx_priv;

	if (!worker_ctx) {
		CAM_ERR(CAM_WORKER, "NULL for worker info");
		return;
	}

	cam_worker_wrapper_destroy(worker_ctx);
	CAM_MEM_FREE(worker_ctx);
}

int cam_worker_wrapper_get(
	void *worker_ctx_priv,
	void *taskdata)
{
	struct cam_worker_wrapper_ctx *worker_get_args = worker_ctx_priv;
	struct cam_worker_wrapper_taskdata_args *taskdata_args = taskdata;
	int rc = 0;

	if (!worker_get_args) {
		CAM_ERR(CAM_WORKER, "NULL worker ctx for get");
		return -EINVAL;
	}

	if (!taskdata_args) {
		CAM_ERR(CAM_WORKER, "NULL taskdata for get");
		return -EINVAL;
	}

	switch (worker_get_args->worker_type) {
	case WORKER_TYPE_WORKQ:
		taskdata_args->task_priority = WORKER_TASK_PRIORITY_0;
		taskdata_args->task_data = (void *)
			cam_workq_get_task(worker_get_args->u.workq);

		if (!taskdata_args->task_data)
			rc = -EINVAL;

		break;
	case WORKER_TYPE_TASKLET:
		rc = cam_tasklet_get_cmd(worker_get_args->u.tasklet,
			&taskdata_args->task_data);

		if (!taskdata_args->task_data)
			rc = -EINVAL;

		break;
	case WORKER_TYPE_KTHREAD:
		taskdata_args->task_priority = WORKER_TASK_PRIORITY_0;
		taskdata_args->task_data = (void *)
			cam_kthread_get_task(worker_get_args->u.kthread);

		if (!taskdata_args->task_data)
			rc = -EINVAL;

		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid worker type: %d when get worker",
			worker_get_args->worker_type);
		rc = -EINVAL;
		break;
	}

	if (rc)
		CAM_ERR(CAM_WORKER, "Error occur when get taskdata for worker: type[%d], rc[%d]",
			worker_get_args->worker_type, rc);

	return rc;
}

int cam_worker_wrapper_enqueue(
	void                        *worker_ctx_priv,
	void                        *taskdata,
	void                        *handler_priv,
	void                        *evt_payload_priv,
	CAM_IRQ_HANDLER_BOTTOM_HALF  bottom_half_handler)
{
	struct cam_worker_wrapper_taskdata_args *taskdata_temp_args = taskdata;
	struct cam_worker_wrapper_ctx           *temp_worker = worker_ctx_priv;
	struct cam_workq_task                   *workq_task;
	struct cam_kthread_task                 *kthread_task;
	int rc = 0;

	switch (temp_worker->worker_type) {
	case WORKER_TYPE_WORKQ:
		workq_task = (struct cam_workq_task *)taskdata_temp_args->task_data;
		workq_task->process_cb = bottom_half_handler;
		workq_task->payload = evt_payload_priv;
		rc = cam_workq_enqueue_task(
			workq_task,
			handler_priv,
			(enum cam_workq_task_priority)taskdata_temp_args->task_priority);
		if (rc)
			CAM_ERR(CAM_WORKER, "worker enqueue for workq failed");
		break;
	case WORKER_TYPE_TASKLET:
		cam_tasklet_enqueue_cmd(temp_worker->u.tasklet,
			taskdata_temp_args->task_data,
			handler_priv,
			evt_payload_priv,
			bottom_half_handler);
		break;
	case WORKER_TYPE_KTHREAD:
		kthread_task = (struct cam_kthread_task *)taskdata_temp_args->task_data;
		kthread_task->process_cb = bottom_half_handler;
		kthread_task->payload = evt_payload_priv;
		rc = cam_kthread_enqueue_task(
			kthread_task,
			handler_priv,
			taskdata_temp_args->task_priority);
		if (rc)
			CAM_ERR(CAM_WORKER, "worker enqueue for kthread failed");
		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid worker type: %d when enqueue worker task",
			temp_worker->worker_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

int cam_worker_wrapper_start(void *worker_ctx_priv)
{
	struct cam_worker_wrapper_ctx *worker_ctx = worker_ctx_priv;
	int rc = 0;

	if (!worker_ctx)
		return -EINVAL;

	switch (worker_ctx->worker_type) {
	case WORKER_TYPE_WORKQ:
		CAM_DBG(CAM_WORKER, "Current bottom half does not support start, worker type: %d",
			worker_ctx->worker_type);
		break;
	case WORKER_TYPE_TASKLET:
		rc = cam_tasklet_start(worker_ctx->u.tasklet);
		break;
	case WORKER_TYPE_KTHREAD:
		CAM_DBG(CAM_WORKER, "Current bottom half does not support start, worker type: %d",
			worker_ctx->worker_type);
		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid Worker Type: %d during start",
			worker_ctx->worker_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

void cam_worker_wrapper_flush(void *worker_ctx_priv)
{
	struct cam_worker_wrapper_ctx *worker_ctx = worker_ctx_priv;

	if (!worker_ctx) {
		CAM_ERR(CAM_WORKER, "Invalid worker ctx priv");
		return;
	}

	CAM_DBG(CAM_WORKER, "Worker Wrapper flush, worker type: %d", worker_ctx->worker_type);
	switch (worker_ctx->worker_type) {
	case WORKER_TYPE_WORKQ:
		cam_workq_flush(worker_ctx->u.workq);
		break;
	case WORKER_TYPE_TASKLET:
		cam_tasklet_stop(worker_ctx->u.tasklet);
		break;
	case WORKER_TYPE_KTHREAD:
		cam_kthread_flush(worker_ctx->u.kthread);
		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid worker type: %d when worker flush",
			worker_ctx->worker_type);
		break;
	}
}

void *cam_worker_wrapper_get_task_payload(
	void                                    *worker_ctx_priv,
	struct cam_worker_wrapper_taskdata_args *taskdata)
{
	struct cam_worker_wrapper_ctx           *worker_ctx = worker_ctx_priv;
	struct cam_worker_wrapper_taskdata_args *taskdata_args = taskdata;
	void                                    *payload = NULL;
	struct cam_workq_task                   *workq_task;
	struct cam_kthread_task                 *kthread_task;

	if (!worker_ctx || !taskdata_args) {
		CAM_ERR(CAM_WORKER, "NULL taskdata or worker_ctx for get payload");
		return NULL;
	}

	switch (worker_ctx->worker_type) {
	case WORKER_TYPE_WORKQ:
		workq_task = (struct cam_workq_task *)taskdata_args->task_data;
		payload = cam_workq_get_task_payload(worker_ctx->u.workq, workq_task);
		break;
	case WORKER_TYPE_TASKLET:
		CAM_DBG(CAM_WORKER, "No task paylaod for current worker: %d",
			worker_ctx->worker_type);
		break;
	case WORKER_TYPE_KTHREAD:
		kthread_task = (struct cam_kthread_task *)taskdata_args->task_data;
		payload = cam_kthread_get_task_payload(worker_ctx->u.kthread, kthread_task);
		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid Worker Type: %d when get payload",
			worker_ctx->worker_type);
		break;
	}

	return payload;
}

int cam_worker_wrapper_payload_bind(
	void                                    *worker_ctx_priv,
	void                                    *work_data,
	int                                      work_data_idx)
{
	struct cam_worker_wrapper_ctx *worker_ctx = worker_ctx_priv;
	int                            rc = 0;

	if (!worker_ctx) {
		CAM_ERR(CAM_WORKER, "NULL worker ctx for payload bind");
		return -EINVAL;
	}

	switch (worker_ctx->worker_type) {
	case WORKER_TYPE_WORKQ:
		worker_ctx->u.workq->task.pool[work_data_idx].payload = work_data;
		break;
	case WORKER_TYPE_TASKLET:
		CAM_ERR(CAM_WORKER, "Invalid payload bind, worker type: %d",
			worker_ctx->worker_type);
		rc = -EINVAL;
		break;
	case WORKER_TYPE_KTHREAD:
		worker_ctx->u.kthread->task.pool[work_data_idx].payload = work_data;
		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid Worker Type: %d when payload bind",
			worker_ctx->worker_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

void cam_worker_wrapper_dump_info_cb(
	void                                *worker_ctx_priv,
	struct cam_worker_wrapper_mini_dump *worker_dump_info)
{
	struct cam_worker_wrapper_ctx *worker_ctx = worker_ctx_priv;

	if (!worker_ctx || !worker_dump_info) {
		CAM_ERR(CAM_WORKER, "Dump worker failed due to invalid worker_ctx or dump struct");
		return;
	}

	switch (worker_ctx->worker_type) {
	case WORKER_TYPE_WORKQ:
		worker_dump_info->worker_scheduled_ts =
			worker_ctx->u.workq->workq_scheduled_ts;
		worker_dump_info->task.pending_cnt =
			atomic_read(&worker_ctx->u.workq->task.pending_cnt);
		worker_dump_info->task.free_cnt =
			atomic_read(&worker_ctx->u.workq->task.free_cnt);
		worker_dump_info->task.num_task = worker_ctx->u.workq->task.num_task;
		CAM_DBG(CAM_WORKER,
			"Dump worker info, worker type: %d, scheduled ts in ms: %lld, pending tasks: %d, free tasks: %d, num tasks: %d",
			WORKER_TYPE_WORKQ, ktime_to_ms(worker_dump_info->worker_scheduled_ts),
			worker_dump_info->task.pending_cnt,
			worker_dump_info->task.free_cnt,
			worker_dump_info->task.num_task);
		break;
	case WORKER_TYPE_TASKLET:
		CAM_DBG(CAM_WORKER, "No support for tasklet info dump");
		break;
	case WORKER_TYPE_KTHREAD:
		worker_dump_info->worker_scheduled_ts =
			worker_ctx->u.kthread->worker_scheduled_ts;
		worker_dump_info->task.pending_cnt =
			atomic_read(&worker_ctx->u.kthread->task.pending_cnt);
		worker_dump_info->task.free_cnt =
			atomic_read(&worker_ctx->u.kthread->task.free_cnt);
		worker_dump_info->task.num_task = worker_ctx->u.kthread->task.num_task;
		break;
	default:
		CAM_ERR(CAM_WORKER, "Invalid Worker Type: %d when payload bind",
			worker_ctx->worker_type);
		break;
	}
}

int cam_worker_wrapper_prop_update_init(void)
{
	int rc;

	/* Only when kthread is in use, thread property can be tuned */
	if (!IS_KTHREAD_IN_USE) {
		CAM_INFO(CAM_WORKER, "Kthread is not in use, not available to tune property");
		return 0;
	}

	rc = cam_kthread_property_update_init();
	if (rc)
		CAM_ERR(CAM_WORKER, "Failed at setting up kthread property update, rc: %d", rc);

	return rc;
}

void cam_worker_wrapper_prop_update_deinit(void)
{
	/* Only when kthread is in use, thread property can be tuned */
	if (!IS_KTHREAD_IN_USE) {
		CAM_INFO(CAM_WORKER, "Kthread is not in use, not available to tune property");
		return;
	}

	cam_kthread_property_update_deinit();
}
