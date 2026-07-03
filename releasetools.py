#!/bin/env python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import common
import re


def FullOTA_InstallEnd(info):
    OTA_InstallEnd(info)


def IncrementalOTA_InstallEnd(info):
    info.input_zip = info.target_zip
    OTA_InstallEnd(info)


def AddImage(info, basename, dest, dir='IMAGES'):
    data = info.input_zip.read(dir + '/' + basename)
    common.ZipWriteStr(info.output_zip, basename, data)
    image = format(dest.split('/')[-1])
    info.script.Print(f'Patching {image} image unconditionally...')
    info.script.AppendExtra(f'package_extract_file("{basename}", "{dest}");')


def OTA_InstallEnd(info):
    AddImage(info, 'dtbo.img', '/dev/block/by-name/dtbo')
    AddImage(info, 'vbmeta.img', '/dev/block/by-name/vbmeta')
    AddImage(info, 'vendor_boot.img', '/dev/block/by-name/vendor_boot')

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
