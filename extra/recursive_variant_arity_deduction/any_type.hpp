// Copyright(c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

#pragma once

// clang-format off
struct any_type
{
    constexpr any_type() noexcept { }
    constexpr any_type(const any_type&) noexcept { }

    template <typename... Ts>
    constexpr any_type(Ts&&...) noexcept { }

    constexpr any_type& operator=(const any_type&) noexcept { return *this; }

    template <typename T>
    constexpr any_type& operator=(T&&) noexcept { return *this; }

    template <typename T>
    constexpr operator T&() const noexcept;

    template <typename T>
    constexpr operator T &&() const noexcept;

    template <typename T>
    constexpr operator T const&() const noexcept;

    template <typename T>
    constexpr operator T const &&() const noexcept;

    template <typename... Ts>
    constexpr any_type operator()(Ts&&...) const noexcept { return {}; }

    template <typename T>
    constexpr any_type operator->*(T&&) noexcept { return {}; }

    template <typename T>
    constexpr any_type operator[](T&&) noexcept { return {}; }

    template <typename T>
    constexpr any_type operator,(T&&) noexcept { return {}; }

    constexpr any_type operator~() const noexcept { return {}; }
    constexpr any_type operator!() const noexcept { return {}; }

    constexpr any_type operator*() const noexcept { return {}; }
    constexpr any_type operator->() const noexcept { return {}; }
    constexpr any_type operator++() const noexcept { return {}; }
    constexpr any_type operator++(int)const noexcept { return {}; }
    constexpr any_type operator--() const noexcept { return {}; }
    constexpr any_type operator--(int)const noexcept { return {}; }

    constexpr any_type begin() const noexcept { return {}; }
    constexpr any_type end() const noexcept { return {}; }
    constexpr any_type cbegin() const noexcept { return {}; }
    constexpr any_type cend() const noexcept { return {}; }
    constexpr any_type rbegin() const noexcept { return {}; }
    constexpr any_type rend() const noexcept { return {}; }
};

constexpr any_type operator+(any_type, any_type) noexcept { return {}; }
constexpr any_type operator-(any_type, any_type) noexcept { return {}; }
constexpr any_type operator*(any_type, any_type) noexcept { return {}; }
constexpr any_type operator/(any_type, any_type) noexcept { return {}; }
constexpr any_type operator%(any_type, any_type) noexcept { return {}; }
constexpr any_type operator^(any_type, any_type) noexcept { return {}; }
constexpr any_type operator&(any_type, any_type) noexcept { return {}; }
constexpr any_type operator|(any_type, any_type) noexcept { return {}; }
constexpr any_type operator<(any_type, any_type) noexcept { return {}; }
constexpr any_type operator>(any_type, any_type) noexcept { return {}; }
constexpr any_type operator+=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator-=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator*=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator/=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator%=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator^=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator&=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator|=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator<<(any_type, any_type) noexcept { return {}; }
constexpr any_type operator>>(any_type, any_type) noexcept { return {}; }
constexpr any_type operator>>=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator<<=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator==(any_type, any_type) noexcept { return {}; }
constexpr any_type operator!=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator<=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator>=(any_type, any_type) noexcept { return {}; }
constexpr any_type operator&&(any_type, any_type) noexcept { return {}; }
constexpr any_type operator||(any_type, any_type) noexcept { return {}; }
// clang-format on

#include <cstdint>
#include <cstddef>

namespace std
{
    template <std::size_t>
    constexpr any_type get(any_type)
    {
        return {};
    }
}
