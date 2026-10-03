#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Fingerprint
$(call soong_config_set,samsungUdfpsVars,dim_layer_zorder,0xff)
$(call soong_config_set,samsungUdfpsVars,udfps_zorder,0x100)
$(call soong_config_set,surfaceflinger,udfps_lib,//hardware/samsung/fingerprint:libudfps_extension.samsung)

# Init
PRODUCT_PACKAGES += init.udfps.rc

# Overlays
PRODUCT_PACKAGES += \
    FrameworkResOverlayUdfps \
    SystemUIOverlayUdfps

# Properties
TARGET_VENDOR_PROP += device/samsung/s5e8825-common/products/udfps/configs/props/vendor.prop

# Sensors
PRODUCT_PACKAGES += sensors.samsung
