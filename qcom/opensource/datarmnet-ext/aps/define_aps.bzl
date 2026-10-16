load(":repo_paths.bzl", "modules_label", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_aps(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    include_base = "../../../{}".format(native.package_name())

    deps_aps = [soc_label("all_headers")]

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))

    ddk_module(
        name = "{}_aps".format(kernel_build_variant),
        out = "rmnet_aps.ko",
        srcs = [
            "rmnet_aps_genl.c",
            "rmnet_aps_main.c",
            "rmnet_aps.h",
            "rmnet_aps_genl.h",
        ],
        kernel_build = kernel_build,
        deps = deps_aps + [
	    ":include_headers",
            modules_label("qcom/opensource/datarmnet:{}_rmnet_core".format(kernel_build_variant)),
            modules_label("qcom/opensource/datarmnet:rmnet_core_headers"),
        ],
        copts = ["-Wno-misleading-indentation"],
        includes = ["include"],
        visibility = [soc_label("__pkg__")],
    )

    pkg_files(
        name = "{}_datarment-ext_dist_files".format(kernel_build_variant),
        srcs = [
            ":{}_aps".format(kernel_build_variant),
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_datarment-ext_dist".format(kernel_build_variant),
        srcs = [":{}_datarment-ext_dist_files".format(kernel_build_variant)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
