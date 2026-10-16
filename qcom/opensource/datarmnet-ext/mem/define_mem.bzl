load(":repo_paths.bzl", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_mem(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    include_base = "../../../{}".format(native.package_name())

    target_copts = []

    deps_mem = [soc_label("all_headers")]

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))

    ddk_module(
        name = "{}_rmnet_mem".format(kernel_build_variant),
        out = "rmnet_mem.ko",
        includes = [".", "include/uapi/linux"],
        hdrs = [ "rmnet_mem.h" ],
        srcs = [
            "rmnet_mem_main.c",
            "rmnet_mem_nl.c",
            "rmnet_mem_nl.h",
            "rmnet_mem_pool.c",
            "rmnet_mem_priv.h",
         ],
        kernel_build = kernel_build,
        deps = deps_mem + [":rmnet_mem_uapi_headers"],
        copts = ["-Wno-misleading-indentation"] + target_copts,
    )

    pkg_files(
        name = "{}_datarmnet-ext_dist_files".format(kernel_build_variant),
        srcs = [
            ":{}_rmnet_mem".format(kernel_build_variant),
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_datarmnet-ext_dist".format(kernel_build_variant),
        srcs = [":{}_datarmnet-ext_dist_files".format(kernel_build_variant)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
