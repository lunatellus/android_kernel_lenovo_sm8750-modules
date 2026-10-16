TARGET_SYNX_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE),true)
	ifeq ($(TARGET_KERNEL_DLKM_SYNX_OVERRIDE),true)
		TARGET_SYNX_ENABLE := true
	endif
else
TARGET_SYNX_ENABLE := true
endif

ifeq ($(TARGET_SYNX_ENABLE), true)
ifeq ($(TARGET_BOARD_PLATFORM), gen5)
PRODUCT_PACKAGES += synx-stub.ko
else
PRODUCT_PACKAGES += synx-driver.ko
endif
endif
