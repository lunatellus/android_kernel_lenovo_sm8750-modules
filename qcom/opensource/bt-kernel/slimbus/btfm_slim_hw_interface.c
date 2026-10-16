// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/slimbus.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "btfm_slim.h"
#include "btfm_slim_hw_interface.h"
#include "btfm_codec_hw_interface.h"

static int bt_soc_enable_status;
int btfm_feedback_ch_setting;
static uint8_t usecase_codec;

/* Runtime flag: true = use ASoC codec registration,
 * false = use HWEP registration (default)
 */
static bool register_with_alsa_in_cp_arch;

static int btfm_slim_hwep_write(struct snd_soc_component *codec,
			unsigned int reg, unsigned int value)
{
	BTFMSLIM_DBG("");
	return 0;
}

static unsigned int btfm_slim_hwep_read(struct snd_soc_component *codec,
				unsigned int reg)
{
	BTFMSLIM_DBG("");
	return 0;
}

static int btfm_soc_status_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSLIM_DBG("");
	ucontrol->value.integer.value[0] = bt_soc_enable_status;
	return 1;
}

static int btfm_soc_status_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSLIM_DBG("");
	return 1;
}

static int btfm_get_feedback_ch_setting(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSLIM_DBG("");
	ucontrol->value.integer.value[0] = btfm_feedback_ch_setting;
	return 1;
}

static int btfm_put_feedback_ch_setting(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSLIM_DBG("");
	btfm_feedback_ch_setting = ucontrol->value.integer.value[0];
	return 1;
}

static int btfm_get_codec_type(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	BTFMSLIM_DBG("current codec type:%s", codec_text[usecase_codec]);
	ucontrol->value.integer.value[0] = usecase_codec;
	return 1;
}

static int btfm_put_codec_type(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	usecase_codec = ucontrol->value.integer.value[0];
	BTFMSLIM_DBG("codec type set to:%s", codec_text[usecase_codec]);
	return 1;
}
/* Mixer controls for the HWEP registration path —
 * includes all 3 controls: SOC status, feedback channel and codec type.
 */
static struct snd_kcontrol_new status_controls[] = {
	SOC_SINGLE_EXT("BT SOC status", 0, 0, 1, 0,
	btfm_soc_status_get, btfm_soc_status_put),
	SOC_SINGLE_EXT("BT set feedback channel", 0, 0, 1, 0,
	btfm_get_feedback_ch_setting,
	btfm_put_feedback_ch_setting),
	SOC_ENUM_EXT("BT codec type", codec_display,
	btfm_get_codec_type, btfm_put_codec_type),
};

/* Mixer controls for the ASoC codec registration path —
 * mirrors btfm_slim_codec.c exactly: only SOC status and
 * feedback channel controls, no codec type control.
 */
static const struct snd_kcontrol_new asoc_codec_controls[] = {
	SOC_SINGLE_EXT("BT SOC status", 0, 0, 1, 0,
		btfm_soc_status_get, btfm_soc_status_put),
	SOC_SINGLE_EXT("BT set feedback channel", 0, 0, 1, 0,
		btfm_get_feedback_ch_setting, btfm_put_feedback_ch_setting),
};


static int btfm_slim_hwep_probe(struct snd_soc_component *codec)
{
	BTFMSLIM_DBG("");
	return 0;
}

static void btfm_slim_hwep_remove(struct snd_soc_component *codec)
{
	BTFMSLIM_DBG("");
}

static int btfm_slim_dai_startup(void *dai)
{
	struct hwep_data *hwep_info = (struct hwep_data *)dai;
	struct btfmslim *btfmslim = dev_get_drvdata(hwep_info->dev);
	int ret = -1;

	BTFMSLIM_DBG("");
	ret = btfm_slim_hw_init(btfmslim);
	return ret;
}

static void btfm_slim_dai_shutdown(void *dai, int id)
{
	struct hwep_data *hwep_info = (struct hwep_data *)dai;
	struct btfmslim *btfmslim = dev_get_drvdata(hwep_info->dev);
	struct btfmslim_ch *ch;
	int i;
	uint8_t rxport, nchan = 1;

	BTFMSLIM_DBG("");
	switch (id) {
	case BTFM_FM_SLIM_TX:
		nchan = 2;
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_SLIM_TX:
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		ch = btfmslim->rx_chs;
		rxport = 1;
		break;
	case BTFM_SLIM_NUM_CODEC_DAIS:
	default:
		BTFMSLIM_ERR("id is invalid:%d", id);
		return;
	}
	/* Search for dai->id matched port handler */
	for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != id); ch++, i++)
		;

	if ((ch->port == BTFM_SLIM_PGD_PORT_LAST) ||
		(ch->id == BTFM_SLIM_NUM_CODEC_DAIS)) {
		BTFMSLIM_ERR("ch is invalid!!");
		return;
	}

	btfm_slim_disable_ch(btfmslim, ch, rxport, nchan);
	btfm_slim_hw_deinit(btfmslim);
}

static int btfm_slim_dai_hw_params(void *dai, uint32_t bps,
				   uint32_t direction,
				   uint8_t num_channels) {
	struct hwep_data *hwep_info = (struct hwep_data *)dai;
	struct btfmslim *btfmslim = dev_get_drvdata(hwep_info->dev);

	BTFMSLIM_DBG("");
	btfmslim->bps = bps;
	btfmslim->direction = direction;

	return 0;
}

void btfm_get_sampling_rate(uint32_t *sampling_rate)
{
	uint8_t codec_types_avb = ARRAY_SIZE(codec_text);
	if (usecase_codec > (codec_types_avb - 1)) {
		BTFMSLIM_ERR("falling back to use default sampling_rate: %u",
				*sampling_rate);
		return;
	}

	if (*sampling_rate == 44100 || *sampling_rate == 48000) {
		if (usecase_codec == LDAC ||
		    usecase_codec == APTX_AD ||
		    usecase_codec == LHDC)
			*sampling_rate = (*sampling_rate) *2;
	}

	if (usecase_codec == LC3_VOICE ||
	    usecase_codec == APTX_AD_SPEECH ||
	    usecase_codec == LC3 || usecase_codec == APTX_AD_R4 ||
	    usecase_codec == RVP) {
		*sampling_rate = 96000;
	}

	if (usecase_codec == APTX_AD_QLEA || usecase_codec == APTX_PLUS)
		*sampling_rate = 192000;

	BTFMSLIM_INFO("current usecase codec type %s and sampling rate:%u khz",
			codec_text[usecase_codec], *sampling_rate);
}
static int btfm_slim_dai_prepare(void *dai, uint32_t sampling_rate, uint32_t direction, int id)
{
	struct hwep_data *hwep_info = (struct hwep_data *)dai;
	struct btfmslim *btfmslim = dev_get_drvdata(hwep_info->dev);
	struct btfmslim_ch *ch;
	int ret = -EINVAL;
	int i = 0;
	uint8_t rxport, nchan = 1;

	btfmslim->direction = direction;
	bt_soc_enable_status = 0;

	btfm_get_sampling_rate(&sampling_rate);
	/* save sample rate */
	btfmslim->sample_rate = sampling_rate;

	switch (id) {
	case BTFM_FM_SLIM_TX:
		nchan = 2;
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_SLIM_TX:
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		ch = btfmslim->rx_chs;
		rxport = 1;
		break;
	case BTFM_SLIM_NUM_CODEC_DAIS:
	default:
		BTFMSLIM_ERR("id is invalid:%d", id);
		return ret;
	}

	/* Search for dai->id matched port handler */
	for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != id); ch++, i++)
		;

	if ((ch->port == BTFM_SLIM_PGD_PORT_LAST) ||
		(ch->id == BTFM_SLIM_NUM_CODEC_DAIS)) {
		BTFMSLIM_ERR("ch is invalid!!");
		return ret;
	}

	ret = btfm_slim_enable_ch(btfmslim, ch, rxport, sampling_rate, nchan);

	/* save the enable channel status */
	if (ret == 0)
		bt_soc_enable_status = 1;

	if (ret == -EISCONN) {
		BTFMSLIM_ERR("channel opened without closing, returning success");
		ret = 0;
	}
	return ret;
}

/* This function will be called once during boot up */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
static int btfm_slim_dai_set_channel_map(void *dai,
				unsigned int tx_num, const unsigned int *tx_slot,
				unsigned int rx_num, const unsigned int *rx_slot)
#else
static int btfm_slim_dai_set_channel_map(void *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
#endif
{

	struct hwep_data *hwep_info = (struct hwep_data *)dai;
	struct btfmslim *btfmslim = dev_get_drvdata(hwep_info->dev);
	struct btfmslim_ch *rx_chs;
	struct btfmslim_ch *tx_chs;
	int ret = 0, i;

	BTFMSLIM_DBG("");

	if (!btfmslim)
		return -EINVAL;

	rx_chs = btfmslim->rx_chs;
	tx_chs = btfmslim->tx_chs;

	if (!rx_chs || !tx_chs)
		return ret;

	BTFMSLIM_DBG("Rx: id\tname\tport\tch");
	for (i = 0; (rx_chs->port != BTFM_SLIM_PGD_PORT_LAST) && (i < rx_num);
		i++, rx_chs++) {
		/* Set Rx Channel number from machine driver and
		 * get channel handler from slimbus driver
		*/
		rx_chs->ch = *(uint8_t *)(rx_slot + i);
		BTFMSLIM_DBG("    %d\t%s\t%d\t%x", rx_chs->id,
			rx_chs->name, rx_chs->port, rx_chs->ch);
	}

	BTFMSLIM_DBG("Tx: id\tname\tport\tch");
	for (i = 0; (tx_chs->port != BTFM_SLIM_PGD_PORT_LAST) && (i < tx_num);
		i++, tx_chs++) {
		/* Set Tx Channel number from machine driver and
		 * get channel handler from slimbus driver
		*/
		tx_chs->ch = *(uint8_t *)(tx_slot + i);
	BTFMSLIM_DBG("    %d\t%s\t%d\t%x", tx_chs->id,
			tx_chs->name, tx_chs->port, tx_chs->ch);
	}

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
static int btfm_slim_dai_get_channel_map(const void *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot, int id)
#else
static int btfm_slim_dai_get_channel_map(void *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot, int id)
#endif
{

	struct hwep_data *hwep_info = (struct hwep_data *)dai;
	struct btfmslim *btfmslim = dev_get_drvdata(hwep_info->dev);
	int i, ret = -EINVAL, *slot = NULL, j = 0, num = 1;
	struct btfmslim_ch *ch = NULL;

	BTFMSLIM_DBG("");
	if (!btfmslim)
		return ret;

	switch (id) {
	case BTFM_FM_SLIM_TX:
		num = 2;
		fallthrough;
	case BTFM_BT_SCO_SLIM_TX:
		if (!tx_slot || !tx_num) {
			BTFMSLIM_ERR("Invalid tx_slot %p or tx_num %p",
				tx_slot, tx_num);
			return -EINVAL;
		}
		ch = btfmslim->tx_chs;
		if (!ch)
			return -EINVAL;
		slot = tx_slot;
		*rx_slot = 0;
		*tx_num = num;
		*rx_num = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		if (!rx_slot || !rx_num) {
			BTFMSLIM_ERR("Invalid rx_slot %p or rx_num %p",
				 rx_slot, rx_num);
			return -EINVAL;
		}
		ch = btfmslim->rx_chs;
		if (!ch)
			return -EINVAL;
		slot = rx_slot;
		*tx_slot = 0;
		*tx_num = 0;
		*rx_num = num;
		break;
	default:
		BTFMSLIM_ERR("Unsupported DAI %d", id);
		return -EINVAL;
	}

	do {
		if (!ch)
			return -EINVAL;
		for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) && (ch->id !=
			BTFM_SLIM_NUM_CODEC_DAIS) && (ch->id != id);
			ch++, i++)
			;

		if (ch->id == BTFM_SLIM_NUM_CODEC_DAIS ||
			i == BTFM_SLIM_NUM_CODEC_DAIS) {
			BTFMSLIM_ERR(
				"No channel has been allocated for dai (%d)",
				id);
			return -EINVAL;
		}
		if (!slot)
			return -EINVAL;
		*(slot + j) = ch->ch;
		BTFMSLIM_DBG("id:%d, port:%d, ch:%d, slot: %d", ch->id,
			ch->port, ch->ch, *(slot + j));

		/* In case it has mulitiple channels */
		if (++j < num)
			ch++;
	} while (j < num);

	return 0;
}

int btfm_slim_dai_get_configs(void *dai, void *config, uint8_t id)
{
	struct hwep_data *hwep_info = (struct hwep_data *)dai;
	struct btfmslim *btfmslim = dev_get_drvdata(hwep_info->dev);
	struct master_hwep_configurations *hwep_config;
	struct btfmslim_ch *ch = NULL;
	int i = 0;

	BTFMSLIM_DBG("");

	hwep_config = (struct master_hwep_configurations *) config;
	hwep_config->stream_id = id;
	hwep_config->device_id = btfmslim->device_id;
	hwep_config->sample_rate = btfmslim->sample_rate;
	hwep_config->bit_width = (uint8_t)btfmslim->bps;
	hwep_config->codectype = usecase_codec;
	hwep_config->direction = btfmslim->direction;

	switch (id) {
		case BTFM_FM_SLIM_TX:
		case BTFM_BT_SCO_SLIM_TX:
			ch = btfmslim->tx_chs;
			break;
		case BTFM_BT_SCO_A2DP_SLIM_RX:
		case BTFM_BT_SPLIT_A2DP_SLIM_RX:
			ch = btfmslim->rx_chs;
			break;
	}

	for (; i < id ; i++) {
		if (ch[i].id == id) {
			BTFMSLIM_DBG("id matched");
			hwep_config->num_channels = 1;
			hwep_config->chan_num = ch[i].ch;
			break;
		}
	}

	return 1;
}

static struct hwep_dai_ops  btfmslim_hw_dai_ops = {
	.hwep_startup = btfm_slim_dai_startup,
	.hwep_shutdown = btfm_slim_dai_shutdown,
	.hwep_hw_params = btfm_slim_dai_hw_params,
	.hwep_prepare = btfm_slim_dai_prepare,
	.hwep_set_channel_map = btfm_slim_dai_set_channel_map,
	.hwep_get_channel_map = btfm_slim_dai_get_channel_map,
	.hwep_get_configs = btfm_slim_dai_get_configs,
	.hwep_codectype = &usecase_codec,
};

static struct hwep_dai_driver btfmslim_dai_driver[] = {
	{	/* FM audio: fm -> lpass */
		.dai_name = "btaudio_fm_tx",
		.id = FMAUDIO_TX,
		.capture = {
			.stream_name = "FM TX Capture",
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.dai_ops = &btfmslim_hw_dai_ops,
	},
	{	/* Bluetooth SCO voice uplink: bt -> lpass */
		.dai_name = "btaudio_tx",
		.id = BTAUDIO_TX,
		.capture = {
			.stream_name = "BT Audio Slim Tx Capture",
			/* 8 KHz or 16 KHz */
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000
				| SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.dai_ops = &btfmslim_hw_dai_ops,
	},
	{	/* Bluetooth SCO voice downlink: lpass -> bt or A2DP Playback */
		.dai_name = "btaudio_rx",
		.id = BTAUDIO_RX,
		.playback = {
			.stream_name = "BT Audio Slim Rx Playback",
			/* 8/16/44.1/48/88.2/96 Khz */
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000
				| SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.dai_ops = &btfmslim_hw_dai_ops,
	},
};

static struct hwep_comp_drv btfmslim_hw_driver = {
	.hwep_probe	= btfm_slim_hwep_probe,
	.hwep_remove	= btfm_slim_hwep_remove,
	.hwep_read	= btfm_slim_hwep_read,
	.hwep_write	= btfm_slim_hwep_write,
};

bool btfm_slim_is_reg_with_alsa_in_cp_enabled_in_dt(struct slim_device *sdev,
					struct btfmslim *btfm_slim)
{
	struct device_node *np = sdev->dev.of_node;

	/* Check DT property: if qcom,btfm-register-with-alsa-in-cp-arch is
	 * present and true, set register_with_alsa_in_cp_arch = true and use
	 * ASoC codec registration path (btfm_slim_register_codec_via_hwep).
	 * If absent or false, set register_with_alsa_in_cp_arch = false and
	 * use HWEP registration path (btfm_slim_register_hw_ep).
	 */
	if (of_property_read_bool(np, "qcom,btfm-register-with-alsa-in-cp-arch")) {
		register_with_alsa_in_cp_arch = true;
		BTFMSLIM_INFO("codec registration disabled, using ASoC codec path");
	} else {
		register_with_alsa_in_cp_arch = false;
		BTFMSLIM_INFO("codec registration not disabled, using HWEP path");
	}
	btfm_slim->register_with_alsa_in_cp_arch = register_with_alsa_in_cp_arch;

	return register_with_alsa_in_cp_arch;
}

/* ASoC codec component driver and DAI drivers mirroring btfm_slim_codec.c,
 * used when CONFIG_SLIM_BTFM_CODEC is enabled but codec registration
 * mode is selected at runtime via DT property.
 */
static int btfm_slim_codec_write_hwep(struct snd_soc_component *codec,
			unsigned int reg, unsigned int value)
{
	BTFMSLIM_DBG("");
	return 0;
}

static unsigned int btfm_slim_codec_read_hwep(struct snd_soc_component *codec,
				unsigned int reg)
{
	BTFMSLIM_DBG("");
	return 0;
}

static int btfm_slim_codec_probe_hwep(struct snd_soc_component *codec)
{
	BTFMSLIM_DBG("");
	/* Use asoc_codec_controls (2 controls) to exactly mirror
	 * btfm_slim_codec.c — excludes the HWEP-specific "BT codec type"
	 * control which belongs only to the HWEP registration path.
	 */
	snd_soc_add_component_controls(codec, asoc_codec_controls,
				   ARRAY_SIZE(asoc_codec_controls));
	return 0;
}

static void btfm_slim_codec_remove_hwep(struct snd_soc_component *codec)
{
	BTFMSLIM_DBG("");
}

static int btfm_slim_asoc_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = -1;
	struct btfmslim *btfmslim =
			snd_soc_component_get_drvdata(dai->component);

	BTFMSLIM_DBG("substream = %s stream = %d dai->name = %s",
		 substream->name, substream->stream, dai->name);
	ret = btfm_slim_hw_init(btfmslim);
	return ret;
}

static void btfm_slim_asoc_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int i;
	struct btfmslim *btfmslim =
			snd_soc_component_get_drvdata(dai->component);
	struct btfmslim_ch *ch;
	uint8_t rxport, nchan = 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	BTFMSLIM_DBG("dai->name: %s, dai->id: %d, dai->rate: %d", dai->name,
		dai->id, dai->symmetric_rate);
#else
	BTFMSLIM_DBG("dai->name: %s, dai->id: %d, dai->rate: %d", dai->name,
		dai->id, dai->rate);
#endif

	switch (dai->id) {
	case BTFM_FM_SLIM_TX:
		nchan = 2;
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_SLIM_TX:
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		ch = btfmslim->rx_chs;
		rxport = 1;
		break;
	case BTFM_SLIM_NUM_CODEC_DAIS:
	default:
		BTFMSLIM_ERR("dai->id is invalid:%d", dai->id);
		return;
	}

	for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != dai->id); ch++, i++)
		;

	if ((ch->port == BTFM_SLIM_PGD_PORT_LAST) ||
		(ch->id == BTFM_SLIM_NUM_CODEC_DAIS)) {
		BTFMSLIM_ERR("ch is invalid!!");
		return;
	}

	btfm_slim_disable_ch(btfmslim, ch, rxport, nchan);
	btfm_slim_hw_deinit(btfmslim);
}

static int btfm_slim_asoc_dai_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct btfmslim *btfmslim =
			snd_soc_component_get_drvdata(dai->component);

	btfmslim->bps = params_width(params);
	btfmslim->direction = substream->stream;
	BTFMSLIM_DBG("dai->name = %s DAI-ID %x rate %d bps %d num_ch %d",
		dai->name, dai->id, params_rate(params),
		params_width(params), params_channels(params));
	return 0;
}

static int btfm_slim_asoc_dai_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int ret = -EINVAL;
	int i = 0;
	struct btfmslim_ch *ch;
	uint8_t rxport, nchan = 1;
	struct btfmslim *btfmslim =
			snd_soc_component_get_drvdata(dai->component);

	btfmslim->direction = substream->stream;
	bt_soc_enable_status = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	BTFMSLIM_INFO("dai->name: %s, dai->id: %d, dai->rate: %d direction: %d",
		dai->name, dai->id, dai->symmetric_rate, btfmslim->direction);
	btfmslim->sample_rate = dai->symmetric_rate;
#else
	BTFMSLIM_INFO("dai->name: %s, dai->id: %d, dai->rate: %d direction: %d",
		dai->name, dai->id, dai->rate, btfmslim->direction);
	btfmslim->sample_rate = dai->rate;
#endif

	switch (dai->id) {
	case BTFM_FM_SLIM_TX:
		nchan = 2;
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_SLIM_TX:
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		ch = btfmslim->rx_chs;
		rxport = 1;
		break;
	case BTFM_SLIM_NUM_CODEC_DAIS:
	default:
		BTFMSLIM_ERR("dai->id is invalid:%d", dai->id);
		return ret;
	}

	for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != dai->id); ch++, i++)
		;

	if ((ch->port == BTFM_SLIM_PGD_PORT_LAST) ||
		(ch->id == BTFM_SLIM_NUM_CODEC_DAIS)) {
		BTFMSLIM_ERR("ch is invalid!!");
		return ret;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	ret = btfm_slim_enable_ch(btfmslim, ch, rxport,
				  dai->symmetric_rate, nchan);
#else
	ret = btfm_slim_enable_ch(btfmslim, ch, rxport, dai->rate, nchan);
#endif

	if (ret == 0)
		bt_soc_enable_status = 1;

	if (ret == -EISCONN) {
		BTFMSLIM_ERR("channel opened without closing, returning success");
		ret = 0;
	}
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
static int btfm_slim_asoc_dai_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, const unsigned int *tx_slot,
				unsigned int rx_num, const unsigned int *rx_slot)
#else
static int btfm_slim_asoc_dai_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
#endif
{
	int ret = 0, i;
	struct btfmslim *btfmslim =
			snd_soc_component_get_drvdata(dai->component);
	struct btfmslim_ch *rx_chs;
	struct btfmslim_ch *tx_chs;

	BTFMSLIM_DBG("");

	if (!btfmslim)
		return -EINVAL;

	rx_chs = btfmslim->rx_chs;
	tx_chs = btfmslim->tx_chs;

	if (!rx_chs || !tx_chs)
		return ret;

	for (i = 0; (rx_chs->port != BTFM_SLIM_PGD_PORT_LAST) && (i < rx_num);
		i++, rx_chs++) {
		rx_chs->ch = *(uint8_t *)(rx_slot + i);
		BTFMSLIM_DBG("Rx id:%d name:%s port:%d ch:%d",
			rx_chs->id, rx_chs->name, rx_chs->port, rx_chs->ch);
	}

	for (i = 0; (tx_chs->port != BTFM_SLIM_PGD_PORT_LAST) && (i < tx_num);
		i++, tx_chs++) {
		tx_chs->ch = *(uint8_t *)(tx_slot + i);
		BTFMSLIM_DBG("Tx id:%d name:%s port:%d ch:%d",
			tx_chs->id, tx_chs->name, tx_chs->port, tx_chs->ch);
	}

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
static int btfm_slim_asoc_dai_get_channel_map(const struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
#else
static int btfm_slim_asoc_dai_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
#endif
{
	int i, ret = -EINVAL, *slot = NULL, j = 0, num = 1;
	struct btfmslim *btfmslim =
			snd_soc_component_get_drvdata(dai->component);
	struct btfmslim_ch *ch = NULL;

	if (!btfmslim)
		return ret;

	switch (dai->id) {
	case BTFM_FM_SLIM_TX:
		num = 2;
		fallthrough;
	case BTFM_BT_SCO_SLIM_TX:
		if (!tx_slot || !tx_num) {
			BTFMSLIM_ERR("Invalid tx_slot %p or tx_num %p",
				tx_slot, tx_num);
			return -EINVAL;
		}
		ch = btfmslim->tx_chs;
		if (!ch)
			return -EINVAL;
		slot = tx_slot;
		*rx_slot = 0;
		*tx_num = num;
		*rx_num = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		if (!rx_slot || !rx_num) {
			BTFMSLIM_ERR("Invalid rx_slot %p or rx_num %p",
				 rx_slot, rx_num);
			return -EINVAL;
		}
		ch = btfmslim->rx_chs;
		if (!ch)
			return -EINVAL;
		slot = rx_slot;
		*tx_slot = 0;
		*tx_num = 0;
		*rx_num = num;
		break;
	default:
		BTFMSLIM_ERR("Unsupported DAI %d", dai->id);
		return -EINVAL;
	}

	do {
		if (!ch)
			return -EINVAL;
		for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) &&
			(ch->id != BTFM_SLIM_NUM_CODEC_DAIS) &&
			(ch->id != dai->id); ch++, i++)
			;

		if (ch->id == BTFM_SLIM_NUM_CODEC_DAIS ||
			i == BTFM_SLIM_NUM_CODEC_DAIS) {
			BTFMSLIM_ERR(
				"No channel allocated for dai (%d)",
				dai->id);
			return -EINVAL;
		}
		if (!slot)
			return -EINVAL;
		*(slot + j) = ch->ch;
		BTFMSLIM_DBG("id:%d, port:%d, ch:%d, slot: %d",
			ch->id, ch->port, ch->ch, *(slot + j));

		if (++j < num)
			ch++;
	} while (j < num);

	return 0;
}

static struct snd_soc_dai_ops btfmslim_asoc_dai_ops = {
	.startup        = btfm_slim_asoc_dai_startup,
	.shutdown       = btfm_slim_asoc_dai_shutdown,
	.hw_params      = btfm_slim_asoc_dai_hw_params,
	.prepare        = btfm_slim_asoc_dai_prepare,
	.set_channel_map = btfm_slim_asoc_dai_set_channel_map,
	.get_channel_map = btfm_slim_asoc_dai_get_channel_map,
};

static struct snd_soc_dai_driver btfmslim_asoc_dai[] = {
	{	/* FM Audio data multiple channel: FM -> qdsp */
		.name = "btfm_fm_slim_tx",
		.id = BTFM_FM_SLIM_TX,
		.capture = {
			.stream_name = "FM TX Capture",
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &btfmslim_asoc_dai_ops,
	},
	{	/* Bluetooth SCO voice uplink: bt -> lpass */
		.name = "btfm_bt_sco_slim_tx",
		.id = BTFM_BT_SCO_SLIM_TX,
		.capture = {
			.stream_name = "SCO TX Capture",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000
				| SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmslim_asoc_dai_ops,
	},
	{	/* Bluetooth SCO voice downlink: lpass -> bt or A2DP Playback */
		.name = "btfm_bt_sco_a2dp_slim_rx",
		.id = BTFM_BT_SCO_A2DP_SLIM_RX,
		.playback = {
			.stream_name = "SCO A2DP RX Playback",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000
				| SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmslim_asoc_dai_ops,
	},
	{	/* Bluetooth Split A2DP data: qdsp -> bt */
		.name = "btfm_bt_split_a2dp_slim_rx",
		.id = BTFM_BT_SPLIT_A2DP_SLIM_RX,
		.playback = {
			.stream_name = "SPLIT A2DP Playback",
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmslim_asoc_dai_ops,
	},
};

static const struct snd_soc_component_driver btfmslim_asoc_codec = {
	.probe  = btfm_slim_codec_probe_hwep,
	.remove = btfm_slim_codec_remove_hwep,
	.read   = btfm_slim_codec_read_hwep,
	.write  = btfm_slim_codec_write_hwep,
};

int btfm_slim_register_codec_via_hwep(struct btfmslim *btfm_slim)
{
	int ret = 0;
	struct device *dev = btfm_slim->dev;

	BTFMSLIM_INFO("Registering ASoC codec component via HWEP path");
	ret = snd_soc_register_component(dev, &btfmslim_asoc_codec,
		btfmslim_asoc_dai, ARRAY_SIZE(btfmslim_asoc_dai));
	if (ret)
		BTFMSLIM_ERR("failed to register ASoC codec (%d)", ret);
	return ret;
}

void btfm_slim_unregister_codec_via_hwep(struct device *dev)
{
	BTFMSLIM_INFO("Unregistering ASoC codec component via HWEP path");
	snd_soc_unregister_component(dev);
}

int btfm_slim_register_hw_ep(struct btfmslim *btfm_slim)
{
	struct device *dev = btfm_slim->dev;
	struct hwep_data *hwep_info;
	int ret = 0;

	BTFMSLIM_INFO("Registering with BTFMCODEC HWEP interface\n");
	hwep_info = kzalloc(sizeof(struct hwep_data), GFP_KERNEL);

	if (!hwep_info) {
		BTFMSLIM_ERR("%s: failed to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto end;
	}

	/* Copy EP device parameters as intercations will be on the same device */
	hwep_info->dev = dev;
	strscpy(hwep_info->driver_name, BTFMSLIM_DEV_NAME, DEVICE_NAME_MAX_LEN);
	hwep_info->drv = &btfmslim_hw_driver;
	hwep_info->dai_drv = btfmslim_dai_driver;
	hwep_info->num_dai = ARRAY_SIZE(btfmslim_dai_driver);
	hwep_info->num_mixer_ctrl = ARRAY_SIZE(status_controls);
	hwep_info->mixer_ctrl = status_controls;
	/* Register to hardware endpoint */
	ret = btfmcodec_register_hw_ep(hwep_info);
	if (ret) {
		BTFMSLIM_ERR("failed to register with btfmcodec driver hw interface (%d)", ret);
		goto end;
	}

	BTFMSLIM_INFO("Registered succesfull with BTFMCODEC HWEP interface\n");
	return ret;
end:
	return ret;
}

void btfm_slim_unregister_hwep(void)
{
	BTFMSLIM_INFO("Unregistered with BTFMCODEC HWEP	interface");
	/* Unregister with BTFMCODEC HWEP	driver */
	btfmcodec_unregister_hw_ep(BTFMSLIM_DEV_NAME);

}

MODULE_DESCRIPTION("BTFM Slimbus driver");
MODULE_LICENSE("GPL v2");
