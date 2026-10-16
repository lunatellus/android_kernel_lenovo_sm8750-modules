load(":repo_paths.bzl", "modules_label", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_modules(target, variant):
    tv = "{}_{}".format(target, variant)
    copts = []
    deps = []
    deps = [
        soc_label("all_headers"),
        soc_label("{}/drivers/pinctrl/qcom/pinctrl-msm".format(tv)),
        soc_label("{}/kernel/trace/qcom_ipc_logging".format(tv)),
    ]
    kernel_build = soc_label("{}_base_kernel".format(tv))
    if target == "sun":
        deps += [soc_label("{}/drivers/misc/qseecom_proxy".format(tv))]
    if target == "sun":
        copts.append("-DNFC_SECURE_PERIPHERAL_ENABLED")
        deps += [
            modules_label("qcom/opensource/securemsm-kernel:smcinvoke_kernel_headers"),
            modules_label("qcom/opensource/securemsm-kernel:{}_smcinvoke_dlkm".format(tv)),
        ]

    if target == "bengal":
        copts.append("-DNFC_CLK_REQ_GPIO_WAKEUP")

    if target == "waipio":
        copts.append("-DNFC_CLK_REQ_GPIO_WAKEUP")

    if target == "parrot":
        copts.append("-DNFC_CLK_REQ_GPIO_WAKEUP")

    if target == "canoe":
        copts.append("-DCONFIG_NFC_BOB1")
        copts.append("-DNFC_SECURE_PERIPHERAL_ENABLED")
        deps += [
            modules_label("qcom/opensource/securemsm-kernel:smcinvoke_kernel_headers"),
            modules_label("qcom/opensource/securemsm-kernel:{}_smcinvoke_dlkm".format(tv)),
        ]

    if target == "chora":
        copts.append("-DCONFIG_NFC_BOB1")

    if target == "malabar":
        copts.append("-DCONFIG_NFC_BOB1")
        copts.append("-DCONFIG_NFC_NXP_I2C_DMA_SAFE")

    ddk_module(
        name = "{}_nxp-nci".format(tv),
        out = "nxp-nci.ko",
        srcs = [
            "nfc/common.c",
            "nfc/common_nxp.c",
            "nfc/common_qcom.c",
            "nfc/ese_cold_reset.c",
            "nfc/i2c_drv.c",
            "nfc/common.h",
            "nfc/common_nxp.h",
            "nfc/ese_cold_reset.h",
            "nfc/i2c_drv.h",
        ],
        hdrs = [
            "include/uapi/linux/nfc/nfcinfo.h",
            "include/uapi/linux/nfc/sn_uapi.h",
        ],
        includes = [".", "linux", "nfc", "include/uapi/linux/nfc"],
        copts = copts,
        deps = deps,
        kernel_build = kernel_build,
        visibility = ["//visibility:public"],
    )

    pkg_files(
        name = "{}_nxp-nci_dist_files".format(tv),
        srcs = [":{}_nxp-nci".format(tv)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_nxp-nci_dist".format(tv),
        srcs = [":{}_nxp-nci_dist_files".format(tv)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
