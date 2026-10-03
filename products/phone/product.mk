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
