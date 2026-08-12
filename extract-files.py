#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)


namespace_imports = [
    'device/samsung/s5e8825-common',
    'hardware/samsung',
]

blob_fixups: blob_fixups_user_type = {
    # RIL
    'vendor/etc/init/init.baseband.rc': blob_fixup().regex_replace('\n.*\n.*\n.*nic\n', ''),
    'vendor/etc/init/init.vendor.rilcommon.rc': blob_fixup().regex_replace('\n.*\n.*n}\n', ''),
    'vendor/lib64/libsec-ril-impl.so': blob_fixup()
        # Always emit uiccApplicationsEnablementChanged
        # Before: [b.gt 0x00382398]
        # After: [nop]
        .sig_replace('1f 00 08 6b 0c 01 00 54', '1f 00 08 6b 1f 20 03 d5')
        # Before: [b.lt 0x00379464]
        # After: [nop]
        .sig_replace('1f 00 08 6b ab 01 00 54', '1f 00 08 6b 1f 20 03 d5')
        # Before: [b.lt 0x00382144]
        # After: [nop]
        .sig_replace('bf 02 08 6b ab 01 00 54', 'bf 02 08 6b 1f 20 03 d5'),
    # Security - Keymint
    (
        'vendor/lib64/libskeymint10device.so',
        'vendor/lib64/libskeymint_cli.so',
    ): blob_fixup()
        .add_needed('android.hardware.security.rkp-V1-ndk.so')
        .add_needed('libbase_shim.so')
        .add_needed('libshim_crypto.so')
        .replace_needed('android.hardware.security.keymint-V1-ndk_platform.so', 'android.hardware.security.keymint-V1-ndk.so')
        .replace_needed('android.hardware.security.secureclock-V1-ndk_platform.so', 'android.hardware.security.secureclock-V1-ndk.so')
        .replace_needed('android.hardware.security.sharedsecret-V1-ndk_platform.so', 'android.hardware.security.sharedsecret-V1-ndk.so')
        .replace_needed('libcrypto.so', 'libcrypto-v33.so'),
}  # fmt: skip

module = ExtractUtilsModule(
    's5e8825-common',
    'samsung',
    namespace_imports=namespace_imports,
    blob_fixups=blob_fixups,
    lib_fixups={
        # Security - TEEGRIS
        'libuuid': lambda lib, *_: f'{lib}.vendor'
    },
)

module.add_proprietary_file(
    'products/ril/proprietary-files.txt'
).add_copy_files_guard('TARGET_HAS_RIL', 'true')

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
