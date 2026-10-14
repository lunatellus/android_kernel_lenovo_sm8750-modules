// SPDX-License-Identifier: GPL-2.0-only
/* Reconstructed elden AW3750 / AS9702 bias driver. */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>

#include <linux/hq_hw_info.h>

#include "dsi_panel.h"



struct elden_bias_reg {
	u8 reg;
	u8 val;
};

struct elden_bias_match {
	u8 reg;
	u8 val;
	u8 mask;
};

struct elden_bias_profile {
	char *name;
	const struct elden_bias_reg *init;
	int init_len;
	const struct elden_bias_match *match;
	int match_len;
	bool retry_init;
};

static struct elden_bias_reg aw3750_init_table[] = {
	{ 0x00, 0x15 }, { 0x01, 0x12 }, { 0x03, 0x43 }, { 0x04, 0x09 },
};
static struct elden_bias_match aw3750_match_table[] = { { 0x04, 0x01, 0x03 } };
static struct elden_bias_reg as9702_init_table[] = {
	{ 0x00, 0x15 }, { 0x01, 0x12 }, { 0x03, 0x73 },
};
static struct elden_bias_match as9702_match_table[] = { { 0x02, 0xf1, 0xff } };

static struct elden_bias_profile data[] = {
	{ "AW37504CSR", aw3750_init_table, ARRAY_SIZE(aw3750_init_table),
	  aw3750_match_table, ARRAY_SIZE(aw3750_match_table) },
	{ "AS9702BRN", as9702_init_table, ARRAY_SIZE(as9702_init_table),
	  as9702_match_table, ARRAY_SIZE(as9702_match_table) },
};

struct elden_bias_gpio {
	char *name;
	struct pinctrl_state *state;
};

static struct elden_bias_gpio bias_gpios[] = {
	{ "enp_active" },
	{ "enn_active" },
	{ "enp_sleep" },
	{ "enn_sleep" },
};

struct elden_bias_context {
	struct regmap *regmap;
	u32 profile_index;
	struct pinctrl *pinctrl;
	u32 initialized;
};

static struct elden_bias_context context;

static const struct regmap_config config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int i2c_write_table(const struct elden_bias_profile *profile)
{
	const struct elden_bias_reg *table = profile->init;
	int len = profile->init_len;
	int i, ret;

	for (i = 0; i < len; i++) {
		ret = regmap_write(context.regmap, table[i].reg, table[i].val);
		if (ret) {
			pr_err("[drm][bias][%s:%d] set reg failed %02x=>%02x\n",
			       __func__, 117, table[i].reg, table[i].val);
			return ret;
		}
	}

	return 0;
}

int bias_enable(int enable)
{
	int ret;

	if (!context.initialized) {
		pr_err("[drm][bias][%s:%d] bias not init\n", __func__, 127);
		return -ENXIO;
	}
	pr_info("[drm][bias][%s:%d] set bias %d\n", __func__, 130, enable);

	if (enable) {
		if (!data[context.profile_index].retry_init) {
			ret = i2c_write_table(&data[context.profile_index]);
			if (ret) {
				pr_err("[drm][bias][%s:%d] can not set bias without enable, fallback enable first\n",
				       __func__, 136);
				data[context.profile_index].retry_init = true;
			}
		}
		ret = pinctrl_select_state(context.pinctrl, bias_gpios[0].state);
		if (ret) {
			pr_err("[drm][bias][%s:%d] set enp error\n", __func__, 142);
			return -EIO;
		}
		if (data[context.profile_index].retry_init) {
			ret = i2c_write_table(&data[context.profile_index]);
			if (ret) {
				pr_err("[drm][bias][%s:%d] can not set bias, maybe device is not online\n",
				       __func__, 150);
				return -EIO;
			}
		}
		msleep(5);
		ret = pinctrl_select_state(context.pinctrl, bias_gpios[1].state);
		if (ret) {
			pr_err("[drm][bias][%s:%d] set enn error\n", __func__, 157);
			return -EIO;
		}
	} else {
		ret = pinctrl_select_state(context.pinctrl, bias_gpios[3].state);
		if (ret) {
			pr_err("[drm][bias][%s:%d] set enn error\n", __func__, 163);
			return -EIO;
		}
		msleep(5);
		ret = pinctrl_select_state(context.pinctrl, bias_gpios[2].state);
		if (ret) {
			pr_err("[drm][bias][%s:%d] set enp error\n", __func__, 170);
			return -EIO;
		}
	}

	return 0;
}

static bool check_match_regs(const struct elden_bias_profile *profile)
{
	const struct elden_bias_match *match = profile->match;
	int len = profile->match_len;
	int i;

	for (i = 0; i < len; i++) {
		unsigned int value = 0;

		if (regmap_read(context.regmap, match[i].reg, &value)) {
			pr_err("[drm][bias][%s:%d] failed to read reg\n",
			       __func__, 184);
			return false;
		}
		pr_info("[drm][bias][%s:%d] read reg %02X = %02X\n",
			__func__, 187, match[i].reg, value);
		if ((value & match[i].mask) != match[i].val)
			break;
	}

	return i == len;
}

static int probe(struct i2c_client *client)
{
	int i;

	context.regmap = devm_regmap_init_i2c(client, &config);
	if (IS_ERR_OR_NULL(context.regmap)) {
		pr_err("[drm][bias][%s:%d] regmap failed, error is %ld\n",
		       __func__, 207, PTR_ERR(context.regmap));
		return -EPROBE_DEFER;
	}

	context.pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(context.pinctrl)) {
		pr_err("[drm][bias][%s:%d] get bias_pinctrl error!\n",
		       __func__, 214);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(bias_gpios); i++) {
		bias_gpios[i].state = pinctrl_lookup_state(context.pinctrl,
							   bias_gpios[i].name);
		if (IS_ERR(bias_gpios[i].state)) {
			pr_err("[drm][bias][%s:%d] pinctrl_lookup_state %s fail, ret %d\n\n",
			       __func__, 226, bias_gpios[i].name,
			       (int)PTR_ERR(bias_gpios[i].state));
			return -EINVAL;
		}
	}

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		pr_info("[drm][bias][%s:%d] do match %s\n",
			__func__, 233, data[i].name);
		if (check_match_regs(&data[i])) {
			pr_info("[drm][bias][%s:%d] match successfully, id is %d\n",
				__func__, 236, i);
			context.profile_index = i;
			break;
		}
	}
	if (i == ARRAY_SIZE(data)) {
		pr_err("[drm][bias][%s:%d] cannot find match bias\n",
		       __func__, 243);
		return -ENXIO;
	}

	if (i2c_write_table(&data[i]))
		pr_err("[drm][bias][%s:%d] failed to init bias\n",
		       __func__, 249);
	if (IS_ENABLED(CONFIG_HQ_SYSFS))
		hq_regiser_hw_info(HWID_BIAS_IC, data[i].name);
	context.initialized = true;

	return 0;
}

static const struct of_device_id i2c_of_match[] = {
	{ .compatible = "bias" },
	{}
};

static struct i2c_driver driver = {
	.driver = {
		.name = "bias",
		.of_match_table = i2c_of_match,
	},
	.probe = probe,
};

void register_bias(void)
{
	i2c_add_driver(&driver);
}
