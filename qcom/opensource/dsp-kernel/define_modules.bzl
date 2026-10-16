# TODO
# Add ddk module definition for frpc-trusted driver
load(":repo_paths.bzl", "soc_label")

load(
    "//build/kernel/kleaf:kernel.bzl",
    "ddk_headers",
    "ddk_module",
    "kernel_module",
    "kernel_modules_install",
    "kernel_unstripped_modules_archive",
)
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_modules(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    if target=="mahua":
        kernel_build_variant = "{}_{}".format("glymur", variant)

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))
    ddk_deps = [
        soc_label("all_headers"),
        soc_label("{}/drivers/firmware/qcom/qcom-scm".format(kernel_build_variant)),
        soc_label("{}/drivers/soc/qcom/mem_buf/mem_buf_dev".format(kernel_build_variant)),
        soc_label("{}/drivers/soc/qcom/pdr_interface".format(kernel_build_variant)),
        soc_label("{}/drivers/rpmsg/qcom_glink".format(kernel_build_variant)),
    ]

    # Path to dsp folder from soc-repo/include/trace directory
    trace_include_path = "../../../{}/dsp".format(native.package_name())

    ddk_module(
        name = "{}_frpc-adsprpc".format(kernel_build_variant),
        kernel_build = kernel_build,
        deps = ddk_deps,
        srcs = [
            "dsp/fastrpc.c",
            "dsp/fastrpc_rpmsg.c",
            "dsp/fastrpc_shared.h",
            "dsp/fastrpc_trace.h",
            "dsp/fastrpc_sysfs.c",
        ],
        local_defines = ["DSP_TRACE_INCLUDE_PATH={}".format(trace_include_path)],
        out = "frpc-adsprpc.ko",
        hdrs = [
            "include/uapi/misc/fastrpc.h",
            "include/linux/fastrpc.h",
        ],
        includes = [
            "include/linux",
            "include/uapi",
        ],
    )

    pkg_files(
        name = "{}_dsp-kernel_dist_files".format(kernel_build_variant),
        srcs = [
            ":{}_frpc-adsprpc".format(kernel_build_variant),
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_dsp-kernel_dist".format(kernel_build_variant),
        srcs = [":{}_dsp-kernel_dist_files".format(kernel_build_variant)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )

def define_vm_modules(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))

    deps = [
        soc_label("all_headers"),
        soc_label("{}/drivers/firmware/qcom/qcom-scm".format(kernel_build_variant)),
        soc_label("{}/drivers/soc/qcom/mem_buf/mem_buf_dev".format(kernel_build_variant)),
        soc_label("{}/drivers/dma-buf/heaps/qcom_dma_heaps".format(kernel_build_variant)),
        ]

    # Path to dsp folder from soc-repo/include/trace directory
    trace_include_path = "../../../{}/dsp".format(native.package_name())

    ddk_module(
        name = "{}_frpc-trusted-adsprpc".format(kernel_build_variant),
        kernel_build = kernel_build,
        deps = deps,
        srcs = [
            "dsp/fastrpc.c",
            "dsp/fastrpc_socket.c",
            "dsp/fastrpc_shared.h",
            "dsp/fastrpc_trace.h",
            "dsp/fastrpc_sysfs.c",
        ],
        local_defines = [
            "DSP_TRACE_INCLUDE_PATH={}".format(trace_include_path),
            "CONFIG_QCOM_FASTRPC_TRUSTED=1",
        ],
        out = "frpc-trusted-adsprpc.ko",
        hdrs = [
            "include/uapi/misc/fastrpc.h",
            "include/linux/fastrpc.h",
        ],
        includes = [
            "include/linux",
            "include/uapi",
        ],
    )

    kernel_unstripped_modules_archive(
        name = "{}_fastrpc_unstripped_modules_tar".format(kernel_build_variant),
        kernel_build = kernel_build,
        kernel_modules = [":{}_frpc-trusted-adsprpc".format(kernel_build_variant)],
    )

    pkg_files(
        name = "{}_dsp-kernel_dist_files".format(kernel_build_variant),
        srcs = [
            ":{}_frpc-trusted-adsprpc".format(kernel_build_variant),
            ":{}_fastrpc_unstripped_modules_tar".format(kernel_build_variant),
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_dsp-kernel_dist".format(kernel_build_variant),
        srcs = [":{}_dsp-kernel_dist_files".format(kernel_build_variant)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )

def define_target_modules():
    # Creates a ddk_headers that exposes the FastRPC UAPI header with public visibility
    ddk_headers(
        name = "frpc_uapi_headers",
        hdrs = native.glob(["include/uapi/misc/fastrpc.h"]),
        visibility = ["//visibility:public"]
    )
