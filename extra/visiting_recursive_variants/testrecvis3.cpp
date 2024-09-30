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
#include <map>
#include <boost/hana.hpp>
#include "variant_aliases.hpp"

namespace impl
{
    struct vnum_wrapper;

    using varr = std::vector<vnum_wrapper>;
    using vmap = std::map<int, vnum_wrapper>;
    using vnum = vr::variant<int, float, double, varr, long, vmap>;

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
using impl::vmap;

template <typename TTpl>
auto overload_tuple(TTpl&& x)
{
    return boost::hana::unpack(FWD(x), boost::hana::overload);
}

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())>
{
};

template <typename T, typename TReturn, typename... TArgs>
struct function_traits<TReturn (T::*)(TArgs...) const>
{
    static constexpr std::size_t arity = sizeof...(TArgs);

    using result_type = TReturn;

    template <std::size_t TI>
    using arg = std::tuple_element_t<TI, std::tuple<TArgs...>>;
};

template <typename T>
using is_not_overloaded_impl = decltype(&std::decay_t<T>::operator());

template <typename T>
using is_not_overloaded = std::experimental::is_detected<is_not_overloaded_impl, T>;



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
using is_binary_callable_impl = decltype(std::declval<T>()(any_type{}, any_type{}));

template <typename T>
using is_binary_callable = std::experimental::is_detected<is_binary_callable_impl, T>;

struct arity_detector_t
{
    template <typename TF>
    constexpr auto operator()(TF&&) const
    {
        return is_binary_callable<TF>{};
    }
};

constexpr arity_detector_t arity_detector;

template <bool TOverloaded>
struct adapt_helper;

template <>
struct adapt_helper<true>
{
    template <typename TF>
    auto operator()(TF&& f)
    {
        return [f = FWD(f)](auto, auto&& x) -> decltype(f(FWD(x)))
        {
            return f(FWD(x));
        };
    }
};

template <>
struct adapt_helper<false>
{
    template <typename TF>
    auto operator()(TF&& f)
    {
        using argt = typename function_traits<
            std::decay_t<decltype(f)>>::template arg<0>;

        return [f = FWD(f)](auto, argt x) -> decltype(f(x))
        {
            return f(x);
        };
    }
};

// Added single 'f' due to gcc61 segfault
template <typename TRet, typename TF, typename... TFs>
auto make_recursive_visitor(TF&& f, TFs&&... fs)
{
    namespace bh = boost::hana;

    auto fns_tuple = bh::make_tuple(FWD(f), FWD(fs)...);

    (std::cout << (arity_detector(FWD(f)) ? "binary" : "unary"));
    std::cout << "\n";
    (std::cout << ... << (arity_detector(FWD(fs)) ? "binary" : "unary"));
    std::cout << "\n";

    auto non_recs = bh::remove_if(fns_tuple, arity_detector);

    auto recs = bh::filter(fns_tuple, arity_detector);

    auto rnrecs = bh::transform(non_recs, [](auto&& f)
        {
           return adapt_helper<!is_not_overloaded<decltype(f)>{}>{}(FWD(f));
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


struct foo { void f() { } };
struct bar { void f() { } };
struct xyz { void f() { } };
using var = vr::variant<foo, bar, xyz>;



int main()
{
    auto vis = make_recursive_visitor<void>
    (
        [](auto x) {  x.f(); },
        [](auto, bar x) { x.f(); }
    );

    var v; vr::visit(vis, v);
}
