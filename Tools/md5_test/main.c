/*
 * Standalone mbedTLS MD5 Verification Test
 *
 * Uses the EXACT same md5.c source from the embedded project to verify
 * that the mbedTLS MD5 implementation produces correct results.
 *
 * Build (from this directory):
 *   make
 * or:
 *   gcc -Wall -std=c99 -O2 -I . -I ../../Middlewares/Third_Party/mbedTLS/include \
 *       -DMBEDTLS_CONFIG_FILE=\"mbedtls_config.h\" \
 *       -o md5_test.exe main.c \
 *       ../../Middlewares/Third_Party/mbedTLS/library/md5.c
 *
 * Usage:
 *   md5_test.exe <firmware.bin>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------
 * Stub: mbedtls_platform_zeroize()
 *
 * This is the ONLY function from the mbedTLS platform layer that md5.c
 * calls (via mbedtls_md5_free).  We provide it directly here so we
 * don't need to compile platform_util.c (which would pull in platform.h,
 * threading.h, time.h, etc.).
 *
 * The volatile-function-pointer pattern prevents the compiler from
 * optimizing the memset away, matching the real implementation in
 * Middlewares/Third_Party/mbedTLS/library/platform_util.c.
 * ------------------------------------------------------------------ */
static void * (* const volatile zeroize_memset)(void *, int, size_t) = memset;

void mbedtls_platform_zeroize(void *buf, size_t len)
{
    if (buf != NULL && len > 0)
        zeroize_memset(buf, 0, len);
}

/* Now include the real mbedTLS MD5 header (resolved via -I include path) */
#include "mbedtls/md5.h"

/* ------------------------------------------------------------------ */

static void print_md5_hex(const unsigned char digest[16])
{
    for (int i = 0; i < 16; i++)
        printf("%02x", digest[i]);
}

/*
 * Compute MD5 incrementally, chunk-by-chunk — EXACT same API pattern
 * as ota_task.c lines 88-142.
 */
static int compute_incremental_md5(const char *path,
                                   unsigned char digest_out[16])
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open '%s'\n", path);
        return -1;
    }

    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);              /* ota_task.c line 90 */

    unsigned char chunk[2048];             /* ota_task.c line 92 — same chunk size */
    size_t total = 0;

    for (;;) {
        size_t n = fread(chunk, 1, sizeof(chunk), fp);
        if (n == 0) break;
        mbedtls_md5_update(&ctx, chunk, n); /* ota_task.c line 127 */
        total += n;
        printf("  Chunk: %zu bytes (running total: %zu)\n", n, total);
    }

    mbedtls_md5_finish(&ctx, digest_out);  /* ota_task.c line 141 */
    mbedtls_md5_free(&ctx);                /* ota_task.c line 142 */
    fclose(fp);
    return 0;
}

/*
 * Compute MD5 in one shot — for cross-validation.
 */
static int compute_oneshot_md5(const unsigned char *data, size_t len,
                               unsigned char digest_out[16])
{
    return mbedtls_md5_ret(data, len, digest_out);
}

int main(int argc, char *argv[])
{
    int exit_code = 0;

    /* ---- Step 1: Self-test against RFC 1321 vectors ---- */
    printf("=== mbedTLS MD5 Self-Test (RFC 1321 vectors) ===\n");
    int st_ret = mbedtls_md5_self_test(1);
    if (st_ret != 0) {
        printf("\n*** SELF-TEST FAILED!  The MD5 implementation is broken. ***\n");
        return 1;
    }
    printf("\nSelf-test PASSED.  The MD5 algorithm itself is correct.\n\n");

    /* ---- Step 2: File argument handling ---- */
    if (argc < 2) {
        printf("Usage: %s <firmware.bin>\n\n", argv[0]);
        printf("Provide a .bin file to compute its MD5 using:\n");
        printf("  1) Incremental API (matching OTA download pattern)\n");
        printf("  2) One-shot API (mbedtls_md5_ret)\n\n");
        printf("Compare the hex digest with PowerShell:\n");
        printf("  Get-FileHash -Algorithm MD5 <firmware.bin>\n");
        return 0;
    }

    const char *filepath = argv[1];

    /* ---- Step 3: Incremental MD5 (matches OTA code exactly) ---- */
    printf("=== Incremental MD5 (OTA-compatible API) ===\n");
    unsigned char digest_inc[16];
    if (compute_incremental_md5(filepath, digest_inc) != 0) {
        exit_code = 1;
    } else {
        printf("  Incremental MD5: ");
        print_md5_hex(digest_inc);
        printf("\n");
    }

    /* ---- Step 4: One-shot MD5 for cross-validation ---- */
    printf("\n=== One-shot MD5 (mbedtls_md5_ret) ===\n");
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open '%s'\n", filepath);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    unsigned char *buf = (unsigned char *)malloc((size_t)fsize);
    if (!buf) {
        fprintf(stderr, "ERROR: malloc(%ld) failed\n", fsize);
        fclose(fp);
        return 1;
    }
    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);

    unsigned char digest_one[16];
    compute_oneshot_md5(buf, nread, digest_one);
    free(buf);

    printf("  One-shot  MD5: ");
    print_md5_hex(digest_one);
    printf("\n");

    /* ---- Step 5: Compare incremental vs one-shot ---- */
    if (memcmp(digest_inc, digest_one, 16) == 0) {
        printf("\n  Incremental and one-shot digests MATCH.\n");
    } else {
        printf("\n  *** WARNING: Incremental and one-shot digests DIFFER! ***\n");
        exit_code = 1;
    }

    return exit_code;
}
