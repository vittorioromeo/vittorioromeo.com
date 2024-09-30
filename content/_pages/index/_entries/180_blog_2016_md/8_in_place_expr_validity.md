


When writing generic code, it is sometimes useful to check whether or not **a particular SFINAE-friendly expression is valid** *(e.g. to branch at compile-time)*. Let's assume that we have the following class declarations...

```cpp
struct Cat
{
    void meow() const { cout << "meow\n"; }
};

struct Dog
{
    void bark() const { cout << "bark\n"; }
};
```

...and that we would like to write a template function `make_noise(x)` that calls `x.meow()` and/or `x.bark()` if they are well-formed expressions:

```cpp
template <typename T>
void make_noise(const T& x)
{
    // Pseudocode:
    /*
        if(`x.meow()` is well-formed)
        {
            execute `x.meow();`
        }
        else if(`x.bark()` is well-formed)
        {
            execute `x.bark();`
        }
        else
        {
            compile-time error
        }
    */
}
```

<p></p>

In this article I'll show how to implement the *pseudocode* in:

* **C++11**: using [`std::void_t`](http://en.cppreference.com/w/cpp/types/void_t) and [`std::enable_if`](http://en.cppreference.com/w/cpp/types/enable_if).

* **C++14**: using [`boost::hana::is_valid`](http://www.boost.org/doc/libs/1_62_0/libs/hana/doc/html/structboost_1_1hana_1_1type.html#a2d2e7e08e284f7e0bd1bd9c3ad0e0a2b) and [`vrm::core::static_if`](https://github.com/SuperV1234/vrm_core/blob/master/include/vrm/core/static_if/static_if.hpp).

* **C++17**: using [`if constexpr(...)`](http://en.cppreference.com/w/cpp/language/if#Constexpr_If), [`constexpr` lambdas](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4487.pdf), and [std::is_callable](http://en.cppreference.com/w/cpp/types/is_callable). This version will allow expression validity to be **checked in-place** *(i.e. directly in the `if constexpr` predicate)*. [Variadic preprocessor macros](http://en.cppreference.com/w/cpp/preprocessor/replace) will also be used to make the user code easier to read and maintain.

*(Note: if you are familiar with C++11 and C++14 techniques for expression validity detection, you can [**directly skip to the in-place C++17 detection technique.**](#cpp17_impl))*



### C++11 implementation

The technique that will be used in the C++11 implementation consists of combining [`std::void_t`](http://en.cppreference.com/w/cpp/types/void_t) with [`std::enable_if`](http://en.cppreference.com/w/cpp/types/enable_if) - this allows us to detect *ill-formed* expressions in SFINAE contexts. I first heard about this method by attending the excellent ["Modern Template Metaprogramming: A Compendium"](https://www.youtube.com/watch?v=a0FliKwcwXE) talk by [*Walter E. Brown*](https://cppcon2014.sched.org/speaker/walter_e_brown) at CppCon 2014 - I remember being *mind-blown* after the talk!

Let's begin by implementing `void_t`, which is only part of the standard since C++17:

```cpp
template <typename...>
using void_t = void;
```

*(Note: as [/u/bluescarni](https://www.reddit.com/user/bluescarni) mentioned [on reddit](https://www.reddit.com/r/cpp/comments/5b0bvh/checking_expression_validity_inplace_with_c17/), you may need [a workaround](http://en.cppreference.com/w/cpp/types/void_t) when compiling in C++11 mode due to [CWG 1558](http://open-std.org/JTC1/SC22/WG21/docs/cwg_defects.html#1558).)*

Yeah, it's *that* simple. In order to use `void_t` to detect expression validity, [*partial template specialization*](http://en.cppreference.com/w/cpp/language/partial_specialization) must be used - here's a detector class for `meow`:

```cpp
template <typename, typename = void>
struct has_meow : std::false_type { };

template <typename T>
struct has_meow<T, void_t<decltype(std::declval<T>().meow())>>
    : std::true_type { };
```

The way this idiom works is not very complex: roughly speaking, instantiating `has_meow<T>` will attempt to evaluate `void_t<decltype(std::declval<T>().meow())>`.

* If `declval<T>().meow()` is *well-formed*, `void_t<decltype(declval<T>().meow())>>` will evaluate to `void`, and `has_meow`'s `std::true_type` specialization will be taken.

* If `declval<T>().meow()` is *ill-formed*, `void_t<decltype(declval<T>().meow())>>` will be *ill-formed* as well, thus removing the `std::true_type` specialization thanks to SFINAE - all that's left is the `std::false_type` specialization.

[`std::declval<T>()`](http://en.cppreference.com/w/cpp/utility/declval) is being used in place of `T{}` because it is not guaranteed that `T` is default-constructible.

After defining the `has_bark` detector class *(which is trivial to implement, as well)*, all that's left to do is use `std::enable_if` to constrain `make_noise`:

```cpp
template <typename T>
auto make_noise(const T& x)
    -> typename std::enable_if<has_meow<T>{}>::type
{
    x.meow();
}

template <typename T>
auto make_noise(const T& x)
    -> typename std::enable_if<has_bark<T>{}>::type
{
    x.bark();
}
```

That's it for C++11! [*You can find a complete example on GitHub.*](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/in_place_expr_validity/0_cpp11.cpp)

*(Note: for more `void_t` goodness, check out [the detection idiom](http://en.cppreference.com/w/cpp/experimental/is_detected).)*



### C++14 implementation

There are some annoyances in the C++11 implementation:

* A detector class has to be defined for every expression we want to check. The class cannot be defined locally.

* `std::enable_if` has to be used to constrain multiple versions of the same function. It is not possible to *"branch"* locally at compile-time.

Both those issues can be solved thanks to one of my favorite features introduced in C++14: [**generic lambdas**](https://en.wikipedia.org/wiki/C%2B%2B14#Generic_lambdas).

Since generic lambdas are [*"templates in disguise"*](http://stackoverflow.com/questions/17233547/how-does-generic-lambda-work-in-c14), they provide a SFINAE-friendly context. Therefore, we can create an `is_valid` function that allows us to take advantage of the previously seen `void_t` detection idiom without explicitly having to create a new `struct`:

```cpp
auto has_meow = is_valid([](auto&& x) -> decltype(x.meow()){});
static_assert(has_meow(Cat{}), "");
static_assert(!has_bark(Cat{}), "");
```

As you can see, `has_meow` can be locally instantiated in any scope, and can be used to check expression validity with a nicer syntax.

> How does it work?

It's not complicated - `is_valid` is a simple `constexpr` function that takes a callable object of type `TF` and returns a `validity_checker<TF>` instance.

```cpp
template <typename TF>
constexpr auto is_valid(TF)
{
    return validity_checker<TF>{};
}
```

The `validity_checker` class's `operator()` is a `constexpr` variadic template that checks whether or not `TF` is callable with the given arguments.

```cpp
template <typename TF>
struct validity_checker
{
    template <typename... Ts>
    constexpr auto operator()(Ts&&...) const
    {
        return is_callable<TF(Ts...)>{};
    }
};
```

Finally, `is_callable` is a *type-trait-like* class that can be easily implemented using `void_t`. It evaluates to `std::true_type` if the passed signature would result in a well-formed function invocation.

```cpp
template <typename, typename = void>
struct is_callable : std::false_type { };

template <typename TF, class... Ts>
struct is_callable<TF(Ts...),
    void_t<decltype(std::declval<TF>()(std::declval<Ts>()...))>>
    : std::true_type { };
```

This solves the first C++11 annoyance, by allowing us to instantiate detectors locally. The second issue is not as easy to straighten out - branching locally at compile-time would require something like [`if constexpr(...)`](http://open-std.org/JTC1/SC22/WG21/docs/papers/2016/p0128r1.html) *(a.k.a. `static_if`)*, which is only available in C++17...

...but it is actually possible to implement a working `static_if` in C++14, albeit with a slightly cumbersome syntax. I explain how in my [**CppCon 2016 talk**: "Implementing `static` control flow in C++14"](https://www.youtube.com/watch?v=aXSsUqVSe2k).

Once we have that, we can finally implement our `make_noise` function:

```cpp
template <typename T>
auto make_noise(const T& x)
{
    auto has_meow = is_valid([](auto&& x) -> decltype(x.meow()){ });
    auto has_bark = is_valid([](auto&& x) -> decltype(x.bark()){ });

    static_if(has_meow(x))
        .then([](auto&& y)
            {
                y.meow();
            })
        .else_if(has_bark(x))
        .then([](auto&& y)
            {
                y.bark();
            })
        .else_([](auto&&)
            {
                // The pattern below generates a compiler-error.

                // It is not possible to use `static_assert(false)`
                // here, as it triggers whether or not the branch
                // is taken.

                struct cannot_meow_or_bark;
                cannot_meow_or_bark{};
            })(x);
}
```

[*You can find a complete example on GitHub.*](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/in_place_expr_validity/1_cpp14.cpp)

Is this a better implementation compared to the C++11 version? That's debatable. There are, however, some objective advantages:

* Expression validity detector definition/instantiation is local to the function scope.

* There is a single overload of `make_noise` - compile-time branching is local to the function scope.

These advantages become more important when nesting multiple `static_if` blocks together and dealing with more complicated validity checking: the equivalent C++11 code would require an huge amount of boilerplate and `std::enable_if` constraints compared to the C++14 implementation.

*(Note: [`boost::hana:is_valid`](https://boostorg.github.io/hana/structboost_1_1hana_1_1type.html#a2d2e7e08e284f7e0bd1bd9c3ad0e0a2b) is a production-ready C++14 implementation of the above `is_valid` function.)*

*(Note: you can find my `static_if` implementation in [`vrm::core::static_if`](https://github.com/SuperV1234/vrm_core/blob/master/include/vrm/core/static_if/static_if.hpp).)*



### C++17 implementation {#cpp17_impl}

The previous implementation took care of C++11's annoyances, but introduced some new ones:

* `is_valid` has to be assigned to a variable in order to be used in a [constant expression](http://en.cppreference.com/w/cpp/language/constant_expression). This happens because lambdas are not `constexpr`.

* **Verbosity.** Having to use something like `static_if` makes the code much less readable. Having to create a lambda with a `decltype(...)` trailing return type for every expression creates noise.

We can solve both these annoyances thanks to some new features introduced in C++17 and to some *macro black magic*. The final result will look like this:

```cpp
template <typename T>
auto make_noise(const T& x)
{
    if constexpr(IS_VALID(T)(_0.meow()))
    {
        x.meow();
    }
    else if constexpr(IS_VALID(T)(_0.bark()))
    {
        x.bark();
    }
    else
    {
        struct cannot_meow_or_bark;
        cannot_meow_or_bark{};
    }
}
```

Before diving into the implementation, let's analyze the user syntax. `IS_VALID` is a variadic macro that takes any number of types and *"returns"* another variadic macro that takes an expression built using some **type placeholders** *(e.g. `_0`, `_1`, ...)*. The combination of the two macros is a **constant expression** that evaluates to `true` if the expression is valid for the given types, to `false` otherwise. Here are some other example invocations:

```cpp
// Can `T` be dereferenced?
IS_VALID(T)(*_0);

// Can `T0` and `T1` be added together?
IS_VALID(T0, T1)(_0 + _1);

// Can `T` be streamed into itself?
IS_VALID(T)(_0 << _0);

// Can a tuple be made out of `T0`, `T1` and `float`?
IS_VALID(T0, T1, float)(std::make_tuple(_0, _1, _2));
```

All the `IS_VALID` invocations shown above can be used in contexts where only a *constant expression* is accepted such as `static_assert(...)` or `if constexpr(...)`.

> What is this magic!?

Time to reveal the dark secrets of `IS_VALID`. Let's begin by defining some utilities that will allow types to be wrapped into values.

```cpp
template <typename T>
struct type_w
{
    using type = T;
};

template <typename T>
constexpr type_w<T> type_c{};
```

Types can now be wrapped into values like this: `type_c<int>`. The type inside a `type_c` wrapped can be retrieved as follows:

```cpp
using inner_type = typename decltype(type_c<int>)::type;
```

*(Note: the idea of wrapping types into values (and viceversa) [is a core principle of `boost::hana`](https://boostorg.github.io/hana/structboost_1_1hana_1_1type.html#ae35139e732c4b75e91061513cf445628).)*

After that, a new implementation of the previously seen `validity_checker` that works with `type_c` is required:

```cpp
template <typename TF>
struct validity_checker
{
    template <typename... Ts>
    constexpr auto operator()(Ts... ts)
    {
        return std::is_callable<
            TF(typename decltype(ts)::type...)
        >{};
    }
};

template <typename TF>
constexpr auto is_valid(TF)
{
    return validity_checker<TF>{};
}
```

*(Note: [`std::is_callable`](http://en.cppreference.com/w/cpp/types/is_callable) is equivalent to the previously seen `is_callable` and is part of the C++17 standard.)*

This `validity_checker` is conceptually equivalent to the previous one, but it expects `ts...` to be a pack of `type_c` instances which are automatically unwrapped in the `std::is_callable` instantiation.

Before moving onto the inner workings of `IS_VALID`, let's see how [`constexpr` lambdas](https://isocpp.org/files/papers/N4487.pdf) *(standardized in C++17)* can be used to evaluate `is_valid` in-place inside a *constant expression*.

```cpp
// Make sure that `int*` can be dereferenced.
static_assert(
    is_valid([](auto _0) constexpr -> decltype(*_0){})
    (type_c<int*>)
);
```

*Yikes.* This works and compiles, but it's verbose and full of noise/boilerplate. That's why a macro *\*shudders\** is needed here. Let's finally check out how `IS_VALID` is implemented. *(For simplicity, only the single-type version will be analyzed. A fully-variadic `IS_VALID` is simple to implement - [see the example on GitHub](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/in_place_expr_validity/2_cpp17.cpp), which uses my [`vrm_pp` preprocessor metaprogramming](https://github.com/SuperV1234/vrm_pp) library.)*

```cpp
template <typename T, typename TF>
constexpr auto operator|(T x, validity_checker<TF> vc)
{
    return std::apply(vc, x);
}

#define IS_VALID_EXPANDER(...) \
    is_valid([](auto _0) constexpr->decltype(__VA_ARGS__){})

#define IS_VALID(type0) \
    std::make_tuple(type_c<type0>) | IS_VALID_EXPANDER
```

In order to understand this madness, let's use an example:

```cpp
// Can `int*` be dereferenced?
IS_VALID(int*)(*_0)
```

Let's expand `IS_VALID`:

```cpp
std::make_tuple(type_c<int*>) | IS_VALID_EXPANDER
```

Let's expand `IS_VALID_EXPANDER`:

```cpp
std::make_tuple(type_c<int*>) | is_valid([](auto _0) constexpr->decltype(*_0){})
```

The `is_valid(...)` call evaluates to a `validity_checker<...>` instance. As it is not possible to explicitly name the type of the lambda, we'll refer to this particular instance as `some_validity_checker` in the following examples.

```cpp
std::make_tuple(type_c<int*>) | some_validity_checker
```

The `operator|(T, validity_checker<...>)` overload can now be evaluated:

```cpp
std::apply(std::make_tuple(type_c<int*>), some_validity_checker);
// ...which is equivalent to...
some_validity_checker(type_c<int*>)
```

*(Note: [`std::apply`](http://en.cppreference.com/w/cpp/utility/apply) invokes a callable object by "unpacking" the contents of a tuple as its arguments. It was introduced in C++17.)*

Finally, `some_validity_checker(type_c<int*>)` is a *constant expression* that evaluates to either `true` or `false`.

The [`std::tuple`](http://en.cppreference.com/w/cpp/utility/tuple) and the `operator|` overload are there just to make the `IS_VALID(types...)(expression)` syntax possible. Alternatively, the user would have had to specify the number of types as part of the macro name itself. Separating the expression from the types allows [variadic macro argument counting techniques](https://github.com/SuperV1234/vrm_pp/blob/master/include/vrm/pp/arg_count.hpp) to be easily applied.

That's it! [*You can find a complete example on GitHub.*](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/in_place_expr_validity/2_cpp17.cpp)

I think this technique is very useful when combined with `if constexpr(...)` - it's a barebones *"in-place concept"* definition and check. Example:

```cpp
template <typename T0, typename T1>
auto some_generic_function(T0 a, T1 b)
{
    if constexpr(IS_VALID(T0, T1)(foo(_0, _1))
    {
        return foo(a, b);
    }
    else if constexpr(IS_VALID(T0, T1)(_0 + _1))
    {
        return a + b;
    }

    // ...
}
```

...there's a small *temporary* caveat, however: neither `g++` nor `clang++`'s latest versions can currently compile `IS_VALID` inside an `if constexpr(...)` branch which is part of a template context:

* `clang++` [hasn't implemented support for `constexpr` lambdas yet](http://clang.llvm.org/cxx_status.html).

* `g++` [has](https://gcc.gnu.org/projects/cxx-status.html#cxx1z), but there's a [bug I found](http://stackoverflow.com/questions/40283001) and [reported *(as #78131)*](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78131).

`IS_VALID` does work properly with `g++` trunk in other contexts where a *constant expression* is required though *(e.g. non-template context `if constexpr(...)` and `static_assert`)*.



### Addendum {#addendum}

*This section was written on 04/02/2017.*

#### Major simplification

When I woke up today I was extremely happy to see that [**Fabio**](https://disqus.com/by/fabio_a/) managed to simplify `IS_VALID`'s implementation significantly. He posted his work in the comments and [sent a PR](https://github.com/SuperV1234/vittorioromeo.com/pull/5) that I accepted. **Thanks - very appreciated!**

I decided to cover his improvements here. Readers interested in implementing `IS_VALID` should definitely use his simplified version.

* Avoid using `type_w` - it was not necessary after all! `validity_checker` can be defined as:

    ```cpp
    template <typename ...Ts>
    struct validity_checker
    {
        template <typename TF>
        static constexpr auto is_valid(TF)
        {
            return std::is_callable<std::decay_t<TF>(Ts...)>{};
        }
    };
    ```

    As an example, for one argument, `IS_VALID` can be expanded to:

    ```cpp
    #define IS_VALID_EXPANDER_END(...) (__VA_ARGS__){})

    #define IS_VALID_1(...) \
        validity_checker<__VA_ARGS__>::is_valid([](auto _0) constexpr \
        -> decltype IS_VALID_EXPANDER_END
    ```



* Generating variadic expansions of `IS_VALID` on the fly using my [`vrm_pp`](https://github.com/SuperV1234/vrm_pp) preprocessor metaprogramming library.

    ```cpp
    #define IS_VALID_EXPANDER_BEGIN(count) \
        is_valid([](VRM_PP_REPEAT_INC(count, IS_VALID_EXPANDER_MIDDLE,_)) \
        constexpr -> decltype IS_VALID_EXPANDER_END

    #define IS_VALID_EXPANDER_MIDDLE(idx, _) \
        VRM_PP_COMMA_IF(idx) auto _##idx

    #define IS_VALID_EXPANDER_END(...) (__VA_ARGS__){})

    #define IS_VALID(...) \
        validity_checker<__VA_ARGS__>:: \
        IS_VALID_EXPANDER_BEGIN(VRM_PP_ARGCOUNT(__VA_ARGS__))
    ```

    * `VRM_PP_REPEAT_INC(count, IS_VALID_EXPANDER_MIDDLE,_)` is used to generate the `auto _0, auto_1, /*...*/` arguments.

    * `IS_VALID_EXPANDER_BEGIN(VRM_PP_ARGCOUNT(__VA_ARGS__))` is used to count the number of types passed to `IS_VALID`, and to begin generating the expansion.
