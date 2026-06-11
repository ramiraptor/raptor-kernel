/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * string.h - freestanding C string/memory routines.
 *
 * The kernel cannot link against a hosted libc, so it carries its own
 * implementations of the handful of routines it needs.
 */
#ifndef RAPTOR_STRING_H
#define RAPTOR_STRING_H

#include <stddef.h>

void  *memset(void *dst, int c, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);
size_t strlcpy(char *dst, const char *src, size_t size);
char  *strcat(char *dst, const char *src);
char  *strchr(const char *s, int c);

#endif /* RAPTOR_STRING_H */
