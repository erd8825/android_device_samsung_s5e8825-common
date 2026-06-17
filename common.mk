#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# All components inherited here go to system image
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_system.mk)

# All components inherited here go to system_ext image
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_system_ext.mk)

# All components inherited here go to product image
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_product.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/window_extensions.mk)

# All components inherited here go to vendor image
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/media_vendor.mk)
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)

# Inherit proprietary blobs
$(call inherit-product, vendor/samsung/s5e8825-common/s5e8825-common-vendor.mk)

# Branding
PRODUCT_BRAND := samsung
PRODUCT_MANUFACTURER := samsung

# Init
PRODUCT_PACKAGES += \
    fstab.s5e8825 \
    fstab.s5e8825.vendor_ramdisk

# Kernel
PRODUCT_ENABLE_UFFD_GC := true

# Kernel - Modules
PRODUCT_PACKAGES += toolbox.vendor_ramdisk

# Overlays
PRODUCT_PACKAGES += LineageSDKOverlayCommon

PRODUCT_ENFORCE_RRO_TARGETS := *

# Partitions
$(call inherit-product, $(SRC_TARGET_DIR)/product/non_ab_device.mk)

# Partitions - Dynamic
PRODUCT_USE_DYNAMIC_PARTITIONS := true

# Partitions - Updater
AB_OTA_UPDATER := false

# Permissions
PRODUCT_PACKAGES += handheld_core_hardware.prebuilt.xml

# Properties
TARGET_PRODUCT_PROP += device/samsung/s5e8825-common/configs/props/product.prop

# Recovery - Fastboot
PRODUCT_PACKAGES += fastbootd

# Recovery - Init
PRODUCT_PACKAGES += init.s5e8825.recovery.rc

# Shipping level
BOARD_SHIPPING_API_LEVEL := 31

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += \
    bootable/deprecated-ota \
    device/samsung/s5e8825-common
