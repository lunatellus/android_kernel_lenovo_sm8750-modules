MM_DRIVER_PATH := $(call my-dir)

MM_DRV_DLKM_ENABLE := true
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
	ifeq ($(TARGET_KERNEL_DLKM_MM_DRV_OVERRIDE), false)
		MM_DRV_DLKM_ENABLE := false
	endif
endif

ifeq ($(TARGET_USES_QMAA), true)
	ifeq ($(TARGET_USES_QMAA_OVERRIDE_MM_DRV), false)
		MM_DRV_DLKM_ENABLE := false
	endif
endif

ifeq ($(MM_DRV_DLKM_ENABLE), true)
	ifneq ($(filter $(TARGET_BOARD_PLATFORM), bengal malabar),$(TARGET_BOARD_PLATFORM))
		include $(MM_DRIVER_PATH)/msm_ext_display/Android.mk
	endif
	include $(MM_DRIVER_PATH)/sync_fence/Android.mk
	ifneq ($(call is-board-platform-in-list, taro vienna bengal malabar), true)
		include $(MM_DRIVER_PATH)/hw_fence/Android.mk
	endif
endif

ifeq ($(MM_DRV_DLKM_ENABLE), true)
	ifeq ($(filter $(TARGET_BOARD_PLATFORM), canoe chora vienna seraph),$(TARGET_BOARD_PLATFORM))
		include $(MM_DRIVER_PATH)/hfi_core/Android.mk
	endif
endif
