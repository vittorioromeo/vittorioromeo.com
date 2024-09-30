


<style>
.inline-link
{
    font-size: small;
    margin-top: -2.6em;
    text-align: right;
    font-weight: bold;
}
</style>

This post continues *(ends?)* the story of my attempts to create a nice "pattern matching" syntax for variant visitation.

Back in October 2016, in [**part 1**](https://vittorioromeo.com/index/blog/variants_lambdas_part_1.html) of the series, I discussed a simple way of implementing a `visit_in_place` function that would allow variants to be visited providing a set of *lambda expressions* that would be overloaded on the spot.

```cpp
std::variant<success, failure> payload{/* ... */};
visit_in_place(payload, [](const success& x){ /* ... */ },
                        [](const failure& x){ /* ... */ });
```

<div class="inline-link">

[*(on wandbox)*](https://wandbox.org/permlink/L0HblAifMkqy3VW0)

</div>

Ten days later I published [**part 2**](http://vittorioromeo.com/index/blog/variants_lambdas_part_2.html), that demonstrated how to apply the same idea to *recursive lambdas*, by using the [Y combinator](https://en.wikipedia.org/wiki/Fixed-point_combinator#Fixed_point_combinators_in_lambda_calculus) higher order function and by using an extra `auto` parameter in the provided lambdas. Below is an example that evaluates a simple arithmetical expression.

```cpp
using op = std::variant<add, sub>;

struct expr_wrapper;
using recr_expr = std::tuple<int, op, expr_wrapper>;
using expr = std::variant<int, std::unique_ptr<recr_expr>>;

// ...

auto evaluator = make_recursive_visitor<int>(
    [](auto, int x) { return x; },
    [](auto recurse, const std::unique_ptr<recr_expr>& x)
    {
        const auto& [lhs, op, rhs] = *x;
        return visit_in_place(op,
            [&](add){ return lhs + recurse(rhs); },
            [&](sub){ return lhs - recurse(rhs); });
    }
);

// ...

expr e{recr(2, add{}, recr(6, sub{}, 3))};
assert(visit(evaluator, e) == 5);
```

<div class="inline-link">

[*(on wandbox)*](https://wandbox.org/permlink/j8R4DzaNGzmmbiVY)

</div>

I then recently gave a talk called [**"Implementing variant visitation using lambdas"**](https://www.youtube.com/watch?v=mqei4JJRQ7s) at ACCU 2017, which showed how to implement the topics covered in the previously mentioned articles step-by-step.

As a final step, I'm currently working on [**"scelta"**](https://github.com/SuperV1234/scelta), a small library that provides syntactic sugar for *variant* and *optional* types, independently from the chosen implementation.



### the problem (nitpick?)

Let's look again at the recursive lambda-based visitation example:

```cpp
auto evaluator = make_recursive_visitor<int>(
    [](auto, int x) { return x; },
    [](auto recurse, const std::unique_ptr<recr_expr>& x)
    {
        /* ... */
    }
);
```



Did you notice how the `auto` parameter appears even in the base case? Ideally, I would like `evaluator` to be defined as follows:

```cpp
auto evaluator = make_recursive_visitor<int>(
    [](int x) { return x; },
    [](auto recurse, const std::unique_ptr<recr_expr>& x)
    {
        /* ... */
    }
);
```

When visiting with it, the extra `recurse` argument should be provided only if a recursive case is matched. This greatly improves the visitation syntax when there multiple base cases - here's a *pseudocode* JSON example:

```cpp
auto printer = make_recursive_visitor<std::string>(
    [](const json_null& x)   { /* ... */ },
    [](const json_true& x)   { /* ... */ },
    [](const json_false& x)  { /* ... */ },
    [](const json_string& x) { /* ... */ },
    [](const json_number& x) { /* ... */ },
    [](auto recurse, const json_array& x)
    {
        /* ... */
    },
    [](auto recurse, const json_object& x)
    {
        /* ... */
    }
);
```

The existence of the extra unnecessary `auto` parameter has been irrationally bothering me since I wrote the "part 2" article. In fact, this is I what I claimed back then:

> I am working on a solution that detects the arity of lambdas at compile-time (yes, even generic lambdas). I am also experimenting with detecting whether or not a lambda is generic in order to automatically deduce the return type of the recursive visitor. *[...]* As soon as I clean it up and make sure that there are no hidden gotchas, I will write an article about its implementation.

Unfortunately, I did encounter some "gotchas".



### deducing arity of lambdas

My first idea was as follows: simply deduce the arity of the lambdas, and provide a `recurse` argument only for the ones that are binary.

When the *function objects* passed to `make_recursive_visitor` are not overloaded or not templates, it is easy to get their arity and parameter types by using something like [CallableTraits](https://github.com/badair/callable_traits) or by writing a set of templates similar to:

```cpp
template <typename T, typename Return, typename... Args>
struct function_traits<Return (T::*)(Args...)>
{
    static constexpr auto arity = sizeof...(TArgs);
};
```

We could assume that all the non-deducible *(i.e. overloaded or template)* lambdas are binary *(due to the `auto recurse` parameter)*, but that's not always the case. A user might provide an unary template lambda as a "default case", or an overloaded function object.

```cpp
make_recursive_visitor(
    [](foo)       { }, // handle foo
    [](auto)      { }, // handle other base cases
    [](auto, bar) { }  // recurse on bar
);
```

A possible way of deducing the arity of *generic lambdas* (and any other overloaded/template function object) is to attempt to call it and check whether or not the invocation was well-formed - [`std::is_invocable`](http://en.cppreference.com/w/cpp/types/is_invocable) allows us to check this easily.

Since a generic lambda takes one or more `auto` parameters, what should we attempt to call it with? We'll use an object that is implicitly convertible to anything - I first learned this trick from Kris Jusiak's [Boost.DI](https://github.com/boost-experimental/di) talk at C++Now 2016.

```cpp
struct any_type
{
    template <typename T>
    constexpr operator T() noexcept
    {
        return {};
    }
};

template <typename F>
using is_binary = std::is_invocable<F, any_type, any_type>;
```

A naive test of `is_binary` produces promising results:

```cpp
auto l0 = [](int)        { };
auto l1 = [](auto)       { };
auto l2 = [](int,  int)  { };
auto l3 = [](int,  auto) { };
auto l4 = [](auto, auto) { };

static_assert(!is_binary<decltype(l0)>::value);
static_assert(!is_binary<decltype(l1)>::value);
static_assert( is_binary<decltype(l2)>::value);
static_assert( is_binary<decltype(l3)>::value);
static_assert( is_binary<decltype(l4)>::value);
```

<div class="inline-link">

[*(on wandbox)*](https://wandbox.org/permlink/OA7IQg1pPnXQeldX)

</div>

Unfortunately, things become broken quite quickly...

```cpp
auto l = [](int, auto x){ x.foo(); };
static_assert(is_binary<decltype(l)>::value);
```

```
error: 'struct any_type' has no member named 'foo'
     auto l = [](int, auto x){ x.foo(); };
                               ~~^~~
```

<div class="inline-link">

[*(on wandbox)*](https://wandbox.org/permlink/NcnIPfHVtw3OoeEB)

</div>

While `any_type` is implicitly convertible to anything, it obviously doesn't provide a `.foo()` member function. Additionally, the `static_assert` doesn't really fail - the program produces an "hard error" which cannot be used in a SFINAE context!

I've asked [a related question](stackoverflow.com/questions/42739837/getting-sfinae-to-work-with-is-callable-on-an-overloaded-function-object) on StackOverflow. In short, the issue is that the error happens during template instantiation and that there is no way for the compiler to detect an invalid instantiation and "roll it back".

Exposing `x.foo()` in the signature of the lambda prevents a hard error from occurring...

```cpp
auto l = [](int, auto x) -> decltype(x.foo()) { x.foo(); };
```

...but makes the `static_assert` fail, as the lambda is not invocable with `any_type`.

This is a non-issue if the second type is concrete...

```cpp
struct bar { void foo(); };

auto l0 = [](auto, bar x) { x.foo(); };
static_assert(is_binary<decltype(l0)>::value); // passes
```

<div class="inline-link">

[*(on wandbox)*](https://wandbox.org/permlink/VkYtZuLAB2tw5HSi)

</div>

...but it doesn't play nicely with concepts and SFINAE constraints:


```cpp
template <typename T>
concept bool HasFoo = requires(T x) { x.foo(); };

auto l1 = [](auto, HasFoo x) { x.foo(); };
static_assert(is_binary<decltype(l1)>::value); // fails!

auto l2 = [](auto, auto x) -> decltype(x.foo()) { x.foo(); };
static_assert(is_binary<decltype(l2)>::value); // fails!
```

<div class="inline-link">

[*(on wandbox)*](https://wandbox.org/permlink/Swu7tyeGuTlDAelV)

</div>

While this behavior might be fine for most use cases of "in place" lambda-based recursive variant visitation, the corner cases are in my opinion very unintuitive and undesirable.

Ideally, I'd like to be able to ask the compiler how many parameters appear in the declaration of the functions, via some sort of *static reflection*.

```cpp
// pseudocode

auto l = [](auto, int, float) { whatever(); };
constexpr auto arity = $l.arity();
static_assert(arity == 3);
```

Unfortunately the current proposals *(see [P0590](http://wg21.link/p0590) and [P0578](http://wg21.link/p0578))* do not seem to allow this for lambdas and/or function templates.



### using existing knowledge

There is some information that we can use to help us deduce the arity.

* In a base case, there will only be a single parameter, which must be one of the variant alternative types.

* In a recursive case, there will be two parameters.

    * The object passed to the `auto recurse` parameter will only be invoked. This means that we can use some kind of "placeholder" invocable type to detect it. The placeholder's `operator()` will return the explicitly-specified visitor return type.

    * The second parameter must be one of the variant alternative types.

Therefore we could define two *type traits* as follows:

```cpp
// pseudocode

template <typename F, /* ... */>
using is_base_case = std::is_invocable<F, /* any variant alternative */>;

struct recurse_placeholder
{
    template <typename... Ts>
    /* visitor return type */
    operator()(Ts&&...) const noexcept { return {}; }
};

template <typename F, /* ... */>
using is_recursive_case = std::is_invocable<F, recurse_placeholder, /* any variant alternative */>;
```

Note that the above strategy will prevent arity from being incorrectly deduced *(e.g. like the `static_assert` failing above due to SFINAE)*, but will still fail in a case like

```cpp
auto l = [](auto, auto x){ x.foo(); };
```

due to errors occurring during the instantiation. I think that this is acceptable, as the case above is pathological and unlikely to occur in real code.

The implementation of the above utilities is pretty simple:

```cpp
template <typename Return>
struct recurse_placeholder
{
    template <typename... Ts>
    Return operator()(Ts&&...) const noexcept { return {}; }
};

template <typename F, typename... Alternatives>
using is_base_case = std::disjunction<
    std::is_invocable<F, Alternatives>...
>;

template <typename F, typename Return, typename... Alternatives>
using is_recursive_case = std::disjunction<
    std::is_invocable<F, recurse_placeholder<Return>, Alternatives>...
>;
```

*(Note that a real implementation would be significantly more complicated, as it would need to support visitation of multiple variants at the same time.)*

After adding some boilerplate to extract the alternatives from a variant, it's easy to show that this approach works nicely with concepts and SFINAE-constrained lambdas:

```cpp
struct bar { void foo(); };
using v = std::variant<int, bar>;

auto l1 = [](auto, HasFoo x) { x.foo(); };
static_assert(is_recursive_case<decltype(l1), void, v>()); // OK

auto l2 = [](auto, auto x) -> decltype(x.foo()) { x.foo(); };
static_assert(is_recursive_case<decltype(l2), void, v>()); // OK
```

<div class="inline-link">

[*(on wandbox)*](https://wandbox.org/permlink/fjACZnWKAWjvPLm9)

</div>

Unfortunately, it still completely fails when `auto` is used as a "default" base case. Consider this scenario:

```cpp
struct a { void foo(); };
struct b { void bar(); };
struct c { void bar(); };
struct d { void bar(); };

using v = std::variant<a, b, c, d, std::vector<SELF>;
//                                 ^^^^^^^^^^^^^^^^^
//                       pseudocode for recursive variant
```

Three of the variant's alternatives have the same interface. It would be desirable for the user to write something similar to this:

```cpp
make_recursive_visitor<void>(
    [](const a& x)    { /* handle x.foo() */ },   // (0)
    [](const auto& x) { /* handle x.bar() */ },   // (1)
    [](auto recurse, const std::vector<SELF>& x)
    {
        /* recursive case */
    }
);
```

* When testing `(0)` with `is_base_case`, the check passes as there are no errors during instantiation and as the lambda can be called with at least one of the variant's alternatives.

* When testing `(1)` with `is_base_case`, a hard compilation error is produced! This happens because the first check is something along the lines of:

    ```cpp
    std::is_invocable<decltype(/* (1) */), a>
    ```

The only way to prevent this is to **completely forget about** `is_base_case` and only check with `is_recursive_case` - lambdas that do not satisfy the check will be assumed to be unary.

Even though I mentioned that this kind of behavior might be acceptable in real world scenarios, I didn't feel comfortable with this "solution". In the current version of [`scelta`](https://github.com/SuperV1234/scelta) I force the users to provide an unused `auto` parameter in base cases.



### conclusion?

Sadly, this article doesn't provide any robust solution to the covered issues.

Please note that I understand the fact that a recursive in place visitation utility will be rarely used in comparison to a
non-recursive one or to a "traditional" visitor struct that doesn't require *Y combinators* to recurse.

While it would have been nice to provide a zero syntactical overhead `recursive_visit_in_place` function, all of this experimentation happened because I was obsessed with that little unnecessary `auto` and wanted to see if there was a way to get rid of it.

Did I waste my time? Somewhat, but I learned very interesting things from this ordeal.

* `std::is_invocable` and similar traits can only look at the signature of a *function object*, never at the body. This becomes obvious quickly, but it can feel jarring when you think of the logic as *"is this particular function valid to call with this type?"*. Being able to detect invalid instantiations and roll them back is not going to happen due to implementation difficulties. There is a great article by *Andrzej Krzemieński* on the subject, [**"Diagnosable validity"**](https://akrzemi1.wordpress.com/2016/05/10/diagnosable-validity/).

    > There is no way to check it from within C++, and there is not going to be any: it has been clearly laid out, that compiler vendors cannot be forced to implement a “speculative compilation” and backtracking from arbitrarily deep template instantiation failures.

    And a related insightful discussion on `std-proposals`: [**"Asserting an expected compile-time failure"**](https://groups.google.com/a/isocpp.org/forum/#!searchin/std-proposals/test$20if$20an$20error/std-proposals/ir1Jq6mtuiE/Lw5fvGlvwpEJ).

* I learned how to abuse *ambiguities in derived-to-base conversions* to overload a bunch of function objects by reference. This happened when the user **bogdan** surprised everyone with an amazing trick on my StackOverflow question: [**"Overloading multiple function objects by reference"**](http://stackoverflow.com/questions/42987144/overloading-multiple-function-objects-by-reference). I posted it because I initially thought that this could solve the issues mentioned in this article, but it actually doesn't...

* I realized that I really want [**"Abbreviated Lambdas for Fun and Profit"**](http://wg21.link/p0573) or some sort of new language feature that generates a complete `noexcept(...)` and `-> decltype(...)` return type from the body of a function lambda. Currently, you have to triplicate your code!

    ```cpp
    auto f = [](auto x)
            noexcept(noexcept(  x.foo()  ))
            -> decltype(        x.foo()  )
            { return            x.foo(); };
    ```

    A new `=> { ... }` body introduction syntax could perform the triplication for you.

    ```cpp
    auto f = [](auto x) => { return x.foo(); };
    ```

Finally, I learned that when there are a lot of things in the language against you, it is not wise to obsess over your goal, trying to find hacky solutions and workaround. The correct approach is improving the language - this is why I hope that some sort of *language-level* pattern matching will make it into C++. There currently is a proposal by *David Sankel* for that: [**"Pattern Matching and Language Variants"**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0095r1.html).

Fingers crossed for C++20!



### series

* [*"visiting variants using lambdas - part 1"*](https://vittorioromeo.com/index/blog/variants_lambdas_part_1.html)

* [*"visiting variants using lambdas - part 2"*](https://vittorioromeo.com/index/blog/variants_lambdas_part_2.html)

* [*"visiting variants using lambdas - part 3"*](https://vittorioromeo.com/index/blog/variants_lambdas_part_3.html)
