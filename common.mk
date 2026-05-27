#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# All components inherited here go to system image
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_system.mk)

# All components inherited here go to system_ext image
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_system_ext.mk)

# All components inherited here go to product image
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_product.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/window_extensions.mk)

# All components inherited here go to vendor image
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/media_vendor.mk)
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)

# Inherit common Lineage stuff
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit Linaro namespacecs
$(call inherit-product, hardware/samsung_slsi-linaro/config/config.mk)

# Inherit proprietary blobs
$(call inherit-product, vendor/samsung/s5e8825-common/s5e8825-common-vendor.mk)

COMMON_PATH := device/samsung/s5e8825-common

# Audio
PRODUCT_PACKAGES += \
    android.hardware.audio@7.1-impl \
    android.hardware.audio.effect@7.0-impl \
    android.hardware.audio.service \
    android.hardware.bluetooth.audio-impl \
    audio.bluetooth.default \
    audio.primary.erd8825 \
    audio.r_submix.default \
    audio.usbv2.default

# Audio - Configuration
PRODUCT_PACKAGES += \
    aosp_audio_policy_volumes.xml \
    aosp_default_volume_tables.xml \
    aosp_r_submix_audio_policy_configuration.xml \
    audio_effects.xml \
    audio_policy_configuration.xml \
    bluetooth_audio_policy_configuration_7_0.xml \
    usbv2_audio_policy_configuration.xml

$(call soong_config_set,exynos_audio,proxy_header,//$(COMMON_PATH):audio_proxy_headers)
$(call soong_config_set,exynos_audio,sec_resampler_library,//vendor/samsung/s5e8825-common:libSamsungPostProcessConvertor)
$(call soong_config_set_bool,exynos_audio,support_direct_multi_channel_stream,true)
$(call soong_config_set_bool,exynos_audio,use_offload_effect_library,true)
$(call soong_config_set_bool,exynos_audio,use_sec_audio_dynamic_nrec,true)
$(call soong_config_set_bool,exynos_audio,use_sec_audio_samsungrecord,true)
$(call soong_config_set_bool,exynos_audio,use_sec_audio_support_gamechat_spk_aec,true)
$(call soong_config_set_bool,exynos_audio,use_sec_audio_support_listenback_dspeffect,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_audio_policy_volumes,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_default_volume_tables,true)
$(call soong_config_set_bool,frameworks_av,use_aosp_r_submix_audio_policy_configuration,true)

# Audio - Effects - Dolby
PRODUCT_PACKAGES += SamsungDAP

# Audio - Effects - SoundBooster
PRODUCT_PACKAGES += \
    libsamsungSoundbooster_plus \
    SoundBoosterStage

# Audio - FX
TARGET_EXCLUDES_AUDIOFX := true

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

# Camera - Provider
PRODUCT_PACKAGES += android.hardware.camera.provider-service.samsung

$(call soong_config_set_bool,samsungCameraVars,usage_64bit,true)

# Codec2
PRODUCT_PACKAGES += samsung.hardware.media.c2-service

# Codec2 - Codecs
PRODUCT_PACKAGES += \
    libExynosC2H264Dec \
    libExynosC2H264Enc \
    libExynosC2HevcDec \
    libExynosC2HevcEnc \
    libExynosC2Vp8Dec \
    libExynosC2Vp8Enc

# Characteristics
PRODUCT_CHARACTERISTICS := phone

# Display
TARGET_SCREEN_DENSITY := 450

# Display - Touch - Lineage
PRODUCT_PACKAGES += vendor.lineage.touch-service.samsung

# Doze
PRODUCT_PACKAGES += SamsungDoze

# DRM - Clearkey
PRODUCT_PACKAGES += com.android.hardware.drm.clearkey

# Fingerprint
PRODUCT_PACKAGES += android.hardware.biometrics.fingerprint-service.samsung

# GMS
PRODUCT_GMS_CLIENTID_BASE := android-samsung-ss

# Graphics
PRODUCT_PACKAGES += \
    android.hardware.composer.hwc3-service.slsi \
    android.hardware.graphics.allocator@4.0-service \
    android.hardware.graphics.mapper@4.0-impl

# Graphics - Linker
PRODUCT_PACKAGES += public.libraries.txt

# Graphics - Memtrack
PRODUCT_PACKAGES += android.hardware.memtrack-service.samsung-mali

# Health - Lineage
PRODUCT_PACKAGES += vendor.lineage.health-service.default

$(call soong_config_set,lineage_health,charging_control_charging_disabled,0)
$(call soong_config_set,lineage_health,charging_control_charging_enabled,1)
$(call soong_config_set,lineage_health,charging_control_charging_path,/sys/class/power_supply/battery/charging_enabled)
$(call soong_config_set,lineage_health,fast_charge_node,/sys/class/sec/switch/afc_disable)
$(call soong_config_set,lineage_health,fast_charge_value_fast_charge,0)
$(call soong_config_set,lineage_health,fast_charge_value_none,1)
$(call soong_config_set_bool,lineage_health,charging_control_charging_toggle,true)
$(call soong_config_set_bool,lineage_health,charging_control_supports_bypass,true)

# Health - Samsung
PRODUCT_PACKAGES += \
    android.hardware.health-service.samsung \
    android.hardware.health-service.samsung-recovery

# IMS
PRODUCT_PACKAGES += \
    CarrierSettings \
    ImsStack \
    Iwlan \
    QualifiedNetworksService

$(call inherit-product, packages/modules/ImsMedia/imsmedia.mk)

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

# Neural Networks
PRODUCT_PACKAGES += android.hardware.neuralnetworks@1.3-service.eden-drv

# Neural Networks - Runtime
PRODUCT_PACKAGES += vendor.samsung_slsi.hardware.eden_runtime@1.0-service

# Overlays
PRODUCT_PACKAGES += \
    ApertureOverlayCommon \
    FrameworkResOverlayCommon \
    Launcher3QuickstepOverlayCommon \
    LineageSDKOverlayCommon \
    SettingsOverlayCommon \
    SystemUIOverlayCommon \
    TelephonyOverlayCommon \
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
    android.hardware.camera.flash-autofocus.prebuilt.xml \
    android.hardware.camera.front.prebuilt.xml \
    android.hardware.camera.full.prebuilt.xml \
    android.hardware.camera.raw.prebuilt.xml \
    android.hardware.fingerprint.prebuilt.xml \
    android.hardware.location.gps.prebuilt.xml \
    android.hardware.nfc.prebuilt.xml \
    android.hardware.nfc.hce.prebuilt.xml \
    android.hardware.nfc.hcef.prebuilt.xml \
    android.hardware.sensor.accelerometer.prebuilt.xml \
    android.hardware.sensor.gyroscope.prebuilt.xml \
    android.hardware.sensor.light.prebuilt.xml \
    android.hardware.sensor.stepcounter.prebuilt.xml \
    android.hardware.sensor.stepdetector.prebuilt.xml \
    android.hardware.telephony.gsm.prebuilt.xml \
    android.hardware.telephony.ims.prebuilt.xml \
    android.hardware.usb.accessory.prebuilt.xml \
    android.hardware.usb.host.prebuilt.xml \
    android.hardware.vulkan.compute-0.prebuilt.xml \
    android.hardware.vulkan.level-1.prebuilt.xml \
    android.hardware.vulkan.version-1_3.prebuilt.xml \
    android.hardware.wifi.direct.prebuilt.xml \
    android.hardware.wifi.passpoint.prebuilt.xml \
    android.hardware.wifi.prebuilt.xml \
    android.software.ipsec_tunnels.prebuilt.xml \
    android.software.opengles.deqp.level-2022-03-01.prebuilt.xml \
    android.software.sip.voip.prebuilt.xml \
    android.software.vulkan.deqp.level-2022-03-01.prebuilt.xml \
    com.nxp.mifare.prebuilt.xml \
    handheld_core_hardware.prebuilt.xml

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.audio.pro.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.audio.pro.xml \
    frameworks/native/data/etc/android.hardware.camera.ar.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.ar.xml \
    frameworks/native/data/etc/android.hardware.nfc.uicc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.uicc.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.software.midi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.midi.xml

# Power
PRODUCT_PACKAGES += android.hardware.power-service.pixel-libperfmgr

# Power - Configuration
PRODUCT_PACKAGES += powerhint.json

# Recovery - Init
PRODUCT_PACKAGES += init.s5e8825.recovery.rc

# RIL
PRODUCT_PACKAGES += \
    cbd \
    libsec-ril \
    secril_config_svc \
    sehradiomanager

$(call soong_config_set,cbd,protocol,sipc)

# RIL - Configuration
PRODUCT_PACKAGES += sehradiomanager.conf

# Security - Gatekeeper
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-impl \
    android.hardware.gatekeeper@1.0-service

# Security - Keymint
PRODUCT_PACKAGES += android.hardware.security.keymint-service.samsung

# Shipping level
BOARD_SHIPPING_API_LEVEL := 31

# Sensors
PRODUCT_PACKAGES += android.hardware.sensors-service.samsung-multihal

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(COMMON_PATH) \
    bootable/deprecated-ota \
    hardware/google/interfaces \
    hardware/google/pixel/pixelstats \
    hardware/google/pixel/power-libperfmgr \
    hardware/google/pixel/thermal \
    hardware/samsung \
    hardware/samsung_slsi/libbt \
    hardware/samsung_slsi-linaro/exynos/cpboot_v3 \
    hardware/samsung_slsi-linaro/exynos/libaudio/audiohal_comv1 \
    hardware/samsung_slsi-linaro/exynos/libaudio/audiohal_comv1/proxy

# Task Profiles
PRODUCT_PACKAGES += task_profiles.json.s5e8825

# USB
PRODUCT_PACKAGES += android.hardware.usb-service.samsung

# USB - Gadget
PRODUCT_PACKAGES += android.hardware.usb.gadget-service.samsung

$(call soong_config_set,samsungUsbGadgetVars,gadget_name,13200000.dwc3)

# USB - Gadget - Init
PRODUCT_PACKAGES += init.s5e8825.usb.rc

# Vibrator
PRODUCT_PACKAGES += android.hardware.vibrator-service.samsung

# Wi-Fi
PRODUCT_PACKAGES += \
    android.hardware.wifi-service \
    hostapd \
    wpa_supplicant

PRODUCT_CFI_INCLUDE_PATHS += hardware/samsung_slsi/scsc_wifibt/wpa_supplicant_lib

# Wi-Fi - Configuration
PRODUCT_PACKAGES += wpa_supplicant.conf
