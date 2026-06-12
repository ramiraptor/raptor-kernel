/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * version.h - kernel identity strings, used by uname and the boot banner.
 */
#ifndef RAPTOR_VERSION_H
#define RAPTOR_VERSION_H

#define RAPTOR_NAME       "Raptor"
#define RAPTOR_VERSION    "0.2.1"
#define RAPTOR_CODENAME   "Velociraptor"
#define RAPTOR_ARCH       "i386"
#define RAPTOR_BUILD      __DATE__ " " __TIME__
#define RAPTOR_COMPILER   "gcc " __VERSION__

#endif /* RAPTOR_VERSION_H */
