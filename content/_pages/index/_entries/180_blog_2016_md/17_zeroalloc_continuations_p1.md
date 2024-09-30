


<style>
.inline-link
{
    font-size: small;
    margin-top: -2.6em;
    text-align: right;
    font-weight: bold;
}
</style>

In [**part 1**](https://vittorioromeo.com/index/blog/zeroalloc_continuations_p0.html) we implemented a barebones `future`-like class that supported `.then` continuations without needing allocations or type-erasure. The idea behind it was to encode the entire computation chain into a single object with a huge type:

```cpp
// pseudocode

auto f = initiate(A).then(B).then(C).then(D);
// ...would become something like:
/*
    D<C<B<A>>>>
*/
```

We previously stored the "parent" node by moving `*this` as part of a generalized lambda capture, and stored the `Callable` itself via EBO *(empty base optimization)*. As we will explicitly need access to the "parent" node's type to support non-blocking schedulers and implement `when_all` in the future, it's time significantly improve our design.



<p></p>

### parent propagation

Let's begin by storing the parent as part of `node` itself. This will require adding an additional template parameter to `node` and updating its constructor and deduction guide:

```cpp
template <typename Parent, typename F>
struct node : Parent, F
{
    template <typename ParentFwd, typename FFwd>
    node(ParentFwd&& p, FFwd&& f) : Parent{FWD(p)}, F{FWD(f)}
    {
    }

    template <typename FThen>
    auto then(FThen&& f_then);

    template <typename Scheduler>
    decltype(auto) execute(Scheduler&& s) &;
};

template <typename ParentFwd, typename FFwd>
node(ParentFwd&&, FFwd&&)
    -> node<std::decay_t<ParentFwd>, std::decay_t<FFwd>>;
```

Let's also add some simple getters in order to make EBO less painful:

```cpp
auto& node</* ... */>::as_parent() noexcept
{
    return static_cast<Parent&>(*this);
}

auto& node</* ... */>::as_f() noexcept
{
    return static_cast<F&>(*this);
}
```

Now that `Parent` is being stored directly as part of `node`, we need to update `.then`. Originally, our implementation was as follows:

```cpp
template <typename FThen>
auto node</* ... */>::then(FThen&& f_then)
{
    return ::node{
        [parent = std::move(*this), f_then = FWD(f_then)]() mutable
        {
            return f_then(static_cast<F&>(parent)());
        }};
}
```

Instead of capturing `std::move(*this)`, we can take `parent` as an argument instead, and move `*this` into the new node:

```cpp
template <typename FThen>
auto node</* ... */>::then(FThen&& f_then)
{
    return ::node{std::move(*this),
        [f_then = FWD(f_then)](auto& parent) mutable
        {
            return /* ??? */
        }};
}
```

What do we return now? The idea here is that we want to invoke `parent`'s stored `Callable` with its own parent, recursively.

```cpp
template <typename FThen>
auto node</* ... */>::then(FThen&& f_then)
{
    return ::node{std::move(*this),
        [f_then = FWD(f_then)](auto& parent) mutable
        {
            return f_then(parent.call_with_parent());
        }};
}
```

> ..."recursively"...

If we're going to do stuff recursively, we need a base case: an empty `root` class will do the trick:

```cpp
struct root { };
```

The `initiate` function will create a `node` whose parent is `root`:

```cpp
template <typename F>
auto initiate(F&& f)
{
    return node{root{}, std::forward<F>(f)};
}
```

The `call_with_parent` member function will deal with the recursive call, using [`if constexpr(...)`](http://en.cppreference.com/w/cpp/language/if#Constexpr_If):

```cpp
decltype(auto) node</* ... */>::call_with_parent()
{
    if constexpr(std::is_same_v<Parent, root>)
    {
        return as_f()();
    }
    else
    {
        return as_f()(as_parent());
    }
}
```

Finally, our `execute` member function can simply invoke `call_with_parent()` through the scheduler:

```cpp
template <typename Scheduler>
decltype(auto) execute(Scheduler&& s) &
{
    return s([this]() -> decltype(auto)
    {
        return call_with_parent();
    });
}
```

You can find a complete snippet [here **on wandbox.org**](https://wandbox.org/permlink/tIgt5wXqE2EV3qEu) or [here **on GitHub**](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/zeroalloc_continuations/p1_0_propagate_parent.cpp).



### non-blocking schedulers

Everything seems fine and dandy, until we realize that our current implementation doesn't play nicely with non-blocking schedulers. Take this marvel of engineering as an example:

```cpp
struct world_s_best_thread_pool
{
    template <typename F>
    void operator()(F&& f)
    {
        std::thread{FWD(f)}.detach();
    }
};
```

Note that a non-blocking scheduler can only return `void`, as `f` will complete execution sometime in the future. This means that we need to change our `execute` method to be non-blocking, and provide a way to wait until the computation is fully executed. The strategy I'll use here is as follows:

* Assume that when `execute` is called, the chain of nodes will be alive until the entire computation is completed.

* Return `void`, and assume that the user will retrieve a possible final result on their own.

Here's an example of a simple non-blocking computation using this model:

```cpp
int out;
auto f = initiate([]{ return 1; })
            .then([](int x){ return x + 1; })
            .then([](int x){ return x + 1; })
            .then([&out](int x){ out = x; });

f.execute(world_s_best_thread_pool{});
std::this_thread::sleep_for(200ms); // Totally safe.

std::cout << out << "\n";
```

Changing `node::execute` is quite straightforward:

```cpp
template <typename Scheduler>
void node</* ... */>::execute(Scheduler&& s) &
{
    s([this]
    {
        call_with_parent();
    });
}
```

You can try this out [here **on wandbox.org**](https://wandbox.org/permlink/nwDKnLGIapBlB37p).

> ...sleeping? ...reference output parameters? ...really?

We can instead provide a `.wait_and_get()` method that properly waits until the computation chain is completely executed and returns the proper result value. We want this to seamlessly work with `void`, so we're going to require some helper type traits... since we don't yet have ["regular `void`" (P0146)](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0146r1.html) :(

```cpp
struct nothing { };

template <typename T>
struct void_to_nothing { using type = T; };

template <>
struct void_to_nothing<void> { using type = nothing; };

template <typename T>
using void_to_nothing_t = typename void_to_nothing<T>::type;
```

Additionally, we need an utility that invokes a function object `f` and returns `nothing` instead of `void`:

```cpp
template <typename F, typename... Ts>
decltype(auto) returning_nothing_instead_of_void(F&& f, Ts&&... xs)
{
    if constexpr(std::is_same_v<decltype(FWD(f)(FWD(xs)...)), void>)
    {
        FWD(f)(FWD(xs)...);
        return nothing{};
    }
    else
    {
        return FWD(f)(FWD(xs)...);
    }
}
```

We can now use `void_to_nothing_t`, `returning_nothing_instead_of_void`, and `std::condition_variable` to correctly wait and return:

```cpp
template <typename Scheduler>
decltype(auto) node</* ... */>::wait_and_get(Scheduler&& s) &
{
    using result_type = void_to_nothing_t<decltype(call_with_parent())>;

    result_type out;
    std::atomic<bool> finished{false};
    std::condition_variable cv;
    std::mutex mtx;

    auto f = then([&](auto&& x)
                    {
                        out = FWD(x);

                        std::scoped_lock lk{mtx};
                        finished.store(true);
                        cv.notify_one();
                    });

    f.execute(s);

    std::unique_lock lk{mtx};
    cv.wait(lk, [&]{ return finished.load(); });

    return out;
}
```

The `wait_and_get` member function is:

* Computing the `result_type` of the computation chain and preparing an `out` buffer for the result value.

* Attaching a continuation to `*this`, which fills the `out` result value buffer and signals a condition variable `cv` that we're `finished`. *(This is basically a poor man's [`std::experimental::latch`](http://en.cppreference.com/w/cpp/experimental/latch).)*

* Finally returning the value stored inside the `out` buffer.

Lastly, we need to change `call_with_parent()` to support `nothing`:

```cpp
decltype(auto) call_with_parent()
{
    if constexpr(std::is_same_v<Parent, root>)
    {
        return returning_nothing_instead_of_void(as_f());
    }
    else
    {
        return returning_nothing_instead_of_void(as_f(), as_parent());
    }
}
```

*Beautiful.*

We can now easily use a non-blocking scheduler without using `std::this_thread::sleep` or ugly output parameters:

```cpp
auto f0 = initiate([]{ return 1; })
            .then([](int x){ return x + 1; })
            .then([](int x){ return x + 1; });

std::cout << f0.wait_and_get(world_s_best_thread_pool{})
            << "\n";

auto f1 = initiate([]{ return 1; })
            .then([](int x){ return x + 1; })
            .then([](int){ std::cout << "void!"; });

f1.wait_and_get(world_s_best_thread_pool{});
```

You can fine the complete snippet [here **on wandbox.org**](https://wandbox.org/permlink/fS6XzrloVExGyJ6c) or [here **on GitHub**](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/zeroalloc_continuations/p1_1_wait_and_get.cpp).

Oh, and [here's also a **godbolt.org** session](https://godbolt.org/g/9CPQTM) that proves that **the only overhead is caused by threading/synchronization**. If we revert to a synchronous single-threaded approach, **everything is completely inlined**.

This post has gotten longer than I wanted, so `when_all(...)` will have to be deferred to the next one... you can blame `void` for that! Thanks for reading!



### series

* [*"zero-allocation continuations - part 1"*](https://vittorioromeo.com/index/blog/zeroalloc_continuations_p0.html)

* [*"zero-allocation continuations - part 2"*](https://vittorioromeo.com/index/blog/zeroalloc_continuations_p1.html)

* [*"zero-allocation continuations - part 3"*](https://vittorioromeo.com/index/blog/zeroalloc_continuations_p2.html)
