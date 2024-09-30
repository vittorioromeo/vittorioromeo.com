// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include <iostream>
#include "variant_aliases.hpp"

using vnum = vr::variant<int, float, double>;

// clang-format off
struct vnum_printer
{
    void operator()(int x) const    { std::cout << x << "i\n"; }
    void operator()(float x) const  { std::cout << x << "f\n"; }
    void operator()(double x) const { std::cout << x << "d\n"; }
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
}
