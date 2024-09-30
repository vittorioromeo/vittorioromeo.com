// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include <iostream>
#include <vector>
#include "variant_aliases.hpp"

struct vnum
{
    using varr = std::vector<vnum>;
    vr::variant<int, float, double, varr> _data;

    template <typename... Ts>
    vnum(Ts&&... xs)
        : _data{FWD(xs)...}
    {
    }
};

using varr = vnum::varr;

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
           // vr::visit(*this, x);
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

    v0 = varr{vnum{5}, varr{vnum{7}, vnum{8.0}, vnum{9.}}, vnum{4.f}};
    vr::visit(vnp, v0);
}
