// Copyright(c)2015 - 2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include <iostream>
#include <vector>
#include <type_traits>
#include <experimental/type_traits>
#include <tuple>
#include <boost/hana.hpp>
#include "variant_aliases.hpp"

namespace impl
{
    struct vnum_wrapper;

    using varr = std::vector<vnum_wrapper>;
    using vnum = vr::variant<int, float, double, varr>;

    struct vnum_wrapper
    {
        vnum _data;

        template <typename... Ts>
        vnum_wrapper(Ts&&... xs)
            : _data{FWD(xs)...}
        {
        }
    };
}

using vnum = impl::vnum;
using impl::varr;

template <typename TTpl>
auto overload_tuple(TTpl&& x)
{
    return boost::hana::unpack(FWD(x), boost::hana::overload);
}

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())>
{
};
// For generic types, directly use the result of the signature of its
// 'operator()'

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const>
// we specialize for pointers to member function
{
    enum
    {
        arity = sizeof...(Args)
    };
    // arity is the number of arguments.

    typedef ReturnType result_type;

    template <size_t i>
    struct arg
    {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
    };
};

struct any_type
{
    template <typename... Ts>
    constexpr auto operator()(Ts&&...){}

    template <typename T>
    constexpr operator T() noexcept
    {
        return {};
    }
};

template<class T>
using callable2 = decltype(std::declval<T>()(any_type{}, any_type{}));


struct arity_detector_t
{
    template <typename TF>
    constexpr auto operator()(TF&&) const
    {
        return std::experimental::is_detected<callable2, TF>{};
    }
};

constexpr arity_detector_t arity_detector;

// Added single 'f' due to gcc61 segfault
template <typename TRet, typename TF, typename... TFs>
auto make_recursive_visitor(TF&& f, TFs&&... fs)
{
    namespace bh = boost::hana;

    auto fns_tuple = bh::make_tuple(FWD(f), FWD(fs)...);

    auto non_recs = bh::remove_if(fns_tuple, arity_detector);

    auto recs = bh::filter(fns_tuple, arity_detector);

    auto rnrecs = bh::transform(non_recs, [](auto&& f)
        {
           // using argt = typename function_traits<
           //    std::decay_t<decltype(f)>>::template arg<0>::type;

            return [&f](auto, auto x)
            {
                return f(FWD(x));
            };
        });

    auto alltpl = bh::concat(rnrecs, recs);

    auto final_overload = overload_tuple(alltpl);

    // Probably need to assert tthat every lambda returns same type
    // TODO:
    // using return_type = decltype(f(any_type{}));

    return bh::fix([fo = std::move(final_overload)](auto self, auto&& x)
                       ->TRet
                   {
                       return fo(
                           [&self](auto&& v)
                           {
                               return vr::visit_recursively(self, v);
                           },
                           FWD(x));
                   });
}


int main()
{
    // clang-format off
    auto vnp = make_recursive_visitor<void>
    (
     //   [](auto x) -> std::enable_if_t<std::is_arithmetic<std::decay_t<decltype(x)>>{}>  { std::cout << x << "N\n"; },
         [](int x)    { std::cout << x << "i\n"; },
         [](float x)  { std::cout << x << "f\n"; },
         [](double x) { std::cout << x << "d\n"; },
        [](auto self, const varr& x) { for(const auto& y : x) self(y); }
    );
    // clang-format on

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
