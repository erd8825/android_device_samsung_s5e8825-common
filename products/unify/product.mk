#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Firmware - Placeholders
PRODUCT_PACKAGES += \
    APDV_AUDIO_SLSI.bin_placeholder \
    AP_AUDIO_SLSI.bin_placeholder \
    calliope_sram.bin_placeholder \
    mfc_fw.bin_placeholder \
    NPU.bin_placeholder \
    os.checked.bin_placeholder \
    vts.bin_placeholder

# Init
PRODUCT_PACKAGES += init.unify.rc

# Soong - Namespaces
PRODUCT_SOONG_NAMESPACES += device/samsung/s5e8825-common/products/unify
