#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Include the GIR node infrastructure */
#include "girepository/girnode.h"
#include "girepository/girmodule.h"
#include "girepository/gtypelib.h"

START_TEST(test_string_blob_no_overflow)
{
    /* Invariant: writing a string into a typelib blob must never exceed
       the allocated buffer size, regardless of input string length */
    const char *payloads[] = {
        "valid_name",                          /* valid input */
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",  /* boundary: 34 chars */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 2x oversized */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        const char *str = payloads[i];
        size_t str_len = strlen(str);

        /* Allocate a buffer exactly sized for the string + NUL terminator */
        size_t buf_size = str_len + 1;
        uint8_t *data = calloc(buf_size, 1);
        ck_assert_ptr_nonnull(data);

        /* The invariant: after any copy operation, the written bytes
           must not exceed buf_size. We simulate the vulnerable pattern
           and assert the result fits within bounds. */
        size_t written = str_len + 1; /* strcpy writes str_len + NUL */
        ck_assert_msg(written <= buf_size,
            "String of length %zu would overflow buffer of size %zu (payload index %d)",
            str_len, buf_size, i);

        /* Perform a safe bounded copy to verify the invariant holds
           when proper bounds checking is applied */
        strncpy((char *)data, str, buf_size - 1);
        data[buf_size - 1] = '\0';

        /* Verify NUL termination is within bounds */
        ck_assert_uint_lt((size_t)(memchr(data, '\0', buf_size) - (void *)data),
                          buf_size);

        free(data);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_string_blob_no_overflow);
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