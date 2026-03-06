Here's an encapsulation challenge that I frequently run into: how to let users iterate over an internal data structure without leaking implementation details, but still giving them full control over the loop?

Implementing a custom iterator type requires significant boilerplate and/or complexity, depending on the underlying data structure.

Coroutines are simple and elegant, but the codegen is atrocious -- definitely unsuitable for hot paths.

Lambdas seem to do the trick -- but they can't `break`!

This article walks through a *lightweight* solution to this classic problem.



### the problem

Suppose we want to store a collection of `Entity` objects while tracking their liveness. A possible internal representation might look like:

```cpp
class EntityStorage
{
private:
    std::vector<std::optional<Entity>> m_entities;
};
```

We want users to iterate over *valid* entities without revealing that we’re using `std::optional` or a `std::vector` under the hood. So exposing `m_entities` directly is not an option.

A custom iterator could solve this, but writing and maintaining one can be boilerplate-heavy -- especially as the complexity of the data structure increases.

Coroutines (e.g., `std::generator`) have beautiful syntax, but the generated code is inefficient, especially for real-time applications.

That leaves us with callbacks -- i.e., higher-order functions -- but there’s a problem: `break` and `continue` don’t work inside lambdas.



### higher-order iteration

Let’s add a simple lambda-based iteration member function:

```cpp
class EntityStorage
{
private:
    std::vector<std::optional<Entity>> m_entities;

public:
    void forEntities(auto&& f)
    {
        for (auto& optEntity : m_entities)
            if (optEntity.has_value())
                f(*optEntity);
    }
};
```

*(We could use concepts to constrain `f`, but it's not a big deal here.)*

The usage is delightfully clean:

```cpp
EntityStorage es;
// ... populate 'es' ...

es.forEntities([&](const Entity& e)
{
    // ... use `e` ...
});
```

This works fine for visiting *all* valid entities -- but what if you want to skip some or exit early?



### you'll never be a real loop

Even if it visually resembles a loop, we are actually invoking a function multiple times. Neither `continue` nor `break` would work here:

```cpp
es.forEntities([&](const Entity& e)
{
    if (shouldSkip(e))
        continue; // Error!

    if (shouldBreak(e))
        break; // Error again!

    // ... use `e` ...
});
```

These won’t compile -- `break` and `continue` are scoped to *actual* loops, and a lambda isn’t one.

You can simulate `continue` with `return`:

```cpp
es.forEntities([&](const Entity& e)
{
    if (shouldSkip(e))
        return; // Semantically equivalent to `continue`.

    // ... use `e` ...
});
```

However, the use of `return` can be misleading to readers. Regardless, still doesn’t give you a way to break out of the underlying loop.



### do what I say

Let’s define an `enum class` that represents what we want to do after each lambda call (i.e. "iteration"):

```cpp
enum class ControlFlow
{
    Continue,
    Break
};
```

Now, we expect `ControlFlow` as to be returned from the lambda:

```cpp
void EntityStorage::forEntities(auto&& f)
{
    for (auto& optEntity : m_entities)
        if (optEntity.has_value())
            if (f(*optEntity) == ControlFlow::Break)
                break; // Aha! Now we can break the actual loop.
}
```

Finally, the user can either explicitly *continue* or *break* from within the lambda:

```cpp
es.forEntities([&](const Entity& e)
{
    if (shouldSkip(e))
        return ControlFlow::Continue;

    if (shouldBreak(e))
        return ControlFlow::Break;

    // ... use `e` ...

    return ControlFlow::Continue; // Default action explicitly required
});
```



### is it fast?

You might be wondering if this approach is slower compared to a regular loop. You might also be wondering why we didn't just use coroutines and `std::generator`. The answer is all in the codegen.

Let's first compare direct iteration versus a higher-order function without `ControlFlow`:

```cpp
extern int ints[256];

void hof(auto&& f)
{
    for (int i : ints)
        f(i);
}

void f_direct(volatile int& target)
{
    for (int i : ints)
        target += i;
}

void f_hof(volatile int& target)
{
    hof([&](int i){ target += i; });
}
```

Without optimizations enabled, there is significant overhead due to a `call` instruction being emitted every time the lambda is invoked.

*(The overhead can be almost entirely be removed by marking the lambda with `__attribute__((always_inline))`. I don't recommend doing that in general, but it might be a reasonable thing to do for hot loops in low-level parts of a library/application that you want to be as fast as possible in `-O0`.)*

As soon as we switch on `-O1`, both `f_direct` and `f_hof` produce the same exact code.

Let's now introduce our `ControlFlow` into the mix:

```cpp
void hof_cf(auto&& f)
{
    for (int i : ints)
        if (f(i) == ControlFlow::Break)
            break;
}

void f_hof_cf(volatile int& target)
{
    hof_cf([&](int i)
    {
        target += i;
        return ControlFlow::Continue;
    });
}
```

Even for `f_hof_cf`, the codegen is exactly the same!

But what about coroutines? It would be extremely nice to write:

```cpp
std::generator<int> gen()
{
    for (int i : ints)
        co_yield i;
}

void f_gen(volatile int& target)
{
    for (int i : gen())
        target += i;
}
```

Unfortunately, the codegen is quite bad, even with `-O2` -- [check it out](https://gcc.godbolt.org/z/T63fhbdnK) for yourself. Despite this being a very simple coroutine, a heap allocation is needed, and dozens of extra bookkeeping instructions are produced.

That's a shame, because the syntax is extremely nice, and `co_yield` allows us to elegantly express any form of iteration/visitation over arbitrarily complex data structures.

Hopefully compilers will be able to do a better job here in the future, however this will always likely be an unacceptable technique for hot paths in realtime applications (e.g. games, audio processing, simulations, etc.).

For many performance-sensitive applications, `ControlFlow` emerges as a practical winner!



### smoothing out the edges

While effective, the `ControlFlow` approach introduces boilerplate. Even in the simplest case where the user *never* wants to `break` and just wants to process all entities, they are forced to add `return ControlFlow::Continue;` at the end of their lambda.

```cpp
es.forEntities([&](const Entity& e)
{
    // ... use `e` ...
    return ControlFlow::Continue; // Required!
});
```

Let’s fix that!

We can infer the user's intent with `if constexpr` and the `std::is_void_v` type trait:

```cpp
void EntityStorage::forEntities(auto&& f)
{
    for (auto& optEntity : m_entities)
        if (optEntity.has_value())
        {
            if constexpr (std::is_void_v<decltype(f(*optEntity))>)
            {
                f(*optEntity);
            }
            else if (f(*optEntity) == ControlFlow::Break)
            {
                break;
            }
        }
}
```

Above, `decltype(f(*optEntity))` evaluates to the return type of the user-provided lambda. If that return type is `void`, we assume that the user always wants to continue.

This allows omitting `return ControlFlow::Continue;` in the simple case -- the boilerplate is only there when needed:

```cpp
es.forEntities([&](const Entity& e)
{
    // ... use `e` ...
});
```



### make it dry

The `if constexpr` logic inside `forEntities` is neat, but if we have many such iteration functions (`forEntitiesMatching`, `forEntitiesInGroup`, etc.), this logic will be repeated. We can encapsulate this "regularization" of the lambda's return value into a helper function:

```cpp
template <typename F, typename... Args>
[[nodiscard]] constexpr ControlFlow regularizedInvoke(F&& f, Args&&... args)
{
    if constexpr (std::is_void_v<decltype(f(args...))>)
    {
        f(args...);
        return ControlFlow::Continue;
    }
    else
    {
        return f(args...);
    }
}
```

*(I deliberately omitted perfect-forwarding above to make the code easier to read. In a real implementation, `std::forward<F>(f)(std::forward<Args>(args)...)` should be used instead of `f(args...)`.)*

Now, `forEntities` (and similar functions) become wonderfully clean:

```cpp
void EntityStorage::forEntities(auto&& f)
{
    for (auto& optEntity : m_entities)
        if (optEntity.has_value())
            if (regularizedInvoke(f, *optEntity) == ControlFlow::Break)
                break;
}
```



### optional: let me out!

What if you want the loop to not only `break`, but also exit the *calling* function?

This is a bigger leap, not always needed -- but let's explore a solution anyway.

In a normal loop, `return` would exit from the function containing the loop. However, using `return` within a lambda only exits the scope of *that* lambda. We must expand `ControlFlow` to represent the intent of returning:

```cpp
enum class ControlFlow
{
    Continue,
    Break,
    Return
};
```

`forEntities` needs to change as well, propagating the `ControlFlow` to its caller:

```cpp
ControlFlow EntityStorage::forEntities(auto&& f)
{
    for (auto& optEntity : m_entities)
        if (optEntity.has_value())
        {
            ControlFlow signal = regularizedInvoke(f, *optEntity);

            if (signal == ControlFlow::Break)
                break; // Break the internal loop

            if (signal == ControlFlow::Return)
                return ControlFlow::Return; // Propagate to caller
        }

    return ControlFlow::Continue;
}
```

The user code would then look like this:

```cpp
ControlFlow signal = es.forEntities([&](const Entity& e)
{
    if (shouldReturn(e))
        return ControlFlow::Return; // Will be propagated to parent scope

    if (shouldBreak(e))
        return ControlFlow::Break;

    // ... use `e` ...
    return ControlFlow::Continue;
});

if (signal == ControlFlow::Return)
    return;
```

This is clunky, but sometimes necessary in deeply nested control flows.

And that's the gist of it! A simple enumeration, combined with a hint of compile-time introspection, can significantly enhance the flexibility of higher-order iteration functions.



### training and mentoring

- Interested in *deeply* understanding Modern C++ concepts?
- Would you like your C++ codebase to compile in *seconds*, not minutes?
- Having trouble managing *complexity* in your games or applications?
- Been hunting down a bug for *days* with little success?

Exciting news! I'm now offering bespoke C++ training, mentoring, and consulting services.

Check out [**romeo.training**](https://romeo.training/), alternatively you can reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).



### shameless self-promotion

- Check out my newly-released game on Steam: [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/) -- it's only $4.99 and one of those games that you can either play actively as a timewaster or more passively to keep you company in the background while you do something else.

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

   - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)

- If you enjoy fast-paced open-source arcade games with user-created content, check out [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/), my VRSFML-powered game [available on Steam](https://store.steampowered.com/app/1358090/Open_Hexagon/) and [on itch.io](https://itch.io/t/1758441/open-hexagon-my-spiritual-successor-to-super-hexagon).

   - Open Hexagon is a community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.
