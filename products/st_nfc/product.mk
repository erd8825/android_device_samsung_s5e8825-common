#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# NFC
PRODUCT_PACKAGES += android.hardware.nfc-service.st

# NFC - Configuration
PRODUCT_PACKAGES += \
    libnfc-hal-st.conf \
    st21nfc_conf.txt

# NFC - Init
PRODUCT_PACKAGES += init.nfc.st.rc
