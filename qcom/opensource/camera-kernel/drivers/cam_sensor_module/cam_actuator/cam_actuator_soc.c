// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>
#include "cam_actuator_soc.h"
#include "cam_soc_util.h"

int32_t cam_actuator_parse_dt(struct cam_actuator_ctrl_t *a_ctrl,
	struct device *dev)
{
	int32_t                          rc = 0;
	struct cam_hw_soc_info          *soc_info = &a_ctrl->soc_info;
	struct cam_actuator_soc_private *soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t  *power_info = &soc_private->power_info;

	rc = cam_sensor_util_parse_and_request_resources(&(a_ctrl->io_master_info),
		soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "Failed in parse_and_request_resources rc : %d", rc);
		return rc;
	}

	if (!soc_info->gpio_data) {
		CAM_DBG(CAM_ACTUATOR, "No GPIO found");
		rc = 0;
		return rc;
	}

	if (!soc_info->gpio_data->cam_gpio_common_tbl_size) {
		CAM_DBG(CAM_ACTUATOR, "No GPIO found");
		return -EINVAL;
	}

	rc = cam_sensor_util_init_gpio_pin_tbl(soc_info,
		&power_info->gpio_num_info);
	if ((rc < 0) || (!power_info->gpio_num_info)) {
		CAM_ERR(CAM_ACTUATOR, "No/Error Actuator GPIOs");
		return -EINVAL;
	}
	return rc;
}
