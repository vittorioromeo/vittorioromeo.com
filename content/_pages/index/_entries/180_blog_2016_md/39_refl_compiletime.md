I am *very* excited about C++26 reflection.

I am also *obsessed* by having my code compile as quickly as possible. Fast compilation times are extremely valuable to keep iteration times low, productivity and motivation high, and to quickly see the impact of your changes. [^comptime_papers]

[^comptime_papers]: Some papers/articles on the subject: [1](https://www.computer.org/csdl/magazine/so/2023/04/10176199/1OAJyfknInm), [2](https://arxiv.org/pdf/1712.06796), [3](https://developer.microsoft.com/en-us/games/articles/2025/03/gdc-2025-build-insights-call-of-duty-modern-warfare/), [4](https://www.faros.ai/blog/insights-from-new-research-on-developer-productivity).

With time and experience, I've realized that C++ can be an extremely fast-to-compile language. Language features like templates are not the issue -- the Standard Library is.

My [fork of SFML](https://vittorioromeo.com/index/blog/vrsfml.html) uses almost no Standard Library at all, and I can recompile the entire thing from scratch in ~4.3s. That's around ~900 TUs, including external dependencies, tests, and examples.[^bottleneck] Incremental builds are, for all intents and purposes, instantaneous. **I love it.**

[^bottleneck]: The current bottlenecks are (by far) `cpptrace` and `nlohmann::json`, used only in a few TUs. Without those, I could easily get a sub 1s build. I could get even lower with unity builds and perhaps some well-chosen PCHs.

I would love to live in a world where C++26 reflection is purely a lightweight language feature, however [that ship has sailed](https://github.com/cplusplus/papers/issues/2096) *(thank you, [Jonathan Müller](https://www.jonathanmueller.dev/), for trying)*.

In this article, I'll try to provide some *early* expectations about the compile-time impact of C++26 reflection.




### let's measure!

I found a [nice Docker image](https://hub.docker.com/r/sourcemation/gcc-16) containing GCC 16, the first version that supports reflection, and got to work. To get reasonably stable measurements, I used the [`hyperfine`](https://github.com/sharkdp/hyperfine) command-line benchmarking tool.

These are my specs:

- **CPU:** 13th Gen Intel Core i9-13900K
- **RAM:** 32GB (2x16GB) DDR5-6400 CL32
- **OS:** Debian 13 Slim (on Docker, `sourcemation/gcc-16`)
- **Compiler:** GCC 16.0.1 20260227 (experimental)
- **Flags:** `-std=c++26 -freflection`

My test scenarios were as follows:

1. Baseline test. Just a `int main() { }`.

2. Header inclusion test. Same as above, but with `#include <meta>`.

3. Basic reflection over a struct's fields:

    ```cpp
    template <typename T> void reflect_struct(const T& obj) {
      template for (constexpr std::meta::info field :
                      std::define_static_array(std::meta::nonstatic_data_members_of(
                          ^^T, std::meta::access_context::current()))) {
        use(std::meta::identifier_of(field));
        use(obj.[:field:]);
      }
    }

    struct User {
      std::string_view name;
      int age;
      bool active;
    };

    int main() {
      reflect_struct(User{.name = "Alice", .age = 30, .active = true});
    }
    ```

4. Barry Revzin's [AoS to SoA transformation example](https://godbolt.org/z/E7aajban7), from [his blog post](https://brevzin.github.io/c++/2025/05/02/soa/).[^barry]

[^barry]: If you haven't already, check out his great CppCon 2025 talk: [**"Practical Reflection With C++26"**](https://www.youtube.com/watch?v=ZX_z6wzEOG0), and perhaps [my **"More Speed & Simplicity: Practical Data-Oriented Design in C++"** keynote](https://www.youtube.com/watch?v=SzjJfKHygaQ) as well!

I've also tested using precompiled headers (PCHs) for `<meta>` and other large dependencies.

⚠️ **DISCLAIMER: please take these benchmark results with a grain of salt.** ⚠️

- My measurements are not that rigorous and that the compiler I used is still work-in-progress.
- Also note that my specs are quite beefy -- YMMV.
- Finally, remember that these measurements are for a single translation unit -- in a real project, you'd have to multiply the compile time overhead by the number of affected TUs.



### benchmark results

| #  | Scenario                               | Code                             | Precompiled Header (PCH)        | Compile Time (Mean) |
|:--:|:---------------------------------------|:---------------------------------|:--------------------------------|:--------------------|
| 1  | **Baseline (No Reflection Flag)**      | `int main()` only                | None                            | **43.9 ms**         |
| 2  | **Baseline + `-freflection`**          | `int main()` only                | None                            | **43.1 ms**         |
| 3  | **`<meta>` Header Inclusion**          | `int main()` + `#include <meta>` | None                            | **310.4 ms**        |
| 4  | **Basic Struct Reflection (1 type)**   | `reflect_struct` with `User`     | None                            | **331.2 ms**        |
| 5  | **Basic Struct Reflection (10 types)** | `reflect_struct` with `User<N>`  | None                            | **388.6 ms**        |
| 6  | **Basic Struct Reflection (20 types)** | `reflect_struct` with `User<N>`  | None                            | **410.9 ms**        |
| 7  | **AoS to SoA (Original)**              | Barry Revzin's Unedited Code     | None                            | **1,622.0 ms**      |
| 8  | **AoS to SoA (No Print)**              | Removed `<print>`                | None                            | **540.1 ms**        |
| 9  | **AoS to SoA (No Print/Ranges)**       | Removed `<print>`, `<ranges>`    | None                            | **391.4 ms**        |
| 10 | **AoS to SoA (Original + PCH)**        | Barry Revzin's Unedited Code     | `<meta>`, `<ranges>`, `<print>` | **1,265.0 ms**      |
| 11 | **AoS to SoA (No Print + PCH)**        | Removed `<print>`                | `<meta>`, `<ranges>`            | **229.7 ms**        |
| 12 | **AoS to SoA (No Print/Ranges + PCH)** | Removed `<print>`, `<ranges>`    | `<meta>`                        | **181.9 ms**        |

A few clarifications:

- The number of types in the "Basic Struct Reflection" example was adjusted by instantiating unique "clones" of the test struct type:

    ```cpp
    template <int>
    struct User {
      std::string_view name;
      int age;
      bool active;
    };

    reflect_struct(User<0>{.name = "Alice", .age = 30, .active = true});
    reflect_struct(User<1>{.name = "Alice", .age = 30, .active = true});
    reflect_struct(User<2>{.name = "Alice", .age = 30, .active = true});
    // ...
    ```

- "Removed `<print>`" implies removing all of the formatting/printing code from Barry's example.

- "Removed `<ranges>`" implies rewriting range-based code like `std::views::transform` or `std::views::iota` to good old boomer loops™.



### insights

1. **The reflection feature flag itself is free.**
  - Simply turning on `-freflection` adds **0 ms** of overhead.

2. **Basic reflection costs can scale up quickly.**
  - Base cost of reflecting 1 struct: **331.2 ms** *(but ~310 ms of this is just including `<meta>`)*.
  - Cost to reflect 9 extra types: **+57.4 ms** *(~6.3 ms per type)*.
  - Cost to reflect 10 *more* types (20 total): **+22.3 ms** *(~2.2 ms per type)*.
  - While this seems cheap on the surface, remember that my example is extremely basic, and that a large project can have hundreds (if not thousands) of types that will be reflected.

3. **Standard Library Headers are a big bottleneck.**
  - The massive compile times in modern C++ don't come from your metaprogramming logic, but from parsing standard library headers.
  - Pulling in `<meta>` adds **~149 ms** of pure parsing time.
  - Pulling in `<ranges>` adds **~440 ms**.
  - Pulling in `<print>` adds an astronomical **~1,082 ms**.

4. **Precompiled Headers (PCH) are mandatory for scaling.**
  - Caching `<meta>` and avoiding heavy dependencies cuts compile time down to **181.9 ms** (scenario 12).
  - Caching `<meta>` + `<ranges>` drops the time from **540ms to 229ms** (scenario 8 vs 11).
  - Interestingly, while caching `<print>` helps a bit, it still leaves the compile time uncomfortably high.
  - Perhaps modules could eventually help here, but I have still not been able to use them in practice successfully.
    - Notably, `<meta>` is not part of `import std` yet, and even with `import std` Barry's example took a whopping **1.346s** to compile.



### what about modules?

I ran some more measurements using `import std;` with a properly-built `std` module that includes reflection.

Firstly, I created the module via:

```bash
g++ -std=c++26 -fmodules -freflection -fsearch-include-path -fmodule-only -c bits/std.cc
```

And then benchmarked with:

```bash
hyperfine "g++ -std=c++26 -fmodules -freflection ./main.cpp"
```

No `#include` was used -- only `import std`.

These are the results (mean compilation time):

- Basic struct reflection (1 type): **~352.8 ms**
- Barry's AoS to SoA example: **~1.077 s**

Compare that with PCH:

- Basic struct reflection (1 type): **~208.7 ms**
- Barry's AoS to SoA example: **~1.261 s**

So... PCH actually wins compared to modules for just `<meta>`, and modules are not that much better than PCH for the larger example. Quite disappointing.



### conclusion

Reflection is going to bring a lot of power to C++26. New libraries that heavily rely on reflection are going to become widespread. Every single TU including one of those libraries will virally include `<meta>` and any other used dependencies.

Assuming the usage of `<meta>` + `<ranges>` becomes widespread, we're looking at a bare minimum of **~540ms** compilation overhead per TU. Using PCHs (or modules) will become pretty much mandatory, especially in large projects.

I really, really wish that Jonathan Müller's paper ([P3429: `<meta>` should minimize standard library dependencies](https://wg21.link/p3429)) was given more thought and support.

I also really wish that a game-changing feature such as reflection wasn't so closely tied to the Standard Library. The less often I use the Standard Library, the more enjoyable and productive I find C++ as a language -- insanely fast compilation times are a large part of that.

Hopefully, as reflection implementations are relatively new, things will only get better from here.



### shameless self-promotion

- I offer training, mentoring, and consulting services. If you are interested, check out [**romeo.training**](https://romeo.training/), alternatively you can reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).

- Check out my newly-released game on Steam: [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/) -- it's only $3.99 and one of those games that you can either play actively as a timewaster or more passively to keep you company in the background while you do something else.

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

   - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)

- If you enjoy fast-paced open-source arcade games with user-created content, check out [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/), my VRSFML-powered game [available on Steam](https://store.steampowered.com/app/1358090/Open_Hexagon/) and [on itch.io](https://itch.io/t/1758441/open-hexagon-my-spiritual-successor-to-super-hexagon).

   - Open Hexagon is a community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.
