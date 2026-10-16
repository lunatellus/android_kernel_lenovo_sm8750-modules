/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CAM_COMMON_DCVS_API_H_
#define _CAM_COMMON_DCVS_API_H_

#include <linux/types.h>
#include <linux/kernel.h>

/**
 *  common dcvs supported clk rate calculation algo type
 *  clock scaling as the primary method to adjust performance and power.
 *  number of remaining requests in the pipeline or queue.
    It’s a more workload-aware approach that considers how much processing is still pending.
 */
enum cam_common_dcvs_scaling_policy {
	CAM_COMMON_DCVS_CLK_SCALING = 1,
	CAM_COMMON_DCVS_REMAINING_REQ_NUM = 2,
};

/**
 *  common dcvs supported stream type
 */
enum cam_common_dcvs_stream_type {
	CAM_COMMON_DCVS_INVALID_STREAM = 0,
	CAM_COMMON_DCVS_RT_STREAM,
	CAM_COMMON_DCVS_NRT_STREAM,
	CAM_COMMON_DCVS_SRT_STREAM,
};

/**
 * struct cam_common_dcvs_init_info : dcvs init information
 * @brief                           : client registration parameters

 * @max_pending_rt_req            : represents a threshold for pending rt
                                    requests that may trigger scaling.
 * @max_pending_nrt_req           : represents a threshold for pending nrt
                                    requests that may trigger scaling.
 * @max_pending_srt_req           : represents a threshold for pending srt
                                    requests that may trigger scaling.
 * @pending_req_scaling_factor    : pending factor to decide the clk bump up
 * @clk_div_factor                : based on applied clk percentage to decide the clk bump up
 * @soc_info                      : hw soc info
 * @hw_dev_type                   : hw device type
 * @overclocking_threshold        : over clked threshold value
 * @scaling_type                  : type of aloroithm to caluculate dcvs clk
 *
 */
struct cam_common_dcvs_init_info {
	enum cam_common_dcvs_hw_type hw_dev_type;
	uint32_t max_pending_rt_req;
	uint32_t max_pending_nrt_req;
	uint32_t max_pending_srt_req;
	uint32_t pending_req_scaling_factor;
	uint32_t clk_div_factor;
	const struct cam_hw_soc_info *soc_info;
	uint32_t overclocking_threshold;
	enum cam_common_dcvs_scaling_policy scaling_policy_type;
};

/**
 * struct cam_common_dcvs_clk_info : dcvs common clk information.
 * @brief                          : this is needed to pass clk information to dcvs.

 * @base_clk_freq                 : the is initial clk value of device
 * @curr_clk_freq                 : current applied clk information of device
 * @accumulated_base_clock        : adding up all base clk of all streams
 * @is_overclocked                : is the total applied clk is over clked
 * @is_active                     : this will give information is the device is busy or not.
 *
 */
struct cam_common_dcvs_clk_info {
	uint32_t base_clk_freq;
	uint32_t curr_clk_freq;
	uint32_t prev_clk_freq;
	uint32_t accumulated_base_clock;
	bool     is_overclocked;
	bool     is_active;
};

/**
 * struct cam_common_dcvs_input_param : dcvs input parameter
 *
 * @stream_id                         : print the ctxt name
 * @req_id                            : req_id information of each clk update request
 * @curr_ctxt_pending_req_count             : number of pending request of current ctxt
 * @all_ctxts_pending_req_count       : number of all pending request from all ctxt
 * @stream_type                       : stream type(RT/NRT/SRT)
 * @accumulated_base_clk              : adding up all base clock for all context
 * @total_curr_req_clk                : This is needed to calculate the clk scalling factor
                                        for scalling algo.
 * @applied_clock                     : applied device clk
 * @frame_cycle_duration              : requested frame cycle
 * @current_frame_cycle               : curr ctxt frame cycle
 * @clock_budget                      : clk budget information
 * @final_clk                         : final clk will be updated and saved
 * @is_clock_updated                  : is there is change in curr_clk
 *
 */
struct cam_common_dcvs_input_param {
	const char *stream_id;
	uint64_t req_id;
	int curr_ctxt_pending_req_count;
	int all_ctxts_pending_req_count;
	uint32_t stream_type;
	uint32_t accumulated_base_clk;
	uint32_t total_curr_req_clk;
	int32_t applied_clock;
	uint32_t frame_cycle_duration;
	uint32_t current_frame_cycle;
	uint32_t clock_budget;
	int32_t final_clk;
	bool is_clock_updated;
};

/**
 * @brief           : API to cam commn dcvs module initialization
 *
 * @return 0 on success
 */
int cam_common_dcvs_init(void);

/**
 * @brief           : API to cam commn dcvs module deinit
 *
 * @return 0 on success
 */
int cam_common_dcvs_deinit(void);

/**
 * @brief           : API to register the device with DCVS framework
 *
 * @dcvs_init _info : struct cam_common_dcvs_input_param
 *
 * @return the device handle
 */
int cam_common_dcvs_register_device(
	struct cam_common_dcvs_init_info *dcvs_init_info);

/**
 * @brief    : API to deregister the device with DCVS framework
 *
 * @dcvs_hdl : device handle to get the retrieve correct information
 *
 * @return 0 if success
 */
int cam_common_dcvs_deregister_device(
	int dcvs_hdl);

/**
 * @brief           : API to return the final clk rate
 *
 * @dcvs_hdl        : device handle to get the retrieve correct information
 * @input_param     : required input parameter to dcvs framework
 * @device_clk_info : device clk information
 *
 * @return rc
 */
int cam_common_dcvs_check_update_clk_rate(
	int dcvs_hdl,
	struct cam_common_dcvs_input_param *input_param,
	struct cam_common_dcvs_clk_info *device_clk_info);

/**
 * @brief       : API to reset the device info structure
 *
 * @input_param : dcvs_hdl
 *
 * @return 0 on success
 */
int cam_common_dcvs_reset_device_info(int dcvs_hdl);

/**
 * @brief       : This API to get the all clk information
 *
 * @input_param : dcvs_hdl
 * @input_param : device_clk_info
 * @return 0 on success
 */
int cam_common_dcvs_debug_info(int dcvs_hdl,
	struct cam_common_dcvs_clk_info *device_clk_info);

#endif /*_CAM_COMMON_DCVS_API_H_*/
