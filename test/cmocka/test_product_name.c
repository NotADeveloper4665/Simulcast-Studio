#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "product-name.h"

static void fallback_build_uses_base_version(void **state)
{
	(void)state;
	char buf[64];

	assert_int_equal(obs_product_string(buf, sizeof(buf), "32.1.2", NULL), (int)strlen("Simulcast Studio 32.1.2"));
	assert_string_equal(buf, "Simulcast Studio 32.1.2");
}

static void release_build_uses_product_version(void **state)
{
	(void)state;
	char buf[96];
	const char *want = "Simulcast Studio 0.3.0";

	assert_int_equal(obs_product_string(buf, sizeof(buf), "32.1.2", "0.3.0"), (int)strlen(want));
	assert_string_equal(buf, want);
}

static void truncates_and_terminates(void **state)
{
	(void)state;
	char buf[8];

	assert_int_equal(obs_product_string(buf, sizeof(buf), "32.1.2", "0.3.0"),
			 (int)strlen("Simulcast Studio 0.3.0"));
	assert_string_equal(buf, "Simulca");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(fallback_build_uses_base_version),
		cmocka_unit_test(release_build_uses_product_version),
		cmocka_unit_test(truncates_and_terminates),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
