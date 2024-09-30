

<style>
.inline-link
{
    font-size: small;
    margin-top: -2.6em;
    text-align: right;
    font-weight: bold;
}
</style>

I'm back in London from Jacksonville, where I attended my first ISO C++ meeting. Apart from the long flights and long working hours, it has been a very enjoyable experience for multiple reasons:

* I was able to actively participate in the evolution of the language and influence it by voting and discussing;

* Seeing the amount of work, the engaging debates, and the formal procedures used to move C++ forward gave me a newfound appreciation for all the members of the committee and for the language;

* I learned quite a lot about potentially upcoming changes and about the direction the language is taking;

* Last but not least, I was really glad to see friends from all around the world again and to make new ones!



I went to the meeting on behalf of my company, [Bloomberg](https://www.techatbloomberg.com/), who sponsored my flights and accommodation - **many thanks**! Two papers I authored were up for discussion:

* [(P0915) "Concept-constrained `auto`"](http://wg21.link/P0915) *(co-authored with John Lakos)*

* [(P0792) "`function_ref`: a non-owning reference to a `Callable`"](http://wg21.link/P0792)

The outcome of those discussions will be covered in this trip report, where I'll give a brief overview of how I spent every day and where I'll share my thoughts about my favourite papers and interactions.



### day 1/6

#### arrival

After landing in Jacksonville on Sunday, I was feeling like a zombie due to my sleep deprivation kicking in. It seems like my genetic makeup has this *feature* that prevents me from sleeping on planes.

![Flying towards JAX](resources/img/blog/tr/p0.jpg)

I rushed to the hotel to get some sleep as the first day of the ISO meeting was quickly approaching. Eight hours later I packed my laptop and my paper notebook and walked to the venue of the meeting, eager to finally have a glimpse of what goes behind the scenes of the C++ standardization process.

The meeting began with a plenary introductory session in a room packed with 100+ attendees - I was not expecting to see such a crowd! The room was uncomfortably hot due to the amount of people.

After a round of introductions and an overview of the week's agenda, the people in the room started splitting into *study groups* and *working groups*.

If you're not aware of how C++'s standardization process works, [the "committee" page on isocpp.org](https://isocpp.org/std/the-committee) has a great explanation that comes with a suprisingly helpful diagram:

![WG21 structure](https://isocpp.org/files/img/wg21-structure.png)



#### joint EWG + LEWG session

I began by attending a joint **EWG** *(Evolution Working Group)* + **LEWG** *(Library Evolution Working Group)* session.

The first discussion revolved around reports of *constrained non-templates* being an heavy burden on implementors. This is referring to the use of a [`requires` clause](http://en.cppreference.com/w/cpp/keyword/requires) to constrain a non-template function. After some short debate about the usefulness of the feature and whether or not it was intended, Faisal Vali volunteered to write a paper on the subject for the next meeting.

This interaction and many subsequent ones made it clear that the committee is only willing to discuss and debate on topics and issues that are accurately described by a paper. This is important as it formalizes the process, minimizes ambiguities/misunderstandings, and makes the content available to everyone.

Don't take it personally when someone tells you to *"write a paper"*!



After that, we discussed a very interesting paper whose aim was to describe the procedures by which WG21 would be willing to break backwards compatibility: [(P0684R2) "C++ Stability, Velocity, and Deployment Plans"](http://open-std.org/JTC1/SC22/WG21/docs/papers/2018/p0684r2.pdf), by Titus Winters. In short, the paper classifies future possible changes to the language/library in multiple categories, depending on whether or not the change is statically detectable and on whether or not it has potential to break legacy code.

I strongly believe that without making huge breaking changes the C++ will never be able to get rid of dangerous and outdated language and library constructs. Most of the weird booby traps in the language today originate from backwards-compatibility with C and previous standards. I also understand that same backwards-compatibility is a huge strength of C++.

However, I do see a way out: **modules**. Languages such as Rust have introduced concepts such as ["editions" (previously called "epochs")](https://github.com/rust-lang/rfcs/pull/2052): module-level switches that change the meaning of a program. Imagine the following:

* What if we could make uninitialized variable declaration (e.g. `int i;`) ill-formed?

* What if we could change our defaults? (e.g. `const` by default)

* What if we could fix uniform initialization and `initializer_list`?

* What if we could have `await` and `yield` instead of `co_await` and `co_yield`?

On a purely conceptual level, I think that this would be possible by allowing a module-level switch that changed the semantics of the language, while still retaining backwards compatibility. E.g.:

```cpp
module my_utilities;
epoch "C++XY"; // enable C++XY language features

import old_utilities; // safely import and use utilities that
                      // would not compile under C++XY

export void foo()
{
    int x; // * compile-time error in C++XY
           // * well-formed before C++XY
}
```

I think this is a reasonable way forward (unsure about technical feasibility), but I got resistance from the room when I spoke about this idea in EWG. Several experts were concerned about fragmentation of the language and about the possibility of multiple "dialects" spawning from this approach.

I think those concerns are not valid: my idea is not to provide many "knobs" that users can finely tune to create their own preferred combination of language features - the point is to have a linear progression that allows WG21 to "fix" the language by breaking backwards-compatibility. There would be **no more fragmentation than what we have today** (e.g. codebase targeting different standards/toolchains), but we would be **finally able to extract the "much smaller and cleaner language struggling to get out"** that Bjarne was talking about in "The Design and Evolution of C++".

Another concern about this idea was that *"moving code from one module to the other might break it or change its semantics"*. This is again a non-issue - the fact that you are able to move code from one module to another means that you are in control of it, and you can choose not to do so or to change it to conform to the target module's epoch semantics.

The real problem is when a language/library breaking change hits code that you have no control over... and that's exactly what my proposed idea prevents. I will probably write a brief informal paper to propose this direction to EWG, and see if the working group agrees with me.

But I digress. Afterwards we discussed [(P0921R0) "Standard Library Compatibility Promises"](http://open-std.org/JTC1/SC22/WG21/docs/papers/2018/p0921r0.pdf) which, similarly to the previous paper, proposed a set of guidelines that the evolution working groups should follow when debating on potential breaking changes to the Standard Library. The general ideas were agreed upon by everyone, but the second half of the paper was quite controversial. Some of the guidelines for the users, such as

> Do not add `using namespace std` (or any using directive for sub-namespaces of `std`) to your code.

were overly restrictive. The above, as an example, would prevent UDLs from being usable.

The last paper discussed during the joint session was [(P0922R0) "LEWG wishlist for EWG"](http://open-std.org/JTC1/SC22/WG21/docs/papers/2018/p0922r0.pdf). This paper contains a list of desired language features that would make the life of LEWG (and library developers in general) easier. The described features are not well-defined, and merely serve as direction guidance for EWG.

Interesting thoughts were shared regarding RAII types such as `scoped_lock`, whose entire purpose is to live for the entire duration of a block. When a variable of one of those types is instantiated without a name, it dies immediately:

```cpp
{
    std::scoped_lock{_mutex};
    foo(); // <- Unprotected (!)
}

{
    std::scoped_lock l{_mutex};
    foo(); // <- Protected
}
```

In order to prevent the first undesired case, the idea of applying `[[nodiscard]]` to a constructor was floated around, alongside the possibility of ref-qualifying it as well. Another major concern was the issue of dangerous lifetimes with types such as `string_view` and `function_ref`:

```cpp
void foo(std::string_view x);

int main()
{
    foo(std::string{"hi"}); // OK
    std::string_view sv{std::string{"hi"}}; // Dangling reference (!)
}
```

Something like [(P0936R0) "Bind Returned/Initialized Objects to the Lifetime of Parameters"](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0936r0.pdf), which reminds me of a barebones version of Rust's lifetimes, would probably be enough to solve this problem and statically prevent a class of dangerous bugs.

The paper also expressed the desire to get rid of macros, which was shared by pretty much everybody. One of the action points that came out of the discussion on that topic was to produce a paper that lists all the use cases for macros, so that we can understand how to provide features that can replace them. Me and some other attendees volunteered to write the document, under the lead of Ville Voutilainen (chair of EWG).

![View from my hotel room](resources/img/blog/tr/p1.jpeg)


#### LEWG

After the joint session, I decided to check out LEWG.

One of the papers we discussed was [(P0966R0) "`string::reserve` Should Not Shrink"](http://wg21.link/P0966), which dealt with a very surprising part of [`std::string::reserve`](http://en.cppreference.com/w/cpp/string/basic_string/reserve)'s behavior: it is allowed to shrink the amount of allocated memory if the argument passed to `reserve` is smaller than the current `capacity` of the `std::string` instance. I wasn't aware of this, and I always believed that it behaved in the same way as [`std::vector::reserve`](http://en.cppreference.com/w/cpp/container/vector/reserve) or [`std::unordered_map::reserve`](http://en.cppreference.com/w/cpp/container/unordered_map/reserve), which can only increase the allocated memory.

The paper proposed to make `std::string::reserve` consistent with the other containers - the main motivation was to avoid performance issues in code such as:

```cpp
std::string s;
for(const auto& x : something)
{
    s.reserve(memory_for(x)); // might continuously shrink/grow
    s = stringify(x);
    send(s);
}
```

If `memory_for(x)` returns wildly different values on each iteration, that could lead to many unnecessary allocations and character buffer copies.

While this paper proposed a change that seems completely reasonable, a good counterpoint was provided by someone in the room: being allowed to shrink the memory allows implementations to bring a dynamically-allocated `std::string` back into the SBO (small buffer optimization) realm.

I had similar experiences throughout the meeting: almost every interaction when debating a paper was very well motivated, and I found myself thinking *"that's a very good point"* very often, even when two conflicting opinions were being stated one right after the other.



#### evening session: concept terse syntax

Almost every day of the meeting had one or more evening sessions planned after dinner, that started around 7.30PM and ended around 10.00PM. I can definitely say that WG21 meetings are exhausting, especially compared to conferences.

Anyway, this evening session was about terse (a.k.a. natural) syntax for concepts. This has been a very controversial topic for a plethora of reasons which have been extensively discussed already. We had several papers at the meeting that were presented and debated during the evening session:

* [(P0915R0) "Concept-constrained `auto`"](http://wg21.link/P0915) *(by Vittorio Romeo and John Lakos)*

* [(P0782R0) "A Case for Simplifying/Improving Natural Syntax Concepts"](http://wg21.link/P0782) *(by Erich Keane, Adam David Alan Martin, Allan Deutsch)*

* [(P0807R0) "An Adjective Syntax for Concepts"](http://wg21.link/P0807) *(by Thomas Köppe)*

* [(P0745R0) "Concepts in-place syntax"](http://wg21.link/P0745) *(by Herb Sutter)*

* [(P0956R0) "Answers to concept syntax suggestions"](http://wg21.link/P0956) *(by Bjarne Stroustrup)*

I was the first one to present. In short, our proposal was aiming to introduce an **unambiguous** syntax for concept-constrained variable declarations. E.g.

```cpp
auto<ForwardIterator> it = foo();
// `it` must satisfy `ForwardIterator`, compile-time error otherwise
```

While explaining the proposed feature I was quickly interrupted by many people in the audience who were strongly against having extra syntax around the concept name and were in favour of just `ForwardIterator it = foo()`. My counterpoint was that the proposed syntax was unambiguous, explicit, and that it could be easily extended to remove the surrounding `auto<...>` in the future when people were ready for it.

Unfortunately I kept getting shut down with the argument that *"generic programming needs to look the same as regular programming"*. While I agree with the general sentiment and desired direction, unfortunately the reality is that **it's not the same thing**, due to issues such as:

* `ForwardIterator&&` can either be an *rvalue reference* or *forwarding reference* depending on whether or not `ForwardIterator` is a concept or a type;

* Functions accepting a concept-constrained argument actually are template functions, which behave very differently from non-template functions.

I am not opposed to something close to the terse syntax proposed by Bjarne - however I strongly believe that a migration path that starts with `auto<Concept>` and then moves towards just `Concept` when the inconsistencies between non-template and template functions are addressed would be a safer and better approach. Let developers be explicit if they want to: they might have very good reasons to do so.

Thomas Köppe's paper was also shut down for similar reasons - this saddened me as the document presented a very elegant, unambiguous, and flexible approach that worked well in every circumstance a concept name could be used. Again, the audience simply did not appreciate the fact that `ForwardIterator typename T` had to be spelled instead of `ForwardIterator T`. I feel that sacrificing an elegant solution such as Thomas's only due to the concern that some people might be put off by writing `typename` and `auto` is a real shame, especially when those keywords could have been made optional in the future.

After the other papers were presented and discussed, the audience had to vote. Further work on our proposal and Thomas's was discouraged, and the room generally agreed to move forward with Herb Sutter's proposed syntax, having "independent bindings" when the same concept name was being used multiple times, and having an explicit syntactical indication of what a concept is. I am generally happy with the outcome, as it pretty much addresses my concerns. Herb's proposed syntax roughly looks as follows:

```cpp
void foo(ForwardIterator{} x);
// `foo` is a template function accepting an argument `x` whose type has to
// satisfy the `ForwardIterator` concept.

void foo(ForwardIterator{} x, ForwardIterator{} y);
// `x` and `y` can have different types, as long as they both satisfy the
// `ForwardIterator` concept`

void foo(ForwardIterator{T} x, T y);
// `T` is a placeholder for the type deduced for `x`. It must satisfy the
// `ForwardIterator` concept. `y` has the same type as `x`, which implies
// that it must also satisfy the `ForwardIterator` concept.

void foo(ForwardIterator{}&& x);
// `x` is unambiguously a forwarding reference.

template <typename T>
void foo(ConvertibleTo{T} x);
// `x` is a type that must be convertible to `T`.

template <typename T>
void foo(ConvertibleTo{U}<T> x);
// `x` is a type that must be convertible to `T`. `U` is a placeholder for
// the type deduced for `x`.
```

One annoying quirk of this syntax is that `Concept{}` does not mean "nullary concept", but it's just notation for a "unary concept" where we don't care about giving a name to the deduced type. I proposed a simple fix for this issue...

```cpp
void foo(ForwardIterator{auto} x);
```

...which is very similar to what John and I were proposing in our paper. Unfortunately the sentiment of the room did not change.

![(Photo by [Tony Lewis](https://www.linkedin.com/in/tony-lewis-6bab3814b) - thanks!)](resources/img/blog/tr/p2.jpg)

### day 2/6

#### EWG

I spent the entire day in EWG. We discussed [(P0963R0) "Structured binding declaration as a condition"](http://wg21.link/P0963) *(by Zhihao Yuan)*, which proposed to make the language more consistent by allowing a *structured bindings declaration* as part of an `if` statement:

```cpp
if(auto [ok, elem] = some_map[key]; ok) { /* ... */ }
// Well-formed in C++17.

if(auto [ok, elem] = some_map[key]) { /* ... */ }
// Ill-formed in C++17, proposed by this paper

if(auto rc = some_map[key]) { /* ... */ }
// Well-formed in C++17, as long as `rc` is convertible to `bool
```

The paper's main motivation was consistency between `if(auto [ok, elem] = some_map[key])` and `if(auto rc = some_map[key])` - unfortunately this would mean that the object to be tested in the first case would be the anonymous object that was being destructured (rather than `ok`), which is not always desirable and requires library developers to add conversions to `bool` to support this idiom.

A valuable concern were raised about the ordering of operations: *"does the object get tested after or before the destructuring happens?"* This is especially significant if the destructuring has side-effects.

In the end, the motivation was weak and the risks of this feature being misused were not worth it - the polls resulted in discouragement for further work.



Afterwards we looked at [(P0946R0) "Towards consistency between `<=>` and other comparison operators"](http://wg21.link/P0946) *(by Richard Smith)*, whose aim was to provide consistency between comparisons performed with the new spaceship `<=>` operator and the existing two-way comparison operators (e.g. `<`, `==`, `>=`). The debate and subsequent voting resulted in some very interesting and bold changes that, if eventually merged into the Standard, would break compatibility not only with C++17 but also with C! In short:

* Two-way comparisons between `enum` and floating-point values would be deprecated;

* Two-way comparisons between different `enum` types would be deprecated;

* Three-way comparsions between `enum` and integral types would be allowed;

* **(!)** Comparisons between arithmetical types would now have mathematical semantics. E.g.

    ```cpp
    assert(-1 < 0u);
    // Fails in C++17 and C
    // Would pass with the proposed changes
    ```

(Note that only the changes regarding `enum` were sent forward to Core.)



[(P0912R0) "Merge Coroutines TS into C++20 working draft"](http://wg21.link/P0912) *(by Gor Nishanov)* was next. This paper proposed merging the current Coroutines TS into C++20!

Google *(Geoff Romer and James Dennett)* presented [(D0973R0) "Coroutines TS Use Cases and Design Issues"](http://wg21.link/D0973), a paper criticizing the current design for coroutines and urging to not adopt P0912R0. The main point against it was:

* Even though for situations where coroutines could have a **zero-overhead abstraction** guarantee (e.g. generators, monadic error handling), they will still always allocate on the heap. In order to prevent extra costs we must rely on a (very likely) optimization... which is not *guaranteed* and doesn't happen in `-O0`.

The presenter claimed to have an alternative design (plus implementation experience) that guarantees no dynamic allocation is performed in cases where it can be avoided, and that a paper describing it wasn't at this meeting due to time constraints.

At the end, the general consensus was that we really want coroutines in C++20, and we would proceed with the current TS ("technical specification") unless Google is able to bring a detailed paper in Rapperswil.

I generally agree with the points raised by Google, but I also agree with the fact that we cannot indefinitely delay an useful feature, especially when concrete evidence of a better design/implementation is not available. Hopefully they'll deliver at the next meeting and we'll have an improved version of Gor (and other contributors)'s very valuable and appreciated work.



The last paper we discussed was [(D0780R2) "Allow pack expansion in lambda init-capture"](http://wg21.link/P0780) *(by Barry Revzin)*, which addressed a huge annoyance I've had with lambda expressions since C++14. We are going to finally be able to say:

```cpp
template <typename... Ts>
void foo(Ts... xs)
{
    auto l = [...xs = std::move(xs)]{ /* ... */ };
    // Capture the `xs` pack "by move" into the closure
}
```

This is obviously not limited to `std::move` - it's a extension of *generalized lambda capture syntax* to support parameter packs.

The original proposal's syntax had the ellipsis on the right hand side of the initializer: `[xs = std::move(xs)...]` - EWG summoned a Core expert in the room in order to explain why having the ellipsis on the left hand side was a better idea. In short, every place in the language where **declaration** happens during pack expansion has the ellipsis on the left hand side, while expansion of an existing pack has them on the right hand side:

```cpp
template <typename ...Ts>
//                 ^^^^^
// Declaration of template parameters (left)
void foo(Ts ...xs)
//          ^^^^^
// Declaration of function parameters (left)
{
    bar(xs...);
//      ^^^^^
// Expansion of existing pack (right)
}
```

Developers usually do not format the code like in the snippet above, but Core's argument does make sense and EWG unanimously agreed to change the syntax.

![(Photo by Tony Lewis)](resources/img/blog/tr/p3.jpg)

### day 3/6

#### EWG

My Wednesday morning began in EWG, with intricate discussion about possible ADL issues with modules and different linkages: [(P0923R0) "Modules:Dependent ADL"](http://wg21.link/P0923) *(by Nathan Sidwell)*.

The discussion then moved onto [(P0924R0) "Modules:Context-Sensitive Keyword"](http://wg21.link/P0923) *(by Nathan Sidwell)*, where we tried to agree on a decision regarding the `module` keyword. The committee would really like to use `module` as a keyword, unfortunately there are major issues with making it uncontextual - see [(P0795R0) "From Vulkan with love: a plea to reconsider the Module Keyword to be contextual"](http://wg21.link/P0795) *(by Simon Brand, Neil Henning, Michael Wong, Christopher Di Bella, Kenneth Benzie)* as an example. Basically, we had a few choices:

* Make `module` an uncontextual keyword and break existing code using `module` as an identifier;

* Make `module` a contextual keyword according to P0924R0. This would add some rules to allow the token `module` to be used both as a keyword and as an identifier;

* Provide a way of allowing users to force a token to be interpreted as an identifier (e.g. `@module` or `__identifier(module)`);

* Standardize `module` and future keywords with an unambigious sigil prefix that cannot be used for identifiers (e.g. `@module` is unambiguously a keyword, `module` is unambiguously an identifier).

I suggested the last bullet point, but it encountered heavy resistance from the audience. I liked the idea as it could have also cleanly solved the issue with `co_await` and `co_yield` (`@await` and `@yield` look nicer), and also would have allowed WG21 to keep standardizing useful and terse keywords without risking code breakage. Oh well - some frustration was also part of the ISO meeting experience...

In the end, the committee agreed on two things:

* Leaving the TS as-is, keeping `module` as an uncontextual keyword;

* Encouraging further work on utilities such as `__identifier` or `@identifier`.



#### LEWG

LEWG was next on my list.

We discussed [(P0670R2) "Static reflection of functions"](http://wg21.link/P0670) *(by David Sankel)*, which described an API for static reflection of functions. In order to reflect over a function, you have to use the `reflexpr` operator over a function invocation expression:

```cpp
reflexpr(foo(5))
```

Note that the above does not reflect on the result of `foo(5)`, but instead reflects on `foo`. So, why is the `foo(5)` expression necessary?

Sadly, it is not possible to reflect over *overload sets* or *templates* and get back information such as *"what are all the overloads of `foo`?"* or *"how many template parameters does `foo` have?"*. I asked David about the reasons this path was not pursued, and unfortunately the answer was simple yet brutal: implementors informed the reflection study group about the unfeasibility of that particular design.

Therefore performing overload resolution with `reflexpr(foo(5))` instead of getting back all possible overloads/meanings of `foo` with `reflexpr(foo)` was necessary in order to make this feature work.

I recall everyone in the room being fine with P0670.

During the session we also relaxed the *Lakos rule* by encouraging types in the Standard Library to have conditional `noexcept` when they have "wrapping" semantics - see [(P0884R0) "Extending the noexcept Policy"](http://wg21.link/P0884) *(by Nicolai Josuttis)* if you're interested.



#### CWG

I decided to give CWG (Core Working Group) a go, curious about the kind of discussions that were happening in that room. I walked in during issue processing: the group was going through [Core language issues](http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html) in order of priority, briefly discussing them and assigning them to CWG members in order to produce a solution.

I will admit that understanding most of the discussions required a very in-depth knowledge of the Standard that I currently lack.

Here are some examples of interesting issues that were discussed:

* [CWG issue #2118 - "Stateful metaprogramming via friend injection"](http://wg21.link/cwg2118)

    * This one was particularly amusing as I really enjoyed the [crazy metaprogramming articles](http://b.atch.se/) that relied on this behavior. I asked CWG *why* they wanted to make this ill-formed (as I don't find *"this technique is arcane"* motivation on the issue a good reason), and they explained that since this relies on non well-specified behavior it should not be imposed on implementations.

* [CWG issue #2306 - "Nested friend templates of class templates"](http://wg21.link/cwg2306)

* [CWG issue #2303 - "Partial ordering and recursive variadic inheritance"](http://wg21.link/cwg2303)

This was a nice glimpse of what CWG is like - I wanted to check it out again during paper discussion but didn't manage to find the time this meeting. Looking forward to do that in a future meeting!

![(Photo by Tony Lewis)](resources/img/blog/tr/p4.jpg)

#### evening session - reflection

This was a very interesting evening session - we discussed many important design decisions about reflection and the future of metaprogramming, which are two of my favorite topics.

Firstly [(P0953R0) "`constexpr reflexpr`"](http://wg21.link/P0953) *(by Matúš Chochlík, Axel Naumann, David Sankel)* was presented: this paper proposes an API for reflection that doesn't look like traditional template metaprogramming, instead moving towards a `constexpr` value-based approach. While this sounds similar to [`boost::hana`](http://www.boost.org/doc/libs/master/libs/hana/doc/html/index.html), it goes beyond that: `boost::hana`'s value-based metaprogramming revolves around encoding types inside values, while the proposed model uses some sort of "magic" `constexpr` values that - albeit having the same type - can store different information.

I like to think of this as a form of "compile-time type erasure" for `constexpr` values.

```cpp
template <typename T, typename U>
void foo()
{
    constexpr const reflect::Type* t = reflexpr(T);
    constexpr const reflect::Type* u = reflexpr(U);
}
```

`t` and `u` above have the same type, but they are "magically" pointing to compile-time objects filled with data about `T` and `U` respectively.

Most of the people in the room found the approach superior to both traditional TMP and `boost::hana`-like type-value encoding. However, in order to make this viable, some new language features would be required - an example of that is `constexpr for`:

```cpp
template <typename T>
void print(const T& t)
{
    constexpr const reflect::Class* t = reflexpr(T);
    constexpr for(const reflect::RecordMember* x : t->get_public_data_members())
    {
        std::cout << '\n' << member->get_name() << "=";
        constexpr const reflect::Constant* p = x->get_pointer();
        print(t.*unreflexpr(p));
    }
}
```

The `constexpr for` above, albeit looking like a regular run-time loop, would generate code for each of the members of `t` at compile-time.

Note that no wording nor any implementation was provided for `constexpr for` - the audience was regardless really interested in moving towards this direction and encouraged further work and research on it.



Bjarne Stroustroup then presented [(P0954R0) "What do we want to do with reflection?"](http://wg21.link/P0954), which was a short overview of the most desired use cases by C++ developers. This paper basically showed a very small subset of [(P0385R2) "Static reflection: Rationale, design and evolution"](wg21.link/P0385) *(by Matúš Chochlík, Axel Naumann, David Sankel)*, making the point that users have been wanting a feature to solve that subset of problems for a lot of years and that we should aim to provide a solution for C++20.



Lastly, Herb Sutter presented the changes made in the latest revision of [(P0707R3) "Metaclasses: Generative C++"](http://wg21.link/P0707). Metaclasses are still well-liked and are moving forward. The main change in R3 was switching from the old `$class` syntax...

```cpp
$class interface
{
    // ...
}
```

...to a new, more familiar and composable, `constexpr`-based one:

```cpp
constexpr void interface(meta::type target, const meta::type source)
{
    // ...
}
```

In the above snippet, `target` is basically an "output parameter", where `source` is the original AST of a class. The function `interface` can then be applied to a class in order to transform it.

We also voted on a syntax to apply metaclasses to classes. Something along the lines of...

```cpp
class<M0, M1> foo
{
    // `M0` and `M1` are metaclasses
    // ...
};
```

...was hated the least. In comparison to simply `M0 foo`, this allows multiple metaclasses to be applied on the spot and it is easier to parse for implementations. The room also agreed that we need syntax to apply a metaclass directly in the definition of a class (useful to replace macros like `Q_OBJECT`). That would look something like:

```cpp
class foo
{
    constexpr { __apply(M0); }
    constexpr { __apply(M1); }
    // ...
};
```

![(Photo by Tony Lewis)](resources/img/blog/tr/p5.jpg)

### day 4/6

#### EWG

I spent the entirety of Thursday in EWG.

We began by looking at [(P0893R0) "Chaining Comparisons"](http://wg21.link/P0893) *(by Barry Revzin and Herb Sutter)*. This paper proposes changing the semantics of comparisons like these ones...

```cpp
if(a < b < c)   { /* ... */ }
if(a == b == c) { /* ... */ }
```

...giving them mathematical meaning. Currently, `a < b < c` is evaluated as `(a < b) < c`. With the proposal, it would be evaluated as `a < b && b < c`.

This is obviously a breaking change from C and previous standards of C++. However, the argument made in the paper is that comparisons like the ones above existing in codebases today are almost certainly wrong. The authors researched open-source code for occurrences of breakage, and reported that currently-broken code would actually be fixed if this proposal were to be accepted!

Chained comparisons would be limited to situations where the relational operators evaluate to a type convertible to `bool`, in order to avoid breaking DSLs (domain-specific languages).

The paper also proposes to slightly change *fold expressions* so that they do not generate parenthesized code when folding over a relational operator - this allows them to produce chaining comparisons:

```cpp
template <typename... Ts>
bool are_ordered(Ts... vals)
{
    return (... <= vals);
}

assert(are_ordered(0, 1, 2, 3, 4));
```

The authors were given encouragement to split the proposal in two (separating the fold expression changes) and to revise the rules so that multiple chains with different operators cannot be mixed together.

After some more spaceship operator (`<=>`) fun, we discussed one of my favorite papers from the meeting: [(P0732R0) "Class Types in Non-Type Template Parameters"](http://wg21.link/P0732) *(by Jeff Snyder)*. This paper proposes to extend the set of types allowed as *non-type template parameters* to include any class type with a `default`ed `operator <=>`.

This means that, as long as your class is a [*literal type*](http://en.cppreference.com/w/cpp/concept/LiteralType) and has...

```cpp
std::strong_equality operator<=>(const foo&, const foo&) = default;
```

...then it can be used as a *non-type template parameter*!

This opens up many amazing possibilities. Obviously, fixed-length strings would be allowed. Great compile-time utilities like [Hana Dusikova's regex literals](https://www.youtube.com/watch?v=3WGsN_Hp9QY) or [Victor Zverovich's `fmt` compile-time library format specifiers](http://fmtlib.net/latest/index.html) would now be intuitive and easy-to-use:

```cpp
matches_regex<"^\d{3}-\d{3}-\d{4}$">("foobar");
// ^ evaluates to `false`

print<"{} is not equal to {}">(5, 4);
// ^ outputs "5 is not equal to 4"
```

"Strong typedefs" are another compelling example:

```cpp
struct id
{
    std::size_t _id;
    friend auto operator<=>(id, id) = default;
};

template <id I, typename T>
auto register_type();
```

The proposal received unanimous consent and was sent to Core. There are some wording issues to be addressed in a future revision, but my understanding is that this paper has a good chance to make it into C++20!

You might be thinking - why the restriction to have a `default`ed `operator<=>`? Currently, we have the following invariant in the language: given...

```cpp
template <auto>
void foo();
```

...then, if `a == b`, `&foo<a> == &foo<b>` must hold. This invariant is beneficial for the simplicity of the language and gives the linker a way to decide whether two template instantiations are the same or not. Typically, mangled symbol names of template instantiations contain the values of *non-type template parameters* as part of the symbol.

Since a `default`ed `operator<=>` means that equality can recursively be defined in terms of fundamental types, implementations can rely on it to hold the invariant. In my opinion, this is a simple and elegant solution.

Herb Sutter then presented [(P0934R0) "A Modest Proposal: Fixing ADL"](http://wg21.link/P0934), which attempted to resurrect a paper from 2005 whose aim was to fix some surprising and potentially dangerous corner cases of ADL. Unfortunately, some details were missing and additional research was required. Herb was encouraged to play around with the proposed solutions and experiment with an implementation.

[(P0784R1) "Standard containers and constexpr"](http://wg21.link/P0784) *(by Louis Dionne, Richard Smith, Nina Ranns, Daveed Vandevoorde)* was next. This is another of my favorite proposals from Jacksonville. In short, it allows destructors to be marked `constexpr` and allows dynamic memory allocation in `constexpr` functions. The final goal is to allow usage of Standard Library containers such as `std::vector<T>` or `std::map<T>` in `constexpr` contexts! While it seemed that some extra changes are required to achieve that, EWG was happy with the paper and sent it to Core.

I was negatively surprised by special-casing `std::allocator<T>` so that it could work in a `constexpr` context, but my concerns were alleviated by the fact that there already is special wording in the Standard for it, and that this allows `constexpr` functions using Standard Library containers to be used at run-time as well (compared to something like `constexpr_allocator`).

![(Photo by Tony Lewis)](resources/img/blog/tr/p6.jpg)

### day 5/6

#### LEWG

I spent the entire day in LEWG, eager to present the second revision of my `function_ref` paper.

The first proposal we discussed was [(P0608R1) "A sane variant converting constructor"](http://wg21.link/P0608) *(by Zhihao Yuan)*. This short paper solves the very surprising and dangerous implicit conversions that happen during `std::variant`'s construction/assingment. Here's an example: what do you think the *active alternative* of the following `variant` is?

```cpp
std::variant<std::string, bool> x = "abc";
```

If you guessed `bool`, you are correct... yuck. Zhihao's paper provides a set of formal rules to make sure that the converting constructor of `std::variant` always does the most obvious thing: in short, it doesn't allow *narrowing conversions* and *boolean conversions*. While this breaks some code, the room agreed on forwarding this to LWG as there is not much C++17 code in the wild and as these fixes are greatly needed to increase the adoption of `std::variant`.

We looked at [(P0887R0) "The identity metafunction"](http://wg21.link/P0887) *(by Timur Doumler)* next, which proposes the addition of a simple "identity" metafunction to the Standard Library:

```cpp
template <typename T>
struct identity
{
    using type = T;
};
```

The name `identity` had met strong opposition in the first revision (as `identity` is often used for normal functions), so most of the discussion was spent trying to figure out a good and unambigious name for an utility that pretty much everyone wanted. It was quite difficult to reach an agreement. I also pointed out that this is not just a metafunction, but it's very useful to just wrap a type into a value:

```cpp
template <typename T>
struct type_wrapper
{
    using type = T;
};

template <typename T>
inline constexpr type_wrapper<T> t;

auto x = some_metaprogramming_thing(t<int>);
```

In the end, we voted and the name `type_identity` won. The paper was forwarded to LWG.

Afterwards, we discussed [(P0267R7) "A Proposal to Add 2D Graphics Rendering and Display to C++"](http://wg21.link/P0267) *(by Michael B. McLaughlin, Herb Sutter, Jason Zink, Guy Davidson)*. I explained why I oppose this proposal back in my [ACCU 2017 trip report](https://vittorioromeo.com/index/blog/accu2017_trip_report.html):

> I think that the scope of "2D graphics API" is way too broad. Some people need high-performance 2D graphics, others need vector graphics, others need something quick for prototyping, others need image manipulation capabilities... and so on. There are good and robust libraries for all of the things I mentioned above (and more). Is standardizing a small API that would be only useful for prototyping or non-performance-intensive applications the right way to go? While it would allow users to quickly display some images/text on the screen, it wouldn't be a huge improvement from simply including something like `SFML` or `cairo` itself. If importing a graphics library in your project is such an hassle that encourages standardization of a library just to avoid the pain, then the real problem is package/dependency management.

Even though further work on this proposal was for six revisions, the room was quite brutal this time, and attempted to shut it down completely. Everyone said something along the lines of my quote above to justify the opposition, and experts from NVIDIA also expressed strong disagreement with the paper.

The room agreed that this was a failure for LEWG: not for the contents of the proposal, but for the fact that it wasn't stopped early, wasting a lot of time for both the working group and the authors. Saying *"no"* is sometimes hard, but it would have prevented this situation if it had been done a while ago.

Nevertheless, the work done for P0672 is in my opinion still valuable. While I strongly believe that the paper is not a good fit for the Standard, it would make a high-quality `boost` or standalone library that fills a particular niche.

Guy Davidson, who presented the paper, has written up [a great trip report](https://hatcat.com/?p=33) on his website that also covers this topic in depth.

After the debate on 2D graphics, we moved onto my proposal: [(P0792R1) "`function_ref`: a non-owning reference to a Callable"](http://wg21.link/P0792). It proposes the addition of `function_ref<R(Args...)>` to the Standard Library, intended to be a "vocabulary type" for non-owning references to [`Callable`](http://en.cppreference.com/w/cpp/concept/Callable) objects.

Here's an overview:

```cpp
int foo(function_ref<int()> f)
{
    return f();
}

void bar()
{
    auto l = [i = 0]() mutable { return i++; };
    assert(foo(l) == 0); // <== reference semantics
    assert(foo(l) == 1);
    assert(foo(l) == 2);

    auto g = [](function_ref<int() const> k){ k(); };
    g(l); // <== does not compile  ^^^^^

    g([]{ return 0; }); // <== works with temporaries
}
```

This paper is the final result of work I started with my ["passing functions to functions"](https://vittorioromeo.com/index/blog/passing_functions_to_functions.html) article written in January 2017.

As proven by my research of existing practice, there is a huge need for a way to define *higher-order functions* without having to use `std::function` (owning semantics, hard to optimize) or templates (hard to constrain, must be defined in headers). `function_ref` solves this problem by providing something lightweight and easy to use.

We began by splitting the room in small groups - each group was assigned a paper to review and discuss. I give my special thanks to *Zhihao Yuan*, *Tim Shen*, and *Vicente J. Botet Escriba* for working closely with me during the small group session and providing very valuable feedback.

No major issues were found with the paper. We discussed the possibility of allowing users to specify *ref-qualifiers* in the signature, but ultimately agreed that there was no real use case for it (however, it can be added in the future as a non-breaking change). Additionally, we decided to make copy construction and assignment *trivial* by using `= default`.

After discussing the paper in front of LEWG, **the room agreed to forward it to LWG without any opposition!** This was an highlight of the meeting for me, as I feel like the proposal has a high chance to make it into the Standard Library, thus being my first official contribution to the C++ Standard!

I will make some small tweaks to the paper and send a new revision to LWG in the next mailing list.

![(Photo by Tony Lewis)](resources/img/blog/tr/p7.jpg)



### day 6/6

#### plenary

The last day began and ended with a plenary session during the morning. The chairs of every working group presented their results, and national bodies + official ISO members voted on the final motions.

You can find well-written reports of the approved motions here:

* ["2018 Jacksonville ISO C++ Committee Reddit Trip Report"](https://www.reddit.com/r/cpp/comments/854mu9/) - by Bryce Lelbach

* ["Jacksonville’18 ISO C++ Report"](https://usingstdcpp.org/2018/03/18/jacksonville18-iso-cpp-report/) - by J. Daniel García

![(Photo by Tony Lewis)](resources/img/blog/tr/p8.jpg)



### closing thoughts

That's it for my trip report!

I thoroughly enjoyed the experience and recommend it to everyone interested in contributing to the evolution of the language.

I want to restate my appreciation for everyone involved - being able to see the huge amount of work behind C++ changes your perspective on the committee and its decisions.

Thanks!
