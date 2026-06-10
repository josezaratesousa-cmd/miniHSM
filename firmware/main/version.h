#pragma once

/* Version del producto Xami.
 * XAMI_VERSION lo subes a mano en releases mayores.
 * El CI genera version_generated.h con el commit y build reales.
 * Sin CI (build local) quedan los valores "dev". */

#define XAMI_PRODUCT_NAME  "Xami"

#ifndef XAMI_VERSION
#define XAMI_VERSION       "1.0.0"
#endif

/* Si el CI genero el header, usarlo */
#if __has_include("version_generated.h")
#include "version_generated.h"
#endif

#ifndef XAMI_GIT_COMMIT
#define XAMI_GIT_COMMIT    "dev"
#endif

#ifndef XAMI_BUILD_NUMBER
#define XAMI_BUILD_NUMBER  "0"
#endif

/* "Xami v1.0.0 · build 15 · 9fac64c"  (· = \xC2\xB7 en UTF-8) */
#define XAMI_VERSION_STRING \
    XAMI_PRODUCT_NAME " v" XAMI_VERSION \
    " \xC2\xB7 build " XAMI_BUILD_NUMBER \
    " \xC2\xB7 " XAMI_GIT_COMMIT
