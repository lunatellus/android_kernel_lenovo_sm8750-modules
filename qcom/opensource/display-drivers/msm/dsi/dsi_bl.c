// SPDX-License-Identifier: GPL-2.0-only
/* Reconstructed elden AW99706A / KTZ8866 backlight driver. */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/math64.h>
#include <linux/pm_runtime.h>
#include <linux/ktime.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/devinfo.h>



#include <linux/hq_hw_info.h>

#include "dsi_panel.h"



struct i2c_client;
struct elden_bl_profile {
	char *name;
	u32 addr;
	const u8 *init;
	int init_len;
	const u8 *suspend;
	int suspend_len;
	const u8 *match;
	int match_len;
	const u32 *hbm;
	int params[10];
	int probe_count;
	struct device *dev;
	struct regmap *regmap;
	struct i2c_client *device;
};

static u8 aw99706a_init_table[] = {
	0x0d, 0x00, 0xff, 0x0c, 0x48, 0xff, 0x06, 0xd9, 0xff,
	0x07, 0x05, 0xff, 0x04, 0x00, 0xff, 0x05, 0x00, 0xff,
	0x08, 0x01, 0xff, 0x0d, 0xf8, 0xff,
};
static u8 aw99706a_suspend_table[] = { 0x0d, 0x00, 0xff };
static u8 aw99706a_match_table[] = { 0x11, 0x07, 0xff };
static u32 aw99706a_hbm_table[] = { 300 };
static u8 ktz8866_init_table[] = {
	0x02, 0xfb, 0xff, 0x03, 0xc5, 0xff, 0x11, 0x37, 0xff,
	0x14, 0x88, 0xff, 0x04, 0x00, 0xff, 0x05, 0x00, 0xff,
	0x08, 0x4f, 0xff,
};
static u8 ktz8866_suspend_table[] = { 0x08, 0x00, 0xff };
static u8 ktz8866_match_table[] = { 0x01, 0x20, 0x38 };
static u32 ktz8866_hbm_table[] = { 300 };

static struct elden_bl_profile bl_data[] = {
	{
		.name = "aw99706a",
		.addr = 0x76,
		.init = aw99706a_init_table,
		.init_len = sizeof(aw99706a_init_table) / 3,
		.suspend = aw99706a_suspend_table,
		.suspend_len = sizeof(aw99706a_suspend_table) / 3,
		.match = aw99706a_match_table,
		.match_len = sizeof(aw99706a_match_table) / 3,
		.hbm = aw99706a_hbm_table,
		.params = { 1, 2, 127, 5, 50, 4, 5, 15, 255, 1 },
	},
	{
		.name = "ktz8866",
		.addr = 0x11,
		.init = ktz8866_init_table,
		.init_len = sizeof(ktz8866_init_table) / 3,
		.suspend = ktz8866_suspend_table,
		.suspend_len = sizeof(ktz8866_suspend_table) / 3,
		.match = ktz8866_match_table,
		.match_len = sizeof(ktz8866_match_table) / 3,
		.hbm = ktz8866_hbm_table,
		.params = { 1, 21, 248, 8, 52, 5, 4, 255, 7, 0 },
	},
};
#define ELDEN_BL_MAX_BRIGHTNESS(p) \
	(((p)->params[7] << __builtin_popcount((p)->params[8])) | (p)->params[8])



struct elden_bl_context {
	u32 probe_count;
	int last_level;
	int hbm_level;
	u8 reserved0[4];
	struct delayed_work check_work;
	u8 reserved1[0x98 - 0x10 - sizeof(struct delayed_work)];
};

static struct elden_bl_context bl_context;
static const struct regmap_config config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static inline int bl_set_init_table(struct elden_bl_profile *profile)
{
	const u8 *table = profile->init;
	int len = profile->init_len;
	int i;
	int ret;

	for (i = 0; i < len; i++, table += 3) {
		if (table[2] != 0xff) {
			ret = regmap_update_bits(profile->regmap, table[0],
						 table[2], table[1]);
			if (ret) {
				pr_err("[drm][bl][%s:%d] set reg failed %02x=>%02x\n",
				       __func__, 203, table[0], table[1]);
				return ret;
			}
		} else {
			ret = regmap_write(profile->regmap, table[0], table[1]);
			if (ret) {
				pr_err("[drm][bl][%s:%d] set reg failed %02x=>%02x\n",
				       __func__, 208, table[0], table[1]);
				return ret;
			}
		}
	}
	return 0;
}

static inline int bl_set_suspend_table(struct elden_bl_profile *profile)
{
	const u8 *table = profile->suspend;
	int len = profile->suspend_len;
	int i;
	int ret;

	for (i = 0; i < len; i++, table += 3) {
		/* The brightness registers were already cleared. */
		if (table[0] == profile->params[6] ||
		    table[0] == profile->params[5])
			continue;
		if (table[2] == 0xff) {
			ret = regmap_write(profile->regmap, table[0], table[1]);
			if (ret) {
				pr_err("[drm][bl][%s:%d] set reg failed %02x=>%02x\n",
				       __func__, 226, table[0], table[1]);
				return ret;
			}
		} else {
			ret = regmap_update_bits(profile->regmap, table[0],
						 table[2], table[1]);
			if (ret) {
				pr_err("[drm][bl][%s:%d] set reg failed %02x=>%02x\n",
				       __func__, 231, table[0], table[1]);
				return ret;
			}
		}
	}
	return 0;
}


static int _bl_set_brightness(struct elden_bl_profile *profile,
			      int level)
{
	u32 shift = profile->params[8] ?
		    __builtin_popcount(profile->params[8]) : 0;
	int ret;

	pr_info("[drm][bl][%s:%d] %s: set backlight %d\n",
		__func__, 321, profile->name, level);

	if (profile->params[9]) {
		ret = regmap_write(profile->regmap, profile->params[5],
				   (((level >> shift) <<
				     __builtin_ctz(profile->params[7])) &
				    profile->params[7]));
		if (ret) {
			pr_err("[drm][bl][%s:%d] %s: write brightness error\n",
			       __func__, 324, profile->name);
			return ret;
		}
		ret = regmap_write(profile->regmap, profile->params[6],
				   ((level <<
				     __builtin_ctz(profile->params[8])) &
				    profile->params[8]));
		if (ret)
			pr_err("[drm][bl][%s:%d] %s: write brightness error\n",
			       __func__, 326, profile->name);
		return ret;
	}
	ret = regmap_write(profile->regmap, profile->params[6],
			   ((level <<
			     __builtin_ctz(profile->params[8])) &
			    profile->params[8]));
	if (ret) {
		pr_err("[drm][bl][%s:%d] %s: write brightness error\n",
		       __func__, 330, profile->name);
		return ret;
	}
	ret = regmap_write(profile->regmap, profile->params[5],
			   (((level >> shift) <<
			     __builtin_ctz(profile->params[7])) &
			    profile->params[7]));
	if (ret)
		pr_err("[drm][bl][%s:%d] %s: write brightness error\n",
		       __func__, 332, profile->name);
	return ret;
}
static inline int bl_set_current(struct elden_bl_profile *profile, long level)
{
	u32 current_value = profile->hbm[level];
	int current_reg = ((int)(current_value - profile->params[4]) /
			   (int)profile->params[3]);
	int ret;

	pr_info("[drm][bl][%s:%d] set current %d.%d mA\n",
		__func__, 246, (int)(current_value / 10),
		(int)(current_value % 10));
	ret = regmap_update_bits(profile->regmap, profile->params[1],
				 profile->params[2],
				 current_reg << __builtin_ctz(profile->params[2]));
	if (ret) {
		pr_err("[drm][bl][%s:%d] update led current reg failed\n",
		       __func__, 248);
		return -EIO;
	}
	return 0;
}

static void bl_check_regs(struct work_struct *work)
{
	u8 regs[32];
	unsigned int i;

	if (!bl_context.last_level) {
		pr_info("[drm][bl][%s:%d] backlight off\n",
			__func__, 393);
		return;
	}
	for (i = 0; i < ARRAY_SIZE(bl_data); i++) {
		struct elden_bl_profile *p = &bl_data[i];
		int ret;

		if (!p->probe_count)
			continue;
		ret = regmap_bulk_read(p->regmap, 0, regs, sizeof(regs));
		if (ret) {
			pr_info("[drm][bl][%s:%d] error read\n",
				__func__, 403);
			break;
		}
		print_hex_dump(KERN_INFO, "[drm] BL_REGS:",
			       DUMP_PREFIX_OFFSET, 16, 1, regs,
			       sizeof(regs), false);
	}
}


static int bl_resume(struct device *dev);

static int probe(struct i2c_client *client)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(bl_data); i++) {
		struct elden_bl_profile *p = &bl_data[i];
		const u8 *match;
		int match_len;
		unsigned int high, low;
		int j, value;

		if (client->addr != p->addr || p->probe_count >= 1) {
			pr_info("[drm][bl][%s:%d] skip match %s\n\n",
				__func__, 428, p->name);
			continue;
		}
		match = p->match;
		match_len = p->match_len;
		i2c_set_clientdata(client, p);
		bl_resume(&client->dev);
		pr_info("[drm][bl][%s:%d] try match bl %s\n",
			__func__, 433, p->name);
		for (j = 0; j < match_len; j++) {
			u8 reg = match[0], val = match[1], mask = match[2];

			value = i2c_smbus_read_byte_data(client, reg);
			if (value < 0) {
				pr_err("[drm][bl][%s:%d] i2c error, check next\n",
				       __func__, 443);
				break;
			}
			pr_info("[drm][bl][%s:%d] reg = %02X, val = %02X, mask = %02X, rv = %02X\n",
				__func__, 446, reg, val, mask, value);
			if ((value & mask) != val)
				break;
		}
		if (j != match_len)
			continue;

		pr_info("[drm][bl][%s:%d] we will use %s\n",
			__func__, 452, p->name);
		if (IS_ENABLED(CONFIG_HQ_SYSFS))
			hq_regiser_hw_info(HWID_BL, p->name);
		p->regmap = devm_regmap_init_i2c(client, &config);
		if (IS_ERR_OR_NULL(p->regmap))
			return -ENOMEM;
		p->probe_count = 1;
		p->device = client;
		p->dev = &client->dev;
		high = 0;
		low = 0;
		regmap_read(p->regmap, p->params[5], &high);
		regmap_read(p->regmap, p->params[6], &low);
		bl_context.last_level =
			(((high & p->params[7]) >> __builtin_ctz(p->params[7])) <<
			 (p->params[8] ? __builtin_popcount(p->params[8]) : 0)) |
			((low & p->params[8]) >> __builtin_ctz(p->params[8]));
		pr_info("[drm][bl][%s:%d] %s: init brightness is %d\n",
			__func__, 473, p->name, bl_context.last_level);
		if (bl_context.last_level >= 1) {
			pr_info("[drm][bl][%s:%d] bl has been inited\n",
				__func__, 476);
			pm_runtime_get_noresume(&client->dev);
			pm_runtime_set_active(&client->dev);
		}
		pm_runtime_use_autosuspend(&client->dev);
		pm_runtime_set_autosuspend_delay(&client->dev, 2000);
		devm_pm_runtime_enable(&client->dev);
		bl_context.probe_count++;
		INIT_DELAYED_WORK(&bl_context.check_work, bl_check_regs);
		return 0;
	}
	pr_err("[drm][bl][%s:%d] no bl ic match\n", __func__, 457);
	return -EIO;
}



int bl_set_hbm(u32 level)
{
	unsigned int i;

	pr_info("[drm][bl][%s:%d] set hbm level %u\n",
		__func__, 266, level);
	for (i = 0; i < ARRAY_SIZE(bl_data); i++) {
		struct elden_bl_profile *p = &bl_data[i];
		int ret;

		if (!p->probe_count)
			continue;
		if (p->params[0] <= (u32)level) {
			pr_err("[drm][bl][%s:%d] %s: hbm level too high\n",
			       __func__, 273, p->name);
			return -EINVAL;
		}
		ret = bl_set_current(p, level);
		if (ret) {
			pr_err("[drm][bl][%s:%d] %s: fail to set hbm\n",
			       __func__, 277, p->name);
			return -EIO;
		}
	}
	bl_context.hbm_level = level;
	return 0;
}

static inline int bl_get_current(struct elden_bl_profile *profile)
{
	unsigned int control = 0;

	if (regmap_read(profile->regmap, profile->params[1], &control))
		return -EIO;
	return ((control & profile->params[2]) >>
		__builtin_ctz(profile->params[2])) *
	       profile->params[3] + profile->params[4];
}

int bl_get_hbm(void)
{
	unsigned int i;
	int level = 0;

	for (i = 0; i < ARRAY_SIZE(bl_data); i++) {
		struct elden_bl_profile *p = &bl_data[i];
		int current_value;
		int j;

		if (!p->probe_count)
			continue;
		current_value = bl_get_current(p);
		if (current_value < 0)
			return current_value;
		for (j = 0; j < p->params[0] && p->hbm[j] < (u32)current_value; j++)
			;
		if (j == p->params[0]) {
			pr_err("[drm][bl][%s:%d] %s: error current config %d\n",
			       __func__, 299, p->name, current_value);
			return -EINVAL;
		}
		pr_info("[drm][bl][%s:%d] %s: max current is %d, hbm level is %d\n",
			__func__, 302, p->name, current_value, j);
		level = j;
	}
	if (bl_context.hbm_level != level)
		pr_err("[drm][bl][%s:%d] hbm level %d is not equal to actual device setting\n",
		       __func__, 305, bl_context.hbm_level);
	return level;
}

static const struct of_device_id i2c_of_match[] = {
	{ .compatible = "i2c-backlight" },
	{}
};

static int bl_resume(struct device *dev)
{
	struct elden_bl_profile *profile = dev_get_drvdata(dev);
	int ret = 0;

	if (!IS_ERR_OR_NULL(dev->pins) &&
	    !IS_ERR_OR_NULL(dev->pins->default_state)) {
		ret = pinctrl_select_state(dev->pins->p, dev->pins->default_state);
		pr_info("[drm][bl][%s:%d] %s set default pins\n", __func__,
			177, profile->name);
	}
	return ret;
}

static int bl_suspend(struct device *dev)
{
	struct elden_bl_profile *profile = dev_get_drvdata(dev);
	int ret = 0;

	if (!IS_ERR_OR_NULL(dev->pins) &&
	    !IS_ERR_OR_NULL(dev->pins->sleep_state)) {
		ret = pinctrl_select_state(dev->pins->p, dev->pins->sleep_state);
		pr_info("[drm][bl][%s:%d] %s set sleep pin\n", __func__, 187,
			profile->name);
	}
	return ret;
}

static const struct dev_pm_ops bl_dev_pm_ops = {
	SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
	.runtime_suspend = bl_suspend,
	.runtime_resume = bl_resume,
};

static struct i2c_driver driver = {
	.driver = {
		.name = "i2c-bl",
		.of_match_table = i2c_of_match,
		.pm = &bl_dev_pm_ops,
	},
	.probe = probe,
};

int bl_set_level(u32 level)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(bl_data); i++) {
		struct elden_bl_profile *p = &bl_data[i];
		struct i2c_client *client;
		struct device *dev;

		if (!p->probe_count)
			continue;
		client = p->device;
		dev = &client->dev;
		if (!level) {
			if (!bl_context.last_level)
				continue;
			ret = _bl_set_brightness(p, 0);
			if (ret)
				return ret;
			ret = bl_set_suspend_table(p);
			if (ret)
				return ret;
			pm_runtime_mark_last_busy(dev);
			pr_info("[drm][bl][%s:%d] %s close backlight, autosuspend %d\n",
				__func__, 378, p->name,
				pm_runtime_put_autosuspend(dev));
			continue;
		}
		if (!bl_context.last_level) {
			ret = __pm_runtime_resume(dev, RPM_GET_PUT);
			if (ret < 0) {
				pm_runtime_put_noidle(dev);
				pr_err("[drm][bl][%s:%d] resume failed\n",
				       __func__, 352);
				return ret;
			}
			msleep(4);
			ret = bl_set_current(p, bl_context.hbm_level);
			if (ret)
				return ret;
			ret = bl_set_init_table(p);
			if (ret)
				return ret;
			ret = bl_set_current(p, bl_context.hbm_level);
			if (ret)
				return ret;
		}
		ret = _bl_set_brightness(p, level);
		if (!delayed_work_pending(&bl_context.check_work))
			mod_delayed_work_on(WORK_CPU_UNBOUND, system_wq,
					    &bl_context.check_work, 50);
		pm_runtime_mark_last_busy(dev);
	}
	bl_context.last_level = level;
	return ret;
}

void register_bl(void)
{
	i2c_add_driver(&driver);
}
