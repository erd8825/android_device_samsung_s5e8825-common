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
