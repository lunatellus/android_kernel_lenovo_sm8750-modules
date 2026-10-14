/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SDE_DRM_COMPAT_H_
#define _SDE_DRM_COMPAT_H_

#include <linux/align.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <drm/drm_property.h>
#include <drm/msm_drm_aiqe.h>
#include <drm/msm_drm_pp.h>
#include <drm/sde_drm.h>

/*
 * Stock callers (vendor/lib64/libsdedrm.so DRMPPManager for SPR dither
 * SDE_DSPP_SPR_DITHER_V1 / demura SDE_DEMURA_INIT_CFG_V1 blobs, libsdm-color.so
 * PPFeaturesConfig converters for the same payloads, and the color-service /
 * composer path for MDNIE range + dest-scaler pointer properties) were built
 * against the 6.6 UAPI and never send the appended fields. Keep the exported,
 * native UAPI intact: only known old wire lengths are translated, and old
 * trailing padding must never become the first newly appended field.
 */
#define SDE_DITHER_LEGACY_PREFIX offsetof(struct drm_msm_dither, dither_matrix_select)
#define SDE_DITHER_LEGACY_SIZE ALIGN(SDE_DITHER_LEGACY_PREFIX, sizeof(__u64))
#define SDE_DEMURA_LEGACY_PREFIX offsetof(struct drm_msm_dem_cfg, cfg0_param_7)
#define SDE_DEMURA_LEGACY_SIZE ALIGN(SDE_DEMURA_LEGACY_PREFIX, sizeof(__u64))

/* Consumes the old reference only after a successful conversion. */
static inline int sde_drm_compat_blob(struct drm_device *dev,
		struct drm_property_blob **blob, size_t native_size,
		size_t legacy_size, size_t legacy_prefix)
{
	struct drm_property_blob *native;

	if ((*blob)->length == native_size)
		return 0;
	if ((*blob)->length != legacy_size)
		return -EINVAL;

	/* drm_property_create_blob(..., NULL) zeroes the new extension. */
	native = drm_property_create_blob(dev, native_size, NULL);
	if (IS_ERR(native))
		return PTR_ERR(native);
	memcpy(native->data, (*blob)->data, legacy_prefix);
	drm_property_blob_put(*blob);
	*blob = native;
	return 0;
}

/*
 * MDNIE is a lengthless pointer property: stock libsdm extension blobs only
 * carry the 480-byte base (flags + param[118]). Copy exactly that prefix and
 * zero the appended v2 fields. A v2-capable caller sets
 * AIQE_MDNIE_PARAM_A_EXT_FLAG/AIQE_MDNIE_PARAM_E_FLAG explicitly; read the
 * marked extension fields only, never probing unflagged trailing memory.
 */
static inline unsigned long sde_drm_copy_mdnie(struct drm_msm_mdnie *cfg,
		const void __user *usr, u32 major_version)
{
	const u8 __user *data = usr;

	memset(cfg, 0, sizeof(*cfg));
	if (copy_from_user(cfg, usr, offsetof(struct drm_msm_mdnie, param_a_ext)))
		return 1;
	if (major_version < 2)
		return 0;
	if ((cfg->flags & AIQE_MDNIE_PARAM_A_EXT_FLAG) &&
			copy_from_user(cfg->param_a_ext,
				data + offsetof(struct drm_msm_mdnie, param_a_ext),
				sizeof(cfg->param_a_ext)))
		return 1;
	if ((cfg->flags & AIQE_MDNIE_PARAM_E_FLAG) &&
			copy_from_user(cfg->param_e,
				data + offsetof(struct drm_msm_mdnie, param_e),
				sizeof(cfg->param_e)))
		return 1;
	return 0;
}

/*
 * SCALER_V2 is a pointer property, not a length-bearing blob. Stock
 * libsdedrm.so (DRMPlane::SetScalerConfig via "dest_scaler" /
 * "dest_scaler_count", DRMCrtc::ConfigureScalerLUTs) sends the pre-QSEED7
 * layout. QSEED7 (0x3005) appended the tail fields; elden DT reports
 * qcom,sde-qseed-scalar-version 0x3004, so this hardware consumes only the
 * Sun prefix. Do not probe beyond that userspace allocation, even if
 * adjacent memory maps.
 */
static inline unsigned long sde_drm_copy_scaler_v2(
		struct sde_drm_scaler_v2 *cfg, const void __user *usr, u32 version)
{
	size_t len = version >= 0x3005 ? sizeof(*cfg) :
		offsetof(struct sde_drm_scaler_v2, edge_bleed_sup_en);

	memset(cfg, 0, sizeof(*cfg));
	return copy_from_user(cfg, usr, len);
}

#endif /* _SDE_DRM_COMPAT_H_ */
