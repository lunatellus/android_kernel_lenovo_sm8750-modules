// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/version.h>
#include <linux/delay.h>

#if (IS_ENABLED(CONFIG_QCOM_SI_CORE) && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE))
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/qseecom_kernel.h>
#include "si_core_test.h"

#define TRF_CREGISTERLISTENERCBO_UID			87
#define TRF_LISTENER_ID					20
#define TRF_LISTENER_DELAY_MS				50
#define TRF_IREGLISTENERCBO_OP_REG			0
#define TRF_APP_NAME					"tzecotestapp"
#define TRF_CMD_LISTENER_20				1
#define TRF_SBUF_SIZE					0x1000
#define Object_ERROR_INVALID				2
#define Object_ERROR_BUSY				-99

struct trf_listener_ctx {
	struct si_object  object;
	uint32_t listener_id;
	unsigned int delay_ms;
	struct completion *entered;
};

#define to_trf_listener_ctx(o) container_of((o), struct trf_listener_ctx, object)

static int trf_listener_dispatch(unsigned int context_id,
	struct si_object *object, unsigned long op, struct si_arg args[])
{
	struct trf_listener_ctx *ctx = to_trf_listener_ctx(object);

	if (op != 0)
		return Object_ERROR_INVALID;
	if (ctx->entered)
		complete(ctx->entered);
	msleep(ctx->delay_ms);
	return 0;
}

static void trf_listener_release(struct si_object *object)
{
	kfree(to_trf_listener_ctx(object));
}

static struct si_object_operations trf_listener_ops = {
	.release  = trf_listener_release,
	.dispatch = trf_listener_dispatch,
};

static int trf_listener_cbo_create(struct si_object **out,
	uint32_t listener_id, unsigned int delay_ms, struct completion *entered)
{
	struct trf_listener_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->listener_id = listener_id;
	ctx->delay_ms = delay_ms;
	ctx->entered = entered;

	ret = init_si_object_user(&ctx->object, SI_OT_CB_OBJECT,
				  &trf_listener_ops, "trf-listener-%u", listener_id);
	if (ret) {
		kfree(ctx);
		return ret;
	}

	*out = &ctx->object;
	return 0;
}


static int trf_register_listener(struct si_object_invoke_ctx *oic,
	struct si_object *register_obj, uint32_t listener_id,
	struct si_object *cbo_obj, struct si_object *mem_obj)
{
	int result = 0;
	int ret;
	struct si_arg args[4] = {0};

	args[0].type   = SI_AT_IB;
	args[0].b.addr = &listener_id;
	args[0].b.size = sizeof(uint32_t);
	args[1].type   = SI_AT_IO;
	args[1].o      = cbo_obj;
	args[2].type   = SI_AT_IO;
	args[2].o      = mem_obj;
	args[3].type   = SI_AT_END;

	get_si_object(cbo_obj);
	get_si_object(mem_obj);

	ret = si_object_do_invoke(oic, register_obj,
			TRF_IREGLISTENERCBO_OP_REG, args, &result);
	if (ret || result)
		return -EINVAL;

	return 0;
}

struct trf_thread_ctx {
	struct qseecom_handle *handle;
	int thread_id;
	int32_t result;
	struct completion done;
};

struct trf_shutdown_busy_ctx {
	struct qseecom_handle *handle;
	int32_t shutdown_result;
	struct completion done;
};

struct trf_send_cmd     { uint32_t cmd_id; };
struct trf_send_cmd_rsp { uint32_t data; int32_t status; };

static int trf_thread_fn(void *data)
{
	struct trf_thread_ctx *tctx = data;
	struct trf_send_cmd *req  = (struct trf_send_cmd *)tctx->handle->sbuf;
	struct trf_send_cmd_rsp *rsp  = (struct trf_send_cmd_rsp *)tctx->handle->sbuf;
	uint32_t sbuf_len = QSEECOM_ALIGN(sizeof(struct trf_send_cmd));
	uint32_t rbuf_len = QSEECOM_ALIGN(sizeof(struct trf_send_cmd_rsp));

	req->cmd_id  = TRF_CMD_LISTENER_20;
	tctx->result = qseecom_send_command(tctx->handle, req, sbuf_len, rsp, rbuf_len);

	complete(&tctx->done);
	return 0;
}

static int trf_shutdown_thread_fn(void *data)
{
	struct trf_shutdown_busy_ctx *sctx = data;

	pr_debug("Attempting shutdown while listener is busy\n");
	sctx->shutdown_result = qseecom_shutdown_app(&sctx->handle);

	complete(&sctx->done);
	return 0;
}

int si_core_kernel_test_compat_retry_send_cmd(void)
{
	int ret = 0;
	struct completion listener_entered;
	struct si_object *client_env   = NULL_SI_OBJECT;
	struct si_object *register_obj = NULL_SI_OBJECT;
	struct si_object *listener_obj = NULL_SI_OBJECT;
	struct test_mem_obj_ctx *mem_obj_ctx = NULL;
	struct si_object_invoke_ctx *reg_oic;
	struct qseecom_handle *handle1 = NULL, *handle2 = NULL;
	struct task_struct *t1 = NULL, *t2 = NULL;
	struct trf_thread_ctx tctx1 = {0};
	struct trf_thread_ctx tctx2 = {0};

	init_completion(&listener_entered);

	reg_oic = kzalloc(sizeof(*reg_oic), GFP_KERNEL);
	if (!reg_oic)
		return -ENOMEM;

	ret = si_core_get_client_env(reg_oic, &client_env);
	if (ret)
		goto out_free_oic;

	ret = si_core_client_env_open(reg_oic, client_env,
			TRF_CREGISTERLISTENERCBO_UID, &register_obj);
	put_si_object(client_env);
	if (ret)
		goto out_free_oic;

	ret = test_mem_obj_create_cma(&mem_obj_ctx, PAGE_SIZE);
	if (ret)
		goto out_register;

	ret = trf_listener_cbo_create(&listener_obj, TRF_LISTENER_ID,
				      TRF_LISTENER_DELAY_MS, &listener_entered);
	if (ret)
		goto out_mem;

	ret = trf_register_listener(reg_oic, register_obj,
			TRF_LISTENER_ID, listener_obj, mem_obj_ctx->object);
	if (ret)
		goto out_listener;

	ret = qseecom_start_app(&handle1, TRF_APP_NAME, TRF_SBUF_SIZE);
	if (ret)
		goto out_listener;

	ret = qseecom_start_app(&handle2, TRF_APP_NAME, TRF_SBUF_SIZE);
	if (ret)
		goto out_shutdown1;

	init_completion(&tctx1.done);
	tctx1.handle    = handle1;
	tctx1.thread_id = 1;

	t1 = kthread_run(trf_thread_fn, &tctx1, "trf-t1");
	if (IS_ERR(t1)) {
		ret = PTR_ERR(t1);
		goto out_shutdown2;
	}

	wait_for_completion(&listener_entered);

	init_completion(&tctx2.done);
	tctx2.handle    = handle2;
	tctx2.thread_id = 2;

	t2 = kthread_run(trf_thread_fn, &tctx2, "trf-t2");
	if (IS_ERR(t2)) {
		ret = PTR_ERR(t2);
		wait_for_completion(&tctx1.done);
		goto out_shutdown2;
	}

	wait_for_completion(&tctx1.done);
	wait_for_completion(&tctx2.done);

	if (tctx1.result || tctx2.result) {
		pr_err("qseecomcompat_trf_test FAILED (t1=%d t2=%d)\n",
		       tctx1.result, tctx2.result);
		ret = tctx1.result ? tctx1.result : tctx2.result;
	} else {
		pr_info("qseecomcompat_trf_test PASSED\n");
		ret = 0;
	}

out_shutdown2:
	qseecom_shutdown_app(&handle2);
out_shutdown1:
	qseecom_shutdown_app(&handle1);
out_listener:
	put_si_object(listener_obj);
out_mem:
	if (mem_obj_ctx)
		put_si_object(mem_obj_ctx->object);
out_register:
	put_si_object(register_obj);
out_free_oic:
	kfree(reg_oic);
	return ret;
}

int si_core_kernel_test_compat_retry_shutdown(void)
{
	int ret = 0;
	struct completion listener_entered;
	struct si_object *client_env   = NULL_SI_OBJECT;
	struct si_object *register_obj = NULL_SI_OBJECT;
	struct si_object *listener_obj = NULL_SI_OBJECT;
	struct test_mem_obj_ctx *mem_obj_ctx = NULL;
	struct si_object_invoke_ctx *reg_oic;
	struct qseecom_handle *handle = NULL;
	struct task_struct *cmd_thread = NULL, *shutdown_thread = NULL;
	struct trf_thread_ctx cmd_ctx = {0};
	struct trf_shutdown_busy_ctx shutdown_ctx = {0};

	pr_info("qseecomcompat_trf_shutdown_retry_test: Starting\n");

	init_completion(&listener_entered);

	reg_oic = kzalloc(sizeof(*reg_oic), GFP_KERNEL);
	if (!reg_oic)
		return -ENOMEM;

	ret = si_core_get_client_env(reg_oic, &client_env);
	if (ret)
		goto out_free_oic;

	ret = si_core_client_env_open(reg_oic, client_env,
			TRF_CREGISTERLISTENERCBO_UID, &register_obj);
	put_si_object(client_env);
	if (ret)
		goto out_free_oic;

	ret = test_mem_obj_create_cma(&mem_obj_ctx, PAGE_SIZE);
	if (ret)
		goto out_register;

	ret = trf_listener_cbo_create(&listener_obj, TRF_LISTENER_ID,
				      TRF_LISTENER_DELAY_MS, &listener_entered);
	if (ret)
		goto out_mem;

	ret = trf_register_listener(reg_oic, register_obj,
			TRF_LISTENER_ID, listener_obj, mem_obj_ctx->object);
	if (ret)
		goto out_listener;

	ret = qseecom_start_app(&handle, TRF_APP_NAME, TRF_SBUF_SIZE);
	if (ret)
		goto out_listener;

	/* Setup command thread context */
	init_completion(&cmd_ctx.done);
	cmd_ctx.handle = handle;
	cmd_ctx.thread_id = 1;

	/* Start thread that will trigger the listener (50ms delay) */
	cmd_thread = kthread_run(trf_thread_fn, &cmd_ctx, "trf-cmd-retry");
	if (IS_ERR(cmd_thread)) {
		ret = PTR_ERR(cmd_thread);
		goto out_shutdown;
	}

	/* Wait for listener to start processing */
	wait_for_completion(&listener_entered);

	/* Setup shutdown thread context */
	init_completion(&shutdown_ctx.done);
	shutdown_ctx.handle = handle;

	/* Start shutdown thread while listener is still busy (within 50ms window) */
	shutdown_thread = kthread_run(trf_shutdown_thread_fn, &shutdown_ctx, "trf-shutdown-retry");
	if (IS_ERR(shutdown_thread)) {
		ret = PTR_ERR(shutdown_thread);
		wait_for_completion(&cmd_ctx.done);
		goto out_shutdown;
	}

	/* Wait for both threads to complete */
	wait_for_completion(&cmd_ctx.done);
	wait_for_completion(&shutdown_ctx.done);

	if (cmd_ctx.result) {
		pr_err("qseecomcompat_trf_shutdown_retry_test FAILED: Command failed with result %d\n",
		       cmd_ctx.result);
		ret = cmd_ctx.result;
		goto out_listener;
	}

	if (shutdown_ctx.shutdown_result) {
		pr_err("qseecomcompat_trf_shutdown_retry_test FAILED: Shutdown failed with result %d\n",
		       shutdown_ctx.shutdown_result);
		ret = shutdown_ctx.shutdown_result;
		goto out_listener;
	}

	pr_info("qseecomcompat_trf_shutdown_retry_test PASSED: Shutdown succeeded despite listener delay\n");
	ret = 0;

	/* Handle is already closed by shutdown thread, so skip cleanup */
	goto out_listener;

out_shutdown:
	if (handle)
		qseecom_shutdown_app(&handle);
out_listener:
	put_si_object(listener_obj);
out_mem:
	if (mem_obj_ctx)
		put_si_object(mem_obj_ctx->object);
out_register:
	put_si_object(register_obj);
out_free_oic:
	kfree(reg_oic);
	return ret;
}

#endif /* CONFIG_QCOM_SI_CORE && (KERNEL_VERSION(6, 12, 0) <= LINUX_VERSION_CODE) */
