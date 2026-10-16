load(":repo_paths.bzl", "modules_label", "soc_label")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load(":target_variants.bzl", "get_all_variants")
load(":project_defconfig.bzl", "get_project_defconfig")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def _define_module(target, variant):
    tv = "{}_{}".format(target, variant)
    base_deps = []
    deps = []
    base_deps = [
        ":camera_headers",
        ":camera_banner",
        soc_label("all_headers"),
        soc_label("{}/drivers/firmware/qcom/qcom-scm".format(tv)),
        soc_label("{}/drivers/iommu/qcom_iommu_util".format(tv)),
        soc_label("{}/drivers/soc/qcom/mem_buf/mem_buf_dev".format(tv)),
        soc_label("{}/drivers/soc/qcom/crm-v2".format(tv)),
        soc_label("{}/drivers/clk/qcom/clk-qcom".format(tv)),
        soc_label("{}/drivers/soc/qcom/qcom_rpmh".format(tv)),
        soc_label("{}/drivers/soc/qcom/socinfo".format(tv)),
        soc_label("{}/drivers/soc/qcom/llcc-qcom".format(tv)),
        soc_label("{}/drivers/soc/qcom/mdt_loader".format(tv)),
        soc_label("{}/drivers/leds/flash/leds-qcom-flash".format(tv)),
        soc_label("{}/drivers/soc/qcom/qcom_va_minidump".format(tv)),
        soc_label("{}/drivers/leds/leds-qti-flash".format(tv)),
        soc_label("{}/drivers/video/backlight/qcom-spmi-wled".format(tv)),
    ]

    kernel_build = soc_label("{}_base_kernel".format(tv))

    # Generate the defconfig file dynamically
    native.genrule(
        name = "{}_defconfig_generated".format(tv),
        srcs = [
            # Use the base target/variant defconfig to start
            # and concatenate and project-specific config
            #"{}_defconfig".format(tv),
            get_project_defconfig(target, variant),
        ],
        outs = ["{}_defconfig.generated".format(tv)],
        cmd = "cat $(SRCS) > $@",
    )

    if target == "pineapple":
        deps.extend([
            modules_label("qcom/opensource/synx-kernel:synx_headers"),
            modules_label("qcom/opensource/synx-kernel:{}_modules".format(tv)),
            modules_label("qcom/opensource/securemsm-kernel:smcinvoke_kernel_headers"),
            modules_label("qcom/opensource/securemsm-kernel:smmu_proxy_headers"),
            modules_label("qcom/opensource/securemsm-kernel:{}_smcinvoke_dlkm".format(tv)),
            modules_label("qcom/opensource/securemsm-kernel:{}_smmu_proxy_dlkm".format(tv)),
            modules_label("qcom/opensource/mmrm-driver:{}_mmrm_driver".format(tv)),
        ])
    if target == "sun":
        deps.extend([
            modules_label("qcom/opensource/synx-kernel:synx_headers"),
            modules_label("qcom/opensource/synx-kernel:{}_modules".format(tv)),
            modules_label("qcom/opensource/securemsm-kernel:smcinvoke_kernel_headers"),
            modules_label("qcom/opensource/securemsm-kernel:smmu_proxy_headers"),
            modules_label("qcom/opensource/securemsm-kernel:{}_smcinvoke_dlkm".format(tv)),
            modules_label("qcom/opensource/securemsm-kernel:{}_smmu_proxy_dlkm".format(tv)),
            modules_label("qcom/opensource/mmrm-driver:{}_mmrm_driver".format(tv)),
        ])
    if target == "canoe":
        deps.extend([
           modules_label("qcom/opensource/synx-kernel:synx_headers"),
            modules_label("qcom/opensource/synx-kernel:{}_modules".format(tv)),
            modules_label("qcom/opensource/securemsm-kernel:smcinvoke_kernel_headers"),
            modules_label("qcom/opensource/securemsm-kernel:smmu_proxy_headers"),
            modules_label("qcom/opensource/securemsm-kernel:{}_smcinvoke_dlkm".format(tv)),
            modules_label("qcom/opensource/securemsm-kernel:{}_smmu_proxy_dlkm".format(tv)),
			modules_label("qcom/opensource/mmrm-driver:{}_mmrm_driver".format(tv)),
        ])
    ddk_module(
        name = "{}_camera".format(tv),
        out = "camera.ko",
        srcs = [
            "drivers/cam_req_mgr/cam_req_mgr_core.c",
            "drivers/cam_req_mgr/cam_req_mgr_dev.c",
            "drivers/cam_req_mgr/cam_req_mgr_util.c",
            "drivers/cam_req_mgr/cam_mem_mgr.c",
            "drivers/cam_req_mgr/cam_req_mgr_timer.c",
            "drivers/cam_req_mgr/cam_req_mgr_debug.c",
            "drivers/cam_utils/cam_soc_util.c",
            "drivers/cam_utils/cam_packet_util.c",
            "drivers/cam_utils/cam_debug_util.c",
            "drivers/cam_utils/cam_trace.c",
            "drivers/cam_utils/cam_common_util.c",
            "drivers/cam_utils/cam_compat.c",
            "drivers/cam_utils/cam_worker/cam_tasklet/cam_tasklet_util.c",
            "drivers/cam_utils/cam_worker/cam_workq/cam_workq_util.c",
            "drivers/cam_utils/cam_worker/cam_worker_wrapper.c",
            "drivers/cam_utils/irq_controller/cam_irq_controller.c",
            "drivers/cam_utils/cam_worker/cam_kthread/cam_kthread_util.c",
            "drivers/cam_core/cam_context.c",
            "drivers/cam_core/cam_context_utils.c",
            "drivers/cam_core/cam_node.c",
            "drivers/cam_core/cam_subdev.c",
            "drivers/cam_smmu/cam_smmu_api.c",
            "drivers/cam_sync/cam_sync.c",
            "drivers/cam_sync/cam_sync_util.c",
            "drivers/cam_sync/cam_sync_dma_fence.c",
            "drivers/cam_cpas/cpas_top/cam_cpastop_hw.c",
            "drivers/cam_cpas/camss_top/cam_camsstop_hw.c",
            "drivers/cam_cpas/cam_cpas_soc.c",
            "drivers/cam_cpas/cam_cpas_intf.c",
            "drivers/cam_cpas/cam_cpas_hw.c",
            "drivers/cam_cdm/cam_cdm_soc.c",
            "drivers/cam_cdm/cam_cdm_util.c",
            "drivers/cam_cdm/cam_cdm_intf.c",
            "drivers/cam_cdm/cam_cdm_core_common.c",
            "drivers/cam_cdm/cam_cdm_virtual_core.c",
            "drivers/cam_cdm/cam_cdm_hw_core.c",
            "drivers/cam_utils/cam_soc_icc.c",
            "drivers/camera_main.c",
        ],
        conditional_srcs = {
            "CONFIG_TARGET_SYNX_ENABLE": {
                True: ["drivers/cam_sync/cam_sync_synx.c"],
            },
            "CONFIG_QCOM_CX_IPEAK": {
                True: ["drivers/cam_utils/cam_cx_ipeak.c"],
            },
            "CONFIG_INTERCONNECT_QCOM": {
                True: ["drivers/cam_utils/cam_soc_icc.c"],
            },
            "CONFIG_SPECTRA_ISP": {
                True: [
                    "drivers/cam_isp/isp_hw_mgr/hw_utils/cam_isp_packet_parser.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_dev.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_soc.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_common.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_hw_ver1.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_hw_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_mod.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_lite_mod.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_soc.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_dev.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_core.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_top/cam_sfe_top.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_rd.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_wr.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_soc.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_dev.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_core.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_rd_ver1.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_common.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver4.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_rdi.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_fe_ver1.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe17x/cam_vfe.c",
                    "drivers/cam_isp/isp_hw_mgr/cam_isp_hw_mgr.c",
                    "drivers/cam_isp/isp_hw_mgr/cam_ife_hw_mgr.c",
                    "drivers/cam_isp/cam_isp_dev.c",
                    "drivers/cam_isp/cam_isp_context.c",
                ],
            },
            "CONFIG_SPECTRA_ICP": {
                True: [
                    "drivers/cam_icp/icp_hw/icp_hw_mgr/cam_icp_hw_mgr.c",
                    "drivers/cam_icp/icp_hw/ipe_hw/ipe_dev.c",
                    "drivers/cam_icp/icp_hw/ipe_hw/ipe_core.c",
                    "drivers/cam_icp/icp_hw/ipe_hw/ipe_soc.c",
                    "drivers/cam_icp/icp_hw/icp_proc/icp_v1_hw/cam_icp_v1_dev.c",
                    "drivers/cam_icp/icp_hw/icp_proc/icp_v1_hw/cam_icp_v1_core.c",
                    "drivers/cam_icp/icp_hw/icp_proc/icp_v2_hw/cam_icp_v2_dev.c",
                    "drivers/cam_icp/icp_hw/icp_proc/icp_v2_hw/cam_icp_v2_core.c",
                    "drivers/cam_icp/icp_hw/icp_proc/icp_common/cam_icp_proc_common.c",
                    "drivers/cam_icp/icp_hw/icp_proc/icp_common/cam_icp_soc_common.c",
                    "drivers/cam_icp/icp_hw/icp_proc/cam_icp_proc.c",
                    "drivers/cam_icp/icp_hw/bps_hw/bps_dev.c",
                    "drivers/cam_icp/icp_hw/bps_hw/bps_core.c",
                    "drivers/cam_icp/icp_hw/bps_hw/bps_soc.c",
                    "drivers/cam_icp/icp_hw/ofe_hw/ofe_dev.c",
                    "drivers/cam_icp/icp_hw/ofe_hw/ofe_core.c",
                    "drivers/cam_icp/icp_hw/ofe_hw/ofe_soc.c",
                    "drivers/cam_icp/cam_icp_subdev.c",
                    "drivers/cam_icp/cam_icp_context.c",
                    "drivers/cam_icp/hfi.c",
                ],
            },
            "CONFIG_SPECTRA_JPEG": {
                True: [
                    "drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_dev.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_core.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_soc.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_dev.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_core.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_soc.c",
                    "drivers/cam_jpeg/jpeg_hw/cam_jpeg_hw_mgr.c",
                    "drivers/cam_jpeg/cam_jpeg_dev.c",
                    "drivers/cam_jpeg/cam_jpeg_context.c",
                ],
            },
            "CONFIG_SPECTRA_SENSOR": {
                True: [
                    "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c",
                    "drivers/cam_sensor_module/cam_actuator/cam_actuator_core.c",
                    "drivers/cam_sensor_module/cam_actuator/cam_actuator_soc.c",
                    "drivers/cam_sensor_module/cam_cci/cam_cci_dev.c",
                    "drivers/cam_sensor_module/cam_cci/cam_cci_core.c",
                    "drivers/cam_sensor_module/cam_cci/cam_cci_soc.c",
                    "drivers/cam_sensor_module/cam_tpg/cam_tpg_dev.c",
                    "drivers/cam_sensor_module/cam_tpg/cam_tpg_core.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_common.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_0/tpg_hw_v_1_0.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_2/tpg_hw_v_1_2.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_3/tpg_hw_v_1_3.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_4/tpg_hw_v_1_4.c",
                    "drivers/cam_sensor_module/cam_csiphy/cam_csiphy_soc.c",
                    "drivers/cam_sensor_module/cam_csiphy/cam_csiphy_dev.c",
                    "drivers/cam_sensor_module/cam_csiphy/cam_csiphy_core.c",
                    "drivers/cam_sensor_module/cam_eeprom/cam_eeprom_dev.c",
                    "drivers/cam_sensor_module/cam_eeprom/cam_eeprom_core.c",
                    "drivers/cam_sensor_module/cam_eeprom/cam_eeprom_soc.c",
                    "drivers/cam_sensor_module/cam_ois/cam_ois_dev.c",
                    "drivers/cam_sensor_module/cam_ois/cam_ois_core.c",
                    "drivers/cam_sensor_module/cam_ois/cam_ois_soc.c",
                    "drivers/cam_sensor_module/cam_sensor/cam_sensor_dev.c",
                    "drivers/cam_sensor_module/cam_sensor/cam_sensor_core.c",
                    "drivers/cam_sensor_module/cam_sensor/cam_sensor_soc.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_io.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_cci_i2c.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_qup_i2c.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_qup_i3c.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_spi.c",
                    "drivers/cam_sensor_module/cam_sensor_utils/cam_sensor_util.c",
                    "drivers/cam_sensor_module/cam_res_mgr/cam_res_mgr.c",
                    "drivers/cam_sensor_module/cam_flash/cam_flash_dev.c",
                    "drivers/cam_sensor_module/cam_flash/cam_flash_core.c",
                    "drivers/cam_sensor_module/cam_flash/cam_flash_soc.c",
                    "drivers/cam_sensor_module/cam_sensor_module_debug.c",
                ],
            },
            "CONFIG_SPECTRA_CUSTOM": {
                True: [
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_soc.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_dev.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_core.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_csid/cam_custom_csid_dev.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw_mgr.c",
                    "drivers/cam_cust/cam_custom_dev.c",
                    "drivers/cam_cust/cam_custom_context.c",
                ],
            },
            "CONFIG_QCOM_BUS_SCALING": {
                True: ["drivers/cam_utils/cam_soc_bus.c"],
            },
            "CONFIG_CAM_PRESIL": {
                # Sources need to be available to specify
                # True: [
                #     "drivers/cam_presil/presil/cam_presil_io_util.c",
                #     "drivers/cam_presil/presil/cam_presil_hw_access.c",
                #     "drivers/cam_presil/presil_framework_dev/cam_presil_framework_dev.c",
                # ],
                False: [
                    "drivers/cam_presil/stub/cam_presil_hw_access_stub.c",
                    "drivers/cam_utils/cam_io_util.c",
                ],
            },
            "CONFIG_SPECTRA_VMRM": {
                True: [
                    "drivers/cam_vmrm/qrtr/cam_qrtr_comms.c",
                    "drivers/cam_vmrm/cam_vmrm_interface.c",
                ],
            },
            "CONFIG_SPECTRA_CRE": {
                True: [
                    "drivers/cam_cre/cam_cre_hw_mgr/cre_hw/cre_core.c",
                    "drivers/cam_cre/cam_cre_hw_mgr/cre_hw/cre_soc.c",
                    "drivers/cam_cre/cam_cre_hw_mgr/cre_hw/cre_dev.c",
                    "drivers/cam_cre/cam_cre_hw_mgr/cre_hw/top/cre_top.c",
                    "drivers/cam_cre/cam_cre_hw_mgr/cre_hw/bus_rd/cre_bus_rd.c",
                    "drivers/cam_cre/cam_cre_hw_mgr/cre_hw/bus_wr/cre_bus_wr.c",
                    "drivers/cam_cre/cam_cre_hw_mgr/cam_cre_hw_mgr.c",
                    "drivers/cam_cre/cam_cre_dev.c",
                    "drivers/cam_cre/cam_cre_context.c",
                ],
           },
        },
        copts = ["-include", "$(location :camera_banner)"],
        deps = base_deps + deps,
        kconfig = "Kconfig",
        defconfig = "{}_defconfig_generated".format(tv),
        kernel_build = kernel_build,
    )

    pkg_files(
       name = tv + "_dist_files",
       srcs = [":{}_camera".format(tv)],
       strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
       name = "{}_camera_dist".format(tv),
       srcs = [":{}_dist_files".format(tv)],
       destdir = "out/target/product/{}/dlkm/lib/modules/".format(target),
    )

def define_camera_module():
    for (t, v) in get_all_variants():
        _define_module(t, v)
