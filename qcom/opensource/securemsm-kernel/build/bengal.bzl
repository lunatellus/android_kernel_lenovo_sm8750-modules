load(":securemsm_kernel.bzl", "define_consolidate_gki_modules")

def define_bengal():
    define_consolidate_gki_modules(
        target = "bengal",
        modules = [
            "smcinvoke_dlkm",
            "tz_log_dlkm",
            "qseecom_dlkm",
            "hdcp_qseecom_dlkm",
            "qce50_dlkm",
            "qcedev-mod_dlkm",
            "qrng_dlkm",
            "qcrypto-msm_dlkm",
	    "seccam_test_driver",
	    "smmu_proxy_dlkm",
            "tmecom-intf_dlkm",
            "hdcp2p2_test",
            "si_core_test",
            "tornado_mod",
         ],
         extra_options = [
             "CONFIG_QCOM_SI_CORE_TEST",
             "CONFIG_QCOM_SMCINVOKE",
             "CONFIG_QSEECOM_COMPAT",
             "CONFIG_QCOM_SI_CORE",
         ],
     )
