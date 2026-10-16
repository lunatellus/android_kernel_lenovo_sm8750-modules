/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CAM_CSIPHY_1_2_3_HWREG_H_
#define _CAM_CSIPHY_1_2_3_HWREG_H_

#include "../cam_csiphy_dev.h"

uint32_t csiphy_reg_addr_func_1_2_3(uint32_t partial_offset, uint32_t lane_offset) {
	// implementation specific to 1_2_3
	int index;
	switch (lane_offset) {
		case 0x100:
			index = 0;
			break;
		case 0x300:
			index = 1;
			break;
		case 0x500:
			index = 2;
			break;
		default:
			index = 0;
			break;
	}
	return partial_offset + lane_offset - (0x100 * index);
}

struct cam_csiphy_aon_sel_params_t aon_cam_select_params_1_2_3 = {
	.aon_cam_sel_offset[0] = 0x01E0,
	.aon_cam_sel_offset[1] = 0x01E4,
	.cam_sel_mask = BIT(0),
	.mclk_sel_mask = BIT(8),
};

struct cam_cphy_dphy_status_reg_params_t status_regs_1_2_3 = {
	.csiphy_3ph_status0_offset = 0x08B0,
	.csiphy_2ph_status0_offset = 0x0890,
	.refgen_status_offset = 0x08FC,
	.cphy_lane_status = {0x0358, 0x0758, 0x0B58},
	.csiphy_3ph_status_size = 20,
	.csiphy_2ph_status_size = 16,
};

struct csiphy_reg_t csiphy_lane_en_reg_1_2_3[] = {
	{0x0814, 0x2A, 0x00, CSIPHY_LANE_ENABLE},
};

struct csiphy_reg_t csiphy_common_reg_1_2_3[] = {
	{0x0814, 0xd5, 0x00, CSIPHY_2PH_REGS},
	{0x0814, 0x2A, 0x00, CSIPHY_3PH_REGS},
	{0x081C, 0x5A, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0818, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0824, 0x72, 0x00, CSIPHY_2PH_REGS},
};

struct csiphy_reg_t csiphy_reset_enter_reg_1_2_3[] = {
	{0x0800, 0x01, 0x14, CSIPHY_DEFAULT_PARAMS},
};

struct csiphy_reg_t csiphy_reset_exit_reg_1_2_3[] = {
	{0x0800, 0x02, 0x00, CSIPHY_2PH_REGS},
	{0x0800, 0x00, 0x00, CSIPHY_2PH_COMBO_REGS},
	{0x0800, 0x0E, 0x00, CSIPHY_3PH_REGS},
};

struct csiphy_reg_t csiphy_irq_reg_1_2_3[] = {
	{0x082c, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0830, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0834, 0xfb, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0838, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x083c, 0x7f, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0840, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0844, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0848, 0xef, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x084c, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0850, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0854, 0xff, 0x00, CSIPHY_DEFAULT_PARAMS},
};

struct csiphy_reg_t csiphy_2ph_v1_2_3_reg[] = {
	{0x0024, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0000, 0x8D, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0038, 0xFE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x002C, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0034, 0x0F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0010, 0x52, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x001C, 0x0A, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0014, 0x60, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x003C, 0xB8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0004, 0x0C, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0020, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0008, 0x10, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{0x005C, 0x00, 0x00, CSIPHY_SKEW_CAL},
	{0x0060, 0xFD, 0x00, CSIPHY_SKEW_CAL},
	{0x0064, 0x7F, 0x00, CSIPHY_SKEW_CAL},
};

struct csiphy_reg_t csiphy_2ph_v1_2_3_clk_ln_reg[] = {
	{0x0028, 0x04, 0x00, CSIPHY_2PH_SEC_CLK_LN_SETTINGS},
	{0x0000, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0038, 0x1F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x000C, 0xFF, 0x00, CSIPHY_DEFAULT_PARAMS},
};

struct csiphy_reg_t csiphy_3ph_v1_2_3_reg[] = {
	{0x0068, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x006C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x005C, 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0004, 0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x000C, 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0008, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0014, 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0050, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0088, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x008C, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0090, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0018, 0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x001C, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0020, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0024, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0028, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x002C, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0044, 0xB2, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0060, 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x00CC, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0064, 0x33, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x00DC, 0x50, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0890, 0x08, 0x00, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
	{0x0894, 0x08, 0x00, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
	{0x0898, 0x1A, 0x00, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
	{0x088C, 0xAF, 0x64, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
	{0x0884, 0x20, 0x00, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
	{0x0888, 0x05, 0x00, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
	{0x0880, 0x61, 0x00, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
	{0x08B0, 0x01, 0x00, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
};

struct csiphy_reg_t datarate_123_1p5Gsps[] = {
	/* AFE Settings */
	{0x005C, 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0068, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x006C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x000C, 0x24, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	/* Datarate Sensitive */
	{0x08B4, 0x08, 0x0A, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
};
struct csiphy_reg_t datarate_123_1p7Gsps[] = {
	/* AFE Settings */
	{0x005C, 0x56, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0068, 0xAE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x006C, 0x65, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x000C, 0x12, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	/* Datarate Sensitive */
	{0x08B4, 0x08, 0x0A, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
};

struct csiphy_reg_t datarate_123_2p5Gsps[] = {
	/* AFE Settings */
	{0x0068, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x005C, 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x006C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x000C, 0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	/* Datarate Sensitive */
	{0x09B4, 0x08, 0x0A, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
};
struct csiphy_reg_t datarate_123_3p5Gsps[] = {
	/* AFE Settings */
	{0x0068, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x005C, 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x006C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x000C, 0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	/* Datarate Sensitive */
	{0x09B4, 0x04, 0x0A, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
};
struct csiphy_reg_t datarate_123_4p5Gsps[] = {
	/* AFE Settings */
	{0x0068, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x005C, 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x006C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x000C, 0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	/* Datarate Sensitive */
	{0x09B4, 0x04, 0x0A, CSIPHY_EDGE_CASE_REG_ADDR_SETTING},
};
static struct data_rate_reg_info_t data_rate_settings_1_2_3[] = {
	{
		/* ((1.5 GSpS) * (10^9) * (2.28 bits/symbol)) rounded value */
		.bandwidth = 3420000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_123_1p5Gsps),
		.data_rate_reg_array[0][0]  = datarate_123_1p5Gsps,
		.data_rate_reg_array[1][0]  = datarate_123_1p5Gsps,
		.data_rate_reg_array[2][0]  = datarate_123_1p5Gsps,
		.data_rate_reg_array[3][0]  = datarate_123_1p5Gsps,
		.data_rate_reg_array[4][0]  = datarate_123_1p5Gsps,
		.data_rate_reg_array[5][0]  = datarate_123_1p5Gsps,
		.data_rate_reg_array[6][0]  = datarate_123_1p5Gsps,
		.data_rate_reg_array[7][0]  = datarate_123_1p5Gsps,
	},
	{
		/* ((1.7 GSpS) * (10^9) * (2.28 bits/symbol)) rounded value */
		.bandwidth = 3876000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_123_1p7Gsps),
		.data_rate_reg_array[0][0] = datarate_123_1p7Gsps,
		.data_rate_reg_array[1][0] = datarate_123_1p7Gsps,
		.data_rate_reg_array[2][0] = datarate_123_1p7Gsps,
		.data_rate_reg_array[3][0] = datarate_123_1p7Gsps,
		.data_rate_reg_array[4][0] = datarate_123_1p7Gsps,
		.data_rate_reg_array[5][0] = datarate_123_1p7Gsps,
		.data_rate_reg_array[6][0] = datarate_123_1p7Gsps,
		.data_rate_reg_array[7][0] = datarate_123_1p7Gsps,
	},
	{
		/* ((2.5 GSpS) * (10^9) * (2.28 bits/symbol)) rounded value */
		.bandwidth = 5700000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_123_2p5Gsps),
		.data_rate_reg_array[0][0] = datarate_123_2p5Gsps,
		.data_rate_reg_array[1][0] = datarate_123_2p5Gsps,
		.data_rate_reg_array[2][0] = datarate_123_2p5Gsps,
		.data_rate_reg_array[3][0] = datarate_123_2p5Gsps,
		.data_rate_reg_array[4][0] = datarate_123_2p5Gsps,
		.data_rate_reg_array[5][0] = datarate_123_2p5Gsps,
		.data_rate_reg_array[6][0] = datarate_123_2p5Gsps,
		.data_rate_reg_array[7][0] = datarate_123_2p5Gsps,
	},
	{
		/* ((3.5 GSpS) * (10^9) * (2.28 bits/symbol)) rounded value */
		.bandwidth = 7980000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_123_3p5Gsps),
		.data_rate_reg_array[0][0] = datarate_123_3p5Gsps,
		.data_rate_reg_array[1][0] = datarate_123_3p5Gsps,
		.data_rate_reg_array[2][0] = datarate_123_3p5Gsps,
		.data_rate_reg_array[3][0] = datarate_123_3p5Gsps,
		.data_rate_reg_array[4][0] = datarate_123_3p5Gsps,
		.data_rate_reg_array[5][0] = datarate_123_3p5Gsps,
		.data_rate_reg_array[6][0] = datarate_123_3p5Gsps,
		.data_rate_reg_array[7][0] = datarate_123_3p5Gsps,
	},
	{
		/* ((4.5 GSpS) * (10^9) * (2.28 bits/symbol)) rounded value */
		.bandwidth = 10260000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_123_4p5Gsps),
		.data_rate_reg_array[0][0] = datarate_123_4p5Gsps,
		.data_rate_reg_array[1][0] = datarate_123_4p5Gsps,
		.data_rate_reg_array[2][0] = datarate_123_4p5Gsps,
		.data_rate_reg_array[3][0] = datarate_123_4p5Gsps,
		.data_rate_reg_array[4][0] = datarate_123_4p5Gsps,
		.data_rate_reg_array[5][0] = datarate_123_4p5Gsps,
		.data_rate_reg_array[6][0] = datarate_123_4p5Gsps,
		.data_rate_reg_array[7][0] = datarate_123_4p5Gsps,
	},
};

struct csiphy_reg_t bist_3ph_arr_1_2_3[] = {
	{0x0030, 0x1C, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0038, 0xD4, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x003C, 0x59, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0058, 0x10, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x00C8, 0xAA, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x00D0, 0xAA, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x00D4, 0x64, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x00D8, 0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0048, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x004C, 0x07, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0050, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0044, 0xB1, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x005C, 0x04, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0040, 0x85, 0x00, CSIPHY_DEFAULT_PARAMS},
};

struct csiphy_reg_t bist_status_arr_1_2_3[] = {
	{0x0344, 0x00, 0x00, CSIPHY_3PH_REGS},
	{0x0744, 0x00, 0x00, CSIPHY_3PH_REGS},
	{0x0B44, 0x00, 0x00, CSIPHY_3PH_REGS},
	{0x00C0, 0x00, 0x00, CSIPHY_2PH_REGS},
	{0x04C0, 0x00, 0x00, CSIPHY_2PH_REGS},
	{0x08C0, 0x00, 0x00, CSIPHY_2PH_REGS},
	{0x0CC0, 0x00, 0x00, CSIPHY_2PH_REGS},
};

struct bist_reg_settings_t bist_setting_1_2_3 = {
	.error_status_val_3ph = 0x10,
	.error_status_val_2ph = 0x10,
	.set_status_update_3ph_base_offset = 0x0240,
	.set_status_update_2ph_base_offset = 0x0050,
	.bist_status_3ph_base_offset = 0x0344,
	.bist_status_2ph_base_offset = 0x00C0,
	.bist_sensor_data_3ph_status_base_offset = 0x0340,
	.bist_counter_3ph_base_offset = 0x0348,
	.bist_counter_2ph_base_offset = 0x00C8,
	.number_of_counters = 2,
	.num_3ph_bist_settings = ARRAY_SIZE(bist_3ph_arr_1_2_3),
	.bist_3ph_settings_arry = bist_3ph_arr_1_2_3,
	.bist_2ph_settings_arry = NULL,
	.num_2ph_bist_settings = 0,
	.num_status_reg = ARRAY_SIZE(bist_status_arr_1_2_3),
	.bist_status_arr = bist_status_arr_1_2_3,
};

struct data_rate_settings_t data_rate_delta_table_1_2_3 = {
	.num_data_rate_settings = ARRAY_SIZE(data_rate_settings_1_2_3),
	.data_rate_settings = data_rate_settings_1_2_3,
};

struct csiphy_reg_parms_t csiphy_v1_2_3 = {
	.mipi_csiphy_interrupt_status0_addr = 0x08B0,
	.mipi_csiphy_interrupt_clear0_addr = 0x0858,
	.mipi_csiphy_glbl_irq_cmd_addr = 0x0828,
	.size_offset_betn_lanes = 0x200,
	.status_reg_params = &status_regs_1_2_3,
	.csiphy_common_reg_array_size = ARRAY_SIZE(csiphy_common_reg_1_2_3),
	.csiphy_reset_enter_array_size = ARRAY_SIZE(csiphy_reset_enter_reg_1_2_3),
	.csiphy_reset_exit_array_size = ARRAY_SIZE(csiphy_reset_exit_reg_1_2_3),
	.csiphy_2ph_config_array_size = ARRAY_SIZE(csiphy_2ph_v1_2_3_reg),
	.csiphy_2ph_clk_cfg_array_size = ARRAY_SIZE(csiphy_2ph_v1_2_3_clk_ln_reg),
	.csiphy_3ph_config_array_size = ARRAY_SIZE(csiphy_3ph_v1_2_3_reg),
	.csiphy_interrupt_status_size = ARRAY_SIZE(csiphy_irq_reg_1_2_3),
	.csiphy_num_common_status_regs = 12,
	.aon_sel_params = &aon_cam_select_params_1_2_3,
};

struct csiphy_ctrl_t ctrl_reg_1_2_3 = {
	.csiphy_common_reg = csiphy_common_reg_1_2_3,
	.csiphy_2ph_reg = csiphy_2ph_v1_2_3_reg,
	.csiphy_2ph_clk_ln_reg = csiphy_2ph_v1_2_3_clk_ln_reg,
	.csiphy_3ph_reg = csiphy_3ph_v1_2_3_reg,
	.csiphy_reg = &csiphy_v1_2_3,
	.csiphy_irq_reg = csiphy_irq_reg_1_2_3,
	.csiphy_reset_enter_regs = csiphy_reset_enter_reg_1_2_3,
	.csiphy_reset_exit_regs = csiphy_reset_exit_reg_1_2_3,
	.csiphy_lane_config_reg = csiphy_lane_en_reg_1_2_3,
	.csiphy_ln_offsets[DPHY_DATA_LANE_POS_0] = 0x0000,
	.csiphy_ln_offsets[DPHY_DATA_LANE_POS_1] = 0x0200,
	.csiphy_ln_offsets[DPHY_DATA_LANE_POS_2] = 0x0400,
	.csiphy_ln_offsets[DPHY_DATA_LANE_POS_3] = 0x0600,
	.csiphy_ln_offsets[DPHY_CLOCK_LANE_POS] = 0x0700,
	.csiphy_ln_offsets[CPHY_LANE_POS_0] = 0x0100,
	.csiphy_ln_offsets[CPHY_LANE_POS_1] = 0x0300,
	.csiphy_ln_offsets[CPHY_LANE_POS_2] = 0x0500,
	.data_rates_settings_table = &data_rate_delta_table_1_2_3,
	.csiphy_bist_reg = &bist_setting_1_2_3,
	.getclockvoting = get_clk_voting_dynamic,
	.csiphy_reg_addr_func = csiphy_reg_addr_func_1_2_3,
};

#endif /* _CAM_CSIPHY_1_2_3_HWREG_H_ */
