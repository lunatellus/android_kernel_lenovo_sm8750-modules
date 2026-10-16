// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/slab.h>
#include <media/cam_isp.h>
#include "cam_io_util.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_soc.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver3.h"
#include "cam_irq_controller.h"
#include "cam_vfe_camif_ver3.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_cpas_api.h"
#include "cam_trace.h"
#include "cam_mem_mgr_api.h"
#include "cam_worker_wrapper_api.h"
#include "cam_vfe_top_common.h"

#define CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX 2

struct cam_vfe_mux_camif_ver3_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_camif_ver3_pp_clc_reg        *camif_reg;
	struct cam_vfe_top_ver3_reg_offset_common   *common_reg;
	struct cam_vfe_camif_ver3_reg_data          *reg_data;
	struct cam_vfe_camif_ver3_hw_info           *hw_info;
	struct cam_hw_soc_info                      *soc_info;
	struct cam_vfe_camif_common_cfg             cam_common_cfg;
	struct cam_vfe_top_ver3_priv                *top_priv;

	cam_hw_mgr_event_cb_func             event_cb;
	void                                *priv;
	int                                  irq_err_handle;
	int                                  irq_handle;
	int                                  sof_irq_handle;
	void                                *vfe_irq_controller;
	struct cam_vfe_top_irq_evt_payload   evt_payload[CAM_VFE_CAMIF_EVT_MAX];
	struct list_head                     free_payload_list;
	spinlock_t                           spin_lock;

	enum cam_isp_hw_sync_mode          sync_mode;
	uint32_t                           dsp_mode;
	uint32_t                           pix_pattern;
	uint32_t                           first_pixel;
	uint32_t                           first_line;
	uint32_t                           last_pixel;
	uint32_t                           last_line;
	uint32_t                           hbi_value;
	uint32_t                           vbi_value;
	bool                               enable_sof_irq_debug;
	uint32_t                           irq_debug_cnt;
	uint32_t                           camif_debug;
	uint32_t                           horizontal_bin;
	uint32_t                           qcfa_bin;
	uint32_t                           dual_hw_idx;
	uint32_t                           is_dual;
	uint32_t                           no_payload_err_cnt;
	bool                               is_fe_enabled;
	bool                               is_offline;
	struct timespec64                  sof_ts;
	struct timespec64                  epoch_ts;
	struct timespec64                  eof_ts;
	struct timespec64                  error_ts;
	uint64_t                           path_reg_base;
	uint8_t                            log_buf[CAM_VFE_LEN_LOG_BUF];
};

static int cam_vfe_camif_ver3_get_evt_payload(
	struct cam_vfe_mux_camif_ver3_data     *camif_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	int rc = 0;

	spin_lock(&camif_priv->spin_lock);
	if (list_empty(&camif_priv->free_payload_list)) {
		if (camif_priv->no_payload_err_cnt == U32_MAX - 1)
			camif_priv->no_payload_err_cnt = 0;
		if (!(camif_priv->no_payload_err_cnt++ % CAM_VFE_GET_PAYLOAD_ERR_MAX)) {
			CAM_ERR(CAM_ISP, "No free CAMIF event payload, no_payload_err_cnt:%u",
			camif_priv->no_payload_err_cnt);
		}
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(&camif_priv->free_payload_list,
		struct cam_vfe_top_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
done:
	spin_unlock(&camif_priv->spin_lock);
	return rc;
}

static int cam_vfe_camif_ver3_put_evt_payload(
	struct cam_vfe_mux_camif_ver3_data     *camif_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	unsigned long flags = 0;

	if (!camif_priv) {
		CAM_ERR(CAM_ISP, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}

	CAM_COMMON_SANITIZE_LIST_ENTRY((*evt_payload), struct cam_vfe_top_irq_evt_payload);
	spin_lock_irqsave(&camif_priv->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list, &camif_priv->free_payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&camif_priv->spin_lock, flags);

	return 0;
}

static int cam_vfe_camif_ver3_err_irq_top_half(
	uint32_t                               evt_id,
	struct cam_irq_th_payload             *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_node;
	struct cam_vfe_mux_camif_ver3_data    *camif_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;
	struct cam_vfe_top_ver3_reg_offset_common   *common_reg;
	bool                                   error_flag = false;

	camif_node = th_payload->handler_priv;
	camif_priv = camif_node->res_priv;
	common_reg = camif_priv->common_reg;

	/*
	 *  need to handle overflow condition here, otherwise irq storm
	 *  will block everything
	 */
	if (th_payload->evt_status_arr[2] || (th_payload->evt_status_arr[0] &
		camif_priv->reg_data->error_irq_mask0)) {
		CAM_ERR(CAM_ISP,
			"VFE:%d CAMIF Err IRQ status_0: 0x%X status_2: 0x%X",
			camif_node->hw_intf->hw_idx,
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[2]);
		CAM_ERR(CAM_ISP, "Stopping further IRQ processing from VFE:%d",
			camif_node->hw_intf->hw_idx);
		cam_irq_controller_disable_all(
			camif_priv->vfe_irq_controller);
		error_flag = true;
	}

	rc  = cam_vfe_camif_ver3_get_evt_payload(camif_priv, &evt_payload);
	if (rc)
		return rc;

	cam_isp_hw_get_timestamp(&evt_payload->ts);
	if (error_flag) {
		camif_priv->error_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		camif_priv->error_ts.tv_nsec =
			evt_payload->ts.mono_time.tv_nsec;
	}

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->irq_reg_val[i] = cam_io_r(camif_priv->mem_base + common_reg->top_hm_base +
		camif_priv->common_reg->violation_status);

	evt_payload->irq_reg_val[++i] = cam_io_r(camif_priv->mem_base + common_reg->bus_wr_base +
		camif_priv->common_reg->bus_overflow_status);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_vfe_camif_ver3_validate_pix_pattern(uint32_t pattern,
	struct cam_vfe_mux_camif_ver3_data *camif_data)
{
	int rc;

	switch (pattern) {
	case CAM_ISP_PATTERN_BAYER_RGRGRG:
	case CAM_ISP_PATTERN_BAYER_GRGRGR:
	case CAM_ISP_PATTERN_BAYER_BGBGBG:
	case CAM_ISP_PATTERN_BAYER_GBGBGB:
		camif_data->cam_common_cfg.input_pp_fmt =
			camif_data->reg_data->input_bayer_fmt;
		rc = 0;
		break;
	case CAM_ISP_PATTERN_YUV_YCBYCR:
	case CAM_ISP_PATTERN_YUV_YCRYCB:
	case CAM_ISP_PATTERN_YUV_CBYCRY:
	case CAM_ISP_PATTERN_YUV_CRYCBY:
		rc = 0;
		camif_data->cam_common_cfg.input_pp_fmt =
			camif_data->reg_data->input_yuv_fmt;
		break;
	default:
		CAM_ERR(CAM_ISP, "Error, Invalid pix pattern:%d", pattern);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int cam_vfe_camif_ver3_get_reg_update(
	struct cam_isp_resource_node  *camif_res,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                           size = 0;
	uint32_t                           reg_val_pair[2];
	struct cam_isp_hw_get_cmd_update   *cdm_args = cmd_args;
	struct cam_cdm_utils_ops           *cdm_util_ops = NULL;
	struct cam_vfe_mux_camif_ver3_data *rsrc_data = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Invalid arg size: %d expected:%ld",
			arg_size, sizeof(struct cam_isp_hw_get_cmd_update));
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res) {
		CAM_ERR(CAM_ISP, "Invalid args: %pK", cdm_args);
		return -EINVAL;
	}

	cdm_util_ops = (struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, (size*4));
		return -EINVAL;
	}

	rsrc_data = camif_res->res_priv;
	reg_val_pair[0] = rsrc_data->common_reg->top_hm_base +
		rsrc_data->common_reg->reg_update_cmd;
	reg_val_pair[1] = rsrc_data->reg_data->reg_update_cmd_data;
	CAM_DBG(CAM_ISP, "VFE:%d CAMIF reg_update_cmd 0x%X offset 0x%X",
		camif_res->hw_intf->hw_idx,
		reg_val_pair[1], reg_val_pair[0]);

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->cmd.used_bytes = size * 4;

	return 0;
}

int cam_vfe_camif_ver3_acquire_resource(
	struct cam_isp_resource_node  *camif_res,
	void                          *acquire_param)
{
	struct cam_vfe_mux_camif_ver3_data    *camif_data;
	struct cam_vfe_acquire_args           *acquire_data;
	int                                    rc = 0;

	camif_data  = (struct cam_vfe_mux_camif_ver3_data *)
		camif_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args *)acquire_param;

	rc = cam_vfe_camif_ver3_validate_pix_pattern(
		acquire_data->vfe_in.in_port->test_pattern, camif_data);

	if (rc) {
		CAM_ERR(CAM_ISP, "Validate pix pattern failed, rc = %d", rc);
		return rc;
	}

	camif_data->sync_mode      = acquire_data->vfe_in.sync_mode;
	camif_data->pix_pattern    = acquire_data->vfe_in.in_port->test_pattern;
	camif_data->dsp_mode       = acquire_data->vfe_in.in_port->dsp_mode;
	camif_data->first_pixel    = acquire_data->vfe_in.in_port->left_start;
	camif_data->last_pixel     = acquire_data->vfe_in.in_port->left_stop;
	camif_data->first_line     = acquire_data->vfe_in.in_port->line_start;
	camif_data->last_line      = acquire_data->vfe_in.in_port->line_stop;
	camif_data->is_fe_enabled  = acquire_data->vfe_in.is_fe_enabled;
	camif_data->is_offline     = acquire_data->vfe_in.is_offline;
	camif_data->is_dual        = acquire_data->vfe_in.is_dual;
	camif_data->event_cb       = acquire_data->event_cb;
	camif_data->priv           = acquire_data->priv;
	camif_data->qcfa_bin       = acquire_data->vfe_in.in_port->qcfa_bin;
	camif_data->horizontal_bin =
		acquire_data->vfe_in.in_port->horizontal_bin;
	camif_data->hbi_value      = 0;
	camif_data->vbi_value      = 0;

	if (camif_data->is_dual)
		camif_data->dual_hw_idx = acquire_data->vfe_in.dual_hw_idx;

	CAM_DBG(CAM_ISP,
		"VFE:%d CAMIF pix_pattern:%d dsp_mode=%d is_dual:%d dual_hw_idx:%d",
		camif_res->hw_intf->hw_idx,
		camif_data->pix_pattern, camif_data->dsp_mode,
		camif_data->is_dual, camif_data->dual_hw_idx);

	return rc;
}

static int cam_vfe_camif_ver3_resource_init(
	struct cam_isp_resource_node *camif_res,
	void *init_args, uint32_t arg_size)
{
	struct cam_vfe_mux_camif_ver3_data    *camif_data;
	struct cam_hw_soc_info                *soc_info;
	int                                    rc = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_data = (struct cam_vfe_mux_camif_ver3_data *)
		camif_res->res_priv;

	soc_info = camif_data->soc_info;

	if ((camif_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		rc = cam_vfe_soc_enable_clk(soc_info, CAM_VFE_DSP_CLK_NAME);
		if (rc)
			CAM_ERR(CAM_ISP,
				"failed to enable dsp clk, rc = %d", rc);
	}
	camif_data->sof_ts.tv_sec = 0;
	camif_data->sof_ts.tv_nsec = 0;
	camif_data->epoch_ts.tv_sec = 0;
	camif_data->epoch_ts.tv_nsec = 0;
	camif_data->eof_ts.tv_sec = 0;
	camif_data->eof_ts.tv_nsec = 0;
	camif_data->error_ts.tv_sec = 0;
	camif_data->error_ts.tv_nsec = 0;

	return rc;
}

static int cam_vfe_camif_ver3_resource_deinit(
	struct cam_isp_resource_node        *camif_res,
	void *init_args, uint32_t arg_size)
{
	struct cam_vfe_mux_camif_ver3_data    *camif_data;
	struct cam_hw_soc_info           *soc_info;
	int rc = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_data   = (struct cam_vfe_mux_camif_ver3_data *)
		camif_res->res_priv;

	soc_info = camif_data->soc_info;

	if ((camif_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		rc = cam_vfe_soc_disable_clk(soc_info, CAM_VFE_DSP_CLK_NAME);
		if (rc)
			CAM_ERR(CAM_ISP, "failed to disable dsp clk");
	}

	return rc;
}

static int cam_vfe_camif_ver3_resource_start(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_ver3_data  *rsrc_data;
	uint32_t                             val = 0;
	uint32_t                             epoch0_line_cfg;
	uint32_t                             epoch1_line_cfg;
	uint32_t                             computed_epoch_line_cfg;
	uint32_t                        err_irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	uint32_t                        irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	struct cam_vfe_soc_private          *soc_private;
	void __iomem                        *mem_base;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	if (camif_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error, Invalid camif res res_state:%d",
			camif_res->res_state);
		return -EINVAL;
	}

	memset(err_irq_mask, 0, sizeof(err_irq_mask));
	memset(irq_mask, 0, sizeof(irq_mask));

	rsrc_data = (struct cam_vfe_mux_camif_ver3_data *)camif_res->res_priv;
	mem_base  = rsrc_data->mem_base + rsrc_data->common_reg->top_hm_base;
	soc_private = rsrc_data->soc_info->soc_private;

	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error, soc_private NULL");
		return -ENODEV;
	}

	/* config debug status registers */
	val = cam_io_r(mem_base + rsrc_data->common_reg->top_debug_cfg);
	val |= rsrc_data->reg_data->top_debug_cfg_en;
	cam_io_w_mb(val, mem_base + rsrc_data->common_reg->top_debug_cfg);

	if (rsrc_data->common_reg->capabilities & CAM_VFE_COMMON_CAP_SKIP_CORE_CFG)
		goto skip_core_cfg;

	val = cam_io_r_mb(mem_base + rsrc_data->common_reg->core_cfg_0);

	/* AF stitching by hw disabled by default
	 * PP CAMIF currently operates only in offline mode
	 */

	if ((rsrc_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(rsrc_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		/* DSP mode reg val is CAM_ISP_DSP_MODE - 1 */
		if (camif_res->hw_intf->hw_idx != CAM_ISP_HW_VFE_CORE_2) {
			val |= (((rsrc_data->dsp_mode - 1) &
				rsrc_data->reg_data->dsp_mode_mask) <<
				rsrc_data->reg_data->dsp_mode_shift);
			val |= (0x1 << rsrc_data->reg_data->dsp_en_shift);
		} else {
			CAM_ERR(CAM_ISP, "Error, HVX not available for IFE_%d",
				camif_res->hw_intf->hw_idx);
			return -EINVAL;
		}
	}

	if (rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		val |= (1 << rsrc_data->reg_data->pp_extern_reg_update_shift);

	if ((rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE) ||
		(rsrc_data->sync_mode == CAM_ISP_HW_SYNC_MASTER))
		val |= (1 << rsrc_data->reg_data->dual_ife_pix_en_shift);

	val |= (~rsrc_data->cam_common_cfg.vid_ds16_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_VID_DS16_R2PD;
	val |= (~rsrc_data->cam_common_cfg.vid_ds4_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_VID_DS4_R2PD;
	val |= (~rsrc_data->cam_common_cfg.disp_ds16_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_DISP_DS16_R2PD;
	val |= (~rsrc_data->cam_common_cfg.disp_ds4_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_DISP_DS4_R2PD;
	val |= (rsrc_data->cam_common_cfg.dsp_streaming_tap_point & 0x3) <<
		CAM_SHIFT_TOP_CORE_CFG_DSP_STREAMING;
	val |= (rsrc_data->cam_common_cfg.ihist_src_sel & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_STATS_IHIST;
	val |= (rsrc_data->cam_common_cfg.hdr_be_src_sel & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_STATS_HDR_BE;
	val |= (rsrc_data->cam_common_cfg.hdr_bhist_src_sel & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_STATS_HDR_BHIST;
	val |= (rsrc_data->cam_common_cfg.input_mux_sel_pp & 0x3) <<
		CAM_SHIFT_TOP_CORE_CFG_INPUTMUX_PP;
	val |= (rsrc_data->cam_common_cfg.input_pp_fmt & 0x3) <<
		CAM_SHIFT_TOP_CORE_CFG_INPUT_PP_FMT;

	if (rsrc_data->is_fe_enabled && !rsrc_data->is_offline)
		val |= 0x2 << rsrc_data->reg_data->operating_mode_shift;
	else
		val |= 0x1 << rsrc_data->reg_data->operating_mode_shift;

	if (rsrc_data->is_dual && rsrc_data->reg_data->dual_vfe_sync_mask) {
		val |= (((rsrc_data->dual_hw_idx &
				rsrc_data->reg_data->dual_vfe_sync_mask) + 1) <<
				rsrc_data->reg_data->dual_ife_sync_sel_shift);
	}

	CAM_DBG(CAM_ISP, "VFE:%d TOP core_cfg: 0x%X",
		camif_res->hw_intf->hw_idx, val);

	cam_io_w_mb(val, mem_base + rsrc_data->common_reg->core_cfg_0);

	/* epoch config */
	switch (soc_private->cpas_version) {
	case CAM_CPAS_TITAN_480_V100:
	case CAM_CPAS_TITAN_580_V100:
	case CAM_CPAS_TITAN_570_V100:
	case CAM_CPAS_TITAN_570_V200:
		epoch0_line_cfg = ((rsrc_data->last_line +
			rsrc_data->vbi_value) -
			rsrc_data->first_line) / 4;
		if ((epoch0_line_cfg * 2) >
			(rsrc_data->last_line - rsrc_data->first_line))
			epoch0_line_cfg = (rsrc_data->last_line -
				rsrc_data->first_line)/2;
	/* epoch line cfg will still be configured at midpoint of the
	 * frame width. We use '/ 4' instead of '/ 2'
	 * cause it is multipixel path
	 */
		if (rsrc_data->horizontal_bin || rsrc_data->qcfa_bin)
			epoch0_line_cfg >>= 1;

		epoch1_line_cfg = rsrc_data->reg_data->epoch_line_cfg &
			0xFFFF;
		computed_epoch_line_cfg = (epoch1_line_cfg << 16) |
			epoch0_line_cfg;
		cam_io_w_mb(computed_epoch_line_cfg,
			rsrc_data->mem_base + rsrc_data->path_reg_base +
			rsrc_data->camif_reg->epoch_irq_cfg);
		CAM_DBG(CAM_ISP, "epoch_line_cfg: 0x%X",
			computed_epoch_line_cfg);
		break;
	default:
			CAM_ERR(CAM_ISP, "Hardware version not proper: 0x%X",
				soc_private->cpas_version);
			return -EINVAL;
		break;
	}

skip_core_cfg:

	camif_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	if (!rsrc_data->is_offline) {
		cam_io_w_mb(rsrc_data->reg_data->reg_update_cmd_data,
			mem_base + rsrc_data->common_reg->reg_update_cmd);
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF RUP val:0x%X",
			camif_res->hw_intf->hw_idx,
			rsrc_data->reg_data->reg_update_cmd_data);
	}

	/* disable sof irq debug flag */
	rsrc_data->enable_sof_irq_debug = false;
	rsrc_data->irq_debug_cnt = 0;
	rsrc_data->no_payload_err_cnt = 0;

	if (rsrc_data->camif_debug &
		CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		val = cam_io_r_mb(mem_base + rsrc_data->common_reg->diag_config);
		val |= rsrc_data->reg_data->enable_diagnostic_hw;
		cam_io_w_mb(val, mem_base + rsrc_data->common_reg->diag_config);
	}

	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
		rsrc_data->reg_data->error_irq_mask0;
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS2] =
		rsrc_data->reg_data->error_irq_mask2;

	if ((rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE) &&
		rsrc_data->is_dual)
		goto subscribe_err;

	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->epoch0_irq_mask |
		rsrc_data->reg_data->eof_irq_mask;

	if (!rsrc_data->irq_handle) {
		rsrc_data->irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_3,
			irq_mask,
			camif_res,
			camif_res->top_half_handler,
			camif_res->bottom_half_handler,
			camif_res->worker_ctx,
			CAM_IRQ_EVT_GROUP_0);

		if (rsrc_data->irq_handle < 1) {
			CAM_ERR(CAM_ISP, "IRQ handle subscribe failure");
			rsrc_data->irq_handle = 0;
		}
	}

	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->sof_irq_mask;
	if (rsrc_data->cam_common_cfg.input_mux_sel_pp & 0x3)
		irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
			rsrc_data->reg_data->frame_id_irq_mask;

	if (!rsrc_data->sof_irq_handle) {
		rsrc_data->sof_irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_1,
			irq_mask,
			camif_res,
			camif_res->top_half_handler,
			camif_res->bottom_half_handler,
			camif_res->worker_ctx,
			CAM_IRQ_EVT_GROUP_0);

		if (rsrc_data->sof_irq_handle < 1) {
			CAM_ERR(CAM_ISP, "SOF IRQ handle subscribe failure");
			rsrc_data->sof_irq_handle = 0;
		}
	}

subscribe_err:
	if (!rsrc_data->irq_err_handle) {
		rsrc_data->irq_err_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_0,
			err_irq_mask,
			camif_res,
			cam_vfe_camif_ver3_err_irq_top_half,
			camif_res->bottom_half_handler,
			camif_res->worker_ctx,
			CAM_IRQ_EVT_GROUP_0);

		if (rsrc_data->irq_err_handle < 1) {
			CAM_ERR(CAM_ISP, "Error IRQ handle subscribe failure");
			rsrc_data->irq_err_handle = 0;
		}
	}

	CAM_DBG(CAM_ISP, "VFE:%d CAMIF Start Done", camif_res->hw_intf->hw_idx);
	return 0;
}

static int cam_vfe_camif_ver3_reg_dump(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	uint32_t offset, val, wm_idx;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_ver3_data *)camif_res->res_priv;

	CAM_INFO(CAM_ISP, "IFE:%d TOP", camif_res->hw_intf->hw_idx);
	for (offset = 0x0; offset <= 0x1FC; offset += 0x4) {
		if (offset == 0x1C || offset == 0x34 ||
			offset == 0x38 || offset == 0x90)
			continue;
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	CAM_INFO(CAM_ISP, "IFE:%d PP CLC PREPROCESS",
		camif_res->hw_intf->hw_idx);
	for (offset = 0x2200; offset <= 0x23FC; offset += 0x4) {
		if (offset == 0x2208)
			offset = 0x2260;
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
		if (offset == 0x2260)
			offset = 0x23F4;
	}

	CAM_INFO(CAM_ISP, "IFE:%d PP CLC CAMIF", camif_res->hw_intf->hw_idx);
	for (offset = 0x2600; offset <= 0x27FC; offset += 0x4) {
		if (offset == 0x2608)
			offset = 0x2660;
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
		if (offset == 0x2660)
			offset = 0x2664;
		else if (offset == 0x2680)
			offset = 0x27EC;
	}

	CAM_INFO(CAM_ISP, "IFE:%d PP CLC Modules", camif_res->hw_intf->hw_idx);
	for (offset = 0x2800; offset <= 0x8FFC; offset += 0x4) {
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	CAM_INFO(CAM_ISP, "IFE:%d BUS RD", camif_res->hw_intf->hw_idx);
	for (offset = 0xA800; offset <= 0xA89C; offset += 0x4) {
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
		if (offset == 0xA838)
			offset = 0xA844;
		if (offset == 0xA864)
			offset = 0xA874;
		if (offset == 0xA878)
			offset = 0xA87C;
	}

	CAM_INFO(CAM_ISP, "IFE:%d BUS WR", camif_res->hw_intf->hw_idx);
	for (offset = 0xAA00; offset <= 0xAADC; offset += 0x4) {
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_DBG(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	for (wm_idx = 0; wm_idx <= 25; wm_idx++) {
		for (offset = 0xAC00 + 0x100 * wm_idx;
			offset < 0xAC84 + 0x100 * wm_idx; offset += 0x4) {
			val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
		}
	}

	return 0;
}

static int cam_vfe_camif_ver3_resource_stop(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_ver3_data        *camif_priv;
	int                                        rc = 0;
	uint32_t                                   val = 0;
	void __iomem                              *mem_base;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_ver3_data *)camif_res->res_priv;
	mem_base   = camif_priv->mem_base + camif_priv->common_reg->top_hm_base;

	if (camif_priv->common_reg->capabilities & CAM_VFE_COMMON_CAP_SKIP_CORE_CFG)
		goto skip_core_decfg;

	if ((camif_priv->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_priv->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		val = cam_io_r_mb(mem_base + camif_priv->common_reg->core_cfg_0);
		val &= (~(1 << camif_priv->reg_data->dsp_en_shift));
		cam_io_w_mb(val, mem_base + camif_priv->common_reg->core_cfg_0);
	}

skip_core_decfg:
	if (camif_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		camif_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	val = cam_io_r_mb(mem_base + camif_priv->common_reg->diag_config);
	if (val & camif_priv->reg_data->enable_diagnostic_hw) {
		val &= ~camif_priv->reg_data->enable_diagnostic_hw;
		cam_io_w_mb(val, mem_base + camif_priv->common_reg->diag_config);
	}

	if (camif_priv->irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller, camif_priv->irq_handle);
		camif_priv->irq_handle = 0;
	}

	if (camif_priv->sof_irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller,
			camif_priv->sof_irq_handle);
		camif_priv->sof_irq_handle = 0;
	}

	if (camif_priv->irq_err_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller,
			camif_priv->irq_err_handle);
		camif_priv->irq_err_handle = 0;
	}

	CAM_DBG(CAM_ISP, "VFE:%d camif res stop success, no_payload_err_cnt:%u",
		camif_res->hw_intf->hw_idx, camif_priv->no_payload_err_cnt);

	return rc;
}

static int cam_vfe_camif_ver3_core_config(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	struct cam_vfe_core_config_args *vfe_core_cfg =
		(struct cam_vfe_core_config_args *)cmd_args;

	camif_priv =
		(struct cam_vfe_mux_camif_ver3_data *)rsrc_node->res_priv;
	camif_priv->cam_common_cfg.vid_ds16_r2pd =
		vfe_core_cfg->core_config.vid_ds16_r2pd;
	camif_priv->cam_common_cfg.vid_ds4_r2pd =
		vfe_core_cfg->core_config.vid_ds4_r2pd;
	camif_priv->cam_common_cfg.disp_ds16_r2pd =
		vfe_core_cfg->core_config.disp_ds16_r2pd;
	camif_priv->cam_common_cfg.disp_ds4_r2pd =
		vfe_core_cfg->core_config.disp_ds4_r2pd;
	camif_priv->cam_common_cfg.dsp_streaming_tap_point =
		vfe_core_cfg->core_config.dsp_streaming_tap_point;
	camif_priv->cam_common_cfg.ihist_src_sel =
		vfe_core_cfg->core_config.ihist_src_sel;
	camif_priv->cam_common_cfg.hdr_be_src_sel =
		vfe_core_cfg->core_config.hdr_be_src_sel;
	camif_priv->cam_common_cfg.hdr_bhist_src_sel =
		vfe_core_cfg->core_config.hdr_bhist_src_sel;
	camif_priv->cam_common_cfg.input_mux_sel_pdaf =
		vfe_core_cfg->core_config.input_mux_sel_pdaf;
	camif_priv->cam_common_cfg.input_mux_sel_pp =
		vfe_core_cfg->core_config.input_mux_sel_pp;

	return 0;
}

static int cam_vfe_camif_ver3_sof_irq_debug(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	uint32_t *enable_sof_irq = (uint32_t *)cmd_args;

	camif_priv =
		(struct cam_vfe_mux_camif_ver3_data *)rsrc_node->res_priv;

	if (*enable_sof_irq == 1)
		camif_priv->enable_sof_irq_debug = true;
	else
		camif_priv->enable_sof_irq_debug = false;

	return 0;
}

int cam_vfe_camif_ver3_dump_timestamps(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv =
		(struct cam_vfe_mux_camif_ver3_data *)rsrc_node->res_priv;

	CAM_INFO(CAM_ISP,
		"CAMIF ERROR timestamp:[%lld.%09lld] SOF timestamp:[%lld.%09lld] EPOCH timestamp:[%lld.%09lld] EOF timestamp:[%lld.%09lld]",
		camif_priv->error_ts.tv_sec,
		camif_priv->error_ts.tv_nsec,
		camif_priv->sof_ts.tv_sec,
		camif_priv->sof_ts.tv_nsec,
		camif_priv->epoch_ts.tv_sec,
		camif_priv->epoch_ts.tv_nsec,
		camif_priv->eof_ts.tv_sec,
		camif_priv->eof_ts.tv_nsec);

	return 0;
}

static int cam_vfe_camif_ver3_blanking_update(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv =
		(struct cam_vfe_mux_camif_ver3_data *)rsrc_node->res_priv;

	struct cam_isp_blanking_config  *blanking_config =
		(struct cam_isp_blanking_config *)cmd_args;

	camif_priv->hbi_value = blanking_config->hbi;
	camif_priv->vbi_value = blanking_config->vbi;

	CAM_DBG(CAM_ISP, "hbi:%d vbi:%d",
		camif_priv->hbi_value, camif_priv->vbi_value);
	return 0;
}

static int cam_vfe_camif_ver3_process_cmd(
	struct cam_isp_resource_node *rsrc_node,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;
	struct cam_vfe_mux_camif_ver3_data *camif_priv = NULL;

	if (!rsrc_node || !cmd_args) {
		CAM_ERR(CAM_ISP,
			"Invalid input arguments rsesource node:%pK cmd_args:%pK",
			rsrc_node, cmd_args);
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_camif_ver3_get_reg_update(rsrc_node, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_SOF_IRQ_DEBUG:
		rc = cam_vfe_camif_ver3_sof_irq_debug(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CORE_CONFIG:
		rc = cam_vfe_camif_ver3_core_config(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_SET_CAMIF_DEBUG:
		camif_priv = (struct cam_vfe_mux_camif_ver3_data *)
			rsrc_node->res_priv;
		camif_priv->camif_debug = *((uint32_t *)cmd_args);
		rc = 0;
		break;
	case CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA:
		camif_priv = (struct cam_vfe_mux_camif_ver3_data *)
			rsrc_node->res_priv;
		*((struct cam_hw_soc_info **)cmd_args) = camif_priv->soc_info;
		rc = 0;
		break;
	case CAM_ISP_HW_CMD_CAMIF_DATA:
		rc = cam_vfe_camif_ver3_dump_timestamps(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_BLANKING_UPDATE:
		rc = cam_vfe_camif_ver3_blanking_update(rsrc_node, cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP,
			"unsupported process command:%d", cmd_type);
		break;
	}

	return rc;
}


static void cam_vfe_camif_ver3_overflow_debug_info(
	struct cam_vfe_mux_camif_ver3_data *camif_priv)
{
	struct cam_hw_soc_info *soc_info;
	void __iomem *mem_base;
	uint32_t i = 0, num_top_debug_reg, val;
	size_t len = 0;
	uint8_t *log_buf;

	mem_base = camif_priv->mem_base + camif_priv->common_reg->top_hm_base;
	num_top_debug_reg = camif_priv->common_reg->num_top_debug_reg;
	soc_info = camif_priv->soc_info;
	log_buf = camif_priv->log_buf;
	memset(log_buf, 0x0, sizeof(uint8_t) * CAM_VFE_LEN_LOG_BUF);

	for (i = 0; i < num_top_debug_reg; i++) {
		val = cam_io_r(mem_base + camif_priv->common_reg->top_debug[i]);
		CAM_INFO_BUF(CAM_ISP, log_buf, CAM_VFE_LEN_LOG_BUF, &len,
			"VFE[%u] status %2d : 0x%08x", soc_info->index, i, val);
	}

	CAM_INFO(CAM_ISP, "VFE[%u]: %s Debug Status: %s", soc_info->index, "TOP", log_buf);

}

static void cam_vfe_camif_ver3_print_status(uint32_t *status,
	int err_type, struct cam_vfe_mux_camif_ver3_data *camif_priv)
{
	uint32_t module_id = 0;
	uint32_t bus_overflow_status = 0, status_0 = 0, status_2 = 0;
	struct cam_vfe_soc_private *soc_private;
	struct cam_vfe_camif_ver3_hw_info  *camif_hw_info;

	if (!status) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return;
	}

	bus_overflow_status = status[CAM_IFE_IRQ_BUS_OVERFLOW_STATUS];
	status_0 = status[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	status_2 = status[CAM_IFE_IRQ_CAMIF_REG_STATUS2];
	camif_hw_info = camif_priv->hw_info;

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW)
		cam_vfe_top_print_error_info(camif_hw_info->top_overflow_err_desc,
			status_0, camif_hw_info->num_top_overflow_errors,
			camif_priv->hw_intf->hw_idx);

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && bus_overflow_status)
		cam_vfe_top_print_error_info(camif_hw_info->bus_overflow_err_desc,
				bus_overflow_status, camif_hw_info->num_bus_overflow_errors,
				camif_priv->hw_intf->hw_idx);

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && !bus_overflow_status) {
		CAM_INFO(CAM_ISP, "VFE[%u] PIXEL PIPE Module hang",
			camif_priv->hw_intf->hw_idx);
		/* print debug registers */
		cam_vfe_camif_ver3_overflow_debug_info(camif_priv);
		goto print_state;
	}

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION)
		cam_vfe_top_print_error_info(camif_hw_info->top_violation_err_desc,
				status_2, camif_hw_info->num_top_violation_errors,
				camif_priv->hw_intf->hw_idx);

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION &&
		status[CAM_IFE_IRQ_VIOLATION_STATUS]) {
		module_id =
			camif_hw_info->violation_mask & status[CAM_IFE_IRQ_VIOLATION_STATUS];
		CAM_INFO(CAM_ISP, "VFE[%u] PIXEL PIPE VIOLATION Module ID:%d",
			camif_priv->hw_intf->hw_idx, module_id);

		if (camif_hw_info->ipp_module_desc) {
			CAM_ERR(CAM_ISP, "VFE[%u] IPP Violation Module id: [%u %s]",
				camif_priv->hw_intf->hw_idx,
				camif_hw_info->ipp_module_desc[module_id].id,
				camif_hw_info->ipp_module_desc[module_id].desc);
		} else {
			CAM_ERR(CAM_ISP, "VFE[%u] Invalid IPP modules description params",
				camif_priv->hw_intf->hw_idx);
			return;
		}
	}

print_state:
	soc_private = camif_priv->soc_info->soc_private;

	cam_cpas_dump_camnoc_buff_fill_info();

	CAM_INFO(CAM_ISP, "VFE[%u] ife_clk_src:%lld",
		camif_priv->hw_intf->hw_idx, soc_private->ife_clk_src);

	if ((err_type == CAM_VFE_IRQ_STATUS_OVERFLOW) &&
		((camif_priv->cam_common_cfg.input_mux_sel_pp & 0x3) ||
		(bus_overflow_status)))
		cam_cpas_log_votes(false);
}

static int cam_vfe_camif_ver3_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_node;
	struct cam_vfe_mux_camif_ver3_data    *camif_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;

	camif_node = th_payload->handler_priv;
	camif_priv = camif_node->res_priv;

	CAM_DBG(CAM_ISP,
		"VFE:%d CAMIF IRQ status_0: 0x%X status_1: 0x%X status_2: 0x%X",
		camif_node->hw_intf->hw_idx, th_payload->evt_status_arr[0],
		th_payload->evt_status_arr[1], th_payload->evt_status_arr[2]);

	rc  = cam_vfe_camif_ver3_get_evt_payload(camif_priv, &evt_payload);
	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_ISP,
		"VFE:%d CAMIF IRQ status_0: 0x%X status_1: 0x%X status_2: 0x%X",
		camif_node->hw_intf->hw_idx, th_payload->evt_status_arr[0],
		th_payload->evt_status_arr[1], th_payload->evt_status_arr[2]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);
	evt_payload->reg_val = 0;

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	/* Read frame_id meta at every epoch if custom hw is enabled */
	if (evt_payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_priv->reg_data->epoch0_irq_mask) {
		if ((camif_priv->common_reg->custom_frame_idx) &&
			(camif_priv->cam_common_cfg.input_mux_sel_pp & 0x3))
			evt_payload->reg_val = cam_io_r_mb(
			camif_priv->mem_base + camif_priv->common_reg->top_hm_base +
			camif_priv->common_reg->custom_frame_idx);
	}

	th_payload->evt_payload_priv = evt_payload;

	if (th_payload->evt_status_arr[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
			& camif_priv->reg_data->sof_irq_mask) {
		if (camif_priv->top_priv->sof_ts_reg_addr.curr0_ts_addr &&
			camif_priv->top_priv->sof_ts_reg_addr.curr1_ts_addr) {
			evt_payload->ts.sof_ts =
				cam_io_r_mb(camif_priv->top_priv->sof_ts_reg_addr.curr1_ts_addr);
			evt_payload->ts.sof_ts = (evt_payload->ts.sof_ts << 32) |
				cam_io_r_mb(camif_priv->top_priv->sof_ts_reg_addr.curr0_ts_addr);
		}
		trace_cam_log_event("SOF", "TOP_HALF",
		th_payload->evt_status_arr[CAM_IFE_IRQ_CAMIF_REG_STATUS1],
		camif_node->hw_intf->hw_idx);
	}

	if (th_payload->evt_status_arr[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
			& camif_priv->reg_data->epoch0_irq_mask) {
		trace_cam_log_event("EPOCH0", "TOP_HALF",
		th_payload->evt_status_arr[CAM_IFE_IRQ_CAMIF_REG_STATUS1],
		camif_node->hw_intf->hw_idx);
	}

	if (th_payload->evt_status_arr[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
			& camif_priv->reg_data->eof_irq_mask) {
		trace_cam_log_event("EOF", "TOP_HALF",
		th_payload->evt_status_arr[CAM_IFE_IRQ_CAMIF_REG_STATUS1],
		camif_node->hw_intf->hw_idx);
	}

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

static int cam_vfe_camif_ver3_handle_irq_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node *camif_node;
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	struct cam_vfe_top_irq_evt_payload *payload;
	struct cam_isp_hw_event_info evt_info;
	struct cam_isp_hw_error_event_info err_evt_info;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_vfe_soc_private *soc_private = NULL;
	struct cam_isp_sof_ts_data sof_and_boot_time;
	uint32_t irq_status[CAM_IFE_IRQ_REGISTERS_MAX] = {0};
	void __iomem *mem_base;
	struct timespec64 ts;
	uint32_t val = 0;
	int i = 0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP,
			"Invalid params handle_priv:%pK, evt_payload_priv:%pK",
			handler_priv, evt_payload_priv);
		return ret;
	}

	camif_node = handler_priv;
	camif_priv = camif_node->res_priv;
	payload = evt_payload_priv;
	mem_base = camif_priv->mem_base + camif_priv->common_reg->top_hm_base;
	soc_info = camif_priv->soc_info;
	soc_private =
		(struct cam_vfe_soc_private *)soc_info->soc_private;

	if ((camif_node->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_node->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)) {
		ret = 0;
		CAM_WARN(CAM_ISP, "BH scheduled in stop state, ignore with success");
		goto end;
	}
	for (i = 0; i < CAM_IFE_IRQ_REGISTERS_MAX; i++)
		irq_status[i] = payload->irq_reg_val[i];

	sof_and_boot_time.boot_time = payload->ts.mono_time;
	sof_and_boot_time.sof_ts = payload->ts.sof_ts;

	evt_info.hw_type  = CAM_ISP_HW_TYPE_VFE;
	evt_info.hw_idx   = camif_node->hw_intf->hw_idx;
	evt_info.res_id   = camif_node->res_id;
	evt_info.res_type = camif_node->res_type;
	evt_info.reg_val = 0;
	evt_info.hw_type = CAM_ISP_HW_TYPE_VFE;
	evt_info.event_data = &sof_and_boot_time;

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_priv->reg_data->sof_irq_mask) {
		if ((camif_priv->enable_sof_irq_debug) &&
			(camif_priv->irq_debug_cnt <=
			CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP, "VFE:%d Received SOF",
				evt_info.hw_idx);

			camif_priv->irq_debug_cnt++;
			if (camif_priv->irq_debug_cnt ==
				CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX) {
				camif_priv->enable_sof_irq_debug =
					false;
				camif_priv->irq_debug_cnt = 0;
			}
		} else {
			CAM_DBG(CAM_ISP, "VFE:%d Received SOF",
				evt_info.hw_idx);
			camif_priv->sof_ts.tv_sec =
				payload->ts.mono_time.tv_sec;
			camif_priv->sof_ts.tv_nsec =
				payload->ts.mono_time.tv_nsec;
		}

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_priv->reg_data->epoch0_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d Received EPOCH", evt_info.hw_idx);
		evt_info.reg_val = payload->reg_val;
		camif_priv->epoch_ts.tv_sec =
			payload->ts.mono_time.tv_sec;
		camif_priv->epoch_ts.tv_nsec =
			payload->ts.mono_time.tv_nsec;

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_priv->reg_data->eof_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d Received EOF", evt_info.hw_idx);
		camif_priv->eof_ts.tv_sec =
			payload->ts.mono_time.tv_sec;
		camif_priv->eof_ts.tv_nsec =
			payload->ts.mono_time.tv_nsec;

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EOF, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS0]
		& camif_priv->reg_data->error_irq_mask0) {
		CAM_ERR(CAM_ISP, "VFE:%d Overflow", evt_info.hw_idx);

		ktime_get_boottime_ts64(&ts);
		CAM_INFO(CAM_ISP,
			"current monotonic timestamp:[%lld.%09lld]",
			ts.tv_sec, ts.tv_nsec);

		ret = CAM_VFE_IRQ_STATUS_OVERFLOW;
		err_evt_info.err_type = CAM_VFE_IRQ_STATUS_OVERFLOW;
		evt_info.event_data = (void *)&err_evt_info;

		CAM_INFO(CAM_ISP, "ife_clk_src:%lld",
			soc_private->ife_clk_src);

		cam_vfe_camif_ver3_print_status(irq_status, ret, camif_priv);

		if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_ver3_reg_dump(camif_node);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS0]
		& camif_priv->reg_data->frame_id_irq_mask) {
		val = cam_io_r_mb(mem_base + camif_priv->common_reg->custom_frame_idx);
		CAM_DBG(CAM_ISP,
			"VFE:%d Frame id change to: %u", evt_info.hw_idx,
			val);
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS2]) {
		CAM_ERR(CAM_ISP, "VFE:%d Violation", evt_info.hw_idx);

		ktime_get_boottime_ts64(&ts);
		CAM_INFO(CAM_ISP,
			"current monotonic timestamp:[%lld.%09lld]",
			ts.tv_sec, ts.tv_nsec);

		ret = CAM_VFE_IRQ_STATUS_VIOLATION;
		err_evt_info.err_type = CAM_VFE_IRQ_STATUS_VIOLATION;
		evt_info.event_data = (void *)&err_evt_info;

		CAM_INFO(CAM_ISP, "ife_clk_src:%lld",
			soc_private->ife_clk_src);

		cam_vfe_camif_ver3_print_status(irq_status, ret, camif_priv);

		if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_ver3_reg_dump(camif_node);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);
	}

	if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		CAM_DBG(CAM_ISP, "VFE:%d VFE_DIAG_SENSOR_STATUS: 0x%X",
			evt_info.hw_idx, camif_priv->mem_base,
			cam_io_r(mem_base + camif_priv->common_reg->diag_sensor_status_0));
	}

end:
	cam_vfe_camif_ver3_put_evt_payload(camif_priv, &payload);

	CAM_DBG(CAM_ISP, "returning status = %d", ret);
	return ret;
}

int cam_vfe_camif_ver3_init(
	void                          *top_priv,
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_hw_info,
	struct cam_isp_resource_node  *camif_node,
	void                          *vfe_irq_controller)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv = NULL;
	struct cam_vfe_camif_ver3_hw_info *camif_info = camif_hw_info;
	int i = 0;

	camif_priv = CAM_MEM_ZALLOC(sizeof(struct cam_vfe_mux_camif_ver3_data),
		GFP_KERNEL);
	if (!camif_priv)
		return -ENOMEM;

	camif_node->res_priv = camif_priv;

	camif_priv->mem_base    = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	camif_priv->camif_reg   = camif_info->camif_reg;
	camif_priv->common_reg  = camif_info->common_reg;
	camif_priv->reg_data    = camif_info->reg_data;
	camif_priv->hw_intf     = hw_intf;
	camif_priv->soc_info    = soc_info;
	camif_priv->path_reg_base      = camif_info->path_reg_base;
	camif_priv->top_priv    = (struct cam_vfe_top_ver3_priv *)top_priv;
	camif_priv->vfe_irq_controller = vfe_irq_controller;
	camif_priv->hw_info     = camif_info;

	camif_node->init    = cam_vfe_camif_ver3_resource_init;
	camif_node->deinit  = cam_vfe_camif_ver3_resource_deinit;
	camif_node->start   = cam_vfe_camif_ver3_resource_start;
	camif_node->stop    = cam_vfe_camif_ver3_resource_stop;
	camif_node->process_cmd = cam_vfe_camif_ver3_process_cmd;
	camif_node->top_half_handler = cam_vfe_camif_ver3_handle_irq_top_half;
	camif_node->bottom_half_handler =
		cam_vfe_camif_ver3_handle_irq_bottom_half;
	spin_lock_init(&camif_priv->spin_lock);
	INIT_LIST_HEAD(&camif_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_EVT_MAX; i++) {
		INIT_LIST_HEAD(&camif_priv->evt_payload[i].list);
		list_add_tail(&camif_priv->evt_payload[i].list,
			&camif_priv->free_payload_list);
	}

	return 0;
}

int cam_vfe_camif_ver3_deinit(
	struct cam_isp_resource_node  *camif_node)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	int i = 0;

	if (!camif_node) {
		CAM_ERR(CAM_ISP, "Error, camif_node is NULL %pK", camif_node);
		return -ENODEV;
	}

	camif_priv = camif_node->res_priv;

	INIT_LIST_HEAD(&camif_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_EVT_MAX; i++)
		INIT_LIST_HEAD(&camif_priv->evt_payload[i].list);

	camif_priv = camif_node->res_priv;

	camif_node->start = NULL;
	camif_node->stop  = NULL;
	camif_node->process_cmd = NULL;
	camif_node->top_half_handler = NULL;
	camif_node->bottom_half_handler = NULL;
	camif_node->res_priv = NULL;

	if (!camif_priv) {
		CAM_ERR(CAM_ISP, "Error, camif_priv is NULL %pK", camif_priv);
		return -ENODEV;
	}

	CAM_MEM_FREE(camif_priv);

	return 0;
}
