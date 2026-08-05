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


namespace_imports = ['hardware/samsung']

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
