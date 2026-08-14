#!/bin/env python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import common
import re


def _append_model_assertion(info, models):
    cond = ' || '.join(
        [f'getprop("ro.boot.em.model") == "{model}"' for model in models]
    )
    info.script.AppendExtra(
        f'{cond} || abort("E3004: This package does not support your model; '
        f'you have \\"" + getprop("ro.boot.em.model") + "\\".");'
    )


def _append_firmware_assertion(info, model, firmwares):
    cond = _firmware_check_condition(firmwares)
    abort_msg = (
        f'abort("E3004: This package requires \\"{"|".join(firmwares)}\\" bootloader; '
        f'you have \\"" + getprop("ro.boot.bootloader") + "\\".");'
    )

    if model is None:
        info.script.AppendExtra(f'if {cond} then')
        info.script.AppendExtra(abort_msg)
        info.script.AppendExtra('endif;')
        return

    if len(model) == 7 and '-' not in model:
        info.script.AppendExtra(
            f'if is_substring("{model}", getprop("ro.boot.bootloader")) then'
        )
        info.script.AppendExtra(f'  if {cond} then')
        info.script.AppendExtra(f'    {abort_msg}')
        info.script.AppendExtra('  endif;')
        info.script.AppendExtra('endif;')
        return

    info.script.AppendExtra(f'if getprop("ro.boot.em.model") == "{model}" then')
    info.script.AppendExtra(f'  if {cond} then')
    info.script.AppendExtra(f'    {abort_msg}')
    info.script.AppendExtra('  endif;')
    info.script.AppendExtra('endif;')


def _firmware_check_condition(firmwares):
    return ' && '.join([f'getprop("ro.boot.bootloader") != "{fw}"' for fw in firmwares])  # fmt: skip


def _parse_android_info(android_info):
    if isinstance(android_info, bytes):
        android_info = android_info.decode('utf-8')

    firmware_skips = []
    required_models = []
    required_firmwares = []

    for line in android_info.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue

        match = re.match(r'^skip\s+modem-flash-on-firmware\s*=\s*(\S+)$', line)
        if match:
            firmware_skips.append(match.group(1))
            continue

        match = re.match(r'^require\s+model\s*=\s*(\S+)$', line)
        if match:
            for model in [
                p.strip() for p in match.group(1).split('|') if p.strip()
            ]:
                if model not in required_models:
                    required_models.append(model)
            continue

        match = re.match(r'^require\s+firmware(?:\.(.+?))?\s*=\s*(\S+)$', line)
        if match:
            required_firmwares.append((match.group(1), match.group(2)))

    return firmware_skips, required_models, required_firmwares


def FullOTA_Assertions(info):
    OTA_Assertions(info, info.input_zip)


def FullOTA_InstallEnd(info):
    OTA_InstallEnd(info)


def IncrementalOTA_InstallEnd(info):
    info.input_zip = info.target_zip
    OTA_InstallEnd(info)


def IncrementalOTA_Assertions(info):
    OTA_Assertions(info, info.input_zip)


def AddImage(info, basename, dest, dir='IMAGES'):
    data = info.input_zip.read(dir + '/' + basename)
    common.ZipWriteStr(info.output_zip, basename, data)
    image = format(dest.split('/')[-1])
    info.script.Print(f'Patching {image} image unconditionally...')
    info.script.AppendExtra(f'package_extract_file("{basename}", "{dest}");')


def OTA_Assertions(info, input_zip):
    android_info = input_zip.read('OTA/android-info-extra.txt')
    _, required_models, required_firmwares = _parse_android_info(android_info)

    if required_models:
        _append_model_assertion(info, required_models)

    for model, firmware in required_firmwares:
        _append_firmware_assertion(info, model, firmware.split('|'))


def OTA_InstallEnd(info):
    AddImage(info, 'dtbo.img', '/dev/block/by-name/dtbo')
    AddImage(info, 'vbmeta.img', '/dev/block/by-name/vbmeta')
    AddImage(info, 'vendor_boot.img', '/dev/block/by-name/vendor_boot')

    android_info = info.input_zip.read('OTA/android-info-extra.txt')
    firmware_skips, _, _ = _parse_android_info(android_info)

    if firmware_skips:
        raw = firmware_skips[0]
        if '|' in raw:
            parts = [p.strip() for p in raw.split('|') if p.strip()]
            conds = [f'getprop("ro.boot.bootloader") != "{p}"' for p in parts]
            info.script.AppendExtra('if ' + ' && '.join(conds) + ' then')
        else:
            fw = raw.strip()
            info.script.AppendExtra(f'if getprop("ro.boot.bootloader") != "{fw}" then')  # fmt: skip

    for e in info.input_zip.namelist():
        match = re.match(r'^RADIO/modem\.bin(?:_(.+))?$', e)
        if not match:
            continue

        model = match.group(1)
        if model is None:
            AddImage(info, 'modem.bin', '/dev/block/by-name/radio', 'RADIO')
        else:
            # fmt: off
            info.script.AppendExtra(f'if getprop("ro.boot.em.model") == "{model}" then')
            AddImage(info, f'modem.bin_{model}', '/dev/block/by-name/radio', 'RADIO')
            # fmt: on
            info.script.AppendExtra('endif;')

    if firmware_skips:
        info.script.AppendExtra('endif;')
