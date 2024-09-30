


<style>
.inline-link
{
    font-size: small;
    margin-top: -2.6em;
    text-align: right;
    font-weight: bold;
}
</style>

You know that you're obsessed with *library design* and *abstractions* when a simple `for` loop like this one...

```cpp
for(int i = 0; i < 10; ++i)
{
    foo();
}
```

...**greatly** bothers you.

> What's wrong with it?

Assuming that there's nothing special about `foo()`, the code above works properly and doesn't have any bug. There's nothing *"wrong"* with it. Except for the fact that **it doesn't clearly express its intent**.

Ask yourself - *"what does the code snippet above **mean**?"* It is just trying to invoke `foo()` ten times. In other words, it is trying to **repeat** an action $n$ times.

If that's the case, why do we need to:

* Use a low-level looping construct;

* Create a local variable `i`, give it a type and initialize it to `0`;

* Increment `i` on every iteration.

This is unnecessary boilerplate, which masks the intent of the code and allows the introduction of subtle bugs *(e.g. using `<=` instead of `<`, or dangerous implicit conversions)*.



### welcome, abstractions!

In my previous [**"passing functions to functions"**](https://vittorioromeo.com/index/blog/passing_functions_to_functions.html) and [**"zero-overhead C++17 currying & partial application"**](https://vittorioromeo.com/index/blog/cpp17_curry.html) articles I've praised C++11 *(and newer standards)* for allowing us to write *"more functional"* code. In particular, I discussed ["higher-order functions"](https://en.wikipedia.org/wiki/Functional_programming#First-class_and_higher-order_functions). These concepts will be used throughout the article - I'm going to assume your familiarity with them from now on.

If I want to repeat an action $n$ times, I exactly want to write that in my code:

```cpp
repeat(10, []
{
    foo();
});
```

Cannot get simpler than that - let's implement it!

```cpp
template <typename N, typename F>
constexpr void repeat(N n, F&& f)
{
    for(N i = 0; i < n; ++i)
    {
        f();
    }
}
```

<div class="inline-link">

[*(on godbolt.org)*](https://godbolt.org/g/ijYidG)

</div>

As you can see from the *godbolt.org* link above, `repeat` is completely inlined by both **g++** and **clang++** with `-O2`. You should not be surprised by this - *templates* and *lambda expressions* are completely transparent to the compiler.

Even in this simple `repeat` implementation, there is an interesting point to note:

* We marked `repeat` as `constexpr`... even though it returns `void`. If you're asking yourself *"does this even make sense?"* the answer is **yes**, and it's best explained [with this example on *godbolt.org*](https://godbolt.org/g/14EFTk). Getting rid of `constexpr` would prevent it from compiling. The lesson to learn here is that `constexpr` doesn't imply immutability - mutable state is allowed *(and often useful)* during compile-time computations. Note that lambdas are implicitly `constexpr` when possible since C++17.

Unfortunately, this implementation is not completely correct. It doesn't properly propagate `noexcept`-ness! In order to fix that, we need to add a `noexcept` specifier composed of various `noexcept` operator checks, which make sure that all the operations we use inside the body are indeed `noexcept`:

```cpp
template <typename N, typename F>
constexpr void repeat(N n, F&& f)
    noexcept(noexcept(f())
          && noexcept(N(0))
          && noexcept(n < n)
          && noexcept(++n)
    )
{
    for(N i = 0; i < n; ++i)
    {
        f();
    }
}
```

<div class="inline-link">

[*(on godbolt.org)*](https://godbolt.org/g/zL9hcM)

</div>

This is important if you're trying to write a `noexcept`-friendly library: the user could pass a custom type to `repeat` that exposes an `int`-like interface but may throw. Unfortunately this compromises the readability of the implementation.



### *"but what if I need the index?"*

While the pattern seen at the beginning of the article involving the `for` loop is extremely common, the snippet below is just as easy to find in any codebase:

```cpp
for(int i = 0; i < 10; ++i)
{
    foo(i);
}
```

...crap. Our `repeat` is not powerful enough to handle this! *(Unless you use a `mutable` lambda, but it definitely won't win a beauty pageant.)*

An easy solution would be introducing a new function, called `repeat_with_index`:

```cpp
repeat_with_index(10, [](int i)
{
    foo(i);
});
```

While this works and it's trivial to implement, it just feels wrong to have two different functions for such a small difference in behavior. Additionally, if an user suddenly realizes he/she now needs an index in his/her `repeat` call, changing the signature of the lambda is not enough - the invoked function must be different as well.

What I want is `repeat` to "detect" whether or not the passed *closure* can be invoked with an index, and if so just pass it as an argument. In short, these two `repeat` calls below must compile and work as expected:

```cpp
repeat(5, []
{
    foo();
})

repeat(5, [](int i)
{
    foo(i);
})
```

Before we implement this new, more flexible version of `repeat`, let's stop for a second and think about `noexcept`. **`noexcept` is the devil and feeds on human offspring.**

> Why?

Because in this particular situation it forces you to take a beautiful piece of code, and replace it with poop.

In order to understand my frustation, please consider this "`noexcept`-incorrect" version of the proposed `repeat`:

```cpp
template <typename N, typename F>
constexpr void repeat(N n, F&& f)
{
    for(N i = 0; i < n; ++i)
    {
        if constexpr(std::is_invocable_v<F&&, N>)
        {
            f(i);
        }
        else
        {
            f();
        }
    }
}
```

<div class="inline-link">

[*(on godbolt.org)*](https://godbolt.org/g/AjZE1c)

</div>

This implementation of `repeat` is, in my opinion, beatiful. We are using C++17's [`constexpr if`](http://en.cppreference.com/w/cpp/language/if#Constexpr_If) in order to elegantly and locally decide whether to pass `i` to `f` or not. We are also using [`std::is_invocable_v`](http://en.cppreference.com/w/cpp/types/is_invocable) to check whether or not `f` can be invoked with `i`.

The best thing about this implementation is that everything is in a single function and that the control flow is easy to reason about. Unfortunately, it is **incorrect**, as it does not propagate `noexcept`-ness.

The issue here is that `noexcept(noexcept(e))` only accepts an **expression** `e`, and doesn't allow us to pass a *compound statement*. Therefore, if we want to make sure that this version of `repeat` is `noexcept`-correct, we need to abandon `constexpr if`... **:(**

Here's a `noexcept`-correct version of `repeat`:

```cpp
template <typename N, typename F>
constexpr auto repeat(N n, F&& f)
    noexcept(noexcept(f(n))
          && noexcept(N(0))
          && noexcept(n < n)
          && noexcept(++n)
    )
    -> std::enable_if_t<std::is_invocable_v<F&&, N>>
{
    for(N i = 0; i < n; ++i)
    {
        f(i);
    }
}

template <typename N, typename F>
constexpr auto repeat(N n, F&& f)
    noexcept(noexcept(f())
          && noexcept(N(0))
          && noexcept(n < n)
          && noexcept(++n)
    )
    -> std::enable_if_t<!std::is_invocable_v<F&&, N>>
{
    for(N i = 0; i < n; ++i)
    {
        f();
    }
}
```

<div class="inline-link">

[*(on godbolt.org)*](https://godbolt.org/g/grR8LQ)

</div>

While the common `noexcept` expressions and the `std::enable_if_t` can be refactored to avoid some repetition, this implementation of `repeat` is nowhere as easy to understand/maintain than the previous one using `constexpr if`. We now require two overloads, two `for` loops, and a lot of extra boilerplate.

Note that this situation gets even worse when you also need to provide a SFINAE-friendly return type. I discussed this in more depth in my [**"You must type it three times"**](https://www.youtube.com/watch?v=I3T4lePH-yA) lightning talk.

I strongly believe that the language should provide a way of automatically deduce `noexcept`-ness from the body of a function *(and also a way of deducing SFINAE-friendly return types)*. There has been an attempt to [standardize `noexcept(auto)`](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4473.html) in the past, but the paper was rejected. The closest thing we have nowadays is the [**"Abbreviated Lambdas for Fun and Profit"**](https://wg21.link/p0573) by Barry Rezvin, which proposes a new syntax for lambda bodies which would automatically generate `noexcept` specifier and SFINAE-friendly return type - unfortunately it is proposed to only work with **expressions**, and not for compound statements.



### lessons learned

I wanted to tell you about multiple unrelated things with this article. Here are, in my opinion, the key takeaways:

* Try to find a way to **directly express your intent in your code**. This sometimes requires creating new abstractions, even for trivial things such as a `for` loop.

* Compilers are really good at inlining/optimizing *templates* and *lambda expressions*. Unless you use the wrong tools for the job, general-purpose abstractions revolving around higher-order functions should be **cost-free**.

* Marking functions returing `void` as `constexpr` makes sense and can be useful.

* Abstractions can become more flexible and more user-friendly if introspection and various forms of "detection" are used. In this case, we generalized `repeat` to work both with and without indices. Consider this while writing a library function - you usually want to maximize user friendliness and ease of refactoring.

* C++17's `constexpr if` can lead to very simple and beautiful code - branching at compile-time becomes easy and natural.

* Library developers who care about `noexcept`-correctness and SFINAE-friendliness have a **really hard time**. They might not be able to use *`auto` as a return type* or *compound statements* if they want the compiler to help with return type deduction and `noexcept(noexcept(...))`. This unnecessarily complicates libraries and widens the abyss between *"library code"* and *"user code"*. If a non-expert C++ developer wants to share a nice abstraction that helped him/her write some business code, it will be likely incorrect under these aspects. I strongly believe that **the language must be improved** to prevent these situations.

I hope you enjoyed this unusual, mixed article - I'm planning to write a short sequel covering `repeat<I>`, a similar abstraction that instead of looping at run-time, loops at compile-time *(i.e. generates unrolled code and provides access to the index in a `constexpr`-friendly manner)*.

The next article in the series, [**"compile-time `repeat` & `noexcept`-correctness"**](https://vittorioromeo.com/index/blog/compile_time_repeat_noexcept_correctness.html), covers a compile-time version of `repeat` and the importance of *`noexcept`-correctness*.
