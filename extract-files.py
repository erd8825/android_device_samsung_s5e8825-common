#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import tempfile

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
    run_cmd,
)
from extract_utils.fixups_lib import (
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)
from extract_utils.tools import (
    DEFAULT_PATCHELF_VERSION,
    patchelf_version_path_map,
)


def lib_fixup_suffix(lib: str, *_):
    return f'{lib}.samsung'


namespace_imports = [
    'device/samsung/s5e8825-common',
    'hardware/samsung',
    'hardware/samsung_slsi-linaro/exynos',
    'hardware/samsung_slsi-linaro/graphics',
]


def rename_dynamic_symbol(
    _ctx: BlobFixupCtx,
    _file: File,
    file_path: str,
    old_name: str,
    new_name: str,
    **_kwargs,
):
    with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8') as tmp:
        tmp.write(f'{old_name} {new_name}')
        tmp.flush()
        run_cmd(
            [
                patchelf_version_path_map[DEFAULT_PATCHELF_VERSION],
                '--rename-dynamic-symbols',
                tmp.name,
                file_path,
            ]
        )


blob_fixups: blob_fixups_user_type = {
    # Camera - Dependecies
    'vendor/lib64/libsensorlistener.so': blob_fixup().add_needed('libsensorndkbridge_shim.samsung.so'),
    # DRM - Widevine
    'vendor/lib64/libwvaidl.so': blob_fixup().replace_needed('libprotobuf-cpp-lite-3.9.1.so', 'libprotobuf-cpp-full-3.9.1.so'),
    # Neural Networks - Dependecies
    'vendor/lib64/libeden_ud_gpu.so': blob_fixup()
        .add_needed('libeden_ud_cpu.so')
        .replace_needed('libOpenCL.so', 'libGLES_mali.so'),
    'vendor/lib64/libgraphgen_ann_import_s.so': blob_fixup()
         .clear_symbol_version('AHardwareBuffer_describe')
         .clear_symbol_version('AHardwareBuffer_lock')
         .clear_symbol_version('AHardwareBuffer_unlock'),
    'vendor/lib64/libnpuc_backend.so': blob_fixup().add_needed('libnpuc_cmdq.so'),
    'vendor/lib64/libnpuc_graph.so': blob_fixup().add_needed('libnpuc_common.so'),
    (
        'vendor/lib64/libnpuc_backend.so',
        'vendor/lib64/libnpuc_common.so',
        'vendor/lib64/libnpuc_controller.so',
        'vendor/lib64/libnpuc_frontend.so',
        'vendor/lib64/libnpuc_template.so'
    ): blob_fixup().add_needed('liblog.so'),
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
    # Security - Keymint
    (
        'vendor/lib64/libskeymint10device.so',
        'vendor/lib64/libskeymint_cli.so',
    ): blob_fixup()
        .add_needed('android.hardware.security.rkp-V1-ndk.so')
        .add_needed('libbase_shim.so')
        .call(rename_dynamic_symbol, 'OPENSSL_sk_new_null', 'sk_new_null')
        .call(rename_dynamic_symbol, 'OPENSSL_sk_num', 'sk_num')
        .call(rename_dynamic_symbol, 'OPENSSL_sk_push', 'sk_push')
        .call(rename_dynamic_symbol, 'OPENSSL_sk_value', 'sk_value')
        .replace_needed('android.hardware.security.keymint-V1-ndk_platform.so', 'android.hardware.security.keymint-V1-ndk.so')
        .replace_needed('android.hardware.security.secureclock-V1-ndk_platform.so', 'android.hardware.security.secureclock-V1-ndk.so')
        .replace_needed('android.hardware.security.sharedsecret-V1-ndk_platform.so', 'android.hardware.security.sharedsecret-V1-ndk.so')
        .replace_needed('libcrypto.so', 'libcrypto-v33.so'),
    # Sensors - Dependecies
    (
        'vendor/lib64/sensors.grip.so',
        'vendor/lib64/sensors.inputvirtual.so',
        'vendor/lib64/sensors.sensorhub.so',
    ): blob_fixup()
        .add_needed('libutils-v32.so')
        .binary_regex_replace(b'_ZN7android6Thread3runEPKcim', b'_ZN7utils326Thread3runEPKcim')
        .remove_needed('libhidltransport.so'),
}  # fmt: skip

lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    'libuuid': lib_fixup_suffix,
}

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
