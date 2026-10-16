/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __H_MSM_VIDC_POWER_IRIS2_H__
#define __H_MSM_VIDC_POWER_IRIS2_H__

#include "msm_vidc_power.h"
#include "msm_vidc_inst.h"

int msm_vidc_calc_bw_iris2(struct msm_vidc_inst *inst,
					struct vidc_bus_vote_data *vote_data);
int msm_vidc_scale_clocks_iris2(struct msm_vidc_inst *inst);


#endif
