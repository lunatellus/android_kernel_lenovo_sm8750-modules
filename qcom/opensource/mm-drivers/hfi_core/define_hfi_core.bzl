load(":repo_paths.bzl", "modules_label", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//vendor/qcom/sm8850-modules/qcom/opensource/mm-drivers:target_variants.bzl", "get_all_variants")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def _define_module(target, variant):
    tv = "{}_{}".format(target, variant)

    deps = [
        soc_label("all_headers"),
        soc_label("{}/drivers/firmware/qcom/qcom-scm".format(tv)),
        soc_label("{}/drivers/soc/qcom/mdt_loader".format(tv)),
        soc_label("{}/drivers/soc/qcom/smem".format(tv)),
    ]

    kernel_build = soc_label("{}_base_kernel".format(tv))

    if target in ["seraph"]:
        target_config = "seraph_defconfig"
    else:
        target_config = "defconfig"

    ddk_module(
        name = "{}_msm_hfi_core".format(tv),
        srcs = [
            "src/hfi_transport/hfi_queue.c",
            "src/hfi_transport/hfi_ipc.c",
            "src/hfi_transport/hfi_smmu.c",
            "src/hfi_transport/hfi_swi.c",
            "src/hfi_transport/hfi_queue_controller.c",
            "src/hfi_transport/hfi_if_abstraction.c",
            "src/hfi_base/hfi_core.c",
            "src/hfi_base/hfi_core_irq.c",
            "src/hfi_base/hfi_core_firmware.c",
            "src/hfi_base/hfi_core_ssr.c",
            "src/hfi_dbg_packet.c",
            "src/hfi_core_debug.c",
            "src/hfi_core_probe.c",
        ],
        out = "msm_hfi_core.ko",
        defconfig = target_config,
        kconfig = "Kconfig",
        deps = deps + [
            modules_label("qcom/opensource/mm-drivers:mm_drivers_headers"),
        ],
        kernel_build = kernel_build,
    )

    pkg_files(
        name = tv + "_dist_files",
        srcs = [":{}_msm_hfi_core".format(tv)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_msm_hfi_core_dist".format(tv),
        srcs = [":{}_dist_files".format(tv)],
        destdir = "out/target/product/{}/dlkm/lib/modules".format(target),
    )

def define_hfi_core():
    for (t, v) in get_all_variants():
        if t == "malabar":
            continue
        _define_module(t, v)
