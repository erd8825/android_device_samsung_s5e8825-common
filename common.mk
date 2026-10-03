#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit proprietary blobs
$(call inherit-product, vendor/samsung/s5e8825-common/s5e8825-common-vendor.mk)

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += device/samsung/s5e8825-common
