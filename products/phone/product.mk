#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit products from the common tree
$(call inherit-product, device/samsung/s5e8825-common/products/ril/product.mk)

# Inherit common Lineage stuff
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Characteristics
PRODUCT_CHARACTERISTICS := phone

# Display
TARGET_SCREEN_DENSITY := 450

# NFC - Init
PRODUCT_PACKAGES += init.nfc.rc

# Permissions
PRODUCT_PACKAGES += \
    android.hardware.nfc.prebuilt.xml \
    android.hardware.nfc.hce.prebuilt.xml \
    android.hardware.nfc.hcef.prebuilt.xml \
    com.nxp.mifare.prebuilt.xml

PRODUCT_COPY_FILES += frameworks/native/data/etc/android.hardware.nfc.uicc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.uicc.xml

# Properties
TARGET_VENDOR_PROP += device/samsung/s5e8825-common/products/phone/configs/props/vendor.prop
