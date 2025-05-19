#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Overlays
PRODUCT_PACKAGES += FrameworkResOverlayRIL

# Permissions
PRODUCT_PACKAGES += android.hardware.telephony.gsm.prebuilt.xml

# RIL
PRODUCT_PACKAGES += \
    cbd \
    secril_config_svc \
    sehradiomanager

$(call soong_config_set,cbd,protocol,sipc)

# RIL - Configuration
PRODUCT_PACKAGES += sehradiomanager.conf

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi-linaro/exynos/cpboot_v3
