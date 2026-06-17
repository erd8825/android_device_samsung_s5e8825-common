#!/bin/env python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import common


def FullOTA_InstallEnd(info):
    OTA_InstallEnd(info)


def IncrementalOTA_InstallEnd(info):
    info.input_zip = info.target_zip
    OTA_InstallEnd(info)


def AddImage(info, basename, dest):
    data = info.input_zip.read('IMAGES/' + basename)
    common.ZipWriteStr(info.output_zip, basename, data)
    image = format(dest.split('/')[-1])
    info.script.Print(f'Patching {image} image unconditionally...')
    info.script.AppendExtra(f'package_extract_file("{basename}", "{dest}");')


def OTA_InstallEnd(info):
    AddImage(info, 'dtbo.img', '/dev/block/by-name/dtbo')
    AddImage(info, 'vbmeta.img', '/dev/block/by-name/vbmeta')
    AddImage(info, 'vendor_boot.img', '/dev/block/by-name/vendor_boot')
