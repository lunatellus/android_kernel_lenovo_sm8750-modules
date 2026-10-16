load(":repo_paths.bzl", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_sch(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    include_base = "../../../{}".format(native.package_name())

    deps_sch = [soc_label("all_headers")]

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))

    ddk_module(
        name = "{}_sch".format(kernel_build_variant),
        out = "rmnet_sch.ko",
        srcs = [
            "rmnet_sch_main.c",
        ],
        deps = deps_sch,
        copts = ["-Wno-misleading-indentation"],
        kernel_build = kernel_build,
        visibility = [soc_label("__pkg__")],
    )

    pkg_files(
        name = "{}_datarment-ext_dist_files".format(kernel_build_variant),
        srcs = [
            ":{}_sch".format(kernel_build_variant),
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_datarment-ext_dist".format(kernel_build_variant),
        srcs = [":{}_datarment-ext_dist_files".format(kernel_build_variant)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
