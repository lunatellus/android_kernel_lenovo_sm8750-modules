load(":repo_paths.bzl", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module", "kernel_module_group")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def _register_module_to_map(module_map, name, path, config_option, srcs, config_srcs, deps, config_deps):
    processed_config_srcs = {}

    for config_src_name in config_srcs:
        config_src = config_srcs[config_src_name]

        if type(config_src) == "list":
            processed_config_srcs[config_src_name] = {True: config_src}
        else:
            processed_config_srcs[config_src_name] = config_src

    module = struct(
        name = name,
        path = path,
        srcs = srcs,
        config_srcs = processed_config_srcs,
        config_option = config_option,
        deps = deps,
        config_deps = config_deps
    )
    module_map[name] = module

def _get_config_choices(map, options):
    choices = []
    for option in map:
        choices.extend(map[option].get(option in options, []))
    return choices

def _get_kernel_build_options(modules, config_options):
    all_options = {option: True for option in config_options}
    all_options = all_options | {module.config_option: True for module in modules if module.config_option}
    return all_options

def _get_kernel_build_module_srcs(module, options, formatter):
    srcs = module.srcs + _get_config_choices(module.config_srcs, options)
    module_path = "{}/".format(module.path) if module.path else ""
    globbed_srcs = native.glob(["{}{}".format(module_path, formatter(src)) for src in srcs])
    return globbed_srcs

def _get_kernel_build_module_deps(module, options, formatter):
    config_deps = []
    for key in options:
        if key in module.config_deps:
            for dep in module.config_deps[key]:
                config_deps.append(formatter(dep))
    deps = [formatter(dep) for dep in module.deps]
    return config_deps + deps

def video_module_entry(hdrs = []):
    module_map = {}

    def register(name, path = None, config_option = None, srcs = [], config_srcs = {}, deps = [], config_deps = {}):
        _register_module_to_map(module_map, name, path, config_option, srcs, config_srcs, deps, config_deps)

    return struct(
        register = register,
        get = module_map.get,
        hdrs = hdrs,
        module_map = module_map,
    )

def define_target_variant_modules(target, variant, registry, modules, config_options = []):
    kernel_build = "{}_{}".format(target, variant)

    deps = []
    all_module_deps = [
        soc_label("all_headers"),
        soc_label("{}/drivers/firmware/qcom/qcom-scm".format(kernel_build)),
        soc_label("{}/drivers/firmware/qcom/qcom_scm_smci".format(kernel_build)),
        soc_label("{}/drivers/firmware/qcom/si_core/si_core_module".format(kernel_build)),
        soc_label("{}/drivers/clk/qcom/clk-qcom".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/mdt_loader".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/llcc-qcom".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/mem_buf/mem_buf_dev".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/qcom_va_minidump".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/minidump".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/socinfo".format(kernel_build)),
        soc_label("{}/drivers/iommu/msm_dma_iommu_mapping".format(kernel_build)),
    ]
    kernel_build_label = soc_label("{}_base_kernel".format(kernel_build))

    modules = [registry.get(module_name) for module_name in modules]
    options = _get_kernel_build_options(modules, config_options)
    build_print = lambda message: print("{}: {}".format(kernel_build, message))
    formatter = lambda s: s.replace("%b", kernel_build).replace("%t", target)
    headers = registry.hdrs + [":{}_headers".format(target)]
    print(headers)
    all_module_rules = []

    for module in modules:
        rule_name = "{}_{}".format(kernel_build, module.name)
        module_srcs = _get_kernel_build_module_srcs(module, options, formatter)

        if not module_srcs:
            continue

        ddk_module(
            name = rule_name,
            srcs = module_srcs,
            out = "{}.ko".format(module.name),
            deps = headers + all_module_deps + _get_kernel_build_module_deps(module, options, formatter),
            local_defines = options.keys(),
            kernel_build = kernel_build_label,
        )
        all_module_rules.append(rule_name)

    kernel_module_group(
        name = "{}_video_modules".format(kernel_build),
        srcs = all_module_rules,
    )

    pkg_files(
        name = "{}_video_driver_modules_dist_files".format(kernel_build),
        srcs = [":{}_video_modules".format(kernel_build)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_video_driver_modules_dist".format(kernel_build),
        srcs = [":{}_video_driver_modules_dist_files".format(kernel_build)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )

def define_lunch_target_variant_modules(target, variant, registry, modules, lunch_target = None):
    print(lunch_target)

    kernel_build = "{}_{}".format(target, variant)
    if target=="mahua":
        kernel_build = "{}_{}".format("glymur", variant)

    deps = []
    all_module_deps = [
        soc_label("all_headers"),
        soc_label("{}/drivers/firmware/qcom/qcom-scm".format(kernel_build)),
        soc_label("{}/drivers/firmware/qcom/qcom_scm_smci".format(kernel_build)),
        soc_label("{}/drivers/firmware/qcom/si_core/si_core_module".format(kernel_build)),
        soc_label("{}/drivers/clk/qcom/clk-qcom".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/mdt_loader".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/llcc-qcom".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/mem_buf/mem_buf_dev".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/qcom_va_minidump".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/minidump".format(kernel_build)),
        soc_label("{}/drivers/soc/qcom/socinfo".format(kernel_build)),
        soc_label("{}/drivers/iommu/msm_dma_iommu_mapping".format(kernel_build)),
    ]
    kernel_build_label = soc_label("{}_base_kernel".format(kernel_build))

    headers = registry.hdrs + [":{}_headers".format(target)]

    if lunch_target != None:
        kernel_build = "{}_{}_{}".format(target, variant, lunch_target)
        print("kernel_build: " + kernel_build)
        headers = registry.hdrs + [":{}_headers".format(lunch_target)]
        dist_target_name = "{}_video_driver_modules_dist".format(kernel_build)
        config_options = [
            "CONFIG_MSM_MMRM",
            "CONFIG_MSM_VIDC_LLCC",
            "CONFIG_MSM_VIDC_ANDROID",
            "CONFIG_MSM_VIDC_MINIDUMP",
            "CONFIG_MSM_VIDC_RAVELIN",
            "CONFIG_MSM_VIDC_{}".format(lunch_target.upper()),
            "CONFIG_MSM_VIDC_SYNX",
        ]
    elif target == "canoe":
        lunch_target_chora = "chora"
        dist_target_name = "{}_video_driver_modules_dist".format(kernel_build)
        headers += [":{}_headers".format(lunch_target_chora)]
        print("dist_target_name: " + dist_target_name)
        config_options = [
            "CONFIG_MSM_MMRM",
            "CONFIG_MSM_VIDC_LLCC",
            "CONFIG_MSM_VIDC_ANDROID",
            "CONFIG_MSM_VIDC_MINIDUMP",
            "CONFIG_MSM_VIDC_RAVELIN",
            "CONFIG_MSM_VIDC_BOURTZI",
            "CONFIG_MSM_VIDC_{}".format(target.upper()),
            "CONFIG_MSM_VIDC_{}".format(lunch_target_chora.upper()),
        ]
        print("  config_options =", config_options)
    elif target in [ "hamoa", "hamoa_la", "shikra" ]:
        if target == "hamoa_la":
            target = "hamoa"
        dist_target_name = "{}_video_driver_modules_dist".format(kernel_build)
        print("dist_target_name: " + dist_target_name)
        config_options = [
            "CONFIG_MSM_VIDC_LLCC",
            "CONFIG_MSM_VIDC_ANDROID",
            "CONFIG_MSM_VIDC_MINIDUMP",
            "CONFIG_MSM_VIDC_{}".format(target.upper()),
            ]
        modules = [m for m in modules if m != "video"]
    elif target == "malabar":
        dist_target_name = "{}_video_driver_modules_dist".format(kernel_build)
        print("dist_target_name: " + dist_target_name)
        config_options = [
            "CONFIG_MSM_VIDC_LLCC",
            "CONFIG_MSM_VIDC_ANDROID",
            "CONFIG_MSM_VIDC_MINIDUMP",
            "CONFIG_MSM_VIDC_{}".format(target.upper()),
        ]
    elif target == "mahua":
        dist_target_name = "{}_video_driver_modules_dist".format(kernel_build)
        print("dist_target_name: " + dist_target_name)
        config_options = [
            "CONFIG_MSM_VIDC_LLCC",
            "CONFIG_MSM_VIDC_ANDROID",
            "CONFIG_MSM_VIDC_MINIDUMP",
            "CONFIG_MSM_VIDC_{}".format(target.upper()),
        ]
        modules = [m for m in modules if m != "video"]
    else:
        dist_target_name = "{}_video_driver_modules_dist".format(kernel_build)
        print("dist_target_name: " + dist_target_name)
        config_options = [
            "CONFIG_MSM_MMRM",
            "CONFIG_MSM_VIDC_LLCC",
            "CONFIG_MSM_VIDC_ANDROID",
            "CONFIG_MSM_VIDC_MINIDUMP",
            "CONFIG_MSM_VIDC_RAVELIN",
            "CONFIG_MSM_VIDC_{}".format(target.upper()),
            "CONFIG_MSM_VIDC_SYNX",
            ]

    modules = [registry.get(module_name) for module_name in modules]

    options = _get_kernel_build_options(modules, config_options)

    build_print = lambda message: print("{}: {}".format(kernel_build, message))

    formatter = lambda s: s.replace("%b", kernel_build).replace("%t", target)

    print(headers)

    all_module_rules = []

    for module in modules:
        print("Module name: " + module.name)
        rule_name = "{}_{}".format(kernel_build, module.name)
        module_srcs = _get_kernel_build_module_srcs(module, options, formatter)

        if not module_srcs:
            continue

        ddk_module(
            name = rule_name,
            srcs = module_srcs,
            out = "{}.ko".format(module.name),
            deps = headers + all_module_deps + _get_kernel_build_module_deps(module, options, formatter),
            kernel_build = kernel_build_label,
            local_defines = options.keys(),
        )
        all_module_rules.append(rule_name)

    kernel_module_group(
        name = "{}_video_modules".format(kernel_build),
        srcs = all_module_rules,
    )

    pkg_files(
        name = dist_target_name + "_files",
        srcs = [":{}_video_modules".format(kernel_build)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = dist_target_name,
        srcs = [":" + dist_target_name + "_files"],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )

def define_consolidate_gki_modules(target, registry, modules, config_options = []):
    define_target_variant_modules(target, "consolidate", registry, modules, config_options)
    define_target_variant_modules(target, "perf", registry, modules, config_options)
    define_target_variant_modules(target, "gki", registry, modules, config_options)
