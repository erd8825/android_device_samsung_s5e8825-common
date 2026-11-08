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


namespace_imports = [
    'device/samsung/s5e8825-common',
    'hardware/samsung',
    'hardware/samsung_slsi-linaro/exynos',
    'hardware/samsung_slsi-linaro/graphics',
]

blob_fixups: blob_fixups_user_type = {
    # Camera - Dependecies
    'vendor/lib64/libsensorlistener.so': blob_fixup().add_needed('libsensorndkbridge_shim.samsung.so'),
    # GNSS
    'vendor/bin/hw/gpsd': blob_fixup().binary_regex_replace(b'libcrypto.so', b'libcryptx.so'),
    'vendor/etc/init/init.gps.rc': blob_fixup().regex_replace(r'\n.*:[\s\S]*', ''),
    # Neural Networks
    'vendor/lib64/libeden_ud_gpu.so': blob_fixup()
        .add_needed('libeden_ud_cpu.so')
        .replace_needed('libOpenCL.so', 'libGLES_mali.so'),
    'vendor/lib64/libeden_wrapper_sdk.so': blob_fixup().add_needed('libc++_ndk.samsung.so'),
    # RIL
    'vendor/etc/init/init.vendor.rilcommon.rc': blob_fixup().regex_replace('\n.*\n.*n}\n', ''),
    'vendor/lib64/libsec-ril-impl.so': blob_fixup()
        # Always emit uiccApplicationsEnablementChanged
        # Before: [b.lt 0x00379464]
        # After: [nop]
        .sig_replace('1f 00 08 6b ab 01 00 54', '1f 00 08 6b 1f 20 03 d5')
        # Before: [b.lt 0x00382144]
        # After: [nop]
        .sig_replace('bf 02 08 6b ab 01 00 54', 'bf 02 08 6b 1f 20 03 d5')
        # Skip legacy UICC code
        # Before: [b.gt 0x00382398]
        # After: [b 0x00382398]
        .sig_replace('1f 00 08 6b 0c 01 00 54', '1f 00 08 6b 08 00 00 14'),
    # Sensors
    (
        'vendor/lib64/sensors.inputvirtual.so',
        'vendor/lib64/sensors.sensorhub.so',
    ): blob_fixup().remove_needed('libhidltransport.so'),
}  # fmt: skip

lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    # Security - TEEGRIS
    'libuuid': lambda lib, *_: f'{lib}.vendor'
}  # fmt: skip

module = ExtractUtilsModule(
    's5e8825-common',
    'samsung',
    namespace_imports=namespace_imports,
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
)

module.add_proprietary_file(
    'products/ril/proprietary-files.txt'
).add_copy_files_guard('TARGET_HAS_RIL', 'true')

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
