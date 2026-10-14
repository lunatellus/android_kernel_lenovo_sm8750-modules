load("//vendor/lenovo/sm8750-modules/kleaf:repo_paths.bzl", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")


def define_modules(target, variant):
    tv = "{}_{}".format(target, variant)

    ddk_module(
        name = "{}_stm_st54se_gpio".format(tv),
        out = "stm_st54se_gpio.ko",
        srcs = ["st54spi_gpio.c"],
        includes = [".", "linux"],
        deps = [soc_label("all_headers")],
        kernel_build = soc_label("{}_base_kernel".format(tv)),
        visibility = ["//visibility:public"]
    )

    pkg_files(
        name = "{}_stm_st54se_gpio_dist_files".format(tv),
        srcs = [":{}_stm_st54se_gpio".format(tv)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_stm_st54se_gpio_dist".format(tv),
        srcs = [":{}_stm_st54se_gpio_dist_files".format(tv)],
        destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )
