#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace megacitycode::tests
{

class TestSkipped : public std::runtime_error
{
public:
    explicit TestSkipped(std::string message)
        : std::runtime_error(std::move(message))
    {
    }
};

inline void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string(message));
    }
}

[[noreturn]] inline void skip(std::string_view message)
{
    throw TestSkipped(std::string(message));
}

template <typename T, typename U>
inline void expect_eq(const T& actual, const U& expected, std::string_view message)
{
    if (!(actual == expected))
    {
        throw std::runtime_error(std::string(message));
    }
}

template <typename Fn>
inline void run_test(std::string_view name, Fn&& fn)
{
    try
    {
        fn();
        std::cout << "[ok] " << name << '\n';
    }
    catch (const TestSkipped& ex)
    {
        std::cout << "[skip] " << name << ": " << ex.what() << '\n';
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string(name) + ": " + ex.what());
    }
}

} // namespace megacitycode::tests
