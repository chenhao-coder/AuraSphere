#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/*
 * Absolute minimal config for standalone MD5 verification.
 * Designed to match the compiled behavior of md5.c from the embedded project
 * while avoiding pulling in any other mbedTLS library modules.
 */

/* Enable the MD5 module itself */
#define MBEDTLS_MD5_C

/* Enable the built-in RFC 1321 self-test vectors */
#define MBEDTLS_SELF_TEST

/*
 * DELIBERATELY NOT DEFINED:
 * - MBEDTLS_PLATFORM_C       -> md5.c falls back to <stdio.h> + #define mbedtls_printf printf
 * - MBEDTLS_HAVE_TIME_DATE   -> platform_util.h skips <time.h> and platform_time.h
 * - MBEDTLS_DEPRECATED_REMOVED -> keeps mbedtls_md5_starts/update/finish (what OTA code uses)
 * - MBEDTLS_PLATFORM_ZEROIZE_ALT -> uses our stub in main.c instead
 * - MBEDTLS_THREADING_C      -> not needed for single-threaded test
 * - MBEDTLS_CHECK_PARAMS     -> keeps validation macros as no-ops
 */

#endif /* MBEDTLS_CONFIG_H */
