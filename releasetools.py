#!/bin/env python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import re

import common

MODEL = 'getprop("ro.boot.em.model")'
RADIO = '/dev/block/by-name/radio'
INDENT = '  '

MODEM_RE = re.compile(r'^RADIO/modem\.bin(?:_(.+))?$')


def Emit(info, depth, line):
    info.script.AppendExtra(INDENT * depth + line)


def AppendGuarded(info, conds, body):
    for depth, cond in enumerate(conds):
        Emit(info, depth, f'if {cond} then')

    body(len(conds))

    for depth in reversed(range(len(conds))):
        Emit(info, depth, 'endif;')


def AddImage(info, basename, dest, dir='IMAGES', depth=0):
    data = info.input_zip.read(dir + '/' + basename)
    common.ZipWriteStr(info.output_zip, basename, data)
    image = format(dest.split('/')[-1])
    Emit(info, depth, f'ui_print("Patching {image} image unconditionally...");')
    Emit(info, depth, f'package_extract_file("{basename}", "{dest}");')


def AppendModemImage(info, model, guarded):
    basename = 'modem.bin' if model is None else f'modem.bin_{model}'
    conds = [f'{MODEL} == "{model}"'] if guarded else []

    AppendGuarded(
        info,
        conds,
        lambda depth: AddImage(info, basename, RADIO, 'RADIO', depth),
    )


def OTA_InstallEnd(info):
    AddImage(info, 'dtbo.img', '/dev/block/by-name/dtbo')
    AddImage(info, 'vbmeta.img', '/dev/block/by-name/vbmeta')
    AddImage(info, 'vendor_boot.img', '/dev/block/by-name/vendor_boot')

    models = [
        match.group(1)
        for match in map(MODEM_RE.match, info.input_zip.namelist())
        if match
    ]

    guarded = len(models) > 1
    for model in models:
        if guarded and model is None:
            continue

        AppendModemImage(info, model, guarded)


def FullOTA_InstallEnd(info):
    OTA_InstallEnd(info)


def IncrementalOTA_InstallEnd(info):
    info.input_zip = info.target_zip
    OTA_InstallEnd(info)
