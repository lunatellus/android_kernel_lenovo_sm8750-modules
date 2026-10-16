ifeq ($(TARGET_DATARMNET_ENABLE), true)
ifneq ($(TARGET_BOARD_PLATFORM),qssi)
RMNET_CORE_DLKM_PLATFORMS_LIST := sun
RMNET_CORE_DLKM_PLATFORMS_LIST += canoe
RMNET_CORE_DLKM_PLATFORMS_LIST += vienna
RMNET_CORE_DLKM_PLATFORMS_LIST += monaco
RMNET_CORE_DLKM_PLATFORMS_LIST += lahaina
RMNET_CORE_DLKM_PLATFORMS_LIST += bengal
RMNET_CORE_DLKM_PLATFORMS_LIST += chora
RMNET_CORE_DLKM_PLATFORMS_LIST += malabar
RMNET_CORE_DLKM_PLATFORMS_LIST += shikra
RMNET_CORE_DLKM_PLATFORMS_LIST += taro

RMNET_CTL_DLKM_PLATFORMS_LIST := sun
RMNET_CTL_DLKM_PLATFORMS_LIST += canoe
RMNET_CTL_DLKM_PLATFORMS_LIST += vienna
RMNET_CTL_DLKM_PLATFORMS_LIST += monaco
RMNET_CTL_DLKM_PLATFORMS_LIST += lahaina
RMNET_CTL_DLKM_PLATFORMS_LIST += bengal
RMNET_CTL_DLKM_PLATFORMS_LIST += chora
RMNET_CTL_DLKM_PLATFORMS_LIST += malabar
RMNET_CTL_DLKM_PLATFORMS_LIST += taro

ifeq ($(call is-board-platform-in-list, $(RMNET_CORE_DLKM_PLATFORMS_LIST)),true)
#Make file to create RMNET_CORE DLKM
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#Enabling BAZEL
LOCAL_MODULE_DDK_BUILD := true

LOCAL_CFLAGS := -Wno-macro-redefined -Wno-unused-function -Wall -Werror
LOCAL_CLANG :=true
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
LOCAL_MODULE := rmnet_core.ko
LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)

ifneq ($(call is-board-platform-in-list, shikra),true)
KBUILD_REQUIRED_KOS := ipam.ko
endif 

DLKM_DIR := $(TOP)/device/qcom/common/dlkm
$(warning $(DLKM_DIR))
include $(DLKM_DIR)/Build_external_kernelmodule.mk

ifeq ($(call is-board-platform-in-list, $(RMNET_CTL_DLKM_PLATFORMS_LIST)),true)
######## Create RMNET_CTL DLKM ########
include $(CLEAR_VARS)

LOCAL_CFLAGS := -Wno-macro-redefined -Wno-unused-function -Wall -Werror
LOCAL_CLANG :=true
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
LOCAL_MODULE := rmnet_ctl.ko
LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
KBUILD_REQUIRED_KOS := ipam.ko
DLKM_DIR := $(TOP)/device/qcom/common/dlkm
$(warning $(DLKM_DIR))
include $(DLKM_DIR)/Build_external_kernelmodule.mk
endif

endif #End of Check for target
endif #End of Check for qssi target
endif #End of Check for datarmnet
