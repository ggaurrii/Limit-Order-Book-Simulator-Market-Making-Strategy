#ifndef TEST_RUNNER_HPP
#define TEST_RUNNER_HPP

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <cmath>

namespace test_framework {

struct TestCase {
    std::string suite_name;
    std::string test_name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline bool register_test(const std::string& suite, const std::string& name, std::function<void()> func) {
    get_tests().push_back(TestCase{suite, name, func});
    return true;
}

struct TestContext {
    int passed = 0;
    int failed = 0;
    bool current_test_passed = true;
};

inline TestContext& get_context() {
    static TestContext ctx;
    return ctx;
}

inline void assert_true(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        get_context().current_test_passed = false;
        std::cerr << "  [FAILED] " << file << ":" << line << " -> Expression (" << expr << ") evaluated to FALSE\n";
    }
}

template<typename T, typename U>
inline void assert_eq(const T& actual, const U& expected, const char* act_expr, const char* exp_expr, const char* file, int line) {
    if (!(actual == expected)) {
        get_context().current_test_passed = false;
        std::cerr << "  [FAILED] " << file << ":" << line << " -> Expected " << exp_expr << " (" << expected << "), but got " << act_expr << " (" << actual << ")\n";
    }
}

inline void assert_double_eq(double actual, double expected, double eps, const char* act_expr, const char* exp_expr, const char* file, int line) {
    if (std::abs(actual - expected) > eps) {
        get_context().current_test_passed = false;
        std::cerr << "  [FAILED] " << file << ":" << line << " -> Expected " << exp_expr << " (" << expected << "), but got " << act_expr << " (" << actual << ")\n";
    }
}

inline int run_all_tests() {
    auto& tests = get_tests();
    auto& ctx = get_context();
    ctx.passed = 0;
    ctx.failed = 0;

    std::cout << "\n================ RUNNING " << tests.size() << " UNIT TESTS ================\n";
    for (const auto& t : tests) {
        ctx.current_test_passed = true;
        std::cout << "[RUN      ] " << t.suite_name << "." << t.test_name << "\n";
        try {
            t.func();
        } catch (const std::exception& ex) {
            ctx.current_test_passed = false;
            std::cerr << "  [EXCEPTION] Caught std::exception: " << ex.what() << "\n";
        } catch (...) {
            ctx.current_test_passed = false;
            std::cerr << "  [EXCEPTION] Caught unknown exception\n";
        }

        if (ctx.current_test_passed) {
            ctx.passed++;
            std::cout << "[       OK] " << t.suite_name << "." << t.test_name << "\n";
        } else {
            ctx.failed++;
            std::cout << "[  FAILED  ] " << t.suite_name << "." << t.test_name << "\n";
        }
    }
    std::cout << "=========================================================\n";
    std::cout << "SUMMARY: " << ctx.passed << " PASSED, " << ctx.failed << " FAILED.\n";
    std::cout << "=========================================================\n\n";

    return (ctx.failed == 0) ? 0 : 1;
}

} // namespace test_framework

#define TEST(suite, name) \
    void suite##_##name(); \
    static bool suite##_##name##_registered = ::test_framework::register_test(#suite, #name, suite##_##name); \
    void suite##_##name()

#define EXPECT_TRUE(cond) ::test_framework::assert_true((cond), #cond, __FILE__, __LINE__)
#define EXPECT_FALSE(cond) ::test_framework::assert_true(!(cond), "!(" #cond ")", __FILE__, __LINE__)
#define EXPECT_EQ(act, exp) ::test_framework::assert_eq((act), (exp), #act, #exp, __FILE__, __LINE__)
#define EXPECT_DOUBLE_EQ(act, exp) ::test_framework::assert_double_eq((act), (exp), 1e-6, #act, #exp, __FILE__, __LINE__)

#endif // TEST_RUNNER_HPP
