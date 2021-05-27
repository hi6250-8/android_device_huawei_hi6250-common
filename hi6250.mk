#
# Copyright (C) 2021 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

## Common Path
COMMON_PATH := device/huawei/hi6250-common

## Inherit common vendor blobs
$(call inherit-product, vendor/huawei/hi6250-common/hi6250-common-vendor.mk)

# Display
TARGET_SCREEN_DENSITY := 480
TARGET_SCREEN_HEIGHT := 1920
TARGET_SCREEN_WIDTH := 1080

# Graphics
PRODUCT_PACKAGES += \
￼    libion

# HIDL
PRODUCT_PACKAGES += \
    android.hidl.base@1.0 \
    android.hidl.base@1.0.vendor

# Media
PRODUCT_COPY_FILES += \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_video.xml

# Init
PRODUCT_PACKAGES += \
    fstab.hi6250 \
    fstab.hi6250_ramdisk \
    init.audio.rc \
    init.balong_modem.rc \
    init.charger.rc \
    init.chip.charger.rc \
    init.connectivity.bcm43455.rc \
    init.connectivity.gps.rc \
    init.connectivity.hi1102.rc \
    init.connectivity.rc \
    init.device.rc \
    init.extmodem.rc \
    init.hi6250.rc \
    init.hisi.rc \
    init.ko.rc \
    init.manufacture.rc \
    init.performance.rc \
    init.platform.rc \
    init.post-fs-data.rc \
    init.protocol.rc \
    init.recovery.hi6250.rc \
    init.tee.rc \
    ueventd.hi6250.rc

# Recovery
PRODUCT_PACKAGES += \
    init.recovery.hi6250.rc

# Treble
PRODUCT_USE_VNDK_OVERRIDE := true
