Two months ago I published [**"the hidden compile-time cost of C++26 reflection"**](https://vittorioromeo.com/index/blog/refl_compiletime.html), where I measured what including `<meta>` and doing some basic reflection actually *costs* per translation unit. If you haven't read it, start there -- this post builds directly on top of it.

That article used a *prerelease* GCC 16 snapshot. Since then, **GCC 16 has been officially released**[^arch] and is now widely available, which seemed like a good excuse to revisit the topic with a more realistic example: **enum-to-string conversion**.

[^arch]: As of writing, GCC 16 has landed in Arch Linux's official repositories, in Fedora 44, and in most rolling distributions. The Fedora image used in this benchmark ships `gcc 16.1.1 20260501`, built with `--enable-checking=release --with-build-config=bootstrap-lto`.

Enum-to-string is the *"hello world"* of reflection -- but it's also genuinely useful in real projects, for things like logging, serialization, debugging, and so on. If you adopt reflection in a real codebase, it might be the first thing you'll write.

So: **how much does reflection-based enum-to-string actually cost, in compile time, compared to the alternatives?**



### the three approaches

I benchmarked three implementations of the same operation: given an enum value, return a `std::string_view` with its enumerator name.

#### 1. reflection (c++26)

No macros, no boilerplate, works for any enum:

```cpp
#include <meta>
#include <string_view>
#include <type_traits>

template <typename T>
    requires std::is_enum_v<T>
constexpr std::string_view to_enum_string(T val)
{
    template for (constexpr auto e :
                std::define_static_array(std::meta::enumerators_of(^^T)))
    {
        if (val == [:e:])
            return std::meta::identifier_of(e);
    }

    return "<unknown>";
}
```

The code was taken from ["What the heck is Reflection?"](https://www.murathepeyiler.com/what-the-heck-is-reflection/) by [Murat Hepeyiler](https://www.murathepeyiler.com/). I think it's a pretty idiomatic example.

#### 2. [`enchantum`](https://github.com/ZXShady/enchantum) (c++17)

A C++17 header-only library by [ZXShady](https://github.com/ZXShady) that achieves enum reflection through `__PRETTY_FUNCTION__` parsing tricks. No macros at the call site, no reflection flag needed:

```cpp
#include <enchantum/enchantum.hpp>

enum class E { V0, V1, V2, V3 };

std::string_view s = enchantum::to_string(E::V0);
```

#### 3. x-macro (preprocessor)

The C-style solution. You list the enumerators once, and a single macro expands into *both* the `enum class` definition and a `to_string` function that uses a `switch`:

```cpp
#include "xmacro_enum.hpp"

#define E_LIST(X) \
    X(V0) X(V1) X(V2) X(V3)

DEFINE_ENUM(E, E_LIST)

// Generates:
//   enum class E { V0, V1, V2, V3 };
//   constexpr std::string_view to_string(E e) { ... }
```

The implementation of `DEFINE_ENUM` is a few lines of preprocessor glue:

```cpp
#include <string_view>

#define XMACRO_VALUE_(name) name,

#define XMACRO_CASE_(name) \
    case EnumType_::name: return #name;

#define DEFINE_ENUM(EnumName, LIST_MACRO)                          \
    enum class EnumName { LIST_MACRO(XMACRO_VALUE_) };             \
    [[nodiscard]] constexpr std::string_view to_string(EnumName e) \
    {                                                              \
        using EnumType_ = EnumName;                                \
        switch (e) { LIST_MACRO(XMACRO_CASE_) }                    \
        return "<unknown>";                                        \
    }
```

The only header it pulls in is `<string_view>`. We'll also test a variant of this approach that returns `const char*` instead, with *zero* standard library includes.



### the benchmark

For each approach, I created several translation units that:

1. Define a single `enum class E` with **N** enumerators (`V0`, `V1`, ..., `V(N-1)`);
2. Include the enum-to-string header;
3. Call the conversion function once with a runtime value to force instantiation.

I varied **N** across `4`, `16`, `64`, `256`, and `1024` to see how cost scales with enum size.

For example, the reflection variant for `N=4` looks like this:

```cpp
#include "enum_to_string.hpp"

enum class E { V0, V1, V2, V3 };

volatile int runtime_idx = 0;

int main()
{
    const E val = static_cast<E>(runtime_idx);
    const auto sv = to_enum_string(val);
    return static_cast<int>(sv.size());
}
```

The `enchantum` and X-macro variants are structurally identical -- only the header and the function call change. I also separately measured the cost of just *including* each header, to isolate the header tax from the reflection work itself.

> ⚠️ **Note on `enchantum`'s default range:** `enchantum` scans enum values across a configurable range (default `[-256, 256]`). For `N=1024`, I had to bump `ENCHANTUM_MAX_RANGE` to `1024`, which also slows down every other enum in the same TU. Keep that in mind when reading the `N=1024` row.

All benchmark files are [available on GitHub](https://github.com/vittorioromeo/vittorioromeo.com/tree/master/extra/reflection_enum_to_string_benchmarks).



### benchmarking setup

Same as the [previous article](https://vittorioromeo.com/index/blog/refl_compiletime.html) -- `hyperfine` inside a Fedora 44 Docker container on a 13th Gen i9-13900K. Two things are different this time:

- **Compiler:** `gcc 16.1.1 20260501` (Red Hat 16.1.1-1, release build) -- the officially released GCC 16, not a prerelease snapshot.

- **Noise control:** the container ran with `--cpuset-cpus=0-7`, the host was set to the `performance` CPU governor, and the compiler process was pinned to a P-core with `taskset -c 0`. `hyperfine` was run with `--warmup 5 --min-runs 20`.

Usual disclaimer: measurements aren't strictly rigorous, my hardware is beefy (YMMV), and single-TU numbers undersell project-wide cost.

Also, these measurements are specific to GCC 16's current reflection and module implementation; other compilers may exhibit very different behavior.



### benchmark results

#### total per-TU compile time

|            N            | X-macro (`const char*`) | X-macro (`string_view`) |   `enchantum`    | Reflection |
|:-----------------------:|:-----------------------:|:-----------------------:|:----------------:|:----------:|
| Baseline (`int main()`) |         25.8 ms         |         25.7 ms         |     25.8 ms      |  25.7 ms   |
|   Header include only   |       **25.7 ms**       |        136.0 ms         |     147.1 ms     |  180.8 ms  |
|            4            |       **26.6 ms**       |        137.6 ms         |     170.6 ms     |  186.7 ms  |
|           16            |       **26.9 ms**       |        138.1 ms         |     170.9 ms     |  187.7 ms  |
|           64            |       **28.0 ms**       |        141.2 ms         |     172.8 ms     |  191.1 ms  |
|           256           |       **32.5 ms**       |        153.0 ms         |     184.1 ms     |  215.0 ms  |
|          1024           |       **54.7 ms**       |        204.5 ms         | 272.0 ms[^range] |  255.0 ms  |

[^range]: With `-DENCHANTUM_MAX_RANGE=1024`. The default would not have compiled.

#### algorithm-only cost (TU time minus include-only time)

This approximates the *additional* reflection work beyond header inclusion:

|  N   | X-macro (`const char*`) | X-macro (`string_view`) | `enchantum` | Reflection |
|:----:|:-----------------------:|:-----------------------:|:-----------:|:----------:|
|  4   |       **0.9 ms**        |         1.6 ms          |   23.5 ms   |   5.9 ms   |
|  16  |       **1.2 ms**        |         2.1 ms          |   23.8 ms   |   6.9 ms   |
|  64  |       **2.3 ms**        |         5.2 ms          |   25.7 ms   |  10.3 ms   |
| 256  |       **6.8 ms**        |         17.0 ms         |   37.0 ms   |  34.2 ms   |
| 1024 |       **29.0 ms**       |         68.5 ms         |  124.9 ms   |  74.2 ms   |

#### per-enumerator scaling

| Approach                    |     ms / enumerator     |
|:----------------------------|:-----------------------:|
| **X-macro (`const char*`)** |       **~0.027**        |
| X-macro (`string_view`)     |          ~0.06          |
| Reflection                  |          ~0.07          |
| `enchantum`                 | O(scan range), not O(N) |

`enchantum` does not scale with the actual enum size -- it scales with the *configured scan range*, since it has to probe every possible value in that range. That's why an `N=4` enum costs almost as much as `N=64`.



#### reflection with PCH and modules

Since the reflection variant pays a ~155 ms header tax for `<meta>`, the obvious question is: does precompiling the header or switching to C++20 modules eliminate it?

I re-ran the reflection benchmark with two extra configurations:

- **PCH**: precompiled `<meta>`, `<string_view>`, `<type_traits>` once, then compiled the TUs with `-include pch.hpp`.

- **Modules**: pre-built the `std` and `std.compat` modules and the `<bits/stdc++.h>` header unit once via GCC 16's new `--compile-std-module` flag (this cost is *not* included in the measurement), then compiled the TUs with `-fmodules` so that `#include <meta>` is [transparently translated](https://gcc.gnu.org/gcc-16/changes.html#cxx) into `import <bits/stdc++.h>`.

|          N          | Reflection (plain `#include`) | **Reflection + PCH** | **Reflection + modules** |
|:-------------------:|:-----------------------------:|:--------------------:|:------------------------:|
| Header include only |           180.8 ms            |     **73.8 ms**      |         397.4 ms         |
|          4          |           186.7 ms            |     **80.6 ms**      |         403.4 ms         |
|         16          |           187.7 ms            |     **81.0 ms**      |         403.1 ms         |
|         64          |           191.1 ms            |     **84.4 ms**      |         409.4 ms         |
|         256         |           215.0 ms            |     **97.5 ms**      |         423.2 ms         |
|        1024         |           255.0 ms            |     **147.9 ms**     |         482.5 ms         |

PCH is the clear winner -- about ~**2.3× speedup** at every enum size, dropping `N=4` from 187 ms to 81 ms. With PCH in place, reflection beats both `enchantum` and the `string_view` X-macro variant outright.

Modules are the opposite: about ~**2.2× slowdown**. I verified that the std module artifacts were genuinely cached *(`mtime` unchanged across runs)* and that GCC was loading them *(confirmed via `-flang-info-module-cmi` and `-ftime-report`, which attributed ~190 ms to `module import` plus another ~190 ms to template instantiation work the module triggered)*.[^module_slow]

Explicit `import std;` performs essentially the same as the transparent translation, because GCC's `std` module is currently implemented as a thin wrapper around the `<bits/stdc++.h>` header unit -- both routes end up loading the same ~34 MB artifact.

[^module_slow]: This is presumably a transient issue. Module loading hasn't been optimized to PCH levels yet. I expect this gap to eventually close, but as of today, modules are not a viable optimization for `<meta>`-heavy TUs.



### insights

1. **The header is the cost. Not the reflection.**

    The reflection *algorithm* is fast -- asymptotically **~0.07 ms per enumerator**, essentially the same as the hand-rolled `switch` in the X-macro version (~0.06 ms). What makes reflection look expensive is `<meta>`: just including it costs **~155 ms per TU** over the baseline.

2. **The X-macro with `const char*` is the fastest tested approach.**

    With zero standard library headers, an `N=4` enum compiles in **26.6 ms** -- within the noise of the baseline. Even `N=1024` (54.7 ms) is *faster than just `#include <meta>` with no reflection work at all*. Most of what we call "slow C++ compilation" is really slow *standard library* compilation.

3. **`enchantum` has the smallest include cost of the non-trivial approaches**[^moreopt] (~147 ms vs reflection's ~181 ms), but the heaviest per-call work (~24 ms even for tiny enums, because it always scans the full configured range, regardless of how many enumerators you actually have). That's why it wins on small enums and loses on large ones.

4. **Reflection has the best ergonomics but the highest header tax.** It works for *any* enum -- sparse, scoped, unscoped -- with no special setup at the declaration site. But every TU that touches `<meta>` pays ~155 ms before any reflection happens.

5. **PCH closes the gap, modules widen it.** Precompiling `<meta>` cuts reflection compile time by ~2.3× and makes it the fastest of the three approaches. C++20 modules in GCC 16, surprisingly, go the other way -- ~2.2× *slower* than the plain include path.

[^moreopt]: I suspect it could be optimized even further by avoiding the Standard Library in lieu of bespoke components and compiler builtins.



### what this means in a real codebase

The single-TU numbers look small. They are not, at scale.

A large C++ codebase can easily have a few hundred translation units that pull in the enum-to-string header, perhaps transitively. Picking **500 TUs** as a round number, and an `N=16` enum as a typical size:

| Approach                | Per-TU cost | Project-wide cost (500 TUs) |
|:------------------------|:-----------:|:---------------------------:|
| X-macro (`const char*`) |   26.9 ms   |       **~13 seconds**       |
| X-macro (`string_view`) |  138.1 ms   |       **~69 seconds**       |
| `enchantum`             |  170.9 ms   |       **~85 seconds**       |
| Reflection              |  187.7 ms   |       **~94 seconds**       |

A few hundred milliseconds per TU turns into **over a minute** of compile time at the project level. That's the difference between a sub-15-second clean build and a minute and a half. Incremental builds won't always save you, because every TU that includes an affected header pays the full price.

This is multiplied by every header in your project that has similar overhead. Real codebases don't have *one* heavy header -- they have dozens. The few hundred milliseconds you see in a microbenchmark become *minutes* once you multiply.

On the other hand, the numbers shown here do not take parallelism into account. E.g. the ~94 CPU-seconds would be ~6s on a 16-core machine, assuming perfect parallelism.



### what to do about it

If you're adopting reflection-based enum-to-string in a large codebase:

1. **Use PCH for `<meta>` -- not modules.** As shown above, a PCH cuts the header cost by ~2.3× and makes reflection the fastest of the three approaches. C++20 modules in GCC 16 do the *opposite* right now (they make things ~2.2× slower), so this is one of those rare cases where the older mechanism is the right answer.

2. **Don't include the enum-to-string header from other headers.** Push it as far down the include graph as possible -- ideally, only into the `.cpp` files that actually need it. Every transitive include multiplies the cost.

3. **For compile-time-sensitive code, X-macros are still the right answer.** They look ugly, they don't compose, they force you to list enumerators in a macro. But **27 ms per TU** is hard to beat. For a project like [my fork of SFML](https://vittorioromeo.com/index/blog/vrsfml.html), where the entire codebase rebuilds in ~4 seconds, anything with `<meta>` in it is a non-starter.

4. **For library authors, think twice before exposing reflection through your public headers.** A library header that drags `<meta>` into every consumer's TU is something I personally would not want to depend on. `enchantum` or an X-macro is a much friendlier choice until `<meta>` gets lighter or modules become ubiquitous.



### conclusion

This benchmark confirms what the original article argued, now with concrete numbers from an operation that people will actually write:

> The cost of C++26 reflection is not the reflection. It's `<meta>`.

The reflection algorithm itself runs at roughly ~0.07 ms per enumerator. The reason a reflection-based `to_string` ends up ~7× slower to compile than the bare-metal `const char*` X-macro variant is almost entirely `<meta>` (and its transitively-included headers).

I'm still excited about reflection. It will replace a lot of ugly macro boilerplate and unlock libraries that weren't really possible before. But until `<meta>` gets lighter *(or until modules get much faster)*, every project that adopts it should know what the bill looks like.



### shameless self-promotion

- I offer training, mentoring, and consulting services.

    If you are interested, check out [**romeo.training**](https://romeo.training/), alternatively you can reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).

- Check out my games on Steam, powered by VRSFML!

    - [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/): A clicker-idle game where you recruit cats to pop bubbles. That's it. Or is it...?

    - [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/): A fast-paced open-source arcade game with user-created content -- the de-facto community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

    - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)
