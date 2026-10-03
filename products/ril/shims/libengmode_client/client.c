/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

int getStatus(unsigned int mode __attribute__((unused)), int timeout_ms __attribute__((unused))) {
    return 0xd0270010; // 6,"engmode_vendor_client","Failed to proceed command\n"
}
