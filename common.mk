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

# Inherit common Lineage stuff
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit proprietary blobs
$(call inherit-product, vendor/samsung/s5e8825-common/s5e8825-common-vendor.mk)

COMMON_PATH := device/samsung/s5e8825-common

# Bluetooth
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-impl \
    android.hardware.bluetooth@1.0-service \
    libbt-vendor

# Bluetooth - Configuration
PRODUCT_COPY_FILES += \
    hardware/samsung_slsi/libbt/conf/bt_did.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/bt_did.conf \
    hardware/samsung_slsi/libbt/conf/bt_vendor.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/bt_vendor.conf

# Branding
PRODUCT_BRAND := samsung
PRODUCT_MANUFACTURER := samsung

# Characteristics
PRODUCT_CHARACTERISTICS := phone

# Display
TARGET_SCREEN_DENSITY := 450

# Fastboot
PRODUCT_PACKAGES += fastbootd

# GMS
PRODUCT_GMS_CLIENTID_BASE := android-samsung-ss

# Health - Samsung
PRODUCT_PACKAGES += \
    android.hardware.health-service.samsung \
    android.hardware.health-service.samsung-recovery

# Init
PRODUCT_PACKAGES += \
    fstab.s5e8825.vendor \
    fstab.s5e8825.vendor_ramdisk \
    init.s5e8825.rc \
    ueventd.s5e8825.rc

# Kernel
PRODUCT_ENABLE_UFFD_GC := true
PRODUCT_SET_DEBUGFS_RESTRICTIONS := true

# Kernel - Modules
PRODUCT_PACKAGES += toolbox.vendor_ramdisk

# Overlays
PRODUCT_PACKAGES += \
    FrameworkResOverlayCommon \
    WiFiOverlayCommon

PRODUCT_ENFORCE_RRO_TARGETS := *

# Partitions
$(call inherit-product, $(SRC_TARGET_DIR)/product/non_ab_device.mk)

# Partitions - Dynamic
PRODUCT_USE_DYNAMIC_PARTITIONS := true

# Partitions - Updater
AB_OTA_UPDATER := false

# Permissions
PRODUCT_PACKAGES += \
    android.hardware.bluetooth.prebuilt.xml \
    android.hardware.usb.accessory.prebuilt.xml \
    android.hardware.usb.host.prebuilt.xml \
    android.hardware.wifi.direct.prebuilt.xml \
    android.hardware.wifi.passpoint.prebuilt.xml \
    android.hardware.wifi.prebuilt.xml \
    android.software.ipsec_tunnels.prebuilt.xml \
    handheld_core_hardware.prebuilt.xml

# Recovery - Init
PRODUCT_PACKAGES += init.s5e8825.recovery.rc

# Security - Gatekeeper
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-impl \
    android.hardware.gatekeeper@1.0-service

# Shipping level
BOARD_SHIPPING_API_LEVEL := 31

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(COMMON_PATH) \
    bootable/deprecated-ota \
    hardware/samsung \
    hardware/samsung_slsi/libbt

# USB
PRODUCT_PACKAGES += android.hardware.usb-service.samsung

# USB - Gadget
PRODUCT_PACKAGES += android.hardware.usb.gadget-service.samsung

$(call soong_config_set,samsungUsbGadgetVars,gadget_name,13200000.dwc3)

# USB - Gadget - Init
PRODUCT_PACKAGES += init.s5e8825.usb.rc

# Wi-Fi
PRODUCT_PACKAGES += \
    android.hardware.wifi-service \
    hostapd \
    wpa_supplicant

PRODUCT_CFI_INCLUDE_PATHS += hardware/samsung_slsi/scsc_wifibt/wpa_supplicant_lib

# Wi-Fi - Configuration
PRODUCT_PACKAGES += wpa_supplicant.conf
