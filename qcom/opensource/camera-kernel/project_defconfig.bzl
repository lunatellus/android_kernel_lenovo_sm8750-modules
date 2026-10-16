load("@bazel_skylib//rules:write_file.bzl", "write_file")

common_configs = [
	"CONFIG_SPECTRA_ISP=y",
	"CONFIG_SPECTRA_ICP=y",
	"CONFIG_SPECTRA_JPEG=y",
	"CONFIG_SPECTRA_SENSOR=y",
	"CONFIG_SPECTRA_USE_RPMH_DRV_API=y",
	"CONFIG_SPECTRA_LLCC_STALING=y",
	"CONFIG_SPECTRA_QULTIVATE_API=y",
	"CONFIG_SPECTRA_SECURE_SCM_API=y",
	"CONFIG_SPECTRA_GET_IOMMU_FAULT_IDS=y",
	"CONFIG_SPECTRA_DMA_MAP_ATTRS=y",
	"CONFIG_SPECTRA_DMABUF_GET_FLAGS=y",
	"CONFIG_SPECTRA_SOC_QCOM_SOCINFO=y",
]

dependency_config = [
	"CONFIG_TARGET_SYNX_ENABLE=y",
	"CONFIG_INTERCONNECT_QCOM=y",
	"CONFIG_SPECTRA_DOMAIN_ID_SECURE_CAMERA=y",
	"CONFIG_SPECTRA_SECURE_CAMERA_25=y",
	"CONFIG_MSM_MMRM=y",
]

project_configs = {
    "pineapple": dependency_config + [
        "CONFIG_SPECTRA_SECURE_CAMNOC_REG_UPDATE=y",
    ],
    "sun": dependency_config + [
        "CONFIG_SPECTRA_SECURE_CAMNOC_REG_UPDATE=y",
    ],
    "canoe": dependency_config + [
        "CONFIG_SPECTRA_POWER_DOMAIN_SET_HW_MODE=y",
    ],
    "chora": [
        "CONFIG_SPECTRA_CRE=y",
        "CONFIG_SPECTRA_SECURE_CAMERA_25=y",
        "CONFIG_INTERCONNECT_QCOM=y",
        "CONFIG_SPECTRA_USE_RPMH_DRV_API=n",
        "CONFIG_SPECTRA_LLCC_STALING=n",
        "CONFIG_RT_MAP_WORKER_KTHREAD=y",
        "CONFIG_NRT_MAP_WORKER_KTHREAD=y",
    ],
    "gen3auto": [],
    "vienna": [],
}

"""
Return a label which defines a project-specific defconfig snippet to be
applied on top of the platform defconfig.
"""

def get_project_defconfig(target, variant):
    rule_name = "{}_{}_project_defconfig".format(target, variant)
    project = target.split("-")[0]

    if project not in project_configs:
        fail("camera: no project defconfig defined for target '{}'".format(target))

    write_file(
        name = rule_name,
        out = "{}.generated".format(rule_name),
        content = common_configs + project_configs[project] + [""],
    )

    return rule_name
