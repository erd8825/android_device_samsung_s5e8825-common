#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# All components inherited here go to system image
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)

# Inherit proprietary blobs
$(call inherit-product, vendor/samsung/s5e8825-common/s5e8825-common-vendor.mk)

COMMON_PATH := device/samsung/s5e8825-common

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += $(COMMON_PATH)
