// test_utils.hpp
#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP

#include <cstdio>
#include <cstdlib>

#define C_RESET  "\033[0m"
#define C_GREEN  "\033[32m"
#define C_RED    "\033[31m"
#define C_YELLOW "\033[33m"
#define C_CYAN   "\033[36m"

// aborts immediately on failure to prevent cascading errors
#define ASSERT(cond, msg) \
    do { \
        if (__builtin_expect(!(cond), 0)) { \
            printf(C_RED "[FAIL]" C_RESET " assertion failed: %s\n", #cond); \
            printf("       message:  %s\n", msg); \
            printf("       location: %s:%d\n", __FILE__, __LINE__); \
            std::exit(1); \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf(C_YELLOW "[*]" C_RESET " running %-30s ---\n", #test_func); \
        test_func(); \
        printf(C_GREEN "[PASS]" C_RESET " %s\n", #test_func); \
    } while(0)

#define LOG_INFO(...)  do { printf(C_CYAN "[INFO] " C_RESET); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOG_ERROR(...) do { printf(C_RED "[ERROR] " C_RESET); printf(__VA_ARGS__); printf("\n"); } while(0)

#endif // TEST_UTILS_HPP
