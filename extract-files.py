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
    # Security - Keymint
    (
        'vendor/bin/hw/android.hardware.security.keymint-service.samsung',
        'vendor/lib64/lib_android_keymaster_skeymint_utils.so',
        'vendor/lib64/libskeymint.so',
        'vendor/lib64/libskeymint10device.so',
        'vendor/lib64/libskeymint_cli.so',
        'vendor/lib64/vendor.samsung.hardware.keymint-V1-ndk_platform.so',
    ): blob_fixup()
        .add_needed('android.hardware.security.rkp-V3-ndk.so')
        .add_needed('libbase_shim.so')
        .replace_needed('android.hardware.security.keymint-V1-ndk_platform.so', 'android.hardware.security.keymint-V1-ndk.so')
        .replace_needed('android.hardware.security.secureclock-V1-ndk_platform.so', 'android.hardware.security.secureclock-V1-ndk.so')
        .replace_needed('android.hardware.security.sharedsecret-V1-ndk_platform.so', 'android.hardware.security.sharedsecret-V1-ndk.so')
        .replace_needed('lib_android_keymaster_keymint_utils.so', 'lib_android_keymaster_skeymint_utils.so')
        .replace_needed('libcrypto.so', 'libcrypto-v33.so')
        .replace_needed('libkeymint.so', 'libskeymint.so')
        .replace_needed('libkeymaster_portable.so', 'libkeymaster_portable.samsung.so'),
    'vendor/etc/init/android.hardware.security.keymint-service.samsung.rc': blob_fixup().regex_replace('-service', '-service.samsung'),
    (
        'vendor/lib64/libskeymint10device.so',
        'vendor/lib64/libskeymint_cli.so',
    ): blob_fixup()
        .call(rename_dynamic_symbol, 'OPENSSL_sk_new_null', 'sk_new_null')
        .call(rename_dynamic_symbol, 'OPENSSL_sk_num', 'sk_num')
        .call(rename_dynamic_symbol, 'OPENSSL_sk_push', 'sk_push')
        .call(rename_dynamic_symbol, 'OPENSSL_sk_value', 'sk_value'),
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
