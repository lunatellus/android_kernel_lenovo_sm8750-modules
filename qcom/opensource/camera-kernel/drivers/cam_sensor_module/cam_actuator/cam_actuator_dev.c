// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "cam_actuator_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_actuator_soc.h"
#include "cam_actuator_core.h"
#include "cam_trace.h"
#include "camera_main.h"
#include "cam_compat.h"
#include "cam_mem_mgr_api.h"
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

u32 globalStepNum;
u32 globalTarget;
u8 movingStatus;
u8 isHoldByNode;
u8 leftVibIsUsed;
u8 rightVibIsUsed;
u8 secondVibHasDelay;
u8 poweroff_workStatus;
int is_actuator_power_by_cam;
int power_status;
struct cam_actuator_ctrl_t *wide_a_ctrl;

struct haptic_data {
	int duration;
	int id;
	int mode;
};

static int haptic_callback(struct notifier_block *block,
	unsigned long action, void *data);
struct notifier_block nb;

extern int register_haptic_notify(struct notifier_block *nb);
extern int unregister_haptic_notify(struct notifier_block *nb);

static void poweron_work_func(struct work_struct *work);
static void poweroff_work_func(struct work_struct *work);
static struct delayed_work poweron_work;
static struct delayed_work poweroff_work;

static void CreateVCMCtrol(int target, int step_num, int mode);

static int haptic_callback(struct notifier_block *nb, unsigned long action,
	void *data)
{
	struct haptic_data *haptic = data;

	if (!haptic) {
		CAM_ERR(CAM_ACTUATOR, "can't gethaptic_data_t!");
		return NOTIFY_OK;
	}

	if (haptic->id == 1)
		leftVibIsUsed = 1;
	else if (haptic->id == 2)
		rightVibIsUsed = 1;

	if (isHoldByNode)
		return NOTIFY_OK;

	if (action == 2) {
		CAM_DBG(CAM_ACTUATOR, "haptic_pull_vcm");
		CreateVCMCtrol(0x200, 0, 0);
	} else if (action == 1 &&
		(haptic->duration > 0xc7 ||
		 (haptic->mode == 2 && haptic->duration == 0))) {
		CAM_DBG(CAM_ACTUATOR, "haptic_push_vcm");
		CreateVCMCtrol(0, 15, 1);
	}

	return NOTIFY_OK;
}

static int readCode(void)
{
	u32 data = 0;
	int rc;

	rc = camera_io_dev_read(&wide_a_ctrl->io_master_info, 3, &data,
		CAMERA_SENSOR_I2C_TYPE_BYTE, CAMERA_SENSOR_I2C_TYPE_WORD, true);
	if (rc < 0) {
		data = -1;
#line 493 "drivers/cam_sensor_module/cam_actuator/cam_actuator_core.c"
		CAM_ERR(CAM_ACTUATOR, "readCode: failed ret =%d", rc);
	}
#line 495 "drivers/cam_sensor_module/cam_actuator/cam_actuator_core.c"
	CAM_DBG(CAM_ACTUATOR, "readCode data:%d", data);

	return data;
}

static ssize_t moveVCM_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	int rc = readCode();

	if (rc < 0) {
#line 719 "drivers/cam_sensor_module/cam_actuator/cam_actuator_core.c"
		CAM_ERR(CAM_ACTUATOR, "failed");
	}
#line 721 "drivers/cam_sensor_module/cam_actuator/cam_actuator_core.c"
	CAM_DBG(CAM_ACTUATOR, "moveVCM_show readCode:%d", rc);

	return sprintf(buf, "current:%d\n", rc);
}

static ssize_t moveVCM_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 value = 0;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "failed to parse moveVCM value: %d", rc);
	} else if (!value) {
		isHoldByNode = 1;
		CreateVCMCtrol(0, 15, 1);
	} else if (value == 0x200) {
		CreateVCMCtrol(0x200, 4, 0);
		isHoldByNode = 0;
	} else if (value <= 0x3ff) {
		CreateVCMCtrol(value, 4, 1);
	} else {
		CAM_ERR(CAM_ACTUATOR, "moveVCM value invalid: %d", value);
	}

	return count;
}
static DEVICE_ATTR(moveVCM, 0664, moveVCM_show, moveVCM_store);

static void CreateVCMCtrol(int target, int step_num, int mode)
{
	struct cam_sensor_cci_client *cci_client;
	int i;

	if (!wide_a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "wide_a_ctrl is NULL");
		return;
	}

	cci_client = wide_a_ctrl->io_master_info.cci_client;
	if (!cci_client->sid) {
		cci_client->sid = 0xc;
		cci_client->i2c_freq_mode = I2C_FAST_MODE;
	}

	CAM_DBG(CAM_ACTUATOR,
		"wide_a_ctrl: salve_add[0x%x] i2c_freq_mode[%d] NAME[%s] power_status:%d poweroff_workStatus:%d movingStatus:%d",
		cci_client->sid, cci_client->i2c_freq_mode,
		wide_a_ctrl->soc_info.dev_name, power_status,
		poweroff_workStatus, movingStatus);

	if (is_actuator_power_by_cam == 1) {
		CAM_ERR(CAM_ACTUATOR, "camera is open, skip move af!!");
		return;
	}

	globalTarget = target;
	globalStepNum = step_num;
	if (mode & 1) {
		if (movingStatus == 1) {
			cancel_delayed_work(&poweroff_work);
			if (leftVibIsUsed == 1 && rightVibIsUsed == 1 &&
				!secondVibHasDelay) {
				leftVibIsUsed = 0;
				rightVibIsUsed = 0;
				secondVibHasDelay = 1;
				for (i = 0; i < 100; i++)
					udelay(1000);
			}
			return;
		}

		if (power_status == 1 && readCode() == target) {
			cancel_delayed_work(&poweroff_work);
			CAM_DBG(CAM_ACTUATOR,
				"vcm is on the target, no need move again");
			return;
		}

		if (poweroff_workStatus == 1)
			cancel_delayed_work(&poweroff_work);
		queue_delayed_work(system_wq, &poweron_work, 0);
		movingStatus = 1;
		for (i = 0; i < 100; i++)
			udelay(1000);
	} else {
		queue_delayed_work(system_wq, &poweroff_work,
			msecs_to_jiffies(375));
		poweroff_workStatus = 1;
	}
}

static void poweron_work_func(struct work_struct *work)
{
	static const u8 init_reg_addr[] = { 2, 3, 4, 6, 7, 8 };
	static const u8 init_reg_data[] = { 2, 2, 0, 4, 0x7c, 0x24 };
	static const u16 center_positions[] = { 0x19a, 0x168, 0x15e };
	struct cam_sensor_i2c_reg_array reg = {
		.reg_addr = 3,
	};
	struct cam_sensor_i2c_reg_setting setting = {
		.reg_setting = &reg,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.data_type = CAMERA_SENSOR_I2C_TYPE_WORD,
	};
	int target = globalTarget;
	int steps = globalStepNum;
	int step_size;
	int position;
	int delay_ms;
	int rc;
	int i;
	int j;

	if (!wide_a_ctrl)
		return;

	rc = cam_actuator_power_on_from_other(wide_a_ctrl);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR, "failed in actuator power up rc %d", rc);

	udelay(350);
	for (i = 0; i < ARRAY_SIZE(init_reg_addr); i++) {
		reg.reg_addr = init_reg_addr[i];
		reg.reg_data = init_reg_data[i];
		rc = camera_io_dev_write(&wide_a_ctrl->io_master_info, &setting);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "VCM init write failed: %d", rc);
	}

	udelay(350);
	reg.reg_addr = 3;
	for (i = 0; i < ARRAY_SIZE(center_positions); i++) {
		reg.reg_data = center_positions[i];
		rc = camera_io_dev_write(&wide_a_ctrl->io_master_info, &setting);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "VCM center write failed: %d", rc);
		for (j = 0; j < 24; j++)
			udelay(1000);
	}

	step_size = steps > 0 ? abs(target - 0x15e) / steps : 0;
	for (i = steps - 1; i >= 0; i--) {
		position = target >= 0x15f ?
			target - i * step_size : target + i * step_size;
		if (position < 0x8d)
			delay_ms = 5;
		else if (position < 0xf1)
			delay_ms = 10;
		else if (position < 0x123)
			delay_ms = 15;
		else
			delay_ms = 20;

		reg.reg_data = position;
		rc = camera_io_dev_write(&wide_a_ctrl->io_master_info, &setting);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "VCM write failed: %d", rc);
		for (j = 0; j < delay_ms; j++)
			udelay(1000);
	}

	if (readCode() != target) {
		for (i = 0; i < 10; i++)
			udelay(1000);
		reg.reg_data = target;
		rc = camera_io_dev_write(&wide_a_ctrl->io_master_info, &setting);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "VCM write failed: %d", rc);
		readCode();
	}
	movingStatus = 0;
}

static void poweroff_work_func(struct work_struct *work)
{
	bool poweroff_requested = poweroff_workStatus == 1;

	poweroff_workStatus = 0;
	leftVibIsUsed = 0;
	rightVibIsUsed = 0;
	secondVibHasDelay = 0;

	if (!poweroff_requested || power_status != 1) {
		CAM_DBG(CAM_ACTUATOR,
			"poweroff_work_func power off no need");
		return;
	}

	if (is_actuator_power_by_cam == 1) {
		CAM_DBG(CAM_ACTUATOR,
			"poweroff_work_func power off no need");
		return;
	}

	CAM_DBG(CAM_ACTUATOR, "poweroff_work_func power off need");
	if (wide_a_ctrl)
		cam_actuator_power_off_from_other(wide_a_ctrl);
}





static struct cam_i3c_actuator_data {
	struct cam_actuator_ctrl_t                  *a_ctrl;
	struct completion                            probe_complete;
} g_i3c_actuator_data[MAX_CAMERAS];

struct completion *cam_actuator_get_i3c_completion(uint32_t index)
{
	return &g_i3c_actuator_data[index].probe_complete;
}

static int cam_actuator_subdev_close_internal(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_actuator_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);

	if (!a_ctrl) {
#line 57 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_ERR(CAM_ACTUATOR, "a_ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	cam_actuator_shutdown(a_ctrl);
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return 0;
}

static int cam_actuator_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open();

	if (crm_active) {
#line 74 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_DBG(CAM_ACTUATOR,
			"CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return cam_actuator_subdev_close_internal(sd, fh);
}

static long cam_actuator_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_actuator_driver_cmd(a_ctrl, arg);
		if (rc) {
			if (rc == -EBADR)
				CAM_INFO(CAM_ACTUATOR,
					"Failed for driver_cmd: %d, it has been flushed",
					rc);
			else
				CAM_ERR(CAM_ACTUATOR,
					"Failed for driver_cmd: %d", rc);
		}
		break;
	case CAM_SD_SHUTDOWN:
		if (!cam_req_mgr_is_shutdown()) {
			CAM_ERR(CAM_CORE, "SD shouldn't come from user space");
			return 0;
		}

		rc = cam_actuator_subdev_close_internal(sd, NULL);
		break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid ioctl cmd: %u", cmd);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_actuator_init_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		cmd = VIDIOC_CAM_CONTROL;
		rc = cam_actuator_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in actuator subdev handling rc: %d",
				rc);
			return rc;
		}
		break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid compat ioctl: %d", cmd);
		rc = -ENOIOCTLCMD;
		break;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}
	return rc;
}
#endif

static struct v4l2_subdev_core_ops cam_actuator_subdev_core_ops = {
	.ioctl = cam_actuator_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_actuator_init_subdev_do_ioctl,
#endif
};

static struct v4l2_subdev_ops cam_actuator_subdev_ops = {
	.core = &cam_actuator_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_actuator_internal_ops = {
	.close = cam_actuator_subdev_close,
};

static int cam_actuator_init_subdev(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;

	a_ctrl->v4l2_dev_str.internal_ops =
		&cam_actuator_internal_ops;
	a_ctrl->v4l2_dev_str.ops =
		&cam_actuator_subdev_ops;
	strscpy(a_ctrl->device_name, CAMX_ACTUATOR_DEV_NAME,
		sizeof(a_ctrl->device_name));
	a_ctrl->v4l2_dev_str.name =
		a_ctrl->device_name;
	a_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	a_ctrl->v4l2_dev_str.ent_function =
		CAM_ACTUATOR_DEVICE_TYPE;
	a_ctrl->v4l2_dev_str.token = a_ctrl;
	a_ctrl->v4l2_dev_str.close_seq_prior =
		 CAM_SD_CLOSE_MEDIUM_PRIORITY;

	rc = cam_register_subdev(&(a_ctrl->v4l2_dev_str));
	if (rc)
		CAM_ERR(CAM_ACTUATOR,
			"Fail with cam_register_subdev rc: %d", rc);

	return rc;
}

static int cam_actuator_i2c_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int32_t                             rc = 0, i = 0;
	struct i2c_client                  *client;
	struct cam_actuator_ctrl_t         *a_ctrl;
	struct cam_hw_soc_info             *soc_info = NULL;
	struct cam_actuator_soc_private    *soc_private = NULL;
	struct timespec64                   ts_start, ts_end;
	long                                microsec = 0;
	struct device_node                 *np = NULL;
	const char                         *drv_name;
	struct cam_worker_wrapper_init_args worker_init_args = {0};

	CAM_GET_TIMESTAMP(ts_start);

	client = container_of(dev, struct i2c_client, dev);
	if (!client) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed to get i2c client");
		return -EFAULT;
	}

	/* Create sensor control structure */
	a_ctrl = CAM_MEM_ZALLOC(sizeof(*a_ctrl), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	a_ctrl->io_master_info.qup_client = CAM_MEM_ZALLOC(sizeof(
		struct cam_sensor_qup_client), GFP_KERNEL);
	if (!(a_ctrl->io_master_info.qup_client)) {
		rc = -ENOMEM;
		goto free_ctrl;
	}

	i2c_set_clientdata(client, a_ctrl);

	soc_private = CAM_MEM_ZALLOC(sizeof(struct cam_actuator_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_qup;
	}
	a_ctrl->soc_info.soc_private = soc_private;

	a_ctrl->io_master_info.qup_client->i2c_client = client;
	soc_info = &a_ctrl->soc_info;
	soc_info->dev = &client->dev;
	soc_info->dev_name = client->name;
	a_ctrl->io_master_info.master_type = I2C_MASTER;

	np = of_node_get(client->dev.of_node);
	drv_name = of_node_full_name(np);

	rc = cam_actuator_parse_dt(a_ctrl, &client->dev);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "failed: cam_actuator_parse_dt rc %d", rc);
		goto free_soc;
	}

	rc = cam_actuator_init_subdev(a_ctrl);
	if (rc)
		goto free_soc;

	if (soc_private->i2c_info.slave_addr != 0)
		a_ctrl->io_master_info.qup_client->i2c_client->addr =
			soc_private->i2c_info.slave_addr;

	a_ctrl->i2c_data.per_frame =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto unreg_subdev;
	}

	cam_sensor_module_add_i2c_device((void *) a_ctrl, CAM_SENSOR_ACTUATOR);

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));
	INIT_LIST_HEAD(&(a_ctrl->i2c_data.config_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	mutex_init(&a_ctrl->actuator_park_mutex);
	init_completion(&a_ctrl->park_lens_complete);

	/* Initialize mutex and read buf list */
	mutex_init(&(a_ctrl->actuator_mutex));
	INIT_LIST_HEAD(&(a_ctrl->read_buf_list));

	/* Create worker for actuator park with 1 task */
	worker_init_args.name = "cam_actuator_worker";
	worker_init_args.num_tasks = 1;
	worker_init_args.max_active = 0;
	worker_init_args.in_irq = WORKER_USAGE_NON_IRQ;
	worker_init_args.flag = 0;
	worker_init_args.priv_data = NULL;
	worker_init_args.index = 0;
	worker_init_args.worker_ctx_priv = &a_ctrl->worker_ctx;
	rc = cam_worker_wrapper_init(&worker_init_args, WORKER_CLASS_NRT);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Unable to create worker");
		goto free_and_destroy;
	}

	a_ctrl->bridge_intf.device_hdl = -1;
	a_ctrl->bridge_intf.link_hdl = -1;
	a_ctrl->bridge_intf.ops.get_dev_info =
		cam_actuator_publish_dev_info;
	a_ctrl->bridge_intf.ops.link_setup =
		cam_actuator_establish_link;
	a_ctrl->bridge_intf.ops.apply_req =
		cam_actuator_apply_request;
	a_ctrl->last_flush_req = 0;
	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
	CAM_GET_TIMESTAMP(ts_end);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts_start, ts_end, microsec);
	cam_record_bind_latency(drv_name, microsec);
	of_node_put(np);

	return rc;

free_and_destroy:
	CAM_MEM_FREE(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	mutex_destroy(&a_ctrl->actuator_park_mutex);
	mutex_destroy(&(a_ctrl->actuator_mutex));
unreg_subdev:
	cam_unregister_subdev(&(a_ctrl->v4l2_dev_str));
free_soc:
	CAM_MEM_FREE(soc_private);
free_qup:
	CAM_MEM_FREE(a_ctrl->io_master_info.qup_client);
free_ctrl:
	CAM_MEM_FREE(a_ctrl);
	return rc;
}

static void cam_actuator_i2c_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct i2c_client               *client = NULL;
	struct cam_actuator_ctrl_t      *a_ctrl = NULL;

	client = container_of(dev, struct i2c_client, dev);
	if (!client) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed to get i2c client");
		return;
	}

	a_ctrl = i2c_get_clientdata(client);
	/* Handle I2C Devices */
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Actuator device is NULL");
		return;
	}

	CAM_INFO(CAM_ACTUATOR, "i2c remove invoked");
	mutex_lock(&(a_ctrl->actuator_mutex));
	cam_actuator_shutdown(a_ctrl);
	mutex_unlock(&(a_ctrl->actuator_mutex));
	cam_unregister_subdev(&(a_ctrl->v4l2_dev_str));
	cam_worker_wrapper_deinit(a_ctrl->worker_ctx);
	mutex_destroy(&a_ctrl->actuator_park_mutex);
	mutex_destroy(&(a_ctrl->actuator_mutex));
	cam_sensor_util_release_resources(&(a_ctrl->io_master_info), &(a_ctrl->soc_info));

	/*Free Allocated Mem */
	CAM_MEM_FREE(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	v4l2_set_subdevdata(&a_ctrl->v4l2_dev_str.sd, NULL);
	CAM_MEM_FREE(a_ctrl);
}

const static struct component_ops cam_actuator_i2c_component_ops = {
	.bind = cam_actuator_i2c_component_bind,
	.unbind = cam_actuator_i2c_component_unbind,
};

#if KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE
static int cam_actuator_driver_i2c_probe(struct i2c_client *client)
{
	int rc = 0;

	if (client == NULL) {
#line 387 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_ERR(CAM_ACTUATOR, "Invalid Args client: %pK",
			client);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
#line 345 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_ERR(CAM_ACTUATOR, "%s :: i2c_check_functionality failed",
			 client->name);
		return -EFAULT;
	}

#line 374 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
	CAM_DBG(CAM_ACTUATOR, "Adding sensor actuator component");

	cam_soc_util_initialize_power_domain(&client->dev);

	rc = component_add(&client->dev, &cam_actuator_i2c_component_ops);
	if (rc)
#line 377 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_ERR(CAM_ACTUATOR, "failed to add component rc: %d", rc);

#line 470 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
	return rc;
}
#else
static int32_t cam_actuator_driver_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;

	if (client == NULL || id == NULL) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args client: %pK id: %pK",
			client, id);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CAM_ERR(CAM_ACTUATOR, "%s :: i2c_check_functionality failed",
			 client->name);
		return -EFAULT;
	}

	CAM_DBG(CAM_ACTUATOR, "Adding sensor actuator component");

	cam_soc_util_initialize_power_domain(&client->dev);

	rc = component_add(&client->dev, &cam_actuator_i2c_component_ops);
	if (rc)
		CAM_ERR(CAM_ACTUATOR, "failed to add component rc: %d", rc);

	return rc;
}
#endif

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int32_t cam_actuator_driver_i2c_remove(
	struct i2c_client *client)
#else
void cam_actuator_driver_i2c_remove(
	struct i2c_client *client)
#endif
{
	component_del(&client->dev, &cam_actuator_i2c_component_ops);

	cam_soc_util_uninitialize_power_domain(&client->dev);

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static int cam_actuator_platform_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int32_t                             rc = 0;
	int32_t                             i = 0;
	struct cam_actuator_ctrl_t         *a_ctrl = NULL;
	struct cam_actuator_soc_private    *soc_private = NULL;
	struct platform_device             *pdev = to_platform_device(dev);
	struct timespec64                   ts_start, ts_end;
	long                                microsec = 0;
	struct cam_worker_wrapper_init_args worker_init_args = {0};

	CAM_GET_TIMESTAMP(ts_start);

	/* Create actuator control structure */
	a_ctrl = devm_kzalloc(&pdev->dev,
		sizeof(struct cam_actuator_ctrl_t), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	/*fill in platform device*/
	a_ctrl->v4l2_dev_str.pdev = pdev;
	a_ctrl->soc_info.pdev = pdev;
	a_ctrl->soc_info.dev = &pdev->dev;
	a_ctrl->soc_info.dev_name = pdev->name;
	a_ctrl->io_master_info.master_type = CCI_MASTER;

	a_ctrl->io_master_info.cci_client = CAM_MEM_ZALLOC(sizeof(
		struct cam_sensor_cci_client), GFP_KERNEL);
	if (!(a_ctrl->io_master_info.cci_client)) {
		rc = -ENOMEM;
		goto free_ctrl;
	}

	soc_private = CAM_MEM_ZALLOC(sizeof(struct cam_actuator_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_cci_client;
	}
	a_ctrl->soc_info.soc_private = soc_private;
	soc_private->power_info.dev = &pdev->dev;

	a_ctrl->i2c_data.per_frame =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto free_soc;
	}

	cam_sensor_module_add_i2c_device((void *) a_ctrl, CAM_SENSOR_ACTUATOR);

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));
	INIT_LIST_HEAD(&(a_ctrl->i2c_data.config_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	mutex_init(&a_ctrl->actuator_park_mutex);
	init_completion(&a_ctrl->park_lens_complete);

	/* Initialize mutex and read buf list */
	mutex_init(&(a_ctrl->actuator_mutex));
	INIT_LIST_HEAD(&(a_ctrl->read_buf_list));

	/* Create worker for actuator park with 1 task */
	worker_init_args.name = "cam_actuator_worker";
	worker_init_args.num_tasks = 1;
	worker_init_args.max_active = 0;
	worker_init_args.in_irq = WORKER_USAGE_NON_IRQ;
	worker_init_args.flag = 0;
	worker_init_args.priv_data = NULL;
	worker_init_args.index = 0;
	worker_init_args.worker_ctx_priv = &a_ctrl->worker_ctx;
	rc = cam_worker_wrapper_init(&worker_init_args, WORKER_CLASS_NRT);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Unable to create worker");
		goto free_and_destroy;
	}

	rc = cam_actuator_parse_dt(a_ctrl, &(pdev->dev));
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Paring actuator dt failed rc %d", rc);
		goto destroy_worker;
	}

	/* Fill platform device id*/
	pdev->id = a_ctrl->soc_info.index;

	rc = cam_actuator_init_subdev(a_ctrl);
	if (rc)
		goto destroy_worker;

	a_ctrl->bridge_intf.device_hdl = -1;
	a_ctrl->bridge_intf.link_hdl = -1;
	a_ctrl->bridge_intf.ops.get_dev_info =
		cam_actuator_publish_dev_info;
	a_ctrl->bridge_intf.ops.link_setup =
		cam_actuator_establish_link;
	a_ctrl->bridge_intf.ops.apply_req =
		cam_actuator_apply_request;
	a_ctrl->bridge_intf.ops.flush_req =
		cam_actuator_flush_request;
	a_ctrl->last_flush_req = 0;

	platform_set_drvdata(pdev, a_ctrl);
	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
	CAM_DBG(CAM_ACTUATOR, "Component bound successfully %d",
		a_ctrl->soc_info.index);

	wide_a_ctrl = a_ctrl;
	rc = sysfs_create_file(&pdev->dev.kobj, &dev_attr_moveVCM.attr);
	INIT_DELAYED_WORK(&poweron_work, poweron_work_func);
	INIT_DELAYED_WORK(&poweroff_work, poweroff_work_func);
	g_i3c_actuator_data[a_ctrl->soc_info.index].a_ctrl = a_ctrl;
	init_completion(&g_i3c_actuator_data[a_ctrl->soc_info.index].probe_complete);
	CAM_GET_TIMESTAMP(ts_end);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts_start, ts_end, microsec);
	cam_record_bind_latency(pdev->name, microsec);

	return rc;

destroy_worker:
	cam_worker_wrapper_deinit(a_ctrl->worker_ctx);
free_and_destroy:
	CAM_MEM_FREE(a_ctrl->i2c_data.per_frame);
	mutex_destroy(&a_ctrl->actuator_park_mutex);
	mutex_destroy(&(a_ctrl->actuator_mutex));
free_soc:
	CAM_MEM_FREE(soc_private);
free_cci_client:
	CAM_MEM_FREE(a_ctrl->io_master_info.cci_client);
free_ctrl:
	devm_kfree(&pdev->dev, a_ctrl);
	return rc;
}

static void cam_actuator_platform_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_actuator_ctrl_t      *a_ctrl;
	struct platform_device *pdev = to_platform_device(dev);

	a_ctrl = platform_get_drvdata(pdev);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Actuator device is NULL");
		return;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	cam_actuator_shutdown(a_ctrl);
	mutex_unlock(&(a_ctrl->actuator_mutex));
	cam_unregister_subdev(&(a_ctrl->v4l2_dev_str));
	if (wide_a_ctrl)
		wide_a_ctrl = NULL;
	cam_worker_wrapper_deinit(a_ctrl->worker_ctx);
	mutex_destroy(&a_ctrl->actuator_park_mutex);
	mutex_destroy(&(a_ctrl->actuator_mutex));
	cam_sensor_util_release_resources(&(a_ctrl->io_master_info), &(a_ctrl->soc_info));
	CAM_MEM_FREE(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	v4l2_set_subdevdata(&a_ctrl->v4l2_dev_str.sd, NULL);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, a_ctrl);
	CAM_INFO(CAM_ACTUATOR, "Actuator component unbinded");
}

const static struct component_ops cam_actuator_platform_component_ops = {
	.bind = cam_actuator_platform_component_bind,
	.unbind = cam_actuator_platform_component_unbind,
};

#if KERNEL_VERSION(6, 10, 0) > LINUX_VERSION_CODE
static int32_t cam_actuator_platform_remove(
	struct platform_device *pdev)
#else
static void cam_actuator_platform_remove(
	struct platform_device *pdev)
#endif
{
	component_del(&pdev->dev, &cam_actuator_platform_component_ops);
	unregister_haptic_notify(&nb);

	cam_soc_util_uninitialize_power_domain(&pdev->dev);

#if KERNEL_VERSION(6, 10, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id cam_actuator_driver_dt_match[] = {
	{.compatible = "qcom,actuator"},
	{}
};

#line 983 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
static int32_t cam_actuator_driver_platform_probe(
	struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_ACTUATOR, "Adding sensor actuator component");

	cam_soc_util_initialize_power_domain(&pdev->dev);

	rc = component_add(&pdev->dev, &cam_actuator_platform_component_ops);
	if (rc)
		CAM_ERR(CAM_ACTUATOR, "failed to add component rc: %d", rc);
	nb.notifier_call = haptic_callback;
	register_haptic_notify(&nb);
	return rc;
}

MODULE_DEVICE_TABLE(of, cam_actuator_driver_dt_match);

struct platform_driver cam_actuator_platform_driver = {
	.probe = cam_actuator_driver_platform_probe,
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = cam_actuator_driver_dt_match,
		.suppress_bind_attrs = true,
	},
	.remove = cam_actuator_platform_remove,
};

static const struct i2c_device_id i2c_id[] = {
	{ACTUATOR_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static const struct of_device_id cam_actuator_i2c_driver_dt_match[] = {
	{.compatible = "qcom,cam-i2c-actuator"},
	{}
};
MODULE_DEVICE_TABLE(of, cam_actuator_i2c_driver_dt_match);

struct i2c_driver cam_actuator_i2c_driver = {
	.id_table = i2c_id,
	.probe  = cam_actuator_driver_i2c_probe,
	.remove = cam_actuator_driver_i2c_remove,
	.driver = {
		.of_match_table = cam_actuator_i2c_driver_dt_match,
		.owner = THIS_MODULE,
		.name = ACTUATOR_DRIVER_I2C,
		.suppress_bind_attrs = true,
	},
};

static struct i3c_device_id actuator_i3c_id[MAX_I3C_DEVICE_ID_ENTRIES + 1];

#line 1039 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
static int cam_actuator_i3c_driver_probe(struct i3c_device *client)
{
	int32_t                          rc = 0;
	struct cam_actuator_ctrl_t       *a_ctrl = NULL;
	uint32_t                          index;
	struct device                    *dev;

	if (!client) {
		CAM_INFO(CAM_ACTUATOR, "Null Client pointer");
		return -EINVAL;
	}

	dev = &client->dev;

	CAM_DBG(CAM_ACTUATOR, "Probe for I3C Slave %s", dev_name(dev));

	rc = of_property_read_u32(dev->of_node, "cell-index", &index);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "device %s failed to read cell-index", dev_name(dev));
		return rc;
	}

	if (index >= MAX_CAMERAS) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Cell-Index: %u for %s", index, dev_name(dev));
		return -EINVAL;
	}

	a_ctrl = g_i3c_actuator_data[index].a_ctrl;
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR,
			"a_ctrl is null. I3C Probe before platfom driver probe for %s",
			dev_name(dev));
		return -EINVAL;
	}
	cam_sensor_utils_parse_pm_ctrl_flag(dev->of_node, &(a_ctrl->io_master_info));

	CAM_INFO(CAM_SENSOR,
		"master: %d (1-CCI, 2-I2C, 3-SPI, 4-I3C) pm_ctrl_client_enable: %d",
		a_ctrl->io_master_info.master_type,
		a_ctrl->io_master_info.qup_client->pm_ctrl_client_enable);

	a_ctrl->io_master_info.qup_client->i3c_client = client;
	a_ctrl->io_master_info.qup_client->i3c_wait_for_hotjoin = false;

	complete_all(&g_i3c_actuator_data[index].probe_complete);

	CAM_DBG(CAM_ACTUATOR, "I3C Probe Finished for %s", dev_name(dev));
	return rc;
}

#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
static void cam_i3c_driver_remove(struct i3c_device *client)
{
	int32_t                        rc = 0;
	struct cam_actuator_ctrl_t     *a_ctrl = NULL;
	struct device                  *dev;
	uint32_t                       index;

	if (!client) {
		CAM_ERR(CAM_SENSOR, "I3C Driver Remove: Invalid input args");
		return;
	}

	dev = &client->dev;

	CAM_DBG(CAM_SENSOR, "driver remove for I3C Slave %s", dev_name(dev));

	rc = of_property_read_u32(dev->of_node, "cell-index", &index);
	if (rc) {
		CAM_ERR(CAM_UTIL, "device %s failed to read cell-index", dev_name(dev));
		return;
	}

	if (index >= MAX_CAMERAS) {
		CAM_ERR(CAM_SENSOR, "Invalid Cell-Index: %u for %s", index, dev_name(dev));
		return;
	}

	a_ctrl = g_i3c_actuator_data[index].a_ctrl;
	if (!a_ctrl) {
		CAM_ERR(CAM_SENSOR, "a_ctrl is null. I3C Probe before platfom driver probe for %s",
			dev_name(dev));
		return;
	}

	CAM_DBG(CAM_SENSOR, "I3C remove invoked for %s",
		(client ? dev_name(&client->dev) : "none"));
	CAM_MEM_FREE(a_ctrl->io_master_info.qup_client);
	a_ctrl->io_master_info.qup_client = NULL;
}

#else
static int cam_i3c_driver_remove(struct i3c_device *client)
{
	struct cam_actuator_ctrl_t     *a_ctrl = NULL;
	struct device                  *dev;
	uint32_t                       index;

	if (!client) {
		CAM_ERR(CAM_SENSOR, "I3C Driver Remove: Invalid input args");
		return -EINVAL;
	}

	dev = &client->dev;

	CAM_DBG(CAM_SENSOR, "driver remove for I3C Slave %s", dev_name(dev));

	rc = of_property_read_u32(dev->of_node, "cell-index", &index);
	if (rc) {
		CAM_ERR(CAM_UTIL, "device %s failed to read cell-index", dev_name(dev));
		return -EINVAL;
	}

	if (index >= MAX_CAMERAS) {
		CAM_ERR(CAM_SENSOR, "Invalid Cell-Index: %u for %s", index, dev_name(dev));
		return -EINVAL;
	}

	a_ctrl = g_i3c_actuator_data[index].a_ctrl;
	if (!a_ctrl) {
		CAM_ERR(CAM_SENSOR, "a_ctrl is null. I3C Probe before platfom driver probe for %s",
			dev_name(dev));
		return -EINVAL;
	}

	CAM_DBG(CAM_SENSOR, "I3C remove invoked for %s",
		(client ? dev_name(&client->dev) : "none"));
	CAM_MEM_FREE(a_ctrl->io_master_info.qup_client);
	a_ctrl->io_master_info.qup_client = NULL;
	return 0;
}
#endif

static struct i3c_driver cam_actuator_i3c_driver = {
	.id_table = actuator_i3c_id,
	.probe = cam_actuator_i3c_driver_probe,
	.remove = cam_i3c_driver_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = ACTUATOR_DRIVER_I3C,
		.of_match_table = cam_actuator_driver_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_actuator_driver_init(void)
{
	int32_t rc = 0;
	struct device_node                      *dev;
	int num_entries = 0;

	rc = platform_driver_register(&cam_actuator_platform_driver);
	if (rc < 0) {
#line 1192 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_ERR(CAM_ACTUATOR,
			"platform_driver_register failed rc = %d", rc);
		return rc;
	}

	rc = i2c_add_driver(&cam_actuator_i2c_driver);
	if (rc) {
#line 1199 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_ERR(CAM_ACTUATOR, "i2c_add_driver failed rc = %d", rc);
		goto i2c_register_err;
	}

	memset(actuator_i3c_id, 0, sizeof(struct i3c_device_id) * (MAX_I3C_DEVICE_ID_ENTRIES + 1));

	dev = of_find_node_by_path(I3C_SENSOR_DEV_ID_DT_PATH);
	if (!dev) {
#line 1207 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_DBG(CAM_ACTUATOR, "Couldnt Find the i3c-id-table dev node");
		return 0;
	}

	rc = cam_sensor_count_elems_i3c_device_id(dev, &num_entries,
		"i3c-actuator-id-table");
	if (rc)
		return 0;

	rc = cam_sensor_fill_i3c_device_id(dev, num_entries,
		"i3c-actuator-id-table", actuator_i3c_id);
	if (rc)
		goto i3c_register_err;

	rc = i3c_driver_register_with_owner(&cam_actuator_i3c_driver, THIS_MODULE);
	if (rc) {
#line 1223 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_ERR(CAM_ACTUATOR, "i3c_driver registration failed, rc: %d", rc);
		goto i3c_register_err;
	}

	return 0;

i3c_register_err:
	i2c_del_driver(&cam_actuator_i2c_driver);
i2c_register_err:
	platform_driver_unregister(&cam_actuator_platform_driver);

	return rc;
}

void cam_actuator_driver_exit(void)
{
	struct device_node *dev;

	platform_driver_unregister(&cam_actuator_platform_driver);
	i2c_del_driver(&cam_actuator_i2c_driver);

	dev = of_find_node_by_path(I3C_SENSOR_DEV_ID_DT_PATH);
	if (!dev) {
#line 1246 "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c"
		CAM_DBG(CAM_ACTUATOR, "Couldnt Find the i3c-id-table dev node");
		return;
	}

	i3c_driver_unregister(&cam_actuator_i3c_driver);
}

MODULE_DESCRIPTION("cam_actuator_driver");
MODULE_LICENSE("GPL v2");
