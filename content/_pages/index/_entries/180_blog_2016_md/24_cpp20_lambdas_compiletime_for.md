

<style>
.inline-link
{
    font-size: small;
    margin-top: -2.6em;
    text-align: right;
    font-weight: bold;
}
</style>

In one of my previous articles, ["compile-time `repeat` & `noexcept`-correctness"](https://vittorioromeo.com/index/blog/compile_time_repeat_noexcept_correctness.html), I have covered the design and implementation of a simple `repeat<n>(f)` function that, when invoked, expands to `n` calls to `f` during compilation. E.g.

```cpp
repeat<4>([]{ std::cout << "hello\n"; });
```

...is roughly equivalent to...

```cpp
[]{ std::cout << "hello\n"; }();
[]{ std::cout << "hello\n"; }();
[]{ std::cout << "hello\n"; }();
[]{ std::cout << "hello\n"; }();
```

If you squint, this is a very limited form of *compile-time iteration*. When writing generic code, I've often needed similar constructs in order to express the following actions:

* iterate over a compile-time list of types `Ts...`;

* iterate over a compile-time list of values `Xs...`;

* iterate over a compile-time integral range `[B, E)`;

* enumerate a compile-time list of types `Ts...` (i.e. iteration alongside an index).

In this article I'm going to show you how to implement the above constructs, relying on a new nifty addition to C++20 lambdas: [**P0428**: "Familiar template syntax for generic lambdas"](http://wg21.link/P0428), by [Louis Dionne](https://twitter.com/LouisDionne).

This feature is currently available in [`g++` **8.x**](https://gcc.gnu.org/projects/cxx-status.html).



### familiar template syntax

Here's an example of a C++20 generic lambda, taking a single template parameter `T` and accepting an `std::vector<T>`:

```cpp
auto print_vector = []<typename T>(const std::vector<T>& v)
{
    for(const auto& x : v) { std::cout << x; }
};

print_vector(std::vector{0, 1, 2, 3, 4, 5});
```

This roughly desugars to the following anonymous *closure* type:

```cpp
struct /* print_vector */
{
    template <typename T>
    auto operator()(const std::vector<T>& v) const
    {
        for(const auto& x : v) { std::cout << x; }
    }
};
```

Compared to a C++14 generic lambda, this feature allows users to easily:

* constrain generic lambdas to any instantiation of a particular class;

* "match" a template parameter (or parameter pack) without having to introduce an additional function.

The second point is particularly useful when dealing with *type lists* and utilities such as `std::index_sequence`.

Another interesting thing that can be done on both C++14 and C++17 generic lambdas is directly calling `operator()` by explicitly passing a template parameter:

* C++14:

    ```cpp
    auto l = [](auto){ };
    l.template operator()<int>(0);
    ```

* C++20:

    ```cpp
    auto l = []<typename T>(){ };
    l.template operator()<int>();
    ```

The C++14 example above is quite useless: there's no way of referring to the type provided to `operator()` in the body of the lambda without giving the argument a name and using `decltype`. Additionally, we're forced to pass an argument even though we might not need it.

The C++20 example shows how `T` is easily accessible in the body of the lambda and that a nullary lambda can now be arbitrarily templatized. This is going to be very useful for the implementation of the aforementioned compile-time constructs.



### iteration over a type list

The first construct we're going to implement is a simple "loop" over a list of user-provided types. Here's a usage example:

```cpp
for_types<int, float, char>([]<typename T>()
{
    std::cout << typeid(T).name();
});
```

The code above prints `"ifc"`. I like to read it as: *"for the types `int`, `float`, and `char`, please execute the following action"*. (The *"please"* is not mandatory.)

The implementation of `for_types` is as follows:

```cpp
template <typename... Ts, typename F>
constexpr void for_types(F&& f)
{
    (f.template operator()<Ts>(), ...);
}
```

The body of `for_types` is a C++17 [*fold expression*](http://en.cppreference.com/w/cpp/language/fold) over the [*comma operator*](http://en.cppreference.com/w/cpp/language/operator_other) invoking `F::operator()<T>` for each `T` in `Ts...`. Some other interesting details:

* the `Ts...` parameter pack cannot be deduced, and is explicitly provided by the user;

* `F` is deduced;

* the closure is taken as a [*forwarding reference*](http://en.cppreference.com/w/cpp/language/reference#Forwarding_references), in order to accept non-`const` temporaries (e.g. `mutable` lambdas);

* `f` is not perfectly-forwarded inside the body of the function as it could be invoked multiple times;

* `for_types` is marked as `constexpr` even though it returns `void` - this allows it to be used inside `constexpr` functions. E.g.

    ```cpp
    constexpr int bar()
    {
        int r = 0;
        for_types<int, char>([&]<typename T>(){ r += sizeof(T); });
        return r;
    }
    ```

`for_types` is useful in various scenarios - as an example, imagine *unit testing* a component or a function over a set of fixed types, or checking if an `std::any` instance contains one of a set of given types.



### iteration over a compile-time list of values

Let's begin with a usage example:

```cpp
for_values<2, 8, 16>([]<auto X>()
{
    std::array<int, X> a;
    something(a);
});
```

This is another useful construct that allows "compile-time iteration" over a set of values, which can be used as part of *constant expressions*. The implementation is almost identical to `for_types`, but we're using `auto...` instead of `typename...`:

```cpp
template <auto... Xs, typename F>
constexpr void for_values(F&& f)
{
    (f.template operator()<Xs>(), ...);
}
```

`auto` as a non-type template parameter was introduced in C++17 thanks to [**P0127**: "Declaring non-type template parameters with `auto`"](http://wg21.link/P0127), by James Touton and Mike Spertus.



### iteration over a range

As always, let's start with a usage example:

```cpp
for_range<(-5), 5>([]<auto X>()
{
    std::cout << X << ' ';
});
```

Output:

> -5 -4 -3 -2 -1 0 1 2 3 4

The implementation is quite interesting, and depends on `for_values`:

```cpp
template <auto B, auto E, typename F>
constexpr void for_range(F&& f)
{
    using t = std::common_type_t<decltype(B), decltype(E)>;

    [&f]<auto... Xs>(std::integer_sequence<t, Xs...>)
    {
        for_values<(B + Xs)...>(f);
    }
    (std::make_integer_sequence<t, E - B>{});
}
```

* Firstly, `for_range` takes a `[B, E)` range via `auto` non-type template parameters. The *common type* between those is computed and aliased as `t`;

* a `std::integer_sequence` of `t` values from `0` to `E - B` is created with:

    ```cpp
    std::make_integer_sequence<t, E - B>{}
    ```

* the sequence is used to invoke a C++20 generic lambda which takes `auto... Xs` as a non-type template parameter pack. The values of `Xs...` are deduced by "matching" them from the `std::integer_sequence` argument;

* finally, the body of the lambda invokes `for_values<(B + Xs)...>(f)`, which expands to an invocation of `f` for every value in the `[B, E)` range.

The implementation of `for_range` is a compelling example of how C++20 generic lambdas can make it really easy to create and use a `std::integer_sequence<T, Xs...>` on the spot, without having to invoke a separate implementation function just to "match" `Xs...`.



### enumeration of a list of types

This construct is useful when you want to iterate over a list of types at compile-time, while also keeping track of the current iteration index as a *constant expression*. I used this in my experimental library [`orizzonte`](https://github.com/SuperV1234/orizzonte) to implement `when_all` and `when_any` abstractions for the composition of asynchronous future graphs.

Usage example:

```cpp
enumerate_types<int, float, char>([]<typename T, auto I>()
{
    std::cout << I << ": " << typeid(T).name() << '\n';
});
```

This prints out:

> 0: i
>
> 1: f
>
> 2: c

The idea is as follows: we'll accept a template parameter pack `Ts...` containing the types from the user, and then generate an index pack of equal length using `std::index_sequence_for`. Finally, both packs will be expanded at the same time with a *fold expression*.

```cpp
template <typename... Ts, typename F>
constexpr void enumerate_types(F&& f)
{
    [&f]<auto... Is>(std::index_sequence<Is...>)
    {
        (f.template operator()<Ts, Is>(), ...);
    }(std::index_sequence_for<Ts...>{});
}
```

As with `for_range`, a C++20 generic lambda is being used to create and consume a `std::index_sequence` on the spot.



### conclusion

The new "familiar template syntax" for lambdas introduced in C++20 makes constructs such as `for_types` and `for_range` viable and way more readable compared to C++17 alternatives.

Being able to expand a sequence on the spot without having to create an extra `detail` function is also a great advantage brought from this new feature.
