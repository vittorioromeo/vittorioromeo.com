// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include <iostream>
#include <boost/hana.hpp>
#include "variant_aliases.hpp"

// TODO:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0051r1.pdf

using vnum = vr::variant<int, float, double>;

template <typename... TFs>
auto make_visitor(TFs&&... fs)
{
    return boost::hana::overload(FWD(fs)...);
}

int main()
{
    // clang-format off
    auto vnp = make_visitor
    (
        [](int x)    { std::cout << x << "i\n"; },
        [](float x)  { std::cout << x << "f\n"; },
        [](double x) { std::cout << x << "d\n"; }
    );
    // clang-format on

    vnum v0{0};
    vr::visit(vnp, v0);

    v0 = 5.f;
    vr::visit(vnp, v0);

    v0 = 33.51;
    vr::visit(vnp, v0);
}
