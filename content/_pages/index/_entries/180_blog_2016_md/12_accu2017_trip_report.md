

Yesterday I came back from my first ever ACCU conference, [**ACCU 2017**](https://conference.accu.org/site/index.html), which happened from April 26 to April 29 in Bristol (UK). The experience was simply fantastic - I wholeheartedly recommend attending the conference!

I reinforced my knowledge on many topics and learned new ideas by attending the high-quality talks and keynotes. The schedule was very well chosen and most of the time I found it very hard to choose which talk to see in a slot - that's a good sign about the quality of the content. Fortunately, all the talks have been professionally recorded and will be available for free on [ACCU's official YouTube channel](https://www.youtube.com/channel/UCJhay24LTpO1s4bIZxuIqKw/videos).

The "social aspect" of the conference was also very important to me. I had a chance to discuss my favorite language over a beer with very smart people that I consider friends, despite the fact we see each other only one or two times per year. I also finally met some active [cpplang.slack.com](http://cpplang.diegostamigni.com/) users face to face and made new interesting acquaintances.

The company I currently work for, [Bloomberg L.P.](https://www.bloomberg.com/company/), was a major sponsor of ACCU 2017 and organized a marvelous event that consisted of a "Chess Armageddon tournament" *(where the winner would compete with a grandmaster)* and a "live coding DJ" that used [Sonic Pi](http://sonic-pi.net/) to generate music via code and play it live, accompanied by a guitarist. The event was one of the highlights of the conference for me - I had no expectations but was very pleasantly surprised by the excellent organization, entertainment choices, and unlimited free food/drinks!

I also spoke at the event - my talk was [**"Implementing variant visitation using lambdas"**](https://conference.accu.org/site/stories/2017/sessions.html#XImplementingvariantvisitationusinglambdas), which explained how to implement a form of *pattern-matching* that can be used for variant visitation. Here's an example:

```cpp
using my_variant = std::variant<int, float, double>;
my_variant v0{20.f};

// Prints "20f".
match(v0)([](int x)   { cout << x << "i\n"; },
          [](float x) { cout << x << "f\n"; },
          [](double x){ cout << x << "d\n"; });
```

I'm happy about the way the presentation went and I have received good feedback so far. The video is [**available here**](https://www.youtube.com/watch?v=mqei4JJRQ7s) and the slides are available on GitHub at [vittorioromeo/accu2017](https://github.com/vittorioromeo/accu2017).


I'll share thoughts and lessons learned from some of my favorite talks below.

* [**Lock-free programming with modern C++**](https://conference.accu.org/site/stories/2017/sessions.html#XLockfreeprogrammingwithmodernC) by [*Timur Doumler*](https://conference.accu.org/site/stories/2017/presenters.html#XTimur_Doumler).

    Timur's talk was a thorough and very well-presented introduction to *lock-free programming* with modern C++. After providing a general explanation of the topic, possible use cases, and related terminology, he discussed `std::atomic`'s usage and interface in depth. The second half of the talk revolved was my favorite part: Timur showed step-by-step implementations of **SPSC** *(single producer, single consumer)* and **SPMC** *(single producer, multiple consumers)* lock-free queues. This part of the presentation made obvious how difficult it is to reason about lock-free data structures: in fact, experienced people in the audience pointed out some possible minor mistakes in the implementations which required thinking about very uncommon edge cases. I also asked if there is any way to formally prove the correctness of a lock-free program, and was suggested to research [**CSP** *(communicating sequential processes)*](https://en.wikipedia.org/wiki/Communicating_sequential_processes) and [**process calculus**](https://en.wikipedia.org/wiki/Process_calculus).

    The biggest takeaway from the talk is that *(surprise surprise)* lock-free programming is **extremely hard to get right**, and should be avoided unless a very good reason to dive into it exists.

* [**Atomic’s memory orders, what for?**](https://conference.accu.org/site/stories/2017/sessions.html#XAtomicsmemoryorderswhatfor) by [*Frank Birbacher*](https://conference.accu.org/site/stories/2017/presenters.html#XFrank_Birbacher).

    Appropriately scheduled right after Timur's presentation, this **excellent** talk consisted of an overview of the [C++ *memory model*](http://en.cppreference.com/w/cpp/language/memory_model) and of a thorough explanation of [`std::memory_order`](http://en.cppreference.com/w/cpp/language/memory_model).

    The presentation began with an analogy between the C++ memory model and the concept of ["*relativity of simultaneity"*](https://en.wikipedia.org/wiki/Relativity_of_simultaneity), which was surprisingly useful. From the first half of the talk, I particularly liked Frank's explanations of [the **"happens-before"** relation](http://preshing.com/20130702/the-happens-before-relation/) and of [the **"synchronized-with"** relation](http://preshing.com/20130823/the-synchronizes-with-relation/) - these formal relationships are used to describe the memory model in the C++ Standard.

    The presenter's example-driven explanation of all the various `std::memory_order` options was really clear to me - I finally understand all the difference between the various semantics *(yes, even `memory_order_consume`)*, though I still need to reinforce my comprehension of the difference between `acq_rel` and `seq_cst`.

    Here are some ASCII diagrams I created to visually remember them.

    ```cpp
    // Relaxed semantics: operations can be arbitrarily reordered.
    //
    //          op0             ^
    //           |              | ok
    //           |              |
    foo.store(x, std::memory_order_relaxed);
    //           |              |
    //        ok |              |
    //           v             op1
    ```

    ```cpp
    // Release semantics: operations appearing before the store cannot
    //                    be reordered after it.
    //
    //          op0             ^
    //           :              | ok
    //           X              |
    foo.store(x, std::memory_order_release);
    //           X              |
    //        no :              |
    //           v             op1
    ```

    ```cpp
    // Acquire semantics: operations appearing after the load cannot
    //                    be reordered before it.
    //
    //          op0             ^
    //           |              : no
    //           |              X
    foo.load(x, std::memory_order_acquire);
    //           |              X
    //        ok |              :
    //           v             op1
    ```

    I highly recommend watching this talk *(it's [already **available** on YouTube](https://www.youtube.com/watch?v=A_vAG6LIHwQ))* if you're interested in the subject. [Jeff Preshing's blog](http://preshing.com) is also an **amazing** resource, containing many illustrated articles *(e.g. ["acquire and release semantics"](http://preshing.com/20120913/acquire-and-release-semantics/))* on the memory model and multithreaded/lock-free programming in general.

* [**C++ Countdown Pub Quiz**](https://conference.accu.org/site/stories/2017/sessions.html#XCCountdownPubQuiz) by [*Jon Jagger*](https://conference.accu.org/site/stories/2017/presenters.html#XJon_Jagger), and [*Robert Chatley*](https://conference.accu.org/site/stories/2017/presenters.html#XRobert_Chatley).

    This was not a conventional session... this was an interactive pub quiz inspired by the ["Countdown" TV show](https://en.wikipedia.org/wiki/Countdown_(game_show)) where small teams of attendees competed with each other over a drink! This session was one of the highlights of the conference for me: it was **so much fun**!

    In short, every round everyone was given a *set of tokens* and the goal was to produce the *smallest valid C++* program containing all of them. An example is [available on the schedule page](https://conference.accu.org/site/stories/2017/sessions.html#XCCountdownPubQuiz).

    Everything happened in a customized [cyber-dojo](http://cyber-dojo.org/) instance running `g++ 6`. Me and [\@jackayline](https://twitter.com/jackayline) got very close to winning but got disqualified in the last round - we tried to use the given `.` token as part of a [*floating point literal*](http://en.cppreference.com/w/cpp/language/floating_literal), only to later realize that the entire literal is considered a single token! Still, it was an extremely entertaining session and I really hope to see similar ideas in the future.

* [**Writing games in very modern C++**](https://conference.accu.org/site/stories/2017/sessions.html#XWritinggamesinverymodernC) by [*Guy Davidson*](https://conference.accu.org/site/stories/2017/presenters.html#XGuy_Davidson).

    If you want to learn about the *history of video games* and want to see a modern C++ clone of [**"Asteroids"**](https://en.wikipedia.org/wiki/Asteroids_(video_game)) implemented live, this is the talk for you!

    This presentation used an experimental implementation of [P0267](wg21.link/p0267), which proposed the addition of a modernized version of [**cairo**](https://cairographics.org/) to the Standard Library. I want to talk about this, because it concerns me and I'm not convinced it's a good idea.

    Firstly, I think that the scope of "2D graphics API" is way too broad. Some people need high-performance 2D graphics, others need vector graphics, others need something quick for prototyping, others need image manipulation capabilities... and so on. There are **good and robust** libraries for all of the things I mentioned above (and more). Is standardizing a small API that would be **only useful for prototyping** or **non-performance-intensive** applications the right way to go? While it would allow users to quickly display some images/text on the screen, it wouldn't be a huge improvement from simply including something like [SFML](https://www.sfml-dev.org/) or *cairo* itself. If importing a graphics library in your project is such an hassle that encourages standardization of a library just to avoid the pain, then the real problem is package/dependency management.

    Another worry is the choice of the *cairo*-like API. I've used SFML for a quite a long time, and from what I've seen in Guy's talk, *cairo*'s API is closer to C and in my opinion inferior to SFML's. If the committee thinks that a graphics API is a good fit for the Standard Library, I'm sure that something much more powerful and flexible could be proposed. Personally, I would love to see a modernized C++17/20 version of SFML.

    Note that these are only "first impressions" and might not be valid concerns and that I also didn't spend months working on a proposal for 2D graphics. I may be wrong on this subject but still wanted to voice my concerns and hopefully spark some interesting discussion around P0267.

Coroutines, parallelism, and asynchronous programming were key topics of ACCU 2017.

* [*Brad Chamberlain*](https://conference.accu.org/site/stories/2017/presenters.html#XBrad_Chamberlain)'s [**"The Audacity of Chapel: Scalable Parallel Programming Done Right"** keynote](https://conference.accu.org/site/stories/2017/sessions.html#XTheAudacityofChapelScalableParallelProgrammingDoneRight) covered [Chapel](http://chapel.cray.com/), a new programming language for productive and scalable parallel HPC development. I was particularly impressed by the terseness of the language and by its ability to automatically seamlessly achieve parallelism *(both via multithreading and through the use of multiple machines)* via [**"domain maps"**](http://chapel.cray.com/tutorials/SC10/M10-6-DomainMaps.pdf). I wonder how many of Chapel's features could be implemented as a C++ library...

* [**"History of Time: Asynchronous C++"**](https://conference.accu.org/site/stories/2017/sessions.html#XHistoryofTimeAsynchronousC) by [*Steven Simpson*](https://conference.accu.org/site/stories/2017/presenters.html#XSteven_Simpson) demonstrated and benchmarked several ways of implemeting asynchronous network I/O in modern C++, and finally introduced me to `epoll`. It was interesting to see how simple implementations of a server varied in terms of readability and performance by using different techniques *(e.g. `poll`, `epoll`, multithreading, callbacks, coroutines)*.

* [*Dominic Robinson*](https://conference.accu.org/site/stories/2017/presenters.html#XDominic_Robinson)'s [**"Coroutines and C++ DSLs for Human Scale Concurrency"**](https://conference.accu.org/site/stories/2017/sessions.html#XCoroutinesandCDSLsforHumanScaleConcurrency) showed how the [**Coroutines TS**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4649.pdf) *(and macros)* can be (ab)used to implement a safe DSL for actor-based concurrency in modern C++. I found his work very impressive but honestly didn't understand it very well due to my unfamiliarity with the actor model and languages such as Erlang and Racket which were used as an inspiration for the DSL.

* [**Concurrency, Parallelism and Coroutines**](https://conference.accu.org/site/stories/2017/sessions.html#XConcurrencyParallelismandCoroutines) by [*Anthony Williams*](https://conference.accu.org/site/stories/2017/presenters.html#XAnthony_Williams) was an extremely well-presented overview of the [**Concurrency**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0159r0.html), [**Parallelism**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4507.pdf), and  [**Coroutines**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4649.pdf) technical specifications, and of the [**Unified Executors**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0443r1.html) proposal. Anthony also showed how powerful coroutines can be when combined with other features, such as [`std::experimental::future`](http://en.cppreference.com/w/cpp/experimental/future), which allows continuations through a monadic interface.

The closing keynote by Herb Sutter was simply **mindblowing** and was I was genuinely happy and excited throughout the presentation. He has been working on some interesting new metaprogramming-related features that I would love to have today and that would elegantly solve a lot of issues. He asked for the recording to not be published until the proposals are presented to the committee, so I won't discuss any details. Trust me though, it was **awesome**.
