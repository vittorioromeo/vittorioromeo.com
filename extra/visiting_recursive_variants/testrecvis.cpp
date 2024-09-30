// Copyright(c)2015 - 2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include <iostream>
#include <vector>
#include <type_traits>
#include <boost/hana.hpp>
#include "variant_aliases.hpp"

namespace impl
{
    struct vnum_wrapper;

    using varr = std::vector<vnum_wrapper>;
    using vnum = vr::variant<int, float, double, varr>;

    struct vnum_wrapper : public vnum
    {
        using vnum::vnum;
    };
}

using vnum = impl::vnum_wrapper;
using impl::varr;

template <typename TF>
struct recurse_wrapper : public TF
{
    template <typename TFFwd>
    recurse_wrapper(TFFwd&& f)
        : TF(FWD(f))
    {
    }
};

template <typename, template <typename...> class>
struct is_specialization_of : boost::hana::false_
{
};

template <template <typename...> class TTemplate, typename... Ts>
struct is_specialization_of<TTemplate<Ts...>, TTemplate> : boost::hana::true_
{
};

namespace impl
{
    struct is_recurse_wrapper_t
    {
        template <typename T>
        constexpr auto operator()(const T&) const
        {
            return is_specialization_of<T, recurse_wrapper>{};
        }
    };
}

constexpr impl::is_recurse_wrapper_t is_recurse_wrapper;

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
    template <typename T>
    constexpr operator T() noexcept
    {
        return {};
    }
};

// Added single 'f' due to gcc61 segfault
template <typename TF, typename... TFs>
auto make_recursive_visitor(TF&& f, TFs&&... fs)
{
    namespace bh = boost::hana;

    auto fns_tuple = bh::make_tuple(FWD(f), FWD(fs)...);

    auto non_recs = bh::remove_if(fns_tuple, is_recurse_wrapper);

    auto recs = bh::filter(fns_tuple, is_recurse_wrapper);

    auto rnrecs = bh::transform(non_recs, [](auto&& f)
        {
            using argt = typename function_traits<
                std::decay_t<decltype(f)>>::template arg<0>::type;

            return [&f](auto, argt x)
            {
                return f(FWD(x));
            };
        });

    auto alltpl = bh::concat(rnrecs, recs);

    auto final_overload = overload_tuple(alltpl);

    // Probably need to assert tthat every lambda returns same type
    using return_type = std::common_type_t<decltype(f(any_type{})),
        decltype(fs(any_type{}))...>;

    return bh::fix([fo = std::move(final_overload)](auto self, auto&& x)
                       ->return_type
                   {
                       return fo(
                           [&self](auto&& v)
                           {
                               return vr::visit(self, v);
                           },
                           FWD(x));
                   });
}

template <typename TF>
auto recurse(TF&& f)
{
    return recurse_wrapper<TF>{FWD(f)};
}

int main()
{
    // clang-format off
    auto vnp = make_recursive_visitor
    (
        [](int x)    { std::cout << x << "i\n"; },
        [](float x)  { std::cout << x << "f\n"; },
        [](double x) { std::cout << x << "d\n"; },

        recurse([](auto visit_self, const varr& arr)
        {
            for(const auto& x : arr)
            {
                visit_self(x);
            }
        })
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
