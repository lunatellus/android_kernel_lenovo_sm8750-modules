load("//vendor/lenovo/sm8750-modules/kleaf:repo_paths.bzl", "modules_label", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_modules(target, variant):
    tv = "{}_{}".format(target, variant)
    copts = []
    deps = [soc_label("all_headers")]

    if target == "sun":
        copts.append("-DNFC_SECURE_PERIPHERAL_ENABLED")
        deps += [
            modules_label("qcom/opensource/securemsm-kernel:smcinvoke_kernel_headers"),
            modules_label("qcom/opensource/securemsm-kernel:{}_smcinvoke_dlkm".format(tv)),
        ]
    ddk_module(
        name = "{}_stm_nfc_i2c".format(tv),
        out = "stm_nfc_i2c.ko",
        srcs = ["nfc/st21nfc.c",
                "nfc/st21nfc.h"
               ],
        hdrs = ["include/uapi/linux/nfc/st_uapi.h"],
        includes = [".", "linux", "nfc", "include/uapi/linux/nfc"],
        copts = copts,
        deps = deps,
        kernel_build = soc_label("{}_base_kernel".format(tv)),
        visibility = ["//visibility:public"]
    )

    pkg_files(
        name = "{}_stm_nfc_i2c_dist_files".format(tv),
        srcs = [":{}_stm_nfc_i2c".format(tv)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_stm_nfc_i2c_dist".format(tv),
        srcs = [":{}_stm_nfc_i2c_dist_files".format(tv)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
