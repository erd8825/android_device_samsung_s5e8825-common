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
}  # fmt: skip

lib_fixups: lib_fixups_user_type = {'libuuid': lib_fixup_suffix}

module = ExtractUtilsModule(
    's5e8825-common',
    'samsung',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
