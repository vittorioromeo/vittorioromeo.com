// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

#pragma once

#include <utility>

#ifndef FWD
#define FWD(...) ::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)
#endif

#if !defined(VR_USE_BOOST_VARIANT) && __has_include(<variant>)

#pragma message("Using std::variant")
#include <variant>

namespace vr
{
    template <typename... Ts>
    using variant = ::std::variant<Ts...>;

    template <typename... Ts>
    decltype(auto) visit(Ts&&... xs)
    {
        return ::std::visit(FWD(xs)...);
    }
}

#else

#pragma message("Using boost::variant")
#include <boost/variant.hpp>

namespace vr
{
    template <typename... Ts>
    using variant = ::boost::variant<Ts...>;

    template <typename... Ts>
    decltype(auto) visit(Ts&&... xs)
    {
        return ::boost::apply_visitor(FWD(xs)...);
    }
}

#endif

namespace vr
{
    template <typename TVisitor, typename TVariant>
    decltype(auto) visit_recursively(TVisitor&& visitor, TVariant&& variant)
    {
        return vr::visit(FWD(visitor), FWD(variant)._data);
    }
}
