

[CppCon 2016](http://cppcon.org) ended yesterday - I had the pleasure of attending and presenting at this amazing conference again this year.

I'm really grateful to [Jon Kalb](https://twitter.com/_JonKalb), [Bryce Lelbach](https://twitter.com/blelbach), the conference staff, the speakers, [my company](https://www.techatbloomberg.com/events/cppcon/) and everyone else involved for making this possible.

In the same vein as [my C++Now 2016 trip report](https://github.com/vittorioromeo/cppnow2016/blob/master/trip_report.md), I wanted to share my thoughts regarding the talks I liked the most and regarding my sessions.

-------

### Favorite talks & lessons learned

#### [Using Types Effectively - *Ben Deane*](https://cppcon2016.sched.org/event/7nDJ/using-types-effectively)

*([Slides available here.](https://github.com/elbeno/using-types-effectively))*

This is the first talk I saw after Bjarne's keynote, and I really found it valuable and entertaining. In short, the goal of the presentation was showing the audience how powerful C++'s type system is and how the upcoming C++17 [`std::variant`](http://en.cppreference.com/w/cpp/utility/variant) and [`std::optional`](http://en.cppreference.com/w/cpp/utility/optional) are game changers.

Ben was great at keeping the audience engaged by asking questions regarding the cardinality of [sum and product types](https://en.wikipedia.org/wiki/Algebraic_data_type), starting with basic types *(such as `void`, `bool`, and `int`)* and moving onto function types and templated variants.

I personally love this presentation style and I wish more speakers would try it out.

The most important lessons I learned from the talk are:

* **Use the type system** to your advantage, in order **to make "illegal states" irrepresentable**:

    I see code like the one in this example provided by Ben very often:

    ```cpp
    enum class ConnectionState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CONNECTION_INTERRUPTED
    };

    struct Connection
    {
        ConnectionState m_connectionState;

        std::string m_serverAddress;
        ConnectionId m_id;
        std::chrono::system_clock::time_point m_connectedTime;
        std::chrono::milliseconds m_lastPingTime;
        Timer m_reconnectTimer;
    };
    ```

    Only some of the fields inside `Connection` are used for specific `ConnectionState` values. If `m_connectionState` equals `ConnectionState::DISCONNECTED`, why should we be able to access and modify `m_id`?

    We can use the type system and `std::variant` to prevent representing illegal states, and also to improve the memory layout of our `Connection` class:

    ```cpp
    struct Connection
    {
        std::string m_serverAddress;

        struct Disconnected
        {
        };

        struct Connecting
        {
        };

        struct Connected
        {
            ConnectionId m_id;
            std::chrono::system_clock::time_point m_connectedTime;
            std::optional<std::chrono::milliseconds> m_lastPingTime;
        };

        struct ConnectionInterrupted
        {
            std::chrono::system_clock::time_point m_disconnectedTime;
            Timer m_reconnectTimer;
        };

        std::variant<Disconnected, Connecting, Connected, ConnectionInterrupted>
            m_connection;
    };
    ```

    In the above example, using multiple `struct` types to represent the various possible states and their associated fields allows us to *"compress"* the memory layout of `Connection` and to avoid accessing/mutating inactive fields.

* **Use "phantom types" to constrain behavior** where possible:

    Considering the following code snippet taken from Ben's talk:

    ```cpp
    std::string GetFormData();
    std::string SanitizeFormData(const std::string&);
    void ExecuteQuery(const std::string&);
    ```

    The intended chain of calls for the user of this hypotetical library is:

    1. Get the form data.

        ```cpp
        auto formData = GetFormData();
        ```

    2. Sanitize the form data.

        ```cpp
        auto sanitizedFormData = SanitizeFormData(formData);
        ```

    3. Execute the query.

        ```cpp
        ExecuteQuery(sanitizedFormData);
        ```

    Can you spot the problem here? **There is nothing preventing the user from calling `ExecuteQuery` with unsanitized form data.**

    C++'s type system comes to the rescue here - we can enforce our desired chain of operations by having different string types that represent the state of the form data. It is sufficient to *"wrap and mark"* `std::string` in order to achieve that. This technique is commonly known as [**"phantom types"**](https://wiki.haskell.org/Phantom_type), and it's similar in concept to [**"strong typedefs"**](http://www.boost.org/doc/libs/1_61_0/boost/serialization/serialization.hpp).

    By defining two empty *"tag"* structs, we can inform the type system of our intent of sanitizing the form data:

    ```cpp
    struct sanitized
    {
    };

    struct unsanitized
    {
    };
    ```

    Then we can *"mark"* `std::string` with one of the types above to create a separate type. In order to prevent implicit conversions *(which would nullify the purpose of phantom types)*, an `explicit` constructor is required:

    ```cpp
    template <typename T>
    struct FormData
    {
        explicit FormData(const string& input) : m_input(input) {}
        std::string m_input;
    };
    ```

    Our marked types can then provide a type-safe interface to the user which won't allow `ExecuteQuery` to be called with an unsanitized string:

    ```cpp
    FormData<unsanitized> GetFormData();
    FormData<sanitized> SanitizeFormData(const FormData<unsanitized>&);
    void ExecuteQuery(const FormData<sanitized>&);
    ```

    This is an extremely simple and powerful technique that you should always try to apply in your APIs.


Ben gave another talk which I didn't attend in person: ["`std::accumulate`: Exploring an Algorithmic Empire"](https://cppcon2016.sched.org/event/7nLN/stdaccumulate-exploring-an-algorithmic-empire) - I'm really looking forward to see the video when it's uploaded on [CppCon's official YouTube channel](https://www.youtube.com/user/CppCon).


-------



#### [C++17 in Breadth - *Alisdair Meredith*](https://cppcon2016.sched.org/event/7nLo/c17-in-breadth-part-1-of-2)

An extremely informative two-part presentation regarding all the changes present in the upcoming C++17 standard. While Bjarne's keynote made me feel disappointed that many major features *(e.g. concepts, modules, UFCS)* were either delayed or canceled, Alisdair's talk made me realize that C++17 is going to be a massive improvement over C++14 and that there will be many useful language and features for everyone.

No major programming paradigm shifts will be introduced by the new standard, but many interesting features that could severely make complex code much easier to write and reason about will be introduced. I'm particularly excited for:

* [**Template argument deduction for class templates**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0091r3.html), which will finally allow us to get rid of `make_xxx(...)` functions. Something cool I realized is that implementing a [*"scope guard"*](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4189) will now be trivial:

    ```cpp
    template <typename TF>
    struct scope_guard : TF
    {
        template <typename TFFwd>
        scope_guard(TFFwd&& f) : TF{std::forward<TFFwd>(f)} { }

        ~scope_guard()
        {
            static_cast<TF&>(*this)();
        }
    };

    int main()
    {
        scope_guard x{[]{ std::cout << "bye!\n"; }};
    }
    ```

* [**`constexpr` lambdas**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0170r1.pdf): oh boy. This feature opens up a whole new world for lambda-based metaprogramming. I'm a huge fan of [Boost.Hana](https://github.com/boostorg/hana) and I think that this allow it become more powerful with simpler interfaces - I can't wait to see what [Louis Dionne](http://ldionne.com) is going to implement with them.

* [**fold expressions**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0036r0.pdf) and [**`if constexpr(...)**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0292r2.html): these two new features hit close to home, as they will allow users to implement patterns I've talked about in conferences in much cleaner and safer ways:

    * Fold expressions over the *"comma operator"* will allow a trivial implementation of `for_each_argument`, a code snippet that I covered both at [*CppCon 2015*](https://www.youtube.com/watch?v=2l83JlqkzBk), [*C++Now 2016*](https://www.youtube.com/watch?v=tMuXZkPiVOY) and [*CppCon 2016*](https://cppcon2016.sched.org/event/7nKy/implementing-static-control-flow-in-c14).

    * `if constexpr(...)` is a sane implementation of `static_if`, which can be implemented *(with a slightly less enticing syntax)* in C++14. I presented a way of doing that at [*C++Now 2016*](https://www.youtube.com/watch?v=tMuXZkPiVOY) and [*CppCon 2016*](https://cppcon2016.sched.org/event/7nKy/implementing-static-control-flow-in-c14).



-------


#### [Hybrid Data Structures - *Chandler Carruth*](https://cppcon2016.sched.org/event/7nM4/high-performance-code-201-hybrid-data-structures)

An enlightening overview of some high-performance data structures used in LLVM's codebase. I really liked the idea of a *"small vector"* class that uses [SBO *("small buffer optimization")*](https://akrzemi1.wordpress.com/2014/04/14/common-optimizations/#sbo) to prevent unnecessary allocations. This is sadly [impossible for `std::vector`](http://stackoverflow.com/questions/8190950/may-stdvector-make-use-of-small-buffer-optimization) and I think that code making use of many small vector instantiations could greatly benefit from SBO.

Chandler also presented some very interesting ways of using [*"tagged pointers"*](https://en.wikipedia.org/wiki/Tagged_pointer), in order to implement lightweight variants as small as a regular pointer.

Questions regarding the possibility of implementing these data structures using allocators were also asked during the talk, which allowed Chandler to explain why sometimes it is easier to build these high-performance structures from scratch and lead to interesting discussion.




-------




#### [Constant Fun - *Dietmar Kuhl*](https://cppcon2016.sched.org/event/7nLY/constant-fun)

*([Slides available here.](https://github.com/CppCon/CppCon2016/blob/master/Presentations/Constant%20Fun/Constant%20Fun%20-%20Dietmar%20Kuehl%20-%20CppCon%202016.pdf))*

An in-depth analysis of [*"constant expressions"*](http://en.cppreference.com/w/cpp/language/constant_expression) and [`constexpr`](http://en.cppreference.com/w/cpp/language/constexpr) in both C++11 and C++14.

If you have never used `constexpr` in the past and want to implement useful compile-time functions and structures *(e.g. mapping strings to types or factory functions)*, this is a fantastic introduction to the `constexpr` specifier.

If you have used `constexpr` and are still not completely confident on when `constexpr` function arguments and return values are constant expressions *(like I was before the talk)*, definitely watch the video when it's available: Dietmar does an excellent job at clearly explaining how and when functions, types and variables can be evaluated as constant expression in `constexpr` contexts and how to write your code to make sure it is as flexible as possible.



-------



#### [From Zero to Iterators - *Patrick Niedzielski*](https://cppcon2016.sched.org/event/7nLu/from-zero-to-iterators-building-and-extending-the-iterator-hierarchy-in-a-modern-multicore-world)

*([Slides available here.](https://github.com/CppCon/CppCon2016/tree/master/Presentations/From%20Zero%20To%20Iterators))*

[Patrick](http://pniedzielski.net/) really **loves** iterators. If you don't already feel the same way, I'm confident you'll start loving them after watching this very well-presented and informative talk.

Patrick implements an hierarchy of iterator concepts *(using ["concepts lite"](https://en.wikipedia.org/wiki/Concepts_(C%2B%2B)))* and various generic functions that show how powerful this abstraction is. The audience is guided from a very simple and naive *pointer-based* `copy` function to the definition and implementation of a `CacheAwareIterator` concept that can be used to automatically parallelize algorithms *(potentially avoiding ["false sharing"](http://mechanical-sympathy.blogspot.com/2011/07/false-sharing.html) with [C++17 features](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0154r1.html))*.

If that doesn't sound exciting to you, I recommend watching the first minutes of this talk when it becomes available - I guarantee you'll be hooked.

Patrick's heavy "concepts lite" usage also shows their benefits and drawbacks clearly. If you haven't yet made up your mind on this feature that could be part of C++20, this talk is also a great place to start developing an opinion.


-------



### My sessions



#### [Implementing `static` control flow in C++14 - *Vittorio Romeo*](https://cppcon2016.sched.org/event/7nKy/implementing-static-control-flow-in-c14)

*([Slides and code available here.](https://cppcon2016.sched.org/event/7nKy/implementing-static-control-flow-in-c14))*

*Abstract:*

> There has always been great interest in imperative compile-time control flow: as an example, consider all the existing `static_if` proposals and the recently accepted `constexpr_if` construct for C++17.
>
> What if you were told that it is actually possible to implement imperative control flow in C++14?
>
> In this tutorial, the implementation and design of a compile-time `static_if` branching construct and of a compile-time `static_for` iteration construct will be shown and analyzed. These constructs will then be compared to traditional solutions and upcoming C++17 features, examining advantages and drawbacks.

I've previously talked about *"`static` control flow"* in a [lightning talk at Meeting C++ 2015](https://www.youtube.com/watch?v=hDwhfjBPKv8) and at [C++Now 2015](https://www.youtube.com/watch?v=tMuXZkPiVOY). I realized that this topic is of more general interest and that it would be a great fit for CppCon's program - I'm really glad my submission got accepted and that I had a chance to show developers how C++14 makes it reasonably easy to implement *imperative-like* compile-time control flow.

In short, this talk shows how to implement a `static_if` C++14 construct similar to C++17's `if constexpr`:

```cpp
template <typename T>
auto consume(T&& x)
{
    static_if(bool_v<is_solid<T>>)
        .then([](auto&& y)
            {
                y.eat();
                std::cout << "eating solid\n";
            })
        .else_if(bool_v<is_liquid<T>>)
        .then([](auto&& y)
            {
                y.drink();
                std::cout << "drinking liquid\n";
            })
        .else_([](auto&&)
            {
                std::cout << "cannot consume\n";
            })(FWD(x));
}
```

It also covers two techniques that can be used to implement compile-time iteration:

* A `for_args` function, similar to the previously covered [`for_each_argument`](https://www.youtube.com/watch?v=2l83JlqkzBk), which uses the ellipsis `...` operator to avoid recursion in order to prevent significantly increasing compilation times.

* A more advanced and feature-rich `static_for` function, which allows users to keep track of the iteration index and use *imperative-like* `continue` and `break` instructions. This is implemented recursively and has a bigger impact on compilation times but is way more flexible than `for_args`.

In constrast to the previous versions, which were 100% code, I added some animated slides that clearly show the idea of the implementation behind `static_if`, giving the audience a visual representation of how branches are *"collapsed"* and how execution is *"deferred"* in order to prevent unmatched branches to generate compiler errors.

Additionally, I covered the history of `static` control flow proposals in the C++ standards, and the rules *(plus some usage examples)* of the upcoming `if constexpr`, which makes `static_if` obsolete.

I'm really happy about the talk: I finished right in time with a few extra minutes for questions, the audience looked engaged and was interested in knowing more about the patterns. I received only positive feedback so far and I'm looking forward to more of that and to constructive criticism.


-------



#### [C++ in Colleges - *(panel)*](https://cppcon2016.sched.org/event/8NUc/c-in-colleges-panel)

*Abstract:*

> Join a panel of college professors, recent and current undergraduates, and professionals with teaching and outreach experience as we discuss how to engage students in C++. Why is it important to teach C++ early, and is it an appropriate choice for a first programming language? Are colleges teaching C++ less than they used to? Are colleges teaching C++ well? How can the C++ community better engage students in open source and in industry?

I was very lucky to be part of this panel moderated by [Jackie Kay](https://twitter.com/jackayline) as a recent college graudate, surrounded by professors and students with many interesting stories and opinions to share *(Bjarne joined the panel as well!)*.

I think that the way C++ is being taught is a **huge** issue that needs to be effectively addressed by the community in order to prevent spreading misinformation about the language and to bring professors up-to-date with the newest standards and best practices. In way too many classes, mine included, C++ is introduced as an *old* and *outdated* language, still alive only for legacy reasons.

In other classes, C++ is taught unidiomatically, excessively using dynamical memory allocations and Java-like idioms.

I really really would love to see all college professors in the world get up-to-date with the newest standards and with idiomatic C++ code: it's a shame that many students avoid the language or even despise it due to improper teaching and misconceptions. Making this a reality is a big problem that I do not know how to solve - interesting suggestions and possible solutions were discussed during the panel, but the only thing that I'm sure will help is **doing your small part**: tell your coworkers and classmates about modern C++, spread the word on how powerful and safe idiomatic C++ is, and make an effort to clear misconceptions away.

One of this year's keynotes by *Dan Saks*, [Keynote: extern "C": Talking to C Programmers About C++](https://cppcon2016.sched.org/event/7opp/keynote-extern-c-talking-to-c-programmers-about-c), is an invaluable resource on how to speak to people who truly believe incorrect information about the language, and I think that the lessons learned from that keynote can be effectively applied in the context of college education as well.

I really enjoyed being part of the discussion on this panel, and also being able to share a very interesting story heard at **++it Florence Meetup 2015**: [*Marco Foco* taught C++14 using Raspberry Pi2](https://www.youtube.com/watch?v=qMRxNQO3qbI) to a class containing students with no previous C++ experience starting from modern code and idioms, **achieving very positive results**. This is, in my opinion, a very remarkable experience that shows how teaching the complexity of the language *(which exists due to compatibility reasons)* can be deferred to a later step in the teaching process.

Another resource that you might find interesting is *Kate Gregory*'s [CppCon 2015 "Stop Teaching C" talk](https://www.youtube.com/watch?v=YnWhqhNdYyk).
