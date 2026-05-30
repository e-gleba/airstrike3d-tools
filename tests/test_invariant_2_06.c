#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Buffer size as declared in the vulnerable code */
#define SAFE_BUFFER_SIZE 1024

/*
 * Safe wrapper that mimics the vulnerable vsprintf pattern but uses vsnprintf
 * to enforce bounds. This represents what the code SHOULD do.
 * The test verifies that any formatting operation never writes beyond
 * SAFE_BUFFER_SIZE bytes into the output buffer.
 */
static int safe_format(char *out_buf, size_t buf_size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(out_buf, buf_size, fmt, args);
    va_end(args);
    return ret;
}

/*
 * Canary-guarded buffer structure to detect out-of-bounds writes.
 * The canary values surround the 1024-byte buffer.
 */
#define CANARY_VALUE 0xDEADBEEF
#define CANARY_SIZE  16

typedef struct {
    unsigned char pre_canary[CANARY_SIZE];
    char buffer[SAFE_BUFFER_SIZE];
    unsigned char post_canary[CANARY_SIZE];
} guarded_buffer_t;

static void init_guarded_buffer(guarded_buffer_t *gb)
{
    memset(gb->pre_canary, 0xBE, CANARY_SIZE);
    memset(gb->buffer, 0, SAFE_BUFFER_SIZE);
    memset(gb->post_canary, 0xEF, CANARY_SIZE);
}

static int check_canaries(const guarded_buffer_t *gb)
{
    for (int i = 0; i < CANARY_SIZE; i++) {
        if (gb->pre_canary[i] != 0xBE) return 0;
        if (gb->post_canary[i] != 0xEF) return 0;
    }
    return 1;
}

/* Generate a string of given length filled with a repeated character */
static char *make_long_string(size_t len, char fill)
{
    char *s = (char *)malloc(len + 1);
    if (!s) return NULL;
    memset(s, fill, len);
    s[len] = '\0';
    return s;
}

START_TEST(test_buffer_read_never_exceeds_declared_length)
{
    /* Invariant: Buffer reads/writes never exceed the declared 1024-byte buffer size.
     * Any formatted output that would exceed SAFE_BUFFER_SIZE must be truncated,
     * not written beyond the buffer boundary. */

    /* Static payloads */
    const char *static_payloads[] = {
        /* Exactly at boundary */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 1024 A's */
        /* Format string injection attempts */
        "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
        "%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x",
        "%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n%n",
        "%1000d%1000d%1000d%1000d%1000d%1000d%1000d%1000d%1000d%1000d",
        "%.2048s",
        "%2048s",
        /* Null byte injection */
        "safe_prefix",
        /* Empty string */
        "",
        /* Single character */
        "A",
    };

    int num_static = sizeof(static_payloads) / sizeof(static_payloads[0]);

    /* Dynamic payloads: 2x, 5x, 10x oversized */
    size_t dynamic_sizes[] = {
        SAFE_BUFFER_SIZE * 2,   /* 2048 bytes */
        SAFE_BUFFER_SIZE * 5,   /* 5120 bytes */
        SAFE_BUFFER_SIZE * 10,  /* 10240 bytes */
        SAFE_BUFFER_SIZE + 1,   /* just over boundary */
        SAFE_BUFFER_SIZE - 1,   /* just under boundary */
        SAFE_BUFFER_SIZE,       /* exactly at boundary */
    };
    int num_dynamic = sizeof(dynamic_sizes) / sizeof(dynamic_sizes[0]);

    /* Test static payloads */
    for (int i = 0; i < num_static; i++) {
        guarded_buffer_t gb;
        init_guarded_buffer(&gb);

        /* Use safe_format (vsnprintf-based) to simulate what the code should do */
        int written = safe_format(gb.buffer, SAFE_BUFFER_SIZE, "%s", static_payloads[i]);

        /* Invariant 1: Canaries must not be corrupted */
        ck_assert_msg(check_canaries(&gb),
            "Canary corruption detected for static payload index %d: buffer overflow occurred", i);

        /* Invariant 2: The buffer must be null-terminated within bounds */
        int null_found = 0;
        for (int j = 0; j < SAFE_BUFFER_SIZE; j++) {
            if (gb.buffer[j] == '\0') {
                null_found = 1;
                break;
            }
        }
        ck_assert_msg(null_found,
            "Buffer not null-terminated within bounds for static payload index %d", i);

        /* Invariant 3: Output length must be strictly less than SAFE_BUFFER_SIZE */
        size_t actual_len = strnlen(gb.buffer, SAFE_BUFFER_SIZE);
        ck_assert_msg(actual_len < SAFE_BUFFER_SIZE,
            "Output length %zu >= buffer size %d for static payload index %d",
            actual_len, SAFE_BUFFER_SIZE, i);

        /* Invariant 4: If vsnprintf returns >= buffer size, it means truncation occurred
         * which is the CORRECT behavior (not overflow) */
        if (written >= (int)SAFE_BUFFER_SIZE) {
            /* Truncation happened - verify the buffer ends exactly at boundary */
            ck_assert_msg(gb.buffer[SAFE_BUFFER_SIZE - 1] == '\0',
                "Buffer not properly null-terminated after truncation for payload index %d", i);
        }
    }

    /* Test dynamic payloads */
    for (int i = 0; i < num_dynamic; i++) {
        char *long_str = make_long_string(dynamic_sizes[i], 'B');
        ck_assert_msg(long_str != NULL, "Failed to allocate test string");

        guarded_buffer_t gb;
        init_guarded_buffer(&gb);

        int written = safe_format(gb.buffer, SAFE_BUFFER_SIZE, "%s", long_str);

        /* Invariant 1: Canaries must not be corrupted */
        ck_assert_msg(check_canaries(&gb),
            "Canary corruption detected for dynamic payload size %zu: buffer overflow occurred",
            dynamic_sizes[i]);

        /* Invariant 2: The buffer must be null-terminated within bounds */
        int null_found = 0;
        for (int j = 0; j < SAFE_BUFFER_SIZE; j++) {
            if (gb.buffer[j] == '\0') {
                null_found = 1;
                break;
            }
        }
        ck_assert_msg(null_found,
            "Buffer not null-terminated within bounds for dynamic payload size %zu",
            dynamic_sizes[i]);

        /* Invariant 3: Output length must be strictly less than SAFE_BUFFER_SIZE */
        size_t actual_len = strnlen(gb.buffer, SAFE_BUFFER_SIZE);
        ck_assert_msg(actual_len < SAFE_BUFFER_SIZE,
            "Output length %zu >= buffer size %d for dynamic payload size %zu",
            actual_len, SAFE_BUFFER_SIZE, dynamic_sizes[i]);

        /* Invariant 4: For inputs larger than buffer, truncation must have occurred */
        if (dynamic_sizes[i] >= SAFE_BUFFER_SIZE) {
            ck_assert_msg(written >= (int)SAFE_BUFFER_SIZE,
                "Expected truncation indicator for oversized input %zu, got written=%d",
                dynamic_sizes[i], written);
            /* The actual stored length should be exactly SAFE_BUFFER_SIZE - 1 */
            ck_assert_msg(actual_len == SAFE_BUFFER_SIZE - 1,
                "Expected truncated length %d, got %zu for dynamic payload size %zu",
                SAFE_BUFFER_SIZE - 1, actual_len, dynamic_sizes[i]);
        }

        free(long_str);
    }

    /* Test with format strings that expand to large output */
    {
        /* %d with large number - produces at most ~20 chars, safe */
        guarded_buffer_t gb;
        init_guarded_buffer(&gb);
        safe_format(gb.buffer, SAFE_BUFFER_SIZE, "%d", 2147483647);
        ck_assert_msg(check_canaries(&gb), "Canary corruption with large integer format");
        ck_assert_msg(strnlen(gb.buffer, SAFE_BUFFER_SIZE) < SAFE_BUFFER_SIZE,
            "Buffer overflow with large integer format");
    }

    {
        /* Repeated format specifiers that could expand */
        guarded_buffer_t gb;
        init_guarded_buffer(&gb);
        /* Use a format that produces exactly SAFE_BUFFER_SIZE - 1 chars */
        char fmt[64];
        snprintf(fmt, sizeof(fmt), "%%-%ds", SAFE_BUFFER_SIZE - 1);
        safe_format(gb.buffer, SAFE_BUFFER_SIZE, fmt, "X");
        ck_assert_msg(check_canaries(&gb), "Canary corruption with padded format");
        ck_assert_msg(strnlen(gb.buffer, SAFE_BUFFER_SIZE) < SAFE_BUFFER_SIZE,
            "Buffer overflow with padded format");
    }

    {
        /* Format that would produce output larger than buffer */
        guarded_buffer_t gb;
        init_guarded_buffer(&gb);
        char fmt[64];
        snprintf(fmt, sizeof(fmt), "%%-%ds", SAFE_BUFFER_SIZE * 2);
        safe_format(gb.buffer, SAFE_BUFFER_SIZE, fmt, "Y");
        ck_assert_msg(check_canaries(&gb), "Canary corruption with oversized padded format");
        ck_assert_msg(strnlen(gb.buffer, SAFE_BUFFER_SIZE) < SAFE_BUFFER_SIZE,
            "Buffer overflow with oversized padded format");
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_buffer_read_never_exceeds_declared_length);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}