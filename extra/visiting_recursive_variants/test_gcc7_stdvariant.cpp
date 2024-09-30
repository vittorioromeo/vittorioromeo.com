// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include <iostream>
#include <vector>
#include "variant_aliases.hpp"

template <typename T>
struct recursion_helper
{
    T _v;

    template<typename... Ts>
    recursion_helper(Ts&&... xs) : _v(FWD(xs)...) { }

    operator T&() & { return _v; }
    operator const T&() const& { return _v; }
    operator T() && { return std::move(_v); }

    auto& as_inner() & { return _v; }
    const auto& as_inner() const& { return _v; }
    auto as_inner() && { return std::move(_v); }
};

namespace impl
{
    struct vnum_wrapper;

    using varr = std::vector<vnum_wrapper>;
    using vnum = vr::variant<int, float, double, varr>;

    struct vnum_wrapper : public recursion_helper<vnum>
    {
        using recursion_helper::recursion_helper;
    };
}

template <typename TVisitor, typename TVariantWrapper>
decltype(auto) visit_recursively(TVisitor&& visitor, TVariantWrapper&& variant_wrapper)
{
    return vr::visit(visitor, FWD(variant_wrapper).as_inner());
}

using vnum = impl::vnum;
using impl::varr;

// clang-format off
struct vnum_printer
{
    void operator()(int x) const    { std::cout << x << "i\n"; }
    void operator()(float x) const  { std::cout << x << "f\n"; }
    void operator()(double x) const { std::cout << x << "d\n"; }

    void operator()(const varr& arr)
    {
        for(const auto& x : arr)
        {
            visit_recursively(*this, x);
        }
    }
};
// clang-format on

int main()
{
    vnum_printer vnp;

    vnum v0{0};
    vr::visit(vnp, v0);

    v0 = 5.f;
    vr::visit(vnp, v0);

    v0 = 33.51;
    vr::visit(vnp, v0);

    v0 = varr{vnum{1}, vnum{2.0}, vnum{3.f}};
    vr::visit(vnp, v0);

}
