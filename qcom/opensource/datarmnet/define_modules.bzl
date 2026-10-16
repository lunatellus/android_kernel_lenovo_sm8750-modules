load(":repo_paths.bzl", "modules_label", "soc_label")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")

def define_rmnet_ctl_module(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    deps_ctl = [
        soc_label("all_headers"),
        soc_label("{}/drivers/soc/qcom/qmi_helpers".format(kernel_build_variant)),
        soc_label("{}/kernel/trace/qcom_ipc_logging".format(kernel_build_variant)),
    ]

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))

    ddk_module(
        name = "{}_rmnet_ctl".format(kernel_build_variant),
        out = "rmnet_ctl.ko",
        srcs = [
            "core/rmnet_ctl_ipa.c",
            "core/rmnet_ctl.h",
            "core/rmnet_ctl_client.h",
        ],
        kconfig = "core/Kconfig",
        conditional_srcs = {
            "CONFIG_ARCH_SUN": {
                True: [
                    "core/rmnet_ctl_client.c",
                ],
            },
            "CONFIG_ARCH_CANOE": {
                True: [
                    "core/rmnet_ctl_client.c",
                ],
            },
            "CONFIG_ARCH_VIENNA": {
                True: [
                    "core/rmnet_ctl_client.c",
                ],
            },
            "CONFIG_ARCH_MONACO": {
                True: [
                    "core/rmnet_ctl_client.c",
                ],
            },
            "CONFIG_ARCH_YUPIK": {
                True: [
                    "core/rmnet_ctl_client.c",
                ],
            },
            "CONFIG_ARCH_BENGAL": {
                 True: [
                     "core/rmnet_ctl_client.c",
                 ],
            },
            "CONFIG_ARCH_CHORA": {
                 True: [
                     "core/rmnet_ctl_client.c",
                 ],
            },
            "CONFIG_ARCH_MALABAR": {
                 True: [
                     "core/rmnet_ctl_client.c",
                 ],
            },
            "CONFIG_ARCH_WAIPIO": {
                 True: [
		    "core/rmnet_ctl_client.c",
		 ],
             },
        },
        kernel_build = kernel_build,
        deps = deps_ctl + [
            modules_label("qcom/opensource/dataipa:{}_ipam".format(kernel_build_variant)),
            modules_label("qcom/opensource/dataipa:include_headers"),
        ],
    )

def define_rmnet_core_module(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    include_base = "../../../{}".format(native.package_name())

    #The below will take care of the defconfig
    #include_defconfig = ":{}_defconfig".format(variant)
    deps_core = [
        soc_label("all_headers"),
        soc_label("{}/drivers/soc/qcom/qmi_helpers".format(kernel_build_variant)),
    ]

    kernel_build = soc_label("{}_base_kernel".format(kernel_build_variant))

    rmnet_core_deps = deps_core + [
        ":rmnet_core_headers",
        modules_label("qcom/opensource/datarmnet-ext/mem:{}_rmnet_mem".format(kernel_build_variant)),
        modules_label("qcom/opensource/datarmnet-ext/mem:rmnet_mem_uapi_headers"),
    ]
    if target != "shikra":
        rmnet_core_deps += [
            ":{}_rmnet_ctl".format(kernel_build_variant),
            modules_label("qcom/opensource/dataipa:{}_ipam".format(kernel_build_variant)),
            modules_label("qcom/opensource/dataipa:include_headers"),
        ]

    rmnet_core_srcs = [
        "core/rmnet_config.c",
        "core/rmnet_descriptor.c",
        "core/rmnet_genl.c",
        "core/rmnet_handlers.c",
        "core/rmnet_map_command.c",
        "core/rmnet_map_data.c",
        "core/rmnet_module.c",
        "core/rmnet_vnd.c",
        "core/dfc_qmap.c",
        "core/dfc_qmi.c",
        "core/qmi_rmnet.c",
        "core/wda_qmi.c",
        "core/rmnet_qmap.c",
        "core/rmnet_ll.c",
        "core/rmnet_ll_ipa.c",
        "core/rmnet_ll_qmap.c",
    ]

    rmnet_local_defines = ["RMNET_TRACE_INCLUDE_PATH={}/core".format(include_base)]
    if target == "shikra":
        rmnet_local_defines += ["TRANSPORT_RMNET_BAM"]

    ddk_module(
        name = "{}_rmnet_core".format(kernel_build_variant),
        out = "rmnet_core.ko",
        srcs = rmnet_core_srcs,
        local_defines = rmnet_local_defines,
        kernel_build = kernel_build,
        deps = rmnet_core_deps,
    )

    dist_files_srcs = [
        "{}_rmnet_core".format(kernel_build_variant),
    ]
    if target != "shikra":
        dist_files_srcs += [
            "{}_rmnet_ctl".format(kernel_build_variant),
        ]

    pkg_files(
        name = kernel_build_variant + "_dist_files",
        srcs = dist_files_srcs,
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_modules_dist".format(kernel_build_variant),
        srcs = [":{}_dist_files".format(kernel_build_variant)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
