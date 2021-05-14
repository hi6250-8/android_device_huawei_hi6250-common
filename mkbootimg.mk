LOCAL_PATH := $(call my-dir)

#
# Global Flags
#
TARGET_DUMMYKERNEL := $(LOCAL_PATH)/dummykernel

#
# kernel.img
#

INTERNAL_CUSTOM_BOOTIMAGE_ARGS += --kernel $(INSTALLED_KERNEL_TARGET)

$(INSTALLED_BOOTIMAGE_TARGET): $(MKBOOTIMG) $(INTERNAL_BOOTIMAGE_FILES) $(BOOTIMAGE_EXTRA_DEPS)
	$(call pretty,"Target boot image: $@")
	$(hide) $(MKBOOTIMG) $(INTERNAL_CUSTOM_BOOTIMAGE_ARGS) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) $(BOARD_MKBOOTIMG_ARGS) --output $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE),raw)
	@echo "Made boot image: $@"

#
# bootramdisk.img
#

ifdef BOARD_BOOTRAMDISKIMAGE_PARTITION_SIZE
  BOARD_KERNEL_BOOTRAMDISKIMAGE_PARTITION_SIZE := $(BOARD_BOOTRAMDISKIMAGE_PARTITION_SIZE)
endif

INSTALLED_BOOTRAMDISKIMAGE_TARGET := $(PRODUCT_OUT)/boot_ramdisk.img

INTERNAL_CUSTOM_BOOTRAMDISKIMAGE_ARGS += --kernel $(TARGET_DUMMYKERNEL) --ramdisk $(BUILT_RAMDISK_TARGET) --cmdline buildvariant=$(TARGET_BUILD_VARIANT)

BOARD_CUSTOM_BOOTRAMDISK_MKBOOTIMG_ARGS := --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --second_offset 0x00f00000 --tags_offset 0x00000100

.PHONY: bootramdiskimage
bootramdiskimage: $(INSTALLED_BOOTRAMDISKIMAGE_TARGET)

$(INSTALLED_BOOTRAMDISKIMAGE_TARGET): $(MKBOOTIMG) $(BUILT_RAMDISK_TARGET)
	$(call pretty,"Target bootramdisk image: $@")
	$(hide) $(MKBOOTIMG) $(INTERNAL_CUSTOM_BOOTRAMDISKIMAGE_ARGS) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) $(BOARD_CUSTOM_BOOTRAMDISK_MKBOOTIMG_ARGS) --output $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTRAMDISKIMAGE_PARTITION_SIZE),raw)
	@echo "Made bootramdisk image: $@"

#
# recovery_ramdisk.img
#

INTERNAL_CUSTOM_RECOVERYIMAGE_ARGS += --kernel $(recovery_kernel) --ramdisk $(recovery_ramdisk) --cmdline buildvariant=$(TARGET_BUILD_VARIANT)

BOARD_CUSTOM_RECOVERY_MKBOOTIMG_ARGS := --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --second_offset 0x00f00000 --tags_offset 0x00000100

recovery_kernel := $(TARGET_DUMMYKERNEL)

$(INSTALLED_RECOVERYIMAGE_TARGET): $(MKBOOTIMG) $(recovery_ramdisk) $(recovery_kernel)
	@echo "----- Making recovery image ------"
	$(hide) $(MKBOOTIMG) $(INTERNAL_CUSTOM_RECOVERYIMAGE_ARGS) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) $(BOARD_INTERNAL_RECOVERY_MKBOOTIMG_ARGS) --output $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE),raw)
	@echo "Made recovery image: $@"
