

**Perfect forwarding** and **forwarding references** allow developers to write generic *template functions* that retain the *lvalueness*/*rvalueness* of passed arguments, in order to avoid unnecessary copies or support reference semantics without having to implement multiple overloads.
*([This article](http://eli.thegreenplace.net/2014/perfect-forwarding-and-universal-references-in-c/) by **Eli Bendersky** explains them in depth. I will assume you're familiar with these concepts for the rest of the article.)*

In this article, I'll show how to correctly capture *perfectly-forwarded* objects into lambdas:

* We'll take a look at an example that shows how using `std::forward` in lambda captures can produce unexpected results.

* A *perfect forward capture* wrapper that prevents the aforementioned unexpected results will then be implemented.

* Macros *(yuck!)* will be used to reduce noise/boilerplate.

* The solution will be finally generalized to *pefectly-forwarded variadic argument packs*.



### The problem

Let's begin with a simple *quiz*. Given this definition of `A`...

```cpp
struct A
{
    int _value{0};
};
```

...and the following code...

```cpp
auto foo = [](auto& a)
{
    return [&a]{ ++a._value; };
};

A my_a;
foo(my_a)();
// What is `my_a._value` here?
```

...**what is the value of `my_a._value`?**

It should be obvious that, since `a` was passed by *lvalue reference* to `foo` and captured by reference from the inner lambda, the increment will be visible outside of the function call. Therefore, `my_a._value` is equal to `1`.

[*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/ZFGfMWyLHzR36l9Z)

Let's now generalize `foo` - it will take `a` by *forwarding reference*, and should have the following semantics:

* When `a` is an *lvalue reference*, it should be **captured by reference** in the inner lambda, and any mutation on `a` should be visible outside of the function call *(i.e. what happened in the example above)*.

* When `a` is an *rvalue reference*, it should be **moved** in the inner lambda. The returned lambda will therefore hold a new instance of `A` whose mutations will only be visible from the lambda itself.

> Sounds like a job for *perfect forwarding*!

Let's try to use `auto&&`, [`std::forward`](http://en.cppreference.com/w/cpp/utility/forward) and [C++14's *generalized lambda capture syntax*](http://en.cppreference.com/w/cpp/language/lambda#Lambda_capture) to implement our desired semantics. The inner lambda will be marked as `mutable` since it will store its own instance of `A` when `a` is an *rvalue reference*.

```cpp
auto foo = [](auto&& a)
{
    return [a = std::forward<decltype(a)>(a)]() mutable
    {
        ++a._value;

        // Print to see `a` being mutated when moved into
        // the inner lambda.
        std::cout << a._value << "\n";
    };
};
```

That was easy, wasn't it? Time to test our code.

* When `a` is an *rvalue reference*...

    ```cpp
    auto l_inner = foo(A{});
    l_inner(); // Prints `1`.
    l_inner(); // Prints `2`.
    l_inner(); // Prints `3`.
    ```

    ...the implementation works as intended. <br> [*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/2FRKc4MboiUBCBLQ)

* When `a` is an *lvalue reference*...

    ```cpp
    A my_a;
    auto l_inner = foo(my_a);
    l_inner(); // Prints `1`.
    l_inner(); // Prints `2`.
    l_inner(); // Prints `3`.

    // Prints `0`... wtf?
    std::cout << my_a._value << "\n";
    ```

    ...reference semantics do not work as expected anymore! <br> [*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/l5m67c2TDKfr0EyD)

While it seems natural to use `std::forward` and *generalized lambda captures* in order to achieve the same semantics as a *template function* that takes *forwarding references*, if we stop and think for a moment our mistake **will become obvious**.


Let's look at `foo` again:

```cpp
auto foo = [](auto&& a)
{
    return [a = std::forward<decltype(a)>(a)]() mutable { /* ... */ };
};
```

What does `a = std::forward<decltype(a)>(a)`{.cpp} really mean? It becomes clear if we *"play compiler"* and write a `struct` that is equivalent to the inner lambda:

```cpp
struct inner_lambda
{
    A _captured_a;

    template <typename T>
    inner_lambda(T&& a) : _captured_a(std::forward<decltype(a)>(a))
    {
    }

    void operator()() { /* ... */ }
};
```

As you can see, the type of the captured `a` is always a value! This is due to the fact that the *initializer* expression in a *generalized lambda capture* does not have effect on the deduced type of the capture. From [*cppreference/lambda/lambda_capture*](http://en.cppreference.com/w/cpp/language/lambda#Lambda_capture):

> **A capture with an initializer acts as if it declares and explicitly captures a variable declared with type `auto`**, whose declarative region is the body of the lambda expression (that is, it is not in scope within its initializer), except that:
>
> * if the capture is by-copy, the non-static data member of the closure object is another way to refer to that `auto` variable.
>
> * if the capture is by-reference, the reference variable's lifetime ends when the lifetime of the closure object ends
>
> This is used to capture move-only types with a capture such as `x = std::move(x)`

Even with *generalized lambda captures* the programmer **has to be explicit** when a reference is desired:

```
int a = 0;
int& a_ref = a;

// Captures `a` by copy.
[b = a_ref]{ };

// Captures `a` by reference.
[&b = a_ref]{ };
```

It should be clear now that our previous approach is **fundamentally flawed**. What we really want to do is:

* *Capture by copy* if `a` is an *rvalue reference*.

* *Capture by reference* if `a` is an *lvalue reference*.

That's impossible without adding an *extra layer of indirection*, as the language requires us to be explicit about our capture semantics *(and does not provide any special syntax for "capture by perfect forwarding")*.



### The solution

One possible way of solving the aforementioned issue and achieve the desired capturing semantics is to **create a *wrapper class***  that will always be *captured by copy* by the inner lambda, but will internally store either *an lvalue-reference* or *a value* depending on how it is constructed.

The solution I'm going to present here was recently designed and implemented because I encountered this particular situation when experimenting with *stack-allocated asynchronous chain generation*. Most of the ideas behind it come from this StackOverflow question I asked back in 2014: ["Capturing perfectly-forwarded variable in lambda"](http://stackoverflow.com/questions/26831382).

The wrapper will be a *template class* called `fwd_capture_wrapper`. It will be specialized depending on the *lvalue-ness*/*rvalue-ness* of the captured object:

```cpp
// Unspecialized version: stores a `T` instance by value.
template <typename T>
struct fwd_capture_wrapper : impl::by_value<T>
{
    // "Inherit" constructors.
    using impl::by_value<T>::by_value;
};

// Specialized version: stores a `T` reference.
template <typename T>
struct fwd_capture_wrapper<T&> : impl::by_ref<T>
{
    // "Inherit" constructors.
    using impl::by_ref<T>::by_ref;
};
```

Let's now implement `impl::by_value<T>`{.cpp}. It will store a plain `T` instance *(initialized via a *perfect forwarding constructor)* and provide `.get()` methods to access it.

```cpp
template <typename T>
class by_value
{
private:
    T _x;

public:
    template <typename TFwd>
    by_value(TFwd&& x) : _x{std::forward<TFwd>(x)}
    {
    }

    auto&       get() &      { return _x; }
    const auto& get() const& { return _x; }
    auto        get() &&     { return std::move(_x); }
};
```

When designing `impl::by_ref<T>`, we could choose to store a `T&`. Unfortunately this would prevent us from easily adding copy/move operations for the wrapper itself. Thankfully [`std::reference_wrapper`](http://en.cppreference.com/w/cpp/utility/functional/reference_wrapper) was introduced in C++11. It is [`TriviallyCopyable`](http://en.cppreference.com/w/cpp/concept/TriviallyCopyable) and "behaves like a pointer", but does not allow any *null state* and prevents accidental construction from temporary objects.

```cpp
template <typename T>
class by_ref
{
private:
    std::reference_wrapper<T> _x;

public:
    by_ref(T& x) : _x{x}
    {
    }

    auto&       get() &      { return _x.get(); }
    const auto& get() const& { return _x.get(); }
    auto        get() &&     { return std::move(_x.get()); }
};
```

That's it! The only missing piece is a nice `fwd_capture` interface function that hides the boilerplate away from the user:

```cpp
template <typename T>
auto fwd_capture(T&& x)
{
    return fwd_capture_wrapper<T>(std::forward<T>(x));
}
```

Let's revisit our flawed implementation of `foo`:

```cpp
auto foo = [](auto&& a)
{
    return [a = fwd_capture(std::forward<decltype(a)>(a))]() mutable
    {
        ++a.get()._value;
        std::cout << a.get()._value << "\n";
    };
};
```

`a` is now a `fwd_capture_wrapper` that will store either a value or a reference as intended, depending on what `a` is in the outer lambda.

Our test cases now work correctly!

```cpp
auto l_inner = foo(A{});
l_inner(); // Prints `1`.
l_inner(); // Prints `2`.
l_inner(); // Prints `3`.
```

```cpp
A my_a;
auto l_inner = foo(my_a);
l_inner(); // Prints `1`.
l_inner(); // Prints `2`.
l_inner(); // Prints `3`.

// Prints `3`, yay!
std::cout << my_a._value << "\n";
```

[*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/CDq6ZLnImy9TABRd)



### Reducing noise

Typing `fwd_capture(std::forward<decltype(a)>(a))`{.cpp} is very annoying, as it's mostly *noise*/*boilerplate*. We have to perfectly-forward `a` into the wrapper though, so avoiding the call to `std::forward` is out of the question.

There's only one **evil** tool that can help here: *macros*.

Let's begin with `std::forward<decltype(a)>(a)`{.cpp}: `a` needs to be repeated twice. We can define a `FWD` macro that does that for us:

```cpp
#define FWD(...) ::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)
```

*(See ["`vrm_core` issue #1"](https://github.com/vittorioromeo/vrm_core/issues/1) for discussion regarding `__VA_ARGS__` in this context.)*

The `FWD` macro can be used as follows:

```cpp
template <typename T>
void foo(T&&);

template <typename T>
void bar(T&& x);
{
    foo(FWD(x));
    // ...equivalent to...
    foo(std::forward<decltype(x)>(x));
}
```

Great! Now `fwd_capture(std::forward<decltype(a)>(a))`{.cpp} can be reduced to `fwd_capture(FWD(a))`{.cpp}:

```cpp
return [a = fwd_capture(FWD(a))]() mutable
{
    /* ... */
};
```

There's still some unnecessary repetition: `fwd_capture` always expects the user to perfectly-forward something - there's no reasonable usage of `fwd_capture` without `FWD`. Therefore... we can hide `fwd_capture` in an implementation namespace and provide a macro that forwards the argument for us.

```cpp
#define FWD_CAPTURE(...) impl::fwd_capture(FWD(__VA_ARGS__))
```

All the noise is finally hidden away:


```cpp
return [a = FWD_CAPTURE(a)]() mutable
{
    /* ... */
};
```

Remember that these macros should have some reasonably unique prefixes to avoid collisions, and that they're not mandatory - they, however, immensely increase code readability when perfectly-capturing a lot of arguments in the same lambda.

[*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/YlJecRF4ztbYmo30)



### Variadic argument packs

> There's something missing! What about *variadic arguments*?

True - let's assume that `foo` takes a variadic amount of arguments...

```cpp
auto foo = [](auto&&... xs) { /* ... */ };
```

...and that we want to *perfectly-capture* all of them in the inner lambda. Unfortunately the following snippet doesn't work:

```cpp
auto foo = [](auto&&... xs)
{
    return [xs = FWD_CAPTURE(xs)...]() mutable
    {

    };
};
```

![Variadic generalized capture error.](resources/img/blog/fwd_capture_variadic_error.png)


[There is no way of doing a *generalized variadic lambda capture*](http://stackoverflow.com/questions/24816557/generalized-lambda-capture-on-param-pack). The solution *(workaround?)* is using [`std::tuple`](http://en.cppreference.com/w/cpp/utility/tuple).

```cpp
template <typename... Ts>
auto /*impl::*/fwd_capture_as_tuple(Ts&&... xs)
{
    return std::make_tuple(FWD_CAPTURE(xs)...);
}

#define FWD_CAPTURE_PACK(...) impl::fwd_capture_as_tuple(FWD(__VA_ARGS__)...)
```

*([`std::make_tuple`](http://en.cppreference.com/w/cpp/utility/tuple/make_tuple) is going to copy/move the wrappers, not the object themselves.)*

After capturing a variadic pack, we may want to *"perfectly-apply"* it to some function or to iterate over the captured objects. In order to do that, we can use [`std::experimental::apply`](http://en.cppreference.com/w/cpp/experimental/apply) and [*fold expressions*](http://en.cppreference.com/w/cpp/language/fold):

```cpp
// Expand all elements of `fc` into a `f(...)` invocation.
template <typename TF, typename TFwdCapture>
decltype(auto) apply_fwd_capture_pack(TF&& f, TFwdCapture&& fc)
{
    return std::experimental::apply(
        [&f](auto&&... xs) -> decltype(auto)
        {
            return f(FWD(xs).get()...);
        }, FWD(fc));
}

// Execute `f` on every element of `fc`.
template <typename TF, typename TFwdCapture>
void for_fwd_capture_tuple(TF&& f, TFwdCapture&& fc)
{
    apply_fwd_capture_pack(
        [&f](auto&&... xs){ (f(FWD(xs)), ...); },
        FWD(fc)
    );
}
```

Using `FWD_CAPTURE_PACK` and `apply_fwd_capture_pack` we can now generalize our `foo` example even further:

```cpp
auto foo = [](auto&&... xs)
{
    // Perfectly-capture `xs...` as `xs_pack`.
    return [xs_pack = FWD_CAPTURE_PACK(xs)]() mutable
    {
        // Perfectly-iterate over elements of `xs_pack`.
        for_fwd_capture_tuple([](auto&& a)
        {
            ++a._value;
            std::cout << a._value << "\n";
        }, xs_pack);
    };
};
```

As an example, the following code snippet...

```cpp
A my_a;
auto l_inner = foo(my_a, A{});
l_inner();
l_inner();

std::cout << my_a._value << "\n";
```

...will print:

```bash
1 # 1st invocation - `my_a`
1 # 1st invocation - `A{}`
2 # 2nd invocation - `my_a`
2 # 2nd invocation - `A{}`
2 # `my_a._value`
```

[*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/FkHFc2PhMX4VkVrc)



### A simpler solution? {#simpler_solution}

*(This section was added on 12/12/2016.)*

As shown by [*Nikos Athanasiou* on reddit](https://www.reddit.com/r/cpp/comments/5hset9), it's possible to solve this issue in a much simpler way. All we need is `std::tuple` and some knowledge of [*template argument deduction* rules](http://en.cppreference.com/w/cpp/language/template_argument_deduction).

Let's think about what `fwd_capture_wrapper` is doing:

* Given an *lvalue reference*, it stores an *lvalue reference*.

* Given an *rvalue reference*, it stores a *value*.

Again... this is [something I asked on StackOverflow a while ago](http://stackoverflow.com/questions/33421323).  *(I find myself coming back to my own questions very often.)*

Nikos suggested that it's sufficient to use `std::tuple` as our wrapper, using some sort of *"type trait"* that keeps *lvalue references*:

```cpp
template <typename T>
using capture_t = std::conditional_t
<
    std::is_lvalue_reference<T>{},

    std::add_lvalue_reference_t<T>,
    std::remove_reference_t<T>
>;

template <typename... Ts>
auto fwd_capture(Ts&&... xs)
{
    return std::tuple<capture_t<Ts>...>(FWD(xs)...);
}
```

This **works**, and doesn't require all the *wrapper* boilerplate. [`std::get`](http://en.cppreference.com/w/cpp/utility/tuple/get) needs to be used in order to access the captured object, but it can always be wrapped in a nicer interface:

```cpp
template <typename T>
decltype(auto) access(T&& x)
{
    return std::get<0>(FWD(x));
}
```

[*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/nXLz6ZCpdembIYTW)

Guess what - it can be simplified *even further*. Turns out that **`capture_t` is actually redundant**! This definition of `fwd_capture` is therefore equivalent to the previous one:

```cpp
template <typename... Ts>
auto fwd_capture(Ts&&... xs)
{
    return std::tuple<Ts...>(FWD(xs)...);
}
```

> Why?

Given this example function...

```cpp
template <typename T>
void foo(T&&)
{
    // What is `T` here?
    std::tuple<T>{};
}
```

...let's look at [the *latest standard draft*](https://timsong-cpp.github.io/cppwp/temp.deduct.call#def:forwarding_reference):

*("P" below means "parameter".)*

> *(from [**$14.8.2.1.3**](https://timsong-cpp.github.io/cppwp/temp.deduct.call#3))*
>
> If P is a cv-qualified type, the top-level cv-qualifiers of P's type are ignored for type deduction. **If P is a reference type, the type referred to by P is used for type deduction.** Example:
> ```cpp
> template<class T> int f(const T&);
> int n1 = f(5);     // calls f<int>(const int&)
>
> const int i = 0;
> int n2 = f(i);     // calls f<int>(const int&)
>
> template <class T> int g(volatile T&);
> int n3 = g(i);     // calls g<const int>(const volatile int&)
> ```

The quote above means that `T` is deduced as `T` when an *rvalue reference* is passed to `foo`.

> *(from [**$14.8.2.1.3**](https://timsong-cpp.github.io/cppwp/temp.deduct.call#3))*
>
> A forwarding reference is an rvalue reference to a cv-unqualified template parameter that does not represent a template parameter of a class template (during class template argument deduction ([over.match.class.deduct])). **If P is a forwarding reference and the argument is an lvalue, the type “lvalue reference to A” is used in place of A for type deduction.** Example:
> ```cpp
> template <class T> int f(T&& heisenreference);
> template <class T> int g(const T&&);
> int i;
>
> int n1 = f(i);     // calls f<int&>(int&)
>
> int n2 = f(0);     // calls f<int>(int&&)
>
> int n3 = g(i);     // error: would call g<int>(const int&&), which
>                    // would bind an rvalue reference to an lvalue
> ```

The quote above means that `T` is deduced as `T&` when an *lvalue reference* is passed to `foo`.

Great! This means that...

```cpp
template <typename... Ts>
auto fwd_capture(Ts&&... xs)
{
    return std::tuple<Ts...>(FWD(xs)...);
}
```

...is all we need!

[*(Complete example on **wandbox**.)*](http://melpon.org/wandbox/permlink/dsjMViJ915lbsbe1)

> What about varidic arguments?

Adapting this technique to variadic argument packs then becomes *trivial* - we just create a tuple of tuples:

```cpp
#define FWD_CAPTURE(...) impl::fwd_capture(FWD(__VA_ARGS__))

template <typename... Ts>
auto fwd_capture_as_tuple(Ts&&... xs)
{
    return std::make_tuple(FWD_CAPTURE(xs)...);
}
```

[*(Complete example on **wandbox**.)*](
http://melpon.org/wandbox/permlink/MMsACfPcsFL8QgLy)
