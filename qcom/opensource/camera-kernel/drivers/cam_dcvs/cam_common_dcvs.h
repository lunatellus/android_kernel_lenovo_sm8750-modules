/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CAM_COMMON_DCVS_H_
#define _CAM_COMMON_DCVS_H_

#include <linux/kernel.h>
#include <linux/mutex.h>

#include "cam_common_dcvs_api.h"
#include "cam_soc_util.h"

#define CAM_COMMON_DCVS_NUM_DEVICES      8
#define CAM_COMMON_DCVS_HDL_IDX_SIZE     8

#define CAM_COMMON_DCVS_HDL_IDX_MASK      ((1 << CAM_COMMON_DCVS_HDL_IDX_SIZE) - 1)
#define GET_DCVS_HANDLE(idx) \
	(idx & CAM_COMMON_DCVS_HDL_IDX_MASK)

#define CAM_UTIL_DCVS_HDL_IDX(hdl) (hdl & CAM_COMMON_DCVS_HDL_IDX_MASK)

#define CAM_COMMON_DCVS_MONITOR_MAX_ENTRIES   100
#define CAM_COMMON_DCVS_INC_MONITOR_HEAD(head, ret) \
	div_u64_rem(atomic64_add_return(1, head),\
	CAM_COMMON_DCVS_MONITOR_MAX_ENTRIES, (ret))

#define CAM_COMMON_DCVS_LOG_BUF_LEN 512

#define CAM_COMMON_DCVS_CALC_BASE_CLK(base_clk, budget) \
	(do_div((base_clk), (budget)))


#define CAM_COMMON_DCVS_COMPUTE_FACTOR(x, y) ((((x)) * ((y))) / 100)

/**
 * struct cam_common_dcvs_monitor : store the dcvs module activity
 * @brief                         :  indicating it's used for monitoring DCVS state.

 * @timestamp                : Timestamp of the monitoring snapshot
 * @log_buf                  : Buffer for logging DCVS-related information
 * @curr_clk_freq            : Current operating clock frequency
 * @accumulated_base_clk     : Total base clock accumulated for all context
 * @base_clk_freq            : Default/base clock frequency of current context
 * @max_pending_rt_req       : Threshold for real-time context,
                               based on this threshold value dcvs algo will be triggered.
 * @max_pending_nrt_req      : Threshold for non-real-time context,
                               based on this threshold value dcvs algo will be triggered.
 * @max_pending_srt_req      : Threshold for semi real-time context
                               based on this threshold value dcvs algo will be triggered.
 * @req_scaling_factor       : Scaling factor based on pending requests
 * @clk_scaling_factor       : Factor used for clock scaling
 * @hw_dev_type              : Type of hardware device
 * @scaling_type             : Policy used for dynamic clock
 * @is_active                : Indicates the hardware is currently active or not
 *
 */
struct cam_common_dcvs_monitor {
	struct timespec64   timestamp;
	char                log_buf[CAM_COMMON_DCVS_LOG_BUF_LEN];
	uint32_t            curr_clk_freq;
	uint32_t            accumulated_base_clk;
	uint32_t            base_clk_freq;
	uint32_t            max_pending_rt_req;
	uint32_t            max_pending_nrt_req;
	uint32_t            max_pending_srt_req;
	uint32_t            req_scaling_factor;
	uint32_t            clk_scaling_factor;
	enum cam_common_dcvs_hw_type    hw_dev_type;
	enum cam_common_dcvs_scaling_policy  scaling_type;
	bool                is_active;
};

/**
 * struct cam_common_dcvs_client_info : store dcvs client information
 * @brief                             : this structure captures a snapshot of the DCVS

 * @client_lock             : lock used to protect concurrent access to the DCVS client’s data.
 * @monitor_head            : An atomic counter or index pointing to the current head.
 * @dcvs_init_info          : Pointer to the DCVS initialization configuration.
 * @dcvs_clk_info           : Pointer to the current clock state information.
 * @dcvs_monitor_entries    : Fixed-size array of monitoring entries that log.
 *
 */
struct cam_common_dcvs_client_info {
	struct mutex client_lock;
	atomic64_t  monitor_head;
	struct cam_common_dcvs_init_info *dcvs_init_info;
	struct cam_common_dcvs_clk_info *dcvs_clk_info;
	struct cam_common_dcvs_monitor
		dcvs_monitor_entries[CAM_COMMON_DCVS_MONITOR_MAX_ENTRIES];
};

/**
 * struct cam_util_dcvs_table : serves as a centralized management table for all DCVS clients
 * @brief                     : it tracks client registration, manages access synchronization.
 *
 * @num_client     : The number of DCVS clients currently registered in the table.
 * @bitmap         : A pointer to a bitmap used for tracking empty slots.
 * @bits           : The total number of bits in the bitmap.
 * @client_info    : An array of DCVS client information structures.
 * @idx_lock       : A mutex used to protect access to the bitmap and client index management.
 *
 */
struct cam_util_dcvs_table {
	int num_client;
	void *bitmap;
	size_t bits;
	struct cam_common_dcvs_client_info
		client_info[CAM_COMMON_DCVS_NUM_DEVICES];
	struct mutex  idx_lock;
};

/**
 * @brief    : API to return the final clk rate
 *
 * @dcvs_hdl : dcvs driver client handle
 *
 * @return clk level
 */
uint32_t cam_util_dcvs_get_next_clk_rate(uint32_t dcvs_hdl);

/**
 * @brief : API to return the final clk rate
 *
 * @dcvs_hdl : dcvs driver client handle
 *
 * @return clk level
 */
uint32_t cam_util_dcvs_get_lower_clk_rate(uint32_t dcvs_hdl);

/**
 * @brief : API to return the final clk rate
 *
 * @dcvs_hdl : dcvs driver client handle
 *
 * @return clk level
 */
uint32_t cam_util_dcvs_get_actual_clk_rate_idx(uint32_t dcvs_hdl);

/**
 * @brief : API to return the final clk rate
 *
 * @dcvs_hdl : dcvs driver client handle
 *
 * @return clk level
 */
uint32_t cam_util_dcvs_get_curr_clk(uint32_t dcvs_hdl);

#endif /* _CAM_COMMON_DCVS_H_ */

