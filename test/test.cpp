#include <unity.h>
#include "../include/ChangeName.h"

void test_function_calculator_addition(void) {
    TEST_ASSERT_EQUAL(32, 30+2);
}

void test_function_calculator_addition2(void) {
    TEST_ASSERT_EQUAL(32, 30+1);
}



int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_function_calculator_addition);
    RUN_TEST(test_function_calculator_addition2);
    UNITY_END();

    return 0;
}