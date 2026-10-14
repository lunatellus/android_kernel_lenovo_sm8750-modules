// SPDX-License-Identifier: GPL-2.0
/* WL2868C camera LDO controller reconstructed from stock camera.ko. */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>

#define REGULATOR_MAX 7
#define TRY_TIMES 10

enum { WL2868C, ET5907, FAN53870, CHIP_NUM_MAX };

static int CI = -1;
struct mutex i2c_control_mutex;

#define PMIC_LOG(fmt, ...)						\
	do {								\
		if (CI == WL2868C)					\
			pr_info("[PMIC-WL2868C]:%d " fmt, __LINE__,	\
				##__VA_ARGS__);				\
		else if (CI == ET5907)					\
			pr_info("[PMIC-ET5907]:%d " fmt, __LINE__,	\
				##__VA_ARGS__);				\
		else if (CI == FAN53870)				\
			pr_info("[PMIC-FAN53870]:%d " fmt, __LINE__,	\
				##__VA_ARGS__);				\
		else							\
			pr_info("[PMIC-NOTMATCH]:%d " fmt, __LINE__,	\
				##__VA_ARGS__);				\
	} while (0)

struct wl2868c_chip_info {
	unsigned int i2c_addr;
	unsigned int chip_id;
	unsigned int chip_id_addr;
	unsigned int ldo_en_addr;
	unsigned int ldo_out_addr;
	u32 (*ldo_range)[3];
};

struct wl2868c_regulator {
	struct regulator_desc rdesc;
	struct regulator_dev *rdev;
	unsigned int channel_num;
	struct device *dev;
	struct device_node *of_node;
	const char *regulator_name;
	struct regmap *regmap;
	struct wl2868c_chip_info *chip_info;
	unsigned int chip_index;
};

/* min_uV, max_uV, step_uV for each LDO channel */
static u32 wl_range[REGULATOR_MAX][3] = {
	{ 496000, 1512000, 8000 },
	{ 496000, 1512000, 8000 },
	{ 1504000, 3544000, 8000 },
	{ 1504000, 3544000, 8000 },
	{ 1504000, 3544000, 8000 },
	{ 1504000, 3544000, 8000 },
	{ 1504000, 3544000, 8000 },
};

static u32 et_range[REGULATOR_MAX][3] = {
	{ 600000, 1800000, 6000 },
	{ 600000, 1800000, 6000 },
	{ 1200000, 3750000, 10000 },
	{ 1200000, 3750000, 10000 },
	{ 1200000, 3750000, 10000 },
	{ 1200000, 3750000, 10000 },
	{ 1200000, 3750000, 10000 },
};

static u32 fan_range[REGULATOR_MAX][3] = {
	{ 800000, 1500000, 8000 },
	{ 800000, 1500000, 8000 },
	{ 1500000, 3400000, 8000 },
	{ 1500000, 3400000, 8000 },
	{ 1500000, 3400000, 8000 },
	{ 1500000, 3400000, 8000 },
	{ 1500000, 3400000, 8000 },
};

static struct wl2868c_chip_info pmic_chip[CHIP_NUM_MAX] = {
	[WL2868C] = { 0x2f, 0x82, 0x00, 0x0e, 0x03, wl_range },
	[ET5907] = { 0x35, 0x01, 0x00, 0x03, 0x04, et_range },
	[FAN53870] = { 0x35, 0x01, 0x01, 0x03, 0x04, fan_range },
};

static bool is_read_wl2868c_reg(struct device *dev, unsigned int reg)
{
	return reg != 15;
}

static bool is_write_wl2868c_reg(struct device *dev, unsigned int reg)
{
	return reg >= 2 && reg <= 14;
}

static bool is_read_et5907_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static bool is_write_et5907_reg(struct device *dev, unsigned int reg)
{
	return reg >= 2 && reg <= 10;
}

static bool is_read_fan53870_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static bool is_write_fan53870_reg(struct device *dev, unsigned int reg)
{
	return reg >= 2 && reg <= 10;
}

static struct regmap_config chip_regmap_config[CHIP_NUM_MAX] = {
	[WL2868C] = {
		.reg_bits = 8,
		.val_bits = 8,
		.readable_reg = is_read_wl2868c_reg,
		.writeable_reg = is_write_wl2868c_reg,
		.max_register = 0x25,
		.cache_type = REGCACHE_RBTREE,
	},
	[ET5907] = {
		.reg_bits = 8,
		.val_bits = 8,
		.readable_reg = is_read_et5907_reg,
		.writeable_reg = is_write_et5907_reg,
		.max_register = 0x1e,
		.cache_type = REGCACHE_RBTREE,
	},
	[FAN53870] = {
		.reg_bits = 8,
		.val_bits = 8,
		.readable_reg = is_read_fan53870_reg,
		.writeable_reg = is_write_fan53870_reg,
		.max_register = 0x1e,
		.cache_type = REGCACHE_RBTREE,
	},
};

/* stock line alignment */














static int wl2868c_regulator_enable(struct regulator_dev *rdev)
{
	unsigned int value = 0;
	struct wl2868c_regulator *reg = rdev_get_drvdata(rdev);
	struct wl2868c_chip_info *chip;
	struct regmap *regmap;
	int ret;

	if (!reg) {
		PMIC_LOG("get wl286c NULL pointer");
		return -EINVAL;
	}
	mutex_lock(&i2c_control_mutex);
	regmap = reg->regmap;
	chip = reg->chip_info;
	ret = regmap_read(regmap, chip->ldo_en_addr, &value);
	if (ret < 0) {
		PMIC_LOG("read ldo en reg failed");
		return -EIO;
	}

	value |= 1 << reg->channel_num;
	ret = regmap_write(regmap, chip->ldo_en_addr, value);
	if (ret < 0) {
		PMIC_LOG("write ldo enable address failed");
		return -EIO;
	}
	PMIC_LOG("Enable %s", reg->regulator_name);
	mutex_unlock(&i2c_control_mutex);
	return ret;
}

static int wl2868c_regulator_disable(struct regulator_dev *rdev)
{
	unsigned int value = 0;
	struct wl2868c_regulator *reg = rdev_get_drvdata(rdev);
	struct wl2868c_chip_info *chip;
	struct regmap *regmap;
	int ret;
	if (!reg) {
		PMIC_LOG("get NULL pointer");
		return -EINVAL;
	}
	mutex_lock(&i2c_control_mutex);
	regmap = reg->regmap;
	chip = reg->chip_info;
	ret = regmap_read(regmap, chip->ldo_en_addr, &value);
	if (ret < 0) {
		PMIC_LOG("read ldo en reg failed");
		return -EIO;
	}

	value &= ~(1 << reg->channel_num);
	PMIC_LOG("disable %s", reg->regulator_name);
	ret = regmap_write(regmap, chip->ldo_en_addr, value);
	if (ret < 0) {
		PMIC_LOG("write ldo enable address failed");
		return -EIO;
	}

	mutex_unlock(&i2c_control_mutex);
	return ret;
}

static int wl2868c_regulator_is_enabled(struct regulator_dev *rdev)
{
	unsigned int value = 0;
	struct wl2868c_regulator *reg = rdev_get_drvdata(rdev);
	unsigned int ch;
	int ret;

	if (!reg) {
		PMIC_LOG("get wl286c NULL pointer");
		return -EINVAL;
	}

	ch = reg->channel_num;
	ret = regmap_read(reg->regmap, reg->chip_info->ldo_en_addr,
		&value);
	if (ret < 0) {
		PMIC_LOG("read ldo enable address failed");
		return -EIO;
	}

	return ((value & 0xff) >> ch) & 0x1;
}

static int wl2868c_regulator_get_voltage(struct regulator_dev *rdev)
{
	unsigned int value = 0;
	struct wl2868c_regulator *reg = rdev_get_drvdata(rdev);
	u32 (*range)[3];
	unsigned int ch, reg_addr;
	int voltage = 0, ret;
	if (!reg) {
		PMIC_LOG("get NULL pointer");
		return -EINVAL;
	}

	reg_addr = reg->chip_info->ldo_out_addr + reg->channel_num;
	range = reg->chip_info->ldo_range;
	ch = reg->channel_num;
	ret = regmap_read(reg->regmap, reg_addr, &value);
	if (ret < 0)
		PMIC_LOG("read voltage failed");

	switch (reg->chip_index) {
	case WL2868C:
		switch (reg->channel_num) {
		case 0 ... 1:
			voltage = (value & 0x7f) * range[ch][2] + range[ch][0];
			break;
		case 2 ... 6:
			voltage = (value & 0xff) * range[ch][2] + range[ch][0];
			break;
		default:
			return -EINVAL;
		}
		break;
	case ET5907:
		if (reg->channel_num <= 6)
			voltage = (value & 0xff) * range[ch][2] + range[ch][0];
		else
			return -EINVAL;
		break;
	case FAN53870:
		switch (reg->channel_num) {
		case 0 ... 1:
			voltage = ((value & 0xff) - 99) * range[ch][2] + range[ch][0];
			break;
		case 2 ... 6:
			voltage = ((value & 0xff) - 16) * range[ch][2] + range[ch][0];
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		break;
	}
	PMIC_LOG("%s get voltage: %d uV", reg->regulator_name, voltage);
	return voltage;
}

static int wl2868c_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uV, int max_uV,
	unsigned int *selector)
{
	struct wl2868c_regulator *reg = rdev_get_drvdata(rdev);
	unsigned int sel;
	unsigned int value = 0;
	unsigned int reg_addr;
	u32 *range;
	int voltage;
	int ret;

	if (!reg) {
		PMIC_LOG("get NULL pointer");
		return -EINVAL;
	}
	range = reg->chip_info->ldo_range[reg->channel_num];
	reg_addr = reg->chip_info->ldo_out_addr + reg->channel_num;
	sel = DIV_ROUND_UP(min_uV - range[0], range[2]);
	voltage = range[0] + sel * range[2];
	switch (reg->chip_index) {
	case WL2868C:
		switch (reg->channel_num) {
		case 0:
			value = sel + 9;
			break;
		case 1:
			value = sel + 11;
			break;
		case 2:
			value = sel + 2;
			break;
		case 3:
			value = sel + 3;
			break;
		case 4:
			value = sel + 1;
			break;
		case 5 ... 6:
			value = sel;
			break;
		default:
			return -EINVAL;
		}
		break;
	case ET5907:
		switch (reg->channel_num) {
		case 0:
			value = sel + 13;
			break;
		case 1:
			value = sel + 14;
			break;
		case 2:
			value = sel + 2;
			break;
		case 3:
			value = sel + 3;
			break;
		case 4:
			value = sel + 1;
			break;
		case 5 ... 6:
			value = sel;
			break;
		default:
			return -EINVAL;
		}
		break;
	case FAN53870:
		switch (reg->channel_num) {
		case 0 ... 1:
			value = sel + 99;
			break;
		case 2 ... 6:
			value = sel + 16;
			break;
		default:
			return -EINVAL;
		}
		break;
	}
	ret = regmap_write(reg->regmap, reg_addr, value);
	if (ret < 0) {
		PMIC_LOG("%s set voltage failed", reg->regulator_name);
		return -EIO;
	}
	PMIC_LOG("%s set voltage: %d uV", reg->regulator_name, voltage);
	return ret;
}

static struct regulator_ops wl2868c_regulator_ops = {
	.is_enabled = wl2868c_regulator_is_enabled,
	.enable = wl2868c_regulator_enable,
	.disable = wl2868c_regulator_disable,
	.get_voltage = wl2868c_regulator_get_voltage,
	.set_voltage = wl2868c_regulator_set_voltage,
};

static int wl2868c_register_ldo(struct wl2868c_regulator *reg,
	struct device *dev, struct device_node *np,
	struct regmap *regmap, struct wl2868c_chip_info *chip)
{
	struct regulator_config config = {};
	struct regulator_init_data *init_data;
	u32 *range;
	int ret;

	of_property_read_string(np, "regulator-name", &reg->regulator_name);

	ret = of_property_read_u32(np, "ldo-channel",
		&reg->channel_num);
	if (ret < 0) {
		PMIC_LOG("ldo esential infomation lost");
		return -EINVAL;
	}
	init_data = of_get_regulator_init_data(dev, np, &reg->rdesc);
	if (!init_data) {
		PMIC_LOG("get ldo init data failed");
		return -ENODATA;
	}

	range = chip->ldo_range[reg->channel_num];
	init_data->constraints.min_uV = range[0];
	init_data->constraints.max_uV = range[1];

	config.dev = dev;
	config.init_data = init_data;
	config.driver_data = reg;
	config.of_node = np;
	config.regmap = regmap;

	reg->rdesc.type = REGULATOR_VOLTAGE;
	reg->rdesc.owner = THIS_MODULE;
	reg->rdesc.name = "WL2868C";
	reg->rdesc.ops = &wl2868c_regulator_ops;
	reg->rdesc.n_voltages = (range[1] - range[0]) / range[2] + 2;

	reg->rdev = devm_regulator_register(dev, &reg->rdesc, &config);
	if (IS_ERR(reg->rdev)) {
		PMIC_LOG("%s: failed to register regulator\n", reg->rdesc.name);
		return -EINVAL;
	}
	PMIC_LOG("%s: regulator registered\n", reg->rdesc.name);
	return 0;
}

static void wl2868c_i2c_remove(struct i2c_client *client)
{
	i2c_unregister_device(client);
}

static const struct of_device_id wl2868c_i2c_of_match[] = {
	{ .compatible = "will,wl2868c" },
	{}
};

static int wl2868c_i2c_probe(struct i2c_client *client)
{
	struct wl2868c_regulator *reg;
	struct device_node *child;
	struct regmap *regmap;
	struct pinctrl *pinctrl;
	const char *pinctrl_name = NULL;
	int i, chip_id, try_time, ret;
	if (!client) {
		pr_info("pmic i2c client is NULL");
		return -EINVAL;
	}
	if (!client->dev.of_node) {
		PMIC_LOG("dev->of_node is NULL");
		return -EINVAL;
	}

	of_property_read_string(client->dev.of_node, "pinctrl-names",
		&pinctrl_name);
	if (pinctrl_name) {
		pinctrl = devm_pinctrl_get_select(&client->dev, pinctrl_name);
		if (IS_ERR(pinctrl)) {
			PMIC_LOG("Couldn't select %s pinctrl rc=%ld",
				pinctrl_name, PTR_ERR(pinctrl));
		} else {
			PMIC_LOG("pinctrl probe success");
		}
	} else {
		PMIC_LOG("no pinctrl configuration");
	}

	for (try_time = 0; try_time < TRY_TIMES; try_time++) {
		for (i = 0; i < ARRAY_SIZE(pmic_chip); i++) {
			client->addr = pmic_chip[i].i2c_addr;
			chip_id = i2c_smbus_read_byte_data(client,
				pmic_chip[i].chip_id_addr);
			PMIC_LOG("I2C result=%d", chip_id);
			if (chip_id < 0) {
				PMIC_LOG("Failed to read chip ID,  chip_addr=0x%x, addr=0x%x",
					pmic_chip[i].i2c_addr, (u8)pmic_chip[i].chip_id_addr);
				continue;
			}

			if (chip_id == pmic_chip[i].chip_id) {
				CI = i;
				PMIC_LOG("chip %d identified", i);
				break;
			}

			CI = -1;
		}

		if (CI >= 0 && CI < CHIP_NUM_MAX)
			break;

		PMIC_LOG("No LDO found, try (%d/%d)", try_time, TRY_TIMES);
		msleep(1);
	}

	if (CI == -1) {
		PMIC_LOG("LDO not found");
		return -EINVAL;
	}

	if (CI < 0 || CI >= CHIP_NUM_MAX) {
		PMIC_LOG("Invalid chip index");
		return -EINVAL;
	}

	regmap = devm_regmap_init_i2c(client, &chip_regmap_config[CI]);
	if (!regmap) {
		PMIC_LOG("chip regmap init failed");
		return -EIO;
	}

	if (CI == WL2868C) {
		/* 7ch-ldo enable voltage */
		ret = regmap_write(regmap, 0x09, 0x25);
		if (ret < 0) {
			PMIC_LOG("write 7ch-ldo enable volt failed");
			return -EIO;
		}
	}
	mutex_init(&i2c_control_mutex);
	PMIC_LOG("i2c probe success");

	/* stock line alignment */





	for_each_available_child_of_node(client->dev.of_node, child) {
		reg = devm_kzalloc(&client->dev, sizeof(*reg), GFP_KERNEL);
		if (!reg) {
			PMIC_LOG("failed to allocate memory for wl2868c_reg");
			return -ENOMEM;
		}
		reg->dev = &client->dev;
		reg->of_node = child;
		reg->regmap = regmap;
		reg->chip_info = &pmic_chip[CI];
		reg->chip_index = CI;
		ret = wl2868c_register_ldo(reg, &client->dev, child, regmap, reg->chip_info);
		if (ret < 0) {
			PMIC_LOG("register regulator failed");
			return ret;
		}
	}
	PMIC_LOG("all regulator register success");
	return 0;
}

static struct i2c_driver wl2868c_i2c_driver = {
	.probe = wl2868c_i2c_probe,
	.remove = wl2868c_i2c_remove,
	.driver = {
		.name = "WL2868C_I2C",
		.of_match_table = wl2868c_i2c_of_match,
		.owner = THIS_MODULE,
	},
};

int __init wl2868c_chip_init_module(void)
{
	int ret;

	ret = i2c_add_driver(&wl2868c_i2c_driver);

	if (ret) {
		PMIC_LOG("i2c driver register failed");
		return -EINVAL;
	}
	return ret;
}

void __exit wl2868c_chip_exit_module(void)
{
	i2c_del_driver(&wl2868c_i2c_driver);
}

MODULE_AUTHOR("Lan Degao  landegao@huaqin.com");
MODULE_DESCRIPTION("7-ldo PMIC power supply for camera and other usage");
MODULE_LICENSE("GPL v2");
