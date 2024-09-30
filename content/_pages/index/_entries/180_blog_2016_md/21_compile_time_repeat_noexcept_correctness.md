


<style>
.inline-link
{
    font-size: small;
    margin-top: -2.6em;
    text-align: right;
    font-weight: bold;
}
</style>

In my previous article *([**"abstraction design and implementation: `repeat`"**](https://vittorioromeo.com/index/blog/abstraction_design_implementation_repeat.html))* I've shown how to implement a simple `repeat(n, f)` abstraction that invokes a `FunctionObject` `f` $n$ times in a row.

A large part of the article focused on adding a `noexcept` specifier to `repeat` that correctly propagated `noexcept`-correctness depending on the actions performed on the passed arguments. Adding a `noexcept` specifier resulted in a less elegant, longer, and less accessible implementation.

*Is it worth the hassle?* [Jason Rice](https://github.com/ricejasonf) posted a comment on the previous article asking:

> What are the benefits of `noexcept`-correctness?

Before diving into the implementation of a compile-time version of `repeat`, let's try to understand the importance *(or lack thereof)* of `noexcept`-correctness.



### `noexcept`-correctness

Let's begin with the `noexcept` keyword itself. When was it added? Why does it exist?

The `noexcept` keyword can have two meanings.

* [**`noexcept` specifier**](http://en.cppreference.com/w/cpp/language/noexcept_spec): appears in a *lambda declarator* or *function declarator*. Since C++17, this [is part of the *function type*](https://stackoverflow.com/questions/38760784).

    ```cpp
    void infallible() noexcept;
    // We are declaring that this function will not throw
    // any exception.

    void everyone_makes_mistakes();
    // We are declaring that this function might throw an
    // exception.

    template <int Age>
    void especially_when_young() noexcept(Age > 18);
    // We are declaring that this function will not throw
    // any exception if `Age > 18`, but might throw an
    // exception otherwise.
    ```

* [**`noexcept` operator**](http://en.cppreference.com/w/cpp/language/noexcept): given an expression `e`, evaluates to `true` if `e` is `noexcept`. *(The expression itself is not evaluated.)*

    ```cpp
    static_assert(noexcept(infallible()));
    static_assert(!noexcept(everyone_makes_mistakes()));
    static_assert(noexcept(especially_when_young<23>()));
    static_assert(!noexcept(especially_when_young<11>()));
    ```

    <div class="inline-link">

    [(on *godbolt.org*)](https://godbolt.org/g/mhhWst)

    </div>

Marking functions *(and constructors/destructors)* `noexcept` has various benefits:

* **Clear semantics and readability.** A function marked `noexcept` is guaranteed to not throw - users therefore know that there's no need for any *try-catch* block.

* **Compiler optimizations.** Imagine the following scenario:

    ```cpp
    bool flag = false;
    try
    {
        foo();
        flag = true;
    }
    catch(...)
    {
        // ...
    }
    ```

    If `foo` is marked `noexcept` then the compiler might decide to get rid of all the extra code related to exception handling and possibly just assume that `flag` is always `true`.

* **Library optimizations.** This is, in my opinion, the most important benefit. Library code can check whether or not an expression is `noexcept` thanks to the previously mentioned *`noexcept` operator*. This can have an *immense* impact on performance.

    Consider [`std::vector::resize`](http://en.cppreference.com/w/cpp/container/vector/resize): when it requires the `vector`'s internal buffer to grow, all elements must be copied or moved from the old buffer to a new one. This member function has the [*strong exception guarantee*](http://en.cppreference.com/w/cpp/language/exceptions) - if any exception occurs during its invocation, no effects will occur.

    Imagine moving the `vector`'s elements from the old buffer to the new one. If any of the elements' move constructor throws an exception, there is no way to satisfy the *strong exception guarantee*, as the original contents of the `vector` might have been irreversibly mutated.

    Implementations of `std::vector<T>::resize` will therefore copy elements instead of moving them, if `T::T(T&&)` is not marked `noexcept`.

This should give you an idea of why `noexcept`-correctness is important. The follow-up question might now be:


> Is `noexcept`-correctness important for `repeat`?
>
> Would it be enough to mark `repeat` as `noexcept(true)`?
>
> Or should it just be `noexcept(false)`?

In order to find an answer, imagine the following two scenarios:

* `repeat` being used in the constructor of some hypothetical library utility `boop`, which itself propagates `noexcept`-correctness:

    ```cpp
    struct boop
    {
        boop() noexcept(noexcept(repeat(2, some_action)))
        {
            repeat(2, some_action);
        }
    };
    ```

    If `boop` is used as an element type for `std::vector` and if `repeat` doesn't correctly propagate `noexcept`... we could get the aforementioned performance pessimization.

* `repeat` being used in a *try-catch* block:

    ```cpp
    try
    {
        repeat(5, []
        {
            attempt_connection(/* ... */); // might throw
            std::this_thread::sleep_for(1s);
        });
    }
    catch(...)
    {
        std::cerr << "Fatal error: could not reach network\n";
    }
    ```

    If `repeat` was marked `noexcept(true)`, the whole *try-catch* block would be useless.

Therefore, I think that neither `noexcept(true)` nor `noexcept(false)` are applicable to `repeat`. The correct thing to do here *(and, in my opinion, for any other general-purpose utility)* is to propagate `noexcept`-correctness by using conditional *`noexcept` operators*.

Let's forget about `noexcept` now, and check out how we can implement `repeat` at compile-time!



### `repeat` at compile-time

Firstly, what does it mean to `repeat` something at compile-time?

* `repeat(n, f)` will execute `f` `n` times at run-time.

* `repeat<n>(f)` will generate code that invokes `f` `n` times.

You can think of `repeat<n>(f)` as a explicit form of *loop unrolling*.

Let's now take a look at some desired usage examples:

```cpp
// Generate code that prints out "hello!" five times in a row:
repeat<5>([]
{
    std::cout << "hello!\n";
});
```

```cpp
// Compile-time loop with an index that's a "constant expression":
repeat<128>([](auto i)
{
    if constexpr(i % 3 == 0 && i % 5 == 0)
    {
        std::cout << "FizzBuzz\n";
    }
    else if constexpr(i % 3 == 0)
    {
        std::cout << "Fizz\n";
    }
    else if constexpr(i % 5 == 0)
    {
        std::cout << "Buzz\n";
    }
    else
    {
        std::cout << i << '\n';
    }
});
```

Similarly to the run-time version of `repeat`, we want an homogeneous interface that will detect whether or not the passed `FunctionObject` accepts an index as an argument. If it does, the compile-time version of `repeat` will provide an index which is a [*constant expression*](http://en.cppreference.com/w/cpp/language/constant_expression) - this is what allows us to use `if constexpr(...)` in our compile-time ["fizz buzz"](https://en.wikipedia.org/wiki/Fizz_buzz) implementation.

So, how do we implement `repeat<n>`? Let's begin with the signature:

```cpp
template <auto N, typename F>
void repeat(F&& f)
{
    // ...
}
```

Note that we're using a new C++17 feature: [`auto` as a *non-type template parameter*](https://stackoverflow.com/questions/38026884/advantages-of-auto-in-template-parameters-in-c17). This allows the user to decide what type the "counter" should be without requiring additional boilerplate.

Let's now focus on implementing `repeat` without passing an index. Our goal is to generate code that invokes `f()` `N` times. We could do this using recursion... or we can embrace C++17's powerful [*fold expressions*](http://en.cppreference.com/w/cpp/language/fold).

The solution I came up with here will work as follows:

* Generate a compile-time integer sequence with [`std::make_integer_sequence`](http://en.cppreference.com/w/cpp/utility/integer_sequence).

* Match the generated indices in a `detail` implementation function.

* Fold over the generated indices, invoking `f()`.

`repeat` will simply create the sequence and invoke the implementation function:

```cpp
template <auto N, typename F>
void repeat(F&& f)
{
    detail::repeat_impl(
        f, std::make_integer_sequence<decltype(N), N>{});
}
```

`repeat_impl` will match the generated indices and perform a fold over the *comma operator*:

```cpp
template <typename F, typename T, T... Is>
void repeat_impl(F&& f, std::integer_sequence<T, Is...>)
{
    ( ((void)Is, f()), ... );
}
```

The *fold expression* `( ((void)Is, f()), ... )` expands to something like:

```cpp
// pseudocode
((void)0, f()),
((void)1, f()),
((void)2, f()),
// ...
((void)N - 1, f())
```

That's it! [Here's a **live example on wandbox.org**](https://wandbox.org/permlink/4bIOe2HYZIJzWsTg).

In order to pass the index to the callback, we can use [`std::integral_constant`](http://en.cppreference.com/w/cpp/types/integral_constant), which has a `constexpr` implicit conversion operator to its underlying type. Like in the previous article, we can use `if constexpr` and [`std::is_invocable_v`](http://en.cppreference.com/w/cpp/types/is_invocable) to detect whether or not `f` accepts an index:

```cpp
template <typename F, typename T, T... Is>
void repeat_impl(F&& f, std::integer_sequence<T, Is...>)
{
    if constexpr((std::is_invocable_v<F&&,
         std::integral_constant<T, Is>> && ...))
    {
        ( f(std::integral_constant<T, Is>{}), ... );
    }
    else
    {
        ( ((void)Is, f()), ... );
    }
}
```

A few points to make here:

* We're using a *fold expression* over `&&` to check if `f` is invocable with all of our integral constants.

* We're using another *fold expression* over the comma operator to invoke `f` with an integral constant created for every index in `Is...`.

[Here's a **live example on wandbox.org**, with the compile-time "fizz buzz".](https://wandbox.org/permlink/sOvEJEMSdMs13O6q)

**`noexcept`-correctness is left as an exercise for the reader ;)**


### minor improvements

`std::integer_sequence` and `std::integral_constant` are *so 2011*. Now that we can use `auto` as a *non-type template parameter*, we can do better:

```cpp
template <auto... Is>
using sequence = std::integer_sequence
<
    detail::seq_type<Is...>,
    Is...
>;
```

`detail::seq_type` calculates the *common type* of `Is...`, or evaluates to `int` if `sizeof...(Is) == 0`.

```cpp
template <auto I>
using constant = std::integral_constant<decltype(I), I>;

template <auto I>
using make_sequence = std::make_integer_sequence<decltype(I), I>;
```

With these minor improvements, the final code is much cleaner:

```cpp
namespace detail
{
    template <typename F, auto... Is>
    void repeat_impl(F&& f, sequence<Is...>)
    {
        if constexpr((std::is_invocable_v<F&&, constant<Is>> && ...))
        {
            ( f(constant<Is>{}), ... );
        }
        else
        {
            ( ((void)Is, f()), ... );
        }
    }
}

template <auto N, typename F>
void repeat(F&& f)
{
    detail::repeat_impl(f, make_sequence<N>{});
}
```

<div class="inline-link">

[*(on wandbox.org)*](https://wandbox.org/permlink/dY7WEx6v31EvLEhc)

</div>

Now, if we only had an `if constexpr(...)` equivalent for the ternary operator, we could write `repeat_impl` as a single expression *(and get `noexcept`-correctness for free)*:

```cpp
// pseudocode
template <typename F, auto... Is>
void repeat_impl(F&& f, sequence<Is...>)
{
    ( constexpr((std::is_invocable_v<F&&, constant<Is>> && ...))
        ? f(constant<Is>{})
        : f(), ... )
}
```

...time for a proposal?
