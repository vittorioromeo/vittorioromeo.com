// Copyright(c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.com | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include "any_type.hpp"
#include "variant_aliases.hpp"
#include <boost/hana.hpp>
#include <experimental/type_traits>
#include <iostream>
#include <map>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

// Compatibility with MinGW:
namespace std
{
    template <class... B>
    constexpr bool disjunction_v = disjunction<B...>::value;
}

namespace deduced_arity
{
    template <std::size_t TS>
    struct deducible_t : std::integral_constant<std::size_t, TS>
    {
    };

    struct undeducible_t
    {
    };

    constexpr undeducible_t undeducible{};
    constexpr deducible_t<1> unary{};
    constexpr deducible_t<2> binary{};
}

template <typename TF, typename... Ts>
using is_unary_callable_with_any =
    std::disjunction<std::is_callable<TF(Ts)>...>;

template <typename TF, typename T>
struct is_binary_callable_with_any_lbound
{
    template <typename... Ts>
    using apply = std::disjunction<std::is_callable<TF(T, Ts)>...>;
};

template <typename TF, typename T>
struct is_binary_callable_with_any_rbound
{
    template <typename... Ts>
    using apply = std::disjunction<std::is_callable<TF(Ts, T)>...>;
};

template <typename TF, typename... Ts>
using is_binary_callable_with_any =
    std::disjunction<typename is_binary_callable_with_any_lbound<TF,
                         Ts>::template apply<Ts...>...,
        typename is_binary_callable_with_any_rbound<TF,
            Ts>::template apply<Ts...>...>;


/*
template <typename... TFs>
auto make_visitor_great_again(TFs... fs)
{
    auto unary = filter([](auto f)( return deduce_arity(f) ==
deduced_arity::unary; }, fs...);
    auto binary = filter([](auto f)( return deduce_arity(f) ==
deduced_arity::binary; }, fs...);

    auto undeducible_fns = filter([](auto f)( return deduce_arity(f) ==
deduced_arity::undeducible; }, fs...);
    static_assert
}
*/

// TODO: assume undeducible are unary? Force disambiguation?
// TODO: abstract wrapper?

namespace impl
{
    template <typename T, typename TDerived>
    struct wrapper_impl
    {
        T _data;

        template <typename... Ts,
            typename = std::enable_if_t<!std::disjunction_v<
                std::is_same<std::decay_t<Ts>, TDerived>...>>>
        wrapper_impl(Ts&&... xs) : _data{FWD(xs)...}
        {
        }
    };
}

// TODO:
struct placeholder
{
};
using _me = placeholder;

template <typename TReplacement, typename T>
struct replace_all
{
    using type =
        std::conditional_t<std::is_same_v<T, placeholder>, TReplacement, T>;
};

template <typename TReplacement, template <typename...> class TTemplate,
    typename... Ts>
struct replace_all<TReplacement, TTemplate<Ts...>>
{
    using type = TTemplate<typename replace_all<TReplacement, Ts>::type...>;
};


template <typename... Ts>
struct recursive_variant_helper
{
    struct me_wrapper;


    template <typename T>
    using sub = typename replace_all<me_wrapper, T>::type;

    using variant_type = typename replace_all<me_wrapper, vr::variant<Ts...>>::type;

    struct me_wrapper : impl::wrapper_impl<variant_type, me_wrapper>
    {
        // seems to work on libc++
        using impl::wrapper_impl<variant_type, me_wrapper>::wrapper_impl;
    };

    using type = variant_type;
};

template <typename TRV, typename T>
using resolve = typename TRV::template sub<T>;

/* template <typename... Ts>
using recursive_variant_builder =
    typename recursive_variant_helper<Ts...>::type;*/



namespace impl
{
    using varr_x = std::vector<_me>;
    using vmap_x = std::map<int, _me>;
    using rv_h =
        recursive_variant_helper<int, float, double, varr_x, long, vmap_x>;

    using vnum = rv_h::type;
    using varr = resolve<rv_h, varr_x>;
    using vmap = resolve<rv_h, vmap_x>;
}

using vnum = impl::vnum;
using impl::varr;
using impl::vmap;



using number = int;

struct plus
{
};
struct minus
{
};
using op = vr::variant<plus, minus>;

namespace impl
{
    using r_expr_x = std::tuple<number, op, _me>;
    using rv_expr_h =
        recursive_variant_helper<number, std::unique_ptr<r_expr_x>>;
}

using r_expr = resolve<impl::rv_expr_h, impl::r_expr_x>;
using expr = impl::rv_expr_h::type;



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
using is_not_overloaded =
    std::experimental::is_detected<is_not_overloaded_impl, T>;

template <typename T, typename... Ts>
using can_invoke_with_impl = decltype(std::declval<T>()(std::declval<Ts>()...));



template <typename T>
using is_binary_callable_impl = can_invoke_with_impl<T, any_type, any_type>;

template <typename T>
using is_unary_callable_impl = can_invoke_with_impl<T, any_type>;


template <typename T>
using is_binary_callable = std::is_callable<T(any_type, any_type)>;
// using is_binary_callable =
// std::experimental::is_detected<is_binary_callable_impl, T>;

template <typename T>
using is_unary_callable = std::is_callable<T(any_type)>;
// using is_unary_callable =
// std::experimental::is_detected<is_unary_callable_impl, T>;

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
        return [f = FWD(f)](auto, auto&& x)->decltype(f(FWD(x)))
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

        return [f = FWD(f)](auto, argt x)->decltype(f(x))
        {
            return f(x);
        };
    }
};

namespace vr
{
    struct recursive_tag_t
    {
    };
    constexpr recursive_tag_t recursive;

    struct non_recursive_tag_t
    {
    };
    constexpr non_recursive_tag_t non_recursive;

    template <typename TTag, typename TF>
    struct tagged_branch : TF
    {
        using tag = TTag;

        template <typename TFFwd>
        tagged_branch(TFFwd&& f) : TF(FWD(f))
        {
        }
    };

    template <typename TF>
    using recursive_visitor_branch = tagged_branch<recursive_tag_t, TF>;

    template <typename TF>
    using non_recursive_visitor_branch = tagged_branch<non_recursive_tag_t, TF>;

    template <typename T>
    auto operator|(recursive_tag_t, T&& x)
    {
        return recursive_visitor_branch<std::decay_t<T>>{x};
    }

    template <typename T>
    auto operator|(non_recursive_tag_t, T&& x)
    {
        return non_recursive_visitor_branch<std::decay_t<T>>{x};
    }
}

template <typename, template <typename...> class>
struct is_specialization_of : boost::hana::false_
{
};

template <template <typename...> class TTemplate, typename... Ts>
struct is_specialization_of<TTemplate<Ts...>, TTemplate> : boost::hana::true_
{
};


template <class T>
using has_tag = typename T::tag;

template <typename T>
using is_tagged_as_recursive =
    std::experimental::is_detected_exact<vr::recursive_tag_t, has_tag, T>;

template <typename T>
using is_tagged_as_non_recursive =
    std::experimental::is_detected_exact<vr::non_recursive_tag_t, has_tag, T>;

template <typename>
struct dependent_false : std::false_type
{
};

namespace impl
{
    template <typename... THelpers>
    struct arity_deducer_t
    {
        template <typename TF>
        constexpr auto operator()(TF&&) const
        {
            using f_type = std::decay_t<TF>;
            using is_unary_c =
                is_unary_callable_with_any<f_type, any_type, THelpers...>;
            using is_binary_c =
                is_binary_callable_with_any<f_type, any_type, THelpers...>;

            // clang-format off
            if constexpr(is_tagged_as_recursive<f_type>{})
            {
                if constexpr(is_unary_c{})
                {
                    static_assert(dependent_false<f_type>{},
                        "Marked recursive but unary callable.");
                }
                else
                {
                    return deduced_arity::binary;
                }
            }
            else if constexpr(is_tagged_as_non_recursive<f_type>{})
            {
                if constexpr(is_binary_c{})
                {
                    static_assert(dependent_false<f_type>{},
                        "Marked non-recursive but binary callable.");
                }
                else
                {
                    return deduced_arity::unary;
                }
            }
            else if constexpr(is_binary_c{})
            {
                return deduced_arity::binary;
            }
            else if constexpr(is_unary_c{})
            {
                return deduced_arity::unary;
            }
            else if constexpr(is_not_overloaded<f_type>{})
            {
                constexpr auto arity = function_traits<f_type>::arity;

                if constexpr(arity == 1)
                {
                    return deduced_arity::unary;
                }
                else if constexpr(arity == 2)
                {
                    return deduced_arity::binary;
                }
                else
                {
                    static_assert(dependent_false<f_type>{},
                        "Unsuppored arity.");
                }
            }
            else
            {
                return deduced_arity::undeducible;
            }
            // clang-format on
        }
    };
}

template <typename... THelpers>
constexpr impl::arity_deducer_t<THelpers...> arity_deducer{};

// Added single 'f' due to gcc61 segfault
template <typename TRet, typename... THelpers, typename TF, typename... TFs>
auto make_recursive_visitor(TF&& f, TFs&&... fs)
{
    namespace bh = boost::hana;


    auto fns_tuple = bh::make_tuple(FWD(f), FWD(fs)...);

    auto fns_arities = bh::transform(fns_tuple, arity_deducer<THelpers...>);


    // clang-format off
    bh::for_each(fns_arities, [](auto x)
    {
        if constexpr(std::is_same<decltype(x), deduced_arity::undeducible_t>{})
        {
            std::cout << "undeducible\n";
        }
        else
        {
            std::cout << x << "\n";
        }
    });
    // clang-format on

    auto non_recs = bh::remove_if(fns_tuple, arity_detector);

    auto recs = bh::filter(fns_tuple, arity_detector);

    auto rnrecs = bh::transform(non_recs, [](auto&& f) {
        return adapt_helper<!is_not_overloaded<decltype(f)>{}>{}(FWD(f));
    });

    auto alltpl = bh::concat(rnrecs, recs);

    auto final_overload = overload_tuple(alltpl);

    // Probably need to assert tthat every lambda returns same type
    // TODO:
    // using return_type = decltype(f(any_type{}));

    return bh::fix([fo = std::move(final_overload)](auto self, auto&& x)->TRet {
        return fo([&self](auto&& v) { return vr::visit_recursively(self, v); },
            FWD(x));
    });
}

template <typename T, typename... TFs>
decltype(auto) match(T&& v, TFs&&... fs)
{
    return vr::visit(boost::hana::overload(FWD(fs)...), FWD(v));
}


int main()
{
    auto k = vr::recursive | [] {};
    static_assert(
        is_tagged_as_recursive<vr::recursive_visitor_branch<decltype(k)>>{});

    // clang-format off
    auto vnp = make_recursive_visitor<void, long> // returns void, `long` helps deduction
    (
        [](auto x) -> std::enable_if_t<std::is_arithmetic<std::decay_t<decltype(x)>>{}> { std::cout << x << "N\n"; },
        [](int x)    { std::cout << x << "i\n"; },
        [](float x)  { std::cout << x << "f\n"; },
        [](double x) { std::cout << x << "d\n"; },
        [](auto recurse, const varr& x) { for(const auto& y : x) recurse(y); },
        [](auto recurse, const vmap& x) { for(const auto& y : x) recurse(y.second); }
    );
    // clang-format on

    /*
    #define MAKE_RECURSIVE_VISITOR(return_type, ...) \
        make_recursive_visitor<return_type>([](auto overload, auto recurse) \
        { \
            return overload \
            ( \
                __VA_ARGS__ \
            ); \
        }
    */

    /*
    // Alternative:
    auto vnp = make_recursive_visitor<void>([](auto overload, auto recurse)
    {
        return overload
        (
            [](auto x) ->
    std::enable_if_t<std::is_arithmetic<std::decay_t<decltype(x)>>{}> {
    std::cout << x << "N\n"; },
            [](int x)          { std::cout << x << "i\n"; },
            [](float x)        { std::cout << x << "f\n"; },
            [](double x)       { std::cout << x << "d\n"; },
            [&](const varr& x) { for(const auto& y : x) recurse(y); },
            [&](const vmap& x) { for(const auto& y : x) recurse(y.second); }
        );
    });
    */

    vnum v0{0};
    vr::visit(vnp, v0);

    v0 = 5.f;
    vr::visit(vnp, v0);

    v0 = 33.51;
    vr::visit(vnp, v0);

    v0 = 500l;
    vr::visit(vnp, v0);

    v0 = varr{vnum{1}, vnum{2.0}, vnum{3.f}};
    vr::visit(vnp, v0);

    v0 = varr{vnum{5}, varr{vnum{7}, vnum{8.0}, vnum{9.}}, vnum{4.f}};
    vr::visit(vnp, v0);

    v0 = varr{vnum{1}, vmap{{1, vnum{2.0}}}, vnum{3.f}};
    vr::visit(vnp, v0);

    std::cout << "\n\n\n\n";

    auto evaluate = make_recursive_visitor<int>( // .
        [](int x) { return x; },                 // .
        [](auto recurse, const std::unique_ptr<r_expr>& x) {
            const auto& lhs = std::get<0>(*x);
            const auto& op = std::get<1>(*x);
            const auto& rhs = std::get<2>(*x);
            return match(op, [&](plus) { return lhs + recurse(rhs); },
                [&](minus) { return lhs - recurse(rhs); });
        } // .
        );

    using std::make_unique;
    expr e0{5};
    expr e1{make_unique<r_expr>(9, plus{}, 3)};
    expr e2{make_unique<r_expr>(1, minus{}, make_unique<r_expr>(3, plus{}, 7))};

    std::cout << vr::visit(evaluate, e0) << "\n";
    std::cout << vr::visit(evaluate, e1) << "\n";
    std::cout << vr::visit(evaluate, e2) << "\n";
}
