/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CPASTOP_V662_100_H_
#define _CPASTOP_V662_100_H_

#define TEST_IRQ_ENABLE 0

static struct cam_camnoc_irq_sbm cam_cpas_v662_100_irq_sbm = {
	.sbm_enable = {
		.access_type = CAM_REG_TYPE_READ_WRITE,
		.enable = true,
		.offset = 0x6840, /* CAM_NOC_SBM_FAULTINEN0_LOW */
		.value = 0x2 |    /* SBM_FAULTINEN0_LOW_PORT1_MASK */
			0x04 |     /* SBM_FAULTINEN0_LOW_PORT2_MASK */
			0x08 |     /* SBM_FAULTINEN0_LOW_PORT3_MASK */
			0x10 |    /* SBM_FAULTINEN0_LOW_PORT4_MASK */
			0x20 |    /* SBM_FAULTINEN0_LOW_PORT5_MASK */
			(TEST_IRQ_ENABLE ?
			0x80 :    /* SBM_FAULTINEN0_LOW_PORT7_MASK */
			0x0),
	},
	.sbm_status = {
		.access_type = CAM_REG_TYPE_READ,
		.enable = true,
		.offset = 0x6848, /* CAM_NOC_SBM_FAULTINSTATUS0_LOW */
	},
	.sbm_clear = {
		.access_type = CAM_REG_TYPE_WRITE,
		.enable = true,
		.offset = 0x6880, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		.value = TEST_IRQ_ENABLE ? 0x5 : 0x1,
	}
};

static struct cam_camnoc_irq_err
	cam_cpas_v662_100_irq_err[] = {
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_SLAVE_ERROR,
		.enable = false,
		.sbm_port = 0x1, /* SBM_FAULTINSTATUS0_LOW */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x6608, /* CAM_NOC_ERL_MAINCTL_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x6610, /* CAM_NOC_ERL_ERRVLD_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x6618, /* CAM_NOC_ERL_ERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x2, /* SBM_FAULTINSTATUS0_LOW_PORT1_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x5DA0, /* WR_NIU_ENCERREN_LOW */
			.value = 0XF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x5D90, /* WR_NIU_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x5D98, /* WR_NIU_ENCERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR,
		.enable = true,
		.sbm_port = 0x4, /* SBM_FAULTINSTATUS0_LOW_PORT2_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x5F20, /* CAM_NOC_IPE_0_RD_NIU_DECERREN_LOW */
			.value = 0xFF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x5F10, /* CAM_NOC_IPE_0_RD_NIU_DECERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x5F18, /* CAM_NOC_IPE_0_RD_NIU_DECERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT,
		.enable = false,
		.sbm_port = 0x40, /* SBM_FAULTINSTATUS0_LOW_PORT6_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x6888, /* CAM_NOC_SBM_FLAGOUTSET0_LOW */
			.value = 0x1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x6890, /* CAM_NOC_SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_RESERVED1,
		.enable = false,
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_RESERVED2,
		.enable = false,
	},
};

static struct cam_camnoc_niu
	cam_cpas_v662_100_camnoc_niu[] = {
	{
		.port_type = CAM_CAMNOC_TFE_BAYER_STATS,
		.port_name = "TFE_BAYER",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5830, /*PRIORITYLUT_LOW */
			.value = 0x55554433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5834, /* PRIORITYLUT_HIGH */
			.value = 0x66665555,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5838, /* URGENCY_LOW */
			.value = 0x1030,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5840, /* DANGERLUT_LOW */
			.value = 0xff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5848, /* SAFELUT_LOW */
			.value = 0xff0f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4208, /* QOSGEN_MAINCTL */
			.value = 0x30,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4220, /* QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4224, /* QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x5820, /* UBWC_MAXWR_LOW */
			.value = 0x0,
		},
		.maxwrclr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_WRITE,
			.masked_value = 0,
			.offset = 0x5828, /* UBWC_MAXWRCLR_LOW */
			.value = 0x1,
		},
	},
	{
		.port_type = CAM_CAMNOC_OPE_BPS_WR,
		.port_name = "OPE_BPS_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5C30, /* PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5C34, /* PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5C38, /* URGENCY_LOW */
			.value = 0x30,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5C40, /* DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5C48, /* SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4608, /* QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4620, /* QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4624, /* QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x5C20, /* MAXWR_LOW */
			.value = 0x0,
		},
		.maxwrclr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_WRITE,
			.masked_value = 0,
			.offset = 0x5C28, /* MAXWRCLR_LOW */
			.value = 0x1,
		},
	},
	{
		.port_type = CAM_CAMNOC_OPE_BPS_CDM_RD,
		.port_name = "OPE_BPS_CDM_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5E30, /* IPE_WR_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5E34, /* IPE_WR_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5E38, /* IPE_WR_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5E40, /* IPE_WR_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5E48, /* IPE_WR_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4808, /* IPE_WR_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4820, /* IPE_WR_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4824, /* IPE_WR_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_CRE,
		.port_name = "CRE_RD_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6030, /* CRE_WR_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6034, /* CRE_WR_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6038, /* CRE_WR_URGENCY_LOW */
			.value = 0x33,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6040, /* CRE_WR_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6048, /* CRE_WR_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4A08, /* CRE_WR_QOSGEN_MAINCTL */
			.value = 0x30,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4A20, /* CRE_WR_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4A24, /* CRE_WR_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x6020, /* CRE_WR_MAXWR_LOW */
			.value = 0x0,
		},
		.maxwrclr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_WRITE,
			.masked_value = 0,
			.offset = 0x6028, /* CRE_WR_MAXWRCLR_LOW */
			.value = 0x1,
		},
	},
	{
		.port_type = CAM_CAMNOC_ICP,
		.port_name = "ICP",
		.enable = false,
		.flag_out_set0_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_WRITE,
			.masked_value = 0,
			.offset = 0x6888,
			.value = 0x100000,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4008, /* ICP_QOSGEN_MAINCTL */
			.value = 0x50,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4020, /* ICP_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4024, /* ICP_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
};

static struct cam_camnoc_err_logger_info cam662_cpas100_err_logger_offsets = {
	.mainctrl     =  0x6608, /* ERRLOGGER_MAINCTL_LOW */
	.errvld       =  0x6610, /* ERRLOGGER_ERRVLD_LOW */
	.errlog0_low  =  0x6620, /* ERRLOGGER_ERRLOG0_LOW */
	.errlog0_high =  0x6624, /* ERRLOGGER_ERRLOG0_HIGH */
	.errlog1_low  =  0x6628, /* ERRLOGGER_ERRLOG1_LOW */
	.errlog1_high =  0x662c, /* ERRLOGGER_ERRLOG1_HIGH */
	.errlog2_low  =  0x6630, /* ERRLOGGER_ERRLOG2_LOW */
	.errlog2_high =  0x6634, /* ERRLOGGER_ERRLOG2_HIGH */
	.errlog3_low  =  0x6638, /* ERRLOGGER_ERRLOG3_LOW */
	.errlog3_high =  0x663c, /* ERRLOGGER_ERRLOG3_HIGH */
};

static struct cam_cpas_hw_errata_wa_list cam662_cpas100_errata_wa_list = {
};

static struct cam_camnoc_info cam662_cpas100_camnoc_info = {
	.camnoc_type = CAM_CAMNOC_HW_COMBINED,
	.reg_base = CAM_CPAS_REG_CAMNOC,
	.niu = &cam_cpas_v662_100_camnoc_niu[0],
	.num_nius = ARRAY_SIZE(cam_cpas_v662_100_camnoc_niu),
	.irq_sbm = &cam_cpas_v662_100_irq_sbm,
	.irq_err = &cam_cpas_v662_100_irq_err[0],
	.irq_err_size = ARRAY_SIZE(cam_cpas_v662_100_irq_err),
	.err_logger = &cam662_cpas100_err_logger_offsets,
	.errata_wa_list = &cam662_cpas100_errata_wa_list,
};

static struct cam_cpas_camnoc_qchannel cam662_cpas100_qchannel_info = {
	.camnoc_info = &cam662_cpas100_camnoc_info,
	.qchannel_ctrl   = 0x14,
	.qchannel_status = 0x18,
};

static struct cam_tpg_mux_regs cam662_cpas100_cpas_tpg_mux_info = {
	.tpg_mux_sel_enabled = true,
	.tpg_mux_sel_shift   = 0x0,
	.tpg_mux_sel         = 0x1C,
};

static struct cam_cpas_subpart_info cam662_cpas_camera_subpart_info = {
	.num_bits = 2,
	/*
	 * Below fuse indexing is based on software fuse definition which is in SMEM and provided
	 * by XBL team.
	 */
	.hw_bitmap_mask = {
		{CAM_CPAS_ISP_FUSE, BIT(0)},
		{CAM_CPAS_ISP_FUSE, BIT(1)},
	}
};

static struct cam_cpas_info cam662_cpas100_cpas_info = {
	.hw_caps_info = {
		.num_caps_registers = 1,
		.hw_caps_offsets = {0x8},
	},
	.qchannel_info = {&cam662_cpas100_qchannel_info},
	.num_qchannel = 1,
	.tpg_mux_info = &cam662_cpas100_cpas_tpg_mux_info,
	.camera_arch = CAM_CPAS_HW_MIMAS_ARCH,
	.subpart_info = &cam662_cpas_camera_subpart_info,
};

static struct cam_cpas_hw_info cam662_cpas100_hw_info = {
	.hw_info_version                     = CAM_CPAS_HW_INFO_VER1,
	.camnoc_info[CAM_CAMNOC_HW_COMBINED] = &cam662_cpas100_camnoc_info,
	.cpas_info                           = &cam662_cpas100_cpas_info,
};

#endif /* _CPASTOP_V662_100_H_ */

