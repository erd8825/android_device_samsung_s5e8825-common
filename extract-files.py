#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)


def lib_fixup_suffix(lib: str, *_):
    return f'{lib}.samsung'


namespace_imports = ['hardware/samsung']

blob_fixups: blob_fixups_user_type = {
    # Audio (- Dependecies)
    (
        'vendor/lib64/libaboxpcmdump.so',
        'vendor/lib64/libaudioparamupdate.so',
        'vendor/lib64/libaudioproxy2.so',
        'vendor/lib64/hw/audio.primary.s5e8825.so',
    ): blob_fixup()
    .replace_needed('libaudioroute.so', 'libaudioroute.samsung.so')
    .replace_needed('libtinyalsa.so', 'libtinyalsa.samsung.so'),
    # RIL
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

lib_fixups: lib_fixups_user_type = {'libuuid': lib_fixup_suffix}

module = ExtractUtilsModule(
    's5e8825-common',
    'samsung',
    namespace_imports=namespace_imports,
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
