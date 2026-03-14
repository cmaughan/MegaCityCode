#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace spectre::tests
{

inline void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string(message));
    }
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
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string(name) + ": " + ex.what());
    }
}

} // namespace spectre::tests
