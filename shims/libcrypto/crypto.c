/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

extern void* sk_new_null(void);
void* OPENSSL_sk_new_null(void) {
    return sk_new_null();
}

extern size_t sk_num(void* sk);
size_t OPENSSL_sk_num(void* sk) {
    return sk_num(sk);
}

extern size_t sk_push(void* sk, void* p);
size_t OPENSSL_sk_push(void* sk, void* p) {
    return sk_push(sk, p);
}

extern void* sk_value(const void* sk, size_t i);
void* OPENSSL_sk_value(const void* sk, size_t i) {
    return sk_value(sk, i);
}
