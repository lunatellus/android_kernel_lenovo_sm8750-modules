#SPDX-License-Identifier: GPL-2.0-only

MM_DRV_DLKM_ENABLE := true
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
	ifeq ($(TARGET_KERNEL_DLKM_MM_DRV_OVERRIDE), false)
		MM_DRV_DLKM_ENABLE := false
	endif
endif

ifeq ($TARGET_USES_QMAA, true)
	ifeq ($(TARGET_USES_QMAA_OVERRIDE_MM_DRV), false)
		MM_DRV_DKLM_ENABLE := false
	endif
endif

ifeq ($(MM_DRV_DLKM_ENABLE), true)
	ifneq (,$(call is-board-platform-in-list2,$(TARGET_BOARD_PLATFORM)))
		# msm_ext_display.ko: enabled for all supported platforms EXCEPT bengal
		ifneq ($(filter $(TARGET_BOARD_PLATFORM), malabar bengal),$(TARGET_BOARD_PLATFORM))
			BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/msm_ext_display.ko
			BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/msm_ext_display.ko
			BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD += $(KERNEL_MODULES_OUT)/msm_ext_display.ko
		endif

		ifneq ($(TARGET_BOARD_PLATFORM), taro)
			BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/sync_fence.ko
			BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/sync_fence.ko
			BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD += $(KERNEL_MODULES_OUT)/sync_fence.ko
		endif

		ifneq ($(filter $(TARGET_BOARD_PLATFORM), vienna bengal malabar),$(TARGET_BOARD_PLATFORM))
			BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/msm_hw_fence.ko
			BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/msm_hw_fence.ko
			BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD += $(KERNEL_MODULES_OUT)/msm_hw_fence.ko
		endif

		ifeq ($(filter $(TARGET_BOARD_PLATFORM), canoe chora vienna seraph),$(TARGET_BOARD_PLATFORM))
			BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/msm_hfi_core.ko
			BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/msm_hfi_core.ko
			BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD += $(KERNEL_MODULES_OUT)/msm_hfi_core.ko
		endif
	endif
endif
