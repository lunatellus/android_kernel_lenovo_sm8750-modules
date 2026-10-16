load(":repo_paths.bzl", "modules_label", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_shs(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    include_base = "../../../{}".format(native.package_name())

    deps_shs = [
		soc_label("all_headers"),
		soc_label("{}/kernel/sched/walt/sched-walt".format(kernel_build_variant)),
	]

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))


    ddk_module(
        name = "{}_shs".format(kernel_build_variant),
        out = "rmnet_shs.ko",
        srcs = [
            "rmnet_shs.h",
            "rmnet_shs_common.c",
            "rmnet_shs_common.h",
            "rmnet_shs_config.c",
            "rmnet_shs_config.h",
            "rmnet_shs_freq.c",
            "rmnet_shs_freq.h",
            "rmnet_shs_ll.c",
            "rmnet_shs_ll.h",
            "rmnet_shs_main.c",
            "rmnet_shs_modules.c",
            "rmnet_shs_modules.h",
            "rmnet_shs_wq.c",
            "rmnet_shs_wq.h",
            "rmnet_shs_wq_genl.c",
            "rmnet_shs_wq_genl.h",
            "rmnet_shs_wq_mem.c",
            "rmnet_shs_wq_mem.h",
        ],
	kernel_build = kernel_build,
	deps = deps_shs + [
		":include_headers",
		modules_label("qcom/opensource/datarmnet:{}_rmnet_core".format(kernel_build_variant)),
		modules_label("qcom/opensource/datarmnet:rmnet_core_headers"),
        ],
        copts = ["-Wno-misleading-indentation"],
        visibility = [soc_label("__pkg__")],
    )

    pkg_files(
        name = "{}_datarment-ext_dist_files".format(kernel_build_variant),
        srcs = [
            ":{}_shs".format(kernel_build_variant),
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_datarment-ext_dist".format(kernel_build_variant),
        srcs = [":{}_datarment-ext_dist_files".format(kernel_build_variant)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
