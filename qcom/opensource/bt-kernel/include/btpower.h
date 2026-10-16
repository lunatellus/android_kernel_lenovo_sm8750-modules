/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LINUX_BLUETOOTH_POWER_H
#define __LINUX_BLUETOOTH_POWER_H

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/soc/qcom/qcom_aoss.h>
#include <linux/workqueue.h>
#include <linux/skbuff.h>

/*
 * voltage regulator information required for configuring the
 * bluetooth chipset
 */

enum power_modes {
	POWER_DISABLE = 0,
	POWER_ENABLE,
	POWER_RETENTION,
	POWER_DISABLE_RETENTION,
};

enum SubSystem {
	BLUETOOTH = 1,
	UWB,
};

enum FmdOperation {
	ENABLE_FMD,
	DISABLE_FMD,
	UPDATE_SOC_VER
};

enum power_states {
	IDLE = 0,
	BT_ON,
	UWB_ON,
	ALL_CLIENTS_ON,
};

enum retention_states {
	/* Default state */
	RETENTION_IDLE = 0,
	/* When BT is only client and it is in retention_state */
	BT_IN_RETENTION,
	/* BT is retention mode and UWB powered ON triggered */
	BT_OUT_OF_RETENTION,
	/* When UWB is only client and it is in retention_state */
	UWB_IN_RETENTION,
	/* UWB is retention mode and BT powered ON triggered */
	UWB_OUT_OF_RETENTION,
	/* Both clients are voted for retention */
	BOTH_CLIENTS_IN_RETENTION,
};

enum grant_return_values {
	ACCESS_GRANTED = 0,
	ACCESS_DENIED  = 1,
	ACCESS_RELEASED = 2,
	ACCESS_DISALLOWED = -1,
};

enum grant_states {
	/* Default state */
	NO_GRANT_FOR_ANY_SS = 0,
	NO_OTHER_CLIENT_WAITING_FOR_GRANT,
	BT_HAS_GRANT,
	UWB_HAS_GRANT,
	BT_WAITING_FOR_GRANT,
	UWB_WAITING_FOR_GRANT,
};

static inline char *ConvertGrantRetToString(enum grant_return_values state)
{
	switch (state) {
	case ACCESS_GRANTED:
		return "ACCESS_GRANTED";
	case ACCESS_DENIED:
		return "ACCESS_DENIED";
	case ACCESS_RELEASED:
		return "ACCESS_RELEASED";
	case ACCESS_DISALLOWED:
		return "ACCESS_DISALLOWED";
	default:
		return "INVALID State";
	}
}

static inline char *ConvertGrantToString(enum grant_states state)
{
	switch (state) {
	case NO_GRANT_FOR_ANY_SS:
		return "NO_GRANT_FOR_ANY_SS";
	case NO_OTHER_CLIENT_WAITING_FOR_GRANT:
		return "NO_OTHER_CLIENT_WAITING_FOR_GRANT";
	case BT_HAS_GRANT:
		return "BT_HAS_GRANT";
	case UWB_HAS_GRANT:
		return "UWB_HAS_GRANT";
	case BT_WAITING_FOR_GRANT:
		return "BT_WAITING_FOR_GRANT";
	case UWB_WAITING_FOR_GRANT:
		return "UWB_WAITING_FOR_GRANT";
	default:
		return "INVALID STATE";
	}
}

enum cores {
	BT_CORE = 0,
	UWB_CORE,
	PLATFORM_CORE
};

enum ssr_states {
	SUB_STATE_IDLE = 0,
	SSR_ON_BT,
	BT_SSR_COMPLETED,
	SSR_ON_UWB,
	UWB_SSR_COMPLETED,
	REG_BT_PID,
	REG_UWB_PID,
};

enum plt_pwr_state {
	POWER_ON_BT = 0,
	POWER_OFF_BT,
	POWER_ON_UWB,
	POWER_OFF_UWB,
	POWER_ON_BT_RETENION,
	POWER_ON_UWB_RETENION,
	BT_ACCESS_REQ,
	UWB_ACCESS_REQ,
	BT_RELEASE_ACCESS,
	UWB_RELEASE_ACCESS,
	BT_GET_PWR_STATE,
	BT_MAX_PWR_STATE,
};

enum {
	PWR_WAITING_RSP = -2,
	PWR_RSP_RECV = 0,
	PWR_FAIL_RSP_RECV = -1,
	PWR_CLIENT_KILLED,
};

static inline char *ConvertRetentionModeToString(int state)
{
	switch (state) {
	case IDLE:
		return "Both client not in Retention";
	case BT_IN_RETENTION:
		return "BT in Retention";
	case BT_OUT_OF_RETENTION:
		return "BT is out off Retention";
	case UWB_IN_RETENTION:
		return "UWB in Retention";
	case UWB_OUT_OF_RETENTION:
		return "UWB is out off Retention";
	case BOTH_CLIENTS_IN_RETENTION:
		return "Both client in Retention";
	default:
		return "Retention state = INVALID STATE";
	}
}

static inline char *ConvertClientReqToString(int arg)
{
	switch (arg) {
	case POWER_DISABLE:
		return "Power OFF";
	case POWER_ENABLE:
		return "Power ON";
	case POWER_RETENTION:
		return "Power Retention";
	default:
		return "INVALID STATE";
	}
}

static inline char *ConvertPowerStatusToString(int state)
{
	switch (state) {
	case IDLE:
		return "Current state is ALL Client OFF";
	case BT_ON:
		return "Current state is BT powered ON";
	case UWB_ON:
		return "Current state is UWB powered ON";
	case ALL_CLIENTS_ON:
		return "Current state is ALL Client ON";
	default:
		return "Current state is = INVALID STATE";
	}
}

static inline char *ConvertSsrStatusToString(int state)
{
	switch (state) {
	case SUB_STATE_IDLE:
		return "and No SSR";
	case SSR_ON_BT:
		return "and SSR on BT";
	case BT_SSR_COMPLETED:
		return "and BT SSR completed";
	case SSR_ON_UWB:
		return "and SSR on UWB";
	case UWB_SSR_COMPLETED:
		return "and UWB SSR completed";
	default:
		return "SSR STATE = INVALID STATE";
	}
}

static inline char *ConvertPowerReqToString(int arg)
{
	switch (arg) {
	case POWER_ON_BT:
		return "POWER_ON_BT";
	case POWER_OFF_BT:
		return "POWER_OFF_BT";
	case POWER_ON_UWB:
		return "POWER_ON_UWB";
	case POWER_OFF_UWB:
		return "POWER_OFF_UWB";
	case POWER_ON_BT_RETENION:
		return "POWER_ON_BT_RETENION";
	case POWER_ON_UWB_RETENION:
		return "POWER_ON_UWB_RETENION";
	case BT_ACCESS_REQ:
		return "BT_ACCESS_REQ";
	case UWB_ACCESS_REQ:
		return "UWB_ACCESS_REQ";
	case BT_RELEASE_ACCESS:
		return "BT_RELEASE_ACCESS";
	case UWB_RELEASE_ACCESS:
		return "UWB_RELEASE_ACCESS";
	case BT_MAX_PWR_STATE:
		return "BT_MAX_PWR_STATE";
	default:
		return "INVALID STATE";
	}
};

static inline char *ConvertRegisterModeToString(int reg_mode)
{
	switch (reg_mode) {
	case POWER_DISABLE:
		return "vote off";
	case POWER_ENABLE:
		return "vote on";
	case POWER_RETENTION:
		return "vote for retention";
	case POWER_DISABLE_RETENTION:
		return "vote offretention";
	default:
		return "INVALID STATE";
	}
}

static inline char *ConvertErrorCodeToString(int error_code)
{
    switch (error_code) {
    case -ENOMEM:
        return "Out of memory";
    case -EIO:
        return "I/O error";
    case -EINVAL:
        return "Invalid argument";
    case -ENODATA:
        return "No data available";
    case -EBUSY:
        return "Device or resource busy";
    case -ETIMEDOUT:
        return "Operation timed out";
    case -ENODEV:
        return "No such device";
    case -EFAULT:
        return "Bad address";
    case -EAGAIN:
        return "Try again";
    default:
        return "UNDEFINED ERROR";
    }
}

struct log_index {
	int init;
	int crash;
};

struct fmdOperationStruct {
	unsigned char fmdOperation;
	unsigned char socFwVer;
	short int rebootStatus;
	short int fmdCycles;
};

struct vreg_data {
	struct regulator *reg;  /* voltage regulator handle */
	const char *name;       /* regulator name */
	u32 min_vol;            /* min voltage level */
	u32 max_vol;            /* max voltage level */
	u32 load_curr;          /* current */
	bool is_enabled;        /* is this regulator enabled? */
	bool is_retention_supp; /* does this regulator support retention mode */
	struct log_index indx;  /* Index for reg. w.r.t init & crash */
	bool fmd_mode_set;
};

struct pwr_data {
	char compatible[32];
	struct vreg_data *bt_vregs;
	int bt_num_vregs;
	struct vreg_data *uwb_vregs;
	int uwb_num_vregs;
	struct vreg_data *platform_vregs;
	int platform_num_vregs;
};

struct bt_power_clk_data {
	struct clk *clk;  /* clock regulator handle */
	const char *name; /* clock name */
	bool is_enabled;  /* is this clock enabled? */
};

struct btpower_state_machine {
	struct mutex state_machine_lock;
	enum power_states power_state;
	enum retention_states retention_mode;
	enum grant_states grant_state;
	enum grant_states grant_pending;
};

#define BTPWR_MAX_REQ         BT_MAX_PWR_STATE 
/*
 * Platform data for the bluetooth power driver.
 */
struct platform_pwr_data {
	struct platform_device *pdev;
	int bt_gpio_sys_rst;                   /* Bluetooth reset gpio */
	int wl_gpio_sys_rst;                   /* Wlan reset gpio */
	int bt_gpio_sw_ctrl;                   /* Bluetooth sw_ctrl gpio */
	int bt_gpio_fmd_clk_ctrl;              /* Bluetooth fmd_clk_ctrl gpio */
	int bt_gpio_debug;                     /* Bluetooth debug gpio */
	unsigned int wlan_sw_ctrl_gpio;        /* Wlan switch control gpio*/
	int bt_gpio_resetb;                    /* BT RESETB GPIO */
	bool bt_gpio_sys_rst_requested;        /* tracks if bt_gpio_sys_rst is currently requested */
	bool bt_gpio_debug_requested;          /* tracks if bt_gpio_debug is currently requested */
	bool bt_gpio_resetb_requested;         /* tracks if bt_gpio_resetb is currently requested */
#ifdef CONFIG_MSM_BT_OOBS
	int bt_gpio_dev_wake;                  /* Bluetooth bt_wake */
	int bt_gpio_host_wake;                 /* Bluetooth bt_host_wake */
	int irq;                               /* Bluetooth host_wake IRQ */
#endif
	int sw_cntrl_gpio;
	int xo_gpio_clk;                       /* XO clock gpio*/
	struct device *slim_dev;
	struct vreg_data *bt_vregs;
	struct vreg_data *uwb_vregs;
	struct vreg_data *wlan_vregs;
	struct vreg_data *platform_vregs;
	struct bt_power_clk_data *bt_chip_clk; /* bluetooth reference clock */
	int (*power_setup)(int core, int id);  /* Bluetooth power setup function */
	char compatible[32];                   /*Bluetooth SoC name */
	int bt_num_vregs;
	int uwb_num_vregs;
	int platform_num_vregs;
	struct qmp *qmp;
	struct mbox_chan *mbox_chan;
	const char *vreg_ipa;
	bool is_multi_tech_soc_dt;
	int pdc_init_table_len;
	const char **pdc_init_table;
	int bt_device_type;
	bool sec_peri_feature_disable;
	int bt_sec_hw_disable;
#ifdef CONFIG_MSM_BT_OOBS
	struct file *reffilp_obs;
	struct task_struct *reftask_obs;
#endif
	struct task_struct *reftask_bt;
	struct task_struct *reftask_uwb;
	struct btpower_state_machine btpower_state;
	enum ssr_states sub_state;
	enum ssr_states wrkq_signal_state;
	struct workqueue_struct *workq;
	struct workqueue_struct *pwr_vote_wq; /* dedicated WQ for power voting */
	struct device_node *bt_of_node;
	struct device_node *uwb_of_node;
	struct work_struct bt_wq;
	struct work_struct uwb_wq;
	wait_queue_head_t rsp_wait_q[BTPWR_MAX_REQ];
	int wait_status[BTPWR_MAX_REQ];
	struct work_struct wq_pwr_voting;
	struct sk_buff_head rxq;
	struct mutex pwr_mtx;
	bool is_fmd_mode_enable;
	struct nvmem_cell *nvmem_cell_fmd_set;
	struct nvmem_cell *nvmem_cell_fmd_chg_pon;
	struct nvmem_cell *nvmem_cell_fmd_cnt2_stop;
#ifdef CONFIG_EXT_BUCK
	struct nvmem_cell *nvmem_cell_ext_bk;
	bool is_ext_bk_enabled;
#endif
	u32 fmd_clk_gpio_id;
};

int btpower_register_slimdev(struct device *dev);
int btpower_get_chipset_version(void);
int btpower_aop_mbox_init(struct platform_pwr_data *pdata);
int bt_aop_pdc_reconfig(struct platform_pwr_data *pdata);

#define WLAN_SW_CTRL_GPIO           "qcom,wlan-sw-ctrl-gpio"
#define BT_CMD_SLIM_TEST            0xbfac
#define BT_CMD_PWR_CTRL             0xbfad
#define BT_CMD_CHIPSET_VERS         0xbfae
#define BT_CMD_GET_CHIPSET_ID       0xbfaf
#define BT_CMD_CHECK_SW_CTRL        0xbfb0
#define BT_CMD_GETVAL_POWER_SRCS    0xbfb1
#define BT_CMD_SET_IPA_TCS_INFO     0xbfc0
#define BT_CMD_KERNEL_PANIC         0xbfc1
#define UWB_CMD_KERNEL_PANIC        0xbfc2
#define UWB_CMD_PWR_CTRL            0xbfe1
#define BT_CMD_REGISTRATION         0xbfe2
#define UWB_CMD_REGISTRATION        0xbfe3
#define BT_CMD_ACCESS_CTRL          0xbfe4
#define UWB_CMD_ACCESS_CTRL         0xbfe5
#define UWB_GET_SSR_STATE           0xbfe6
#define BT_CMD_GET_DT_PROPERTIES    0xbfe8    /* Batch ioctl: query multiple DT properties in one call */

/* Max DT property name length including null terminator */
#define BT_DT_PROP_NAME_MAX_LEN         64
/* Max raw DT property data bytes — covers all BT DT property types */
#define BT_DT_PROP_DATA_MAX_LEN         256
/* Max properties per batch query */
#define BT_DT_PROP_BATCH_MAX            8

/* status codes for bt_dt_property.status */
enum bt_dt_prop_status {
	BT_DT_PROP_FOUND        = 0,  /* property found in DT */
	BT_DT_PROP_NOT_FOUND    = 1,  /* property not present in DT */
	BT_DT_PROP_INVALID_NAME = 2,  /* name failed prefix/safety check */
};

/* holds DT property name and raw bytes returned by kernel */
struct bt_dt_property {
	char     name[BT_DT_PROP_NAME_MAX_LEN];
	uint32_t length;                          /* bytes populated in data[] */
	uint8_t  data[BT_DT_PROP_DATA_MAX_LEN];  /* raw big-endian bytes from DT */
	uint32_t status;                          /* see enum bt_dt_prop_status */
};

/* holds a batch of DT property queries — one ioctl replaces N individual ioctls */
struct bt_dt_property_batch {
	uint32_t              count;                           /* IN: number of properties to query */
	struct bt_dt_property props[BT_DT_PROP_BATCH_MAX];    /* IN: names; OUT: length+data */
};
#define BT_CMD_FMD_OPERATION        0xbfb2
#define BT_CMD_GET_PWR_STATE        0xbfb4
#define BT_CMD_CP_ENABLE_CHECK      0xbfe7

#ifdef CONFIG_MSM_BT_OOBS
#define BT_CMD_OBS_VOTE_CLOCK		0xbfd1
/**
 * enum btpower_obs_param: OOBS low power param
 * @BTPOWER_OBS_CLK_OFF: Transport bus is no longer acquired
 * @BTPOWER_OBS_CLK_ON: Acquire transport bus for either transmitting or receiving
 * @BTPOWER_OBS_DEV_OFF: Bluetooth is released because of no more transmission
 * @BTPOWER_OBS_DEV_ON: Wake up the Bluetooth controller for transmission
 */
enum btpower_obs_param {
	BTPOWER_OBS_CLK_OFF = 0,
	BTPOWER_OBS_CLK_ON,
	BTPOWER_OBS_DEV_OFF,
	BTPOWER_OBS_DEV_ON,
};
#endif
#endif /* __LINUX_BLUETOOTH_POWER_H */
