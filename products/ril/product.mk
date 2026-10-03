#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Overlays
PRODUCT_PACKAGES += FrameworkResOverlayRIL

# Permissions
PRODUCT_PACKAGES += android.hardware.telephony.gsm.prebuilt.xml

# Properties
PRODUCT_VENDOR_PROPERTIES += ro.vendor.epdg.support=1

# RIL
PRODUCT_PACKAGES += \
    cbd \
    secril_config_svc \
    sehradiomanager

$(call soong_config_set,cbd,protocol,sipc)

# RIL - IMS
$(call inherit-product, hardware/lineage/generic-ims/ims.mk)

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi-linaro/exynos/cpboot_v3
