load(":repo_paths.bzl", "modules_label", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//vendor/qcom/sm8850-modules/qcom/opensource/mm-drivers:target_variants.bzl", "get_all_variants")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def _define_module(target, variant):
    tv = "{}_{}".format(target, variant)

    deps = [soc_label("all_headers")]
    kernel_build = soc_label("{}_base_kernel".format(tv))

    ddk_module(
        name = "{}_msm_ext_display".format(tv),
        srcs = ["src/msm_ext_display.c"],
        out = "msm_ext_display.ko",
        defconfig = "defconfig",
        kconfig = "Kconfig",
        deps = deps + [
            modules_label("qcom/opensource/mm-drivers:mm_drivers_headers"),
        ],
        kernel_build = kernel_build,
    )

    pkg_files(
        name = tv + "_dist_files",
        srcs = [":{}_msm_ext_display".format(tv)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_msm_ext_display_dist".format(tv),
        srcs = [":{}_dist_files".format(tv)],
        destdir = "out/target/product/{}/dlkm/lib/modules".format(target),
    )

def define_msm_ext_display():
    for (t, v) in get_all_variants():
        if t == "malabar":
            continue
        _define_module(t, v)
