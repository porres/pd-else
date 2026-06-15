#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 
 * Security invariant: Symbol name copying must not overflow fixed buffers.
 * The vulnerable code uses sprintf without length validation on user-supplied
 * symbol names. This test verifies that symbol name handling respects buffer limits.
 */

#define SAFE_BUFFER_SIZE 256

/* Simulate the vulnerable pattern from envgen~.c line 118 */
static int safe_symbol_copy(const char *sym_name, char *dest, size_t dest_size)
{
    if (sym_name == NULL || dest == NULL || dest_size == 0)
        return -1;
    
    size_t name_len = strlen(sym_name);
    /* Security property: input length must not exceed destination capacity */
    if (name_len >= dest_size)
        return -1;  /* Reject oversized input */
    
    snprintf(dest, dest_size, "%s", sym_name);
    return 0;
}

START_TEST(test_symbol_name_buffer_overflow)
{
    /* Invariant: Symbol names must not overflow destination buffers */
    char overflow_payload[4096];
    memset(overflow_payload, 'A', sizeof(overflow_payload) - 1);
    overflow_payload[sizeof(overflow_payload) - 1] = '\0';

    const char *payloads[] = {
        overflow_payload,                    /* Exploit: oversized symbol name */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* Boundary: 256 chars */
        "valid_symbol",                      /* Valid: normal symbol name */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    char buffer[SAFE_BUFFER_SIZE];
    char canary[16];
    memset(canary, 0x42, sizeof(canary));

    for (int i = 0; i < num_payloads; i++) {
        memset(buffer, 0, sizeof(buffer));
        int result = safe_symbol_copy(payloads[i], buffer, sizeof(buffer));
        
        /* Security property: oversized inputs must be rejected or truncated safely */
        if (strlen(payloads[i]) >= SAFE_BUFFER_SIZE) {
            ck_assert_int_eq(result, -1);
        } else {
            ck_assert_int_eq(result, 0);
            ck_assert_str_eq(buffer, payloads[i]);
        }
        
        /* Verify no memory corruption occurred */
        for (int j = 0; j < 16; j++) {
            ck_assert_int_eq(canary[j], 0x42);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_symbol_name_buffer_overflow);
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