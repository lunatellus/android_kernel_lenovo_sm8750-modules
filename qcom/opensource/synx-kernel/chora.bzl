load(":synx_modules.bzl", "synx_modules")
load(":synx_module_build.bzl", "define_consolidate_perf_modules")

def define_chora():
    define_consolidate_perf_modules(
        target = "chora",
        registry = synx_modules,
        modules = [
            "synx-driver",
            "ipclite",
            "ipclite_test",
        ],
        config_options = [
            "TARGET_SYNX_ENABLE",
            "CONFIG_QTI_HW_FENCE",
            "CONFIG_EXTENSIBLE_GLCOREDATA",
        ],
    )
