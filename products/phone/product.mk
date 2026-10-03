#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit common Lineage stuff
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Characteristics
PRODUCT_CHARACTERISTICS := phone

# Display
TARGET_SCREEN_DENSITY := 450

# Overlays
PRODUCT_PACKAGES += SettingsOverlayPhone

# Properties
TARGET_VENDOR_PROP += device/samsung/s5e8825-common/products/phone/configs/props/vendor.prop

# RIL
$(call inherit-product, device/samsung/s5e8825-common/products/ril/product.mk)
