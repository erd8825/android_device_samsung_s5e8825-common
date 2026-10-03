#!/bin/env python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import re

import common

BOOTLOADER = 'getprop("ro.boot.bootloader")'
MODEL = 'getprop("ro.boot.em.model")'
RADIO = '/dev/block/by-name/radio'
INDENT = '  '

SKIP_RE = re.compile(
    r'^skip\s+modem-flash-on-firmware(?:\.(.+?))?\s*=\s*(\S+)$'
)
MODEL_RE = re.compile(r'^require\s+model\s*=\s*(\S+)$')
FIRMWARE_RE = re.compile(r'^require\s+firmware(?:\.(.+?))?\s*=\s*(\S+)$')
MODEM_RE = re.compile(r'^RADIO/modem\.bin(?:_(.+))?$')


def SplitAlternatives(value):
    return [v.strip() for v in value.split('|') if v.strip()]


def BootloaderMismatch(firmwares):
    return ' && '.join(f'{BOOTLOADER} != "{fw}"' for fw in firmwares)


def ModelGuard(model):
    if len(model) == 7 and '-' not in model:
        return f'is_substring("{model}", {BOOTLOADER})'

    return f'{MODEL} == "{model}"'


def Emit(info, depth, line):
    info.script.AppendExtra(INDENT * depth + line)


def AppendGuarded(info, conds, body):
    for depth, cond in enumerate(conds):
        Emit(info, depth, f'if {cond} then')

    body(len(conds))

    for depth in reversed(range(len(conds))):
        Emit(info, depth, 'endif;')


def ParseAndroidInfo(android_info):
    if isinstance(android_info, bytes):
        android_info = android_info.decode('utf-8')

    firmware_skips = {}
    required_models = []
    required_firmwares = []

    for line in android_info.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue

        match = SKIP_RE.match(line)
        if match:
            firmwares = firmware_skips.setdefault(match.group(1), [])
            firmwares.extend(SplitAlternatives(match.group(2)))
            continue

        match = MODEL_RE.match(line)
        if match:
            for model in SplitAlternatives(match.group(1)):
                if model not in required_models:
                    required_models.append(model)
            continue

        match = FIRMWARE_RE.match(line)
        if match:
            model = match.group(1)
            required_firmwares.append((model, match.group(2)))
            if (
                model
                and not (len(model) == 7 and '-' not in model)
                and model not in required_models
            ):
                required_models.append(model)

    return firmware_skips, required_models, required_firmwares


def AppendModelAssertion(info, models):
    cond = ' || '.join(f'{MODEL} == "{model}"' for model in models)
    info.script.AppendExtra(
        f'{cond} || abort("E3004: This package does not support your model; '
        f'you have \\"" + {MODEL} + "\\".");'
    )


def AppendFirmwareAssertion(info, model, firmwares):
    abort_msg = (
        f'abort("E3004: This package requires \\"{"|".join(firmwares)}\\" '
        f'bootloader; you have \\"" + {BOOTLOADER} + "\\".");'
    )

    conds = [ModelGuard(model)] if model else []
    conds.append(BootloaderMismatch(firmwares))

    AppendGuarded(info, conds, lambda depth: Emit(info, depth, abort_msg))


def AddImage(info, basename, dest, dir='IMAGES', depth=0):
    data = info.input_zip.read(dir + '/' + basename)
    common.ZipWriteStr(info.output_zip, basename, data)
    image = format(dest.split('/')[-1])
    Emit(info, depth, f'ui_print("Patching {image} image unconditionally...");')
    Emit(info, depth, f'package_extract_file("{basename}", "{dest}");')


def AppendModemImage(info, model, skips, guarded):
    basename = 'modem.bin' if model is None else f'modem.bin_{model}'
    conds = []

    if guarded:
        conds.append(f'{MODEL} == "{model}"')
        firmwares = skips.get(None, []) + skips.get(model, [])
        if firmwares:
            conds.append(BootloaderMismatch(firmwares))

    AppendGuarded(
        info,
        conds,
        lambda depth: AddImage(info, basename, RADIO, 'RADIO', depth),
    )


def OTA_Assertions(info):
    android_info = info.input_zip.read('OTA/android-info-extra.txt')
    _, required_models, required_firmwares = ParseAndroidInfo(android_info)

    if required_models:
        AppendModelAssertion(info, required_models)

    for model, firmware in required_firmwares:
        AppendFirmwareAssertion(info, model, SplitAlternatives(firmware))


def OTA_InstallEnd(info):
    AddImage(info, 'dtbo.img', '/dev/block/by-name/dtbo')
    AddImage(info, 'vbmeta.img', '/dev/block/by-name/vbmeta')
    AddImage(info, 'vendor_boot.img', '/dev/block/by-name/vendor_boot')

    android_info = info.input_zip.read('OTA/android-info-extra.txt')
    skips, _, _ = ParseAndroidInfo(android_info)

    models = [
        match.group(1)
        for match in map(MODEM_RE.match, info.input_zip.namelist())
        if match
    ]

    guarded = len(models) > 1
    for model in models:
        if guarded and model is None:
            continue

        AppendModemImage(info, model, skips, guarded)


def FullOTA_Assertions(info):
    OTA_Assertions(info)


def FullOTA_InstallEnd(info):
    OTA_InstallEnd(info)


def IncrementalOTA_Assertions(info):
    OTA_Assertions(info)


def IncrementalOTA_InstallEnd(info):
    info.input_zip = info.target_zip
    OTA_InstallEnd(info)
