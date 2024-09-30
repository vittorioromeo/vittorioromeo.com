


In my previous article, [*"visiting variants using lambdas - part 1"*](https://vittorioromeo.com/index/blog/variants_lambdas_part_1.html), I wrote about a simple technique *(using [`boost::hana`](http://www.boost.org/doc/libs/1_62_0/libs/hana/doc/html/index.html))* that allows variant visitation using lambdas.

The technique consisted in passing several lambdas to [`boost::hana::overload`](http://www.boost.org/doc/libs/1_61_0/libs/hana/doc/html/group__group-functional.html#ga83e71bae315e299f9f5f9de77b012139) in order to create a *"local"* visitor, without having to define a `class`/`struct`.

<p></p>

Here's an example:

```cpp
using vnum = vr::variant<int, float, double>;

auto vnp = make_visitor
(
    [](int x)    { cout << x << "i\n"; },
    [](float x)  { cout << x << "f\n"; },
    [](double x) { cout << x << "d\n"; }
);

// Prints "0i"
vnum v0{0};
vr::visit(vnp, v0);

// Prints "5f"
v0 = 5.f;
vr::visit(vnp, v0);

// Prints "33.51d"
v0 = 33.51;
vr::visit(vnp, v0);
```

*(You can find a similar example [on GitHub](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/visiting_variants/2_lambda_visitation.cpp).)*



### "Recursive" variants

A *"recursive" variant* is a variant which can **contain itself**, and can be used to **represent recursive structures** *(e.g. JSON objects)*.

Neither [`std::variant`](http://en.cppreference.com/w/cpp/utility/variant) nor [`boost::variant`](http://www.boost.org/doc/libs/1_62_0/doc/html/variant.html) make use of dynamic memory allocation, however - you can think of them as [tagged unions](https://en.wikipedia.org/wiki/Tagged_union). This means that variants **have fixed size**, and that the snippet below will not compile:

```cpp
struct my_variant
{
    // ...what is `sizeof(_data)`?
    vr::variant<int, my_variant> _data;
};
```

Therefore, in order to allow the definition of recursive variants, **indirection must be used**. Making use of dynamic memory allocation is a straightforward way of achieving this: [`std::unique_ptr`](http://en.cppreference.com/w/cpp/memory/unique_ptr) or [`std::vector`](http://en.cppreference.com/w/cpp/container/vector) are examples of data structures that can be used to define recursive variants. Here's an example that compiles:

```cpp
struct my_variant
{
    // OK!
    vr::variant<int, std::unique_ptr<my_variant>> _data;
};
```

Let's move on by extending the `vnum` variant type *(seen in [part 1](https://vittorioromeo.com/index/blog/variants_lambdas_part_1.html))* to support vectors of other `vnum` instances.

The first problem is that the variant will have to refer to itself in its own type alias definition - one possible solution is **forward-declaring** a `vnum_wrapper` class *(which can be safely "stored" in `std::vector` since the approval of [N4510](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4510.html))*:

```cpp
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
            : _data{std::forward<Ts>(xs)...}
        {
        }
    };
}

// Expose `vnum` and `varr` to the user
using vnum = impl::vnum;
using impl::varr;
```

Thanks to the indirection provided by `std::vector` and to the wrapper class, `vnum` can now be used recursively:

```cpp
vnum v0 = 0;
vnum v1 = 5.f;
vnum v2 = 33.51;
vnum v3 = varr{vnum{1}, vnum{2.0}, vnum{3.f}};
vnum v4 = varr{vnum{5}, varr{vnum{7}, vnum{8.0}, vnum{9.}}, vnum{4.f}};
```

*(Note that creating something similar to `vnum_wrapper` works well with both `boost::variant` and `std::variant`. There is a small caveat: it does not compile with `libc++` unless the constructor is constrained using [`enable_if`](http://en.cppreference.com/w/cpp/types/enable_if). See [the addendum section](#libcpp_constraint) for more information.)*

Let's take a look visitation techniques in the following sections.



### *"Traditional"* recursive visitation

As seen in the last article, this technique requires the definition of a separate `class`/`struct`. The implementation is straightforward:

```cpp
struct vnum_printer
{
    void operator()(int x)    { cout << x << "i\n"; }
    void operator()(float x)  { cout << x << "f\n"; }
    void operator()(double x) { cout << x << "d\n"; }

    void operator()(const varr& arr)
    {
        for(const auto& x : arr)
        {
            vr::visit_recursively(*this, x);
        }
    }
};
```

The `vr::visit_recursively` function is a simple wrapper for `vr::visit` that hides the `vnum_wrapper::_data` access:

```cpp
template <typename TVisitor, typename TVariant>
decltype(auto) visit_recursively(TVisitor&& visitor, TVariant&& variant)
{
    return vr::visit
    (
        std::forward<TVisitor>(visitor),
        std::forward<TVariant>(variant)._data
    );
}
```

All that's left is invoking `vr::visit`, and everything *just works™*:

```cpp
// Prints "0i".
vnum v0{0};
vr::visit(vnum_printer{}, v0);

// Prints "5f".
v0 = 5.f;
vr::visit(vnum_printer, v0);

// Prints "33.51d".
v0 = 33.51;
vr::visit(vnum_printer, v0);

// Prints "1i 2d 3f".
v0 = varr{vnum{1}, vnum{2.0}, vnum{3.f}};
vr::visit(vnp, v0);

// Prints "5i 7i 8d 9d 4f".
v0 = varr{vnum{5}, varr{vnum{7}, vnum{8.0}, vnum{9.}}, vnum{4.f}};
vr::visit(vnp, v0);
```

*(You can find a similar example [on GitHub](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/visiting_recursive_variants/0_traditional.cpp).)*



### *"Lambda-based"* recursive visitation - take one

Applying the `boost::hana::overload` solution seen in [part one](https://vittorioromeo.com/index/blog/variants_lambdas_part_1.html) to recursive variants seems like a reasonable plan.

```cpp
auto my_visitor = boost::hana::overload
(
    [](int x)    { cout << x << "i\n"; },
    [](float x)  { cout << x << "f\n"; },
    [](double x) { cout << x << "d\n"; },

    [&](const varr& arr)
    {
        for(const auto& x : arr)
        {
            vr::visit_recursively(my_visitor, x);
        }
    }
);
```

Unfortunately, we are greeted with a *compiler error*:

> error: variable 'my_visitor' declared with 'auto' type cannot appear in its own initializer

*(You can find a similar example [on GitHub](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/visiting_recursive_variants/2_lambda_notworking.cpp).)*

In short, the problem is that `my_visitor`'s type will be deduced from its own initialization... but `my_visitor` is also part of the initialization! If we had a way to explicitly specify the lambda's type in place of `auto`, the above code snippet could compile. [More details about this issue can be found here.](http://stackoverflow.com/questions/7861506/recursive-call-in-lambda-c11)

One common solution that is used to implement recursive lambdas is using [`std::function`](http://en.cppreference.com/w/cpp/utility/functional/function), which allows `auto` to be replaced with an explicit type that does not need to be deduced. Unfortunately `std::function` **is not a zero-cost abstraction**, as it's a general-purpose polymorphic wrapper. *(Benchmarks supporting my claim will be shown later in this article.)*



### *"Lambda-based"* recursive visitation - take two

Bringing [algebraic data types](https://en.wikipedia.org/wiki/Algebraic_data_type) from the functional programming world into C++ isn't enough - we're also going to adopt another powerful construct: the [**Y Combinator**](https://en.wikipedia.org/wiki/Fixed-point_combinator#Fixed_point_combinators_in_lambda_calculus).

Long story short, this *fixed-point combinator* allows recursion to be implemented in programming languages that do not natively support it. The previous statement **applies to C++ lambdas**: we can use the Y Combinator to implement recursion. *(A very good in-depth explanation of the combinator [is available here](http://mvanier.livejournal.com/2897.html).)*

Thankfully, a production-ready implementation of the Y Combinator is available as [`boost::hana::fix`](http://www.boost.org/doc/libs/1_61_0/libs/hana/doc/html/group__group-functional.html#ga1393f40da2e8da6e0c12fce953e56a6c). Here's an example of its usage:

```cpp
auto factorial = boost::hana::fix([](auto self, auto n) -> int
    {
        if(n == 0)
        {
            return 1;
        }

        return n * self(n - 1);
    });

assert(factorial(5) == 120);
```

Here are some important points you need to take note of:

* `factorial`'s type is deduced through `auto`. No additional indirection *a la* `std::function` is introduced here.

* `boost::hana::fix` takes a callable object with the *desired arity plus one* as its argument, because **"the lambda is passed to itself"** on every recursive step as the `self` parameter.

* The recursive step is implemented by calling `self`, **not** `factorial`.

    * Note that `factorial` does not appear in the body of the lambda, thus avoiding the previously seen compiler error.

* `boost::hana::fix` requires a [*trailing return type*](http://en.cppreference.com/w/cpp/language/function).

* The `factorial` function can be called as usual - the user does not have to pass any additional argument to it.

*(If you are interested in learning how to implement your own Y Combinator, check out [this question](http://stackoverflow.com/questions/35608977/understanding-y-combinator-through-generic-lambdas) I asked on StackOverflow when trying to understand the construct and write my own version of it.)*

Now that we have a way of defining recursive lambdas, we can finally implement a recursive lambda-based visitor. In order to make it easy for the user to implement their own visitors, a `make_recursive_visitor` function will be provided, which can be used as follows:

```cpp
// The desired return type must be explicitly specified.
auto vnp = make_recursive_visitor<void>
(
    // Non-recursive cases.
    // The first argument is ignored.
    [](auto, int x)    { cout << x << "i\n"; },
    [](auto, float x)  { cout << x << "f\n"; },
    [](auto, double x) { cout << x << "d\n"; },

    // Recursive case.
    // The first argument allows recursive visitation.
    [](auto recurse, const varr& arr)
    {
        for(const auto& x : arr)
        {
            recurse(x);
        }
    }
);
```

All the *magic* is inside `make_recursive_visitor` - here's its commented implementation:


```cpp
template <typename TReturn, typename... TFs>
auto make_recursive_visitor(TFs&&... fs)
{
    // Create and return a Y Combinator that allows the visitor to call itself.
    // The trailing return type is required.
    return boost::hana::fix([&fs...](auto self, auto&& x) -> TReturn
        {
            // Immediately build and call an overload of all visitor "branches".
            // The created overload is called with:
            //
            // * A function that takes a variant and visits it recursively as
            //   the first argument.
            //
            // * The current value of the variant as the second argument.
            //
            return boost::hana::overload(std::forward<TFs>(fs)...)(
                [&self](auto&& v)
                {
                    return vr::visit_recursively(self, v);
                },
                std::forward<decltype(x)>(x));
        });
}
```

*(Note that the return type could probably be deduced inside `make_recursive_visitor` by inspecting the return type of every passed lambda using `decltype`.)*

Now we can put everything together to finally **visit a recursive variant using lambdas!**

```cpp
auto vnp = make_recursive_visitor<void>
(
    [](auto, int x)    { cout << x << "i\n"; },
    [](auto, float x)  { cout << x << "f\n"; },
    [](auto, double x) { cout << x << "d\n"; },
    [](auto recurse, const varr& arr)
    {
        for(const auto& x : arr) recurse(x);
    }
);

// Prints "0i".
vnum v0{0};
vr::visit(vnp, v0);

// Prints "5f".
v0 = 5.f;
vr::visit(vnp, v0);

// Prints "33.51d".
v0 = 33.51;
vr::visit(vnp, v0);

// Prints "1i 2d 3f".
v0 = varr{vnum{1}, vnum{2.0}, vnum{3.f}};
vr::visit(vnp, v0);

// Prints "5i 7i 8d 9d 4f".
v0 = varr{vnum{5}, varr{vnum{7}, vnum{8.0}, vnum{9.}}, vnum{4.f}};
vr::visit(vnp, v0);
```

Again, keep in mind that this technique does not work well if you require *stateful* visitors - use a `class`/`struct` in that situation.

*(You can find a similar example [on GitHub](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/visiting_recursive_variants/1_lambda.cpp).)*


You're probably now asking...

> Why go through all that trouble? Why not just use `std::function`?

As I mentioned earlier, `std::function` is not a *zero-cost abstraction*. To prove it, [I've written a simple factorial lambda test](https://github.com/SuperV1234/Experiments/blob/master/recursive_lambda_asm/x.cpp) that can be conditionally compiled to either use `std::function` or `boost::hana::fix`. The [complete results are available in the addendum section](#stdfunction_vs_ycombinator). In short:

* `g++ -O3` produces **4572** bytes of assembly for `std::function`. *(!)*

* `g++ -O3` produces **1583** bytes of assembly for `boost::hana::fix`.

* `clang++ -O3` produces **7146** bytes of assembly for `std::function`. *(!)*

* `clang++ -O3` produces **765** bytes of assembly for `boost::hana::fix`.



### *"Lambda-based"* recursive visitation - *take three?*

If you value minimalist and elegant interfaces, you may have noticed that the user is forced to add an extra `auto` parameter to every non-recursive visitor lambda. This is a very annoying problem which is not easy to solve.

I am working on a solution that detects the arity of lambdas at compile-time *(yes, even generic lambdas)*. I am also experimenting with detecting whether or not a lambda is generic in order to automatically deduce the return type of the recursive visitor.

So far, the following code seems to work properly on both major compilers and with both `std::variant` and `boost::variant`:

```cpp
make_recursive_visitor<void>
(
    // "Base cases", unary.
    [](int x)    { cout << x << "i\n"; },
    [](float x)  { cout << x << "f\n"; },
    [](double x) { cout << x << "d\n"; },

    // "Special case", constrained.
    [](auto x) -> enable_if_t<is_arithmetic<decay_t<decltype(x)>>{}>
    {
        cout << x << "n\n";
    },

    // Recursive case, `std::vector`.
    [](auto recurse, const varr& x)
    {
        for(const auto& y : x) recurse(y);
    },

    // Recursive case, `std::map`.
    [](auto recurse, const vmap& x)
    {
        for(const auto& y : x) recurse(y.second);
    }
);
```

The code above can be successfully used to visit a recursive variant. The lambdas can be passed in any order to `make_recursive_visitor`. As soon as I clean it up and make sure that there are no hidden *gotchas*, I will write an article about its implementation.



### Addendum

#### Fixing `vnum_wrapper` in `libc++` {#libcpp_constraint}

When attempting to visit *(either "traditionally" or with lambdas)* a recursive variant defined using a wrapper similar to `vnum_wrapper`, everything seems to work properly... until you try compiling with **libc++**. That's when you get an **horrible error**...

![*Recursive variant visitation error on libc++*](resources/img/blog/variants_libcpp_error.png)

...and that's when you [ask a question on StackOverflow](http://stackoverflow.com/questions/40018753).

Thank's to [T.C.](http://stackoverflow.com/users/2756719/t-c)'s extremely helpful and in-depth reply, I did not only manage to solve the issue, but also to understand its obscure cause. I strongly recommend going through the SO thread to learn more about this.

In the end, adding this [`std::enable_if_t`](http://en.cppreference.com/w/cpp/types/enable_if) to the wrapper's constructor did the trick:

```cpp
std::enable_if_t
<
    !std::disjunction_v
    <
        std::is_same<std::decay_t<Ts>, my_variant_wrapper>...
    >
>
```

A complete minimal example is available [here on **wandbox**](http://melpon.org/wandbox/permlink/VKwUbJOPAatY0kYY).

#### `std::function` vs Y combinator {#stdfunction_vs_ycombinator}

Here are the full results for the simple benchmark I ran. The goal of the benchmark was to show people the overhead introduced by `std::function`, which is a very generic callable object wrapper that can introduce dynamic memory allocation and `virtual`-like function call overhead.

The benchmarks compares the size of the generated assembly between these two *recursive factorial* implementations:

```cpp
#ifndef USE_YCOMBINATOR

std::function<int(int)> f = [&](int x)
{
    if(x == 0)
        return 1;
    return x * f(x - 1);
};

#else

auto f = boost::hana::fix([](auto self, int x) -> int
{
    if(x == 0)
        return 1;
    return x * self(x - 1);
});

#endif
```

The tested compilers were:

* `g++ 6.2.1`

* `clang++ 3.8.1`

The code and the assembly output [is available here](https://github.com/SuperV1234/Experiments/tree/master/recursive_lambda_asm).

##### `g++ -O1`

|                  | Bytes | Relative size change |
|:----------------:|:-----:|:--------------------:|
| Baseline         | 257   |   0                  |
| `hana::fix`      | 1983  |   +671%              |
| `std::function`  | 4531  |   +1663%             |



##### `g++ -O2`

|                  | Bytes | Relative size change |
|:----------------:|:-----:|:--------------------:|
| Baseline         | 307   |   0                  |
| `hana::fix`      | 1583  |   +415%              |
| `std::function`  | 4572  |   +1389%             |



##### `g++ -O3` *(and `-Ofast`)*

|                  | Bytes | Relative size change |
|:----------------:|:-----:|:--------------------:|
| Baseline         | 307   |   0                  |
| `hana::fix`      | 1583  |   +415%              |
| `std::function`  | 4572  |   +1389%             |






##### `clang++ -O1`

|                  | Bytes | Relative size change |
|:----------------:|:-----:|:--------------------:|
| Baseline         | 680   |   0                  |
| `hana::fix`      | 2347  |   +245%              |
| `std::function`  | 18638 |   +2640%             |



##### `clang++ -O2`

|                  | Bytes | Relative size change |
|:----------------:|:-----:|:--------------------:|
| Baseline         | 680   |   0                  |
| `hana::fix`      | 1848  |   +171%              |
| `std::function`  | 7146  |   +950%              |



##### `clang++ -O3` *(and `-Ofast`)*

|                  | Bytes | Relative size change |
|:----------------:|:-----:|:--------------------:|
| Baseline         | 680   |   0                  |
| `hana::fix`      | 765   |   +12.5%             |
| `std::function`  | 7146  |   +950%              |



### series

* [*"visiting variants using lambdas - part 1"*](https://vittorioromeo.com/index/blog/variants_lambdas_part_1.html)

* [*"visiting variants using lambdas - part 2"*](https://vittorioromeo.com/index/blog/variants_lambdas_part_2.html)

* [*"visiting variants using lambdas - part 3"*](https://vittorioromeo.com/index/blog/variants_lambdas_part_3.html)
