C++20 coroutines have lovely[^lovely_syntax] syntax. They are also a *terrible* fit for game development.[^unity_too]

[^lovely_syntax]: For C++ standards.

[^unity_too]: This isn't unique to C++. Unity's C# `IEnumerator` coroutines have, in my opinion, similar problems for the same structural reasons: they hide their state behind iterator objects you can't introspect, serialize, copy, or network. I can't prove it, but I'd bet a non-trivial fraction of bugs in Unity games come from coroutine overuse -- and a non-trivial fraction of save / multiplayer / replay headaches come from the same place.

If you've ever tried to use them for boss scripts, dialogue, or AI behaviors -- anywhere you want straight-line code that pauses for a few frames -- you've probably hit the same wall I did: opaque handles, heap allocations[^heap], hidden compiler lowering, and -- most damning for games -- *no way to serialize a paused coroutine to disk*.

[^heap]: Yes, in theory the heap allocations *can* be elided via HALO ([P0981](https://wg21.link/P0981)). In practice, you can't *rely* on it.

In this article, I will present **`sfex::Coroutine`**: a ~200-line stackless macro-based coroutine library built around a variant of the classic `switch` + `__LINE__` trick. Like my previously discussed [**`sfex::Profiler`**](https://vittorioromeo.com/index/blog/sfex_profiler.html), these coroutines are meant to be simple and lightweight.




### what's our goal?

Our goal is to implement coroutines that meet these criteria:

1. **Allocation-free**: The entire coroutine state is one `int` plus whatever members[^member_caveat] you put on the struct.

2. **Trivially serializable**: Save the integer state and the struct's data, and that's it. Mid-cutscene saving and loading just works™.

3. **Deterministic**: No hidden compiler lowering; the generated control flow could be written by hand.

4. **Composable**: Coroutines can `await` other coroutines, all in straight-line code.

5. **Tiny**: The whole runtime is a small header you can read in one sitting.

[^member_caveat]: Both "allocation-free" and "trivially serializable" are claims about the *coroutine machinery*, not about whatever members you choose to put on the coroutine struct. If you stick a `std::string` or `std::vector` on it, those types have their own allocations and their own non-trivial serialization story -- the coroutine doesn't change that. The same caveat applies to pointers and references: they would need to be re-resolved on load, just like in any other game object.

The full implementation is part of [my fork of SFML](https://vittorioromeo.com/index/blog/vrsfml.html), in [`SfexCoroutine.hpp`](https://github.com/vittorioromeo/VRSFML/blob/stateless_view_and_text_api_staging/examples/include/ExampleUtils/SfexCoroutine.hpp). Here are a few examples that showcase the feature, that you can directly play here in your browser[^vrsfml]!

[^vrsfml]: My [fork of SFML](https://github.com/vittorioromeo/VRSFML/) natively supports Emscripten as a target. The exact same code works both on desktop and the browser.

<style>
details summary::before {
    content: '';
}

details[open] summary::before {
    content: '';
}
</style>

<details>
  <summary>  🕹️ Shoot-em-up boss fight</summary>
  <iframe src="https://vittorioromeo.github.io/VRSFML_HTML5_Examples/coroutine_embed.html"
          width="100%" height="700" loading="lazy"
          style="border:0; margin:0.5em 0;"></iframe>
</details>

<details>
  <summary>  🕹️ Coreographed top-down stealth game</summary>
  <iframe src="https://vittorioromeo.github.io/VRSFML_HTML5_Examples/coroutine_tutorial_embed.html"
          width="100%" height="700" loading="lazy"
          style="border:0; margin:0.5em 0;"></iframe>
</details>

<details>
  <summary>  🕹️ Dialogue cutscene</summary>
  <iframe src="https://vittorioromeo.github.io/VRSFML_HTML5_Examples/coroutine_dialogue_embed.html"
          width="100%" height="700" loading="lazy"
          style="border:0; margin:0.5em 0;"></iframe>
</details>



### the trouble with C++20 coroutines

Before we get to work, let's see what's wrong with `co_await` for game programming.

1. **Unpredictable heap allocations**: The frame of a C++20 coroutine is allocated dynamically. The standard provides "Heap Allocation eLision Optimization" (HALO) as an *allowed* optimization, but it is *not guaranteed* by the language.

    Compilers do it inconsistently[^inconsistently], and any change to the coroutine body or to where its handle escapes can silently re-introduce the allocation.

    Unoptimized builds might not get HALO at all -- so your debug build *might* silently be allocating per-coroutine on the hot path while your `-O2` build *might* not. For a real-time game with tight latency deadlines, "might" is an uncomfortable term.[^custom_alloc]

    As far as I know, there's no attribute that you can apply to a coroutine to ensure that HALO is applied.

[^inconsistently]: The optimization can also silently break from a compiler update to another, see [LLVM issue #64586](https://github.com/llvm/llvm-project/issues/64586) for an example.

[^custom_alloc]: You *can* provide a custom allocator by overriding `promise_type::operator new`, but this adds more plumbing/complexity. It also doesn't solve the rest of the problems.

2. **Opaque handles and hidden state**: `std::coroutine_handle` is a pointer to compiler-managed state.

    There is no portable way to ask: *"what's in this coroutine's frame? what local variables does it have? where in the source code is it suspended right now?"*

    That makes serialization impossible without compiler hooks.[^reflection] For a game where the player can press F5 to quick-save at any moment, that's a deal-breaker.

    Serialization is not only useful for player-facing features such as quick saving, but is also very important during development. Being able to deterministically replay save states greatly helps with iteration speed and debugging, and being able to pause/introspect the state of the program at any time and interact with it is a massive productivity boost.

    C++20 coroutines are well-shaped for *transient* work: an async HTTP request that flips a "loaded" flag when it completes, a generator that yields a finite sequence and is then discarded, a one-off scheduled task. The coroutine runs, eventually has some side effect on your real game state, and goes away. Its internal state during the suspension is scaffolding -- you don't reach into it, you just wait for the side effect.

    Game logic is *not* transient. The boss's behaviour is *part of the boss's data*: *"we're paused on phase 3, with 0.3 seconds of wait remaining"* must travel alongside the boss's HP and position through saves, quick-loads, and network sync.

    C++20 coroutines push hard against this: the only state model you get is *"internal compiler scaffolding, you don't get to look at it"*.

[^reflection]: Maybe a future reflection API on top of [P2996](https://wg21.link/P2996) will eventually let us peek inside the coroutine frame, but there's no such proposal, and it would take years from now to become usable.

3. **Cumbersome customization machinery**: To get a working coroutine type you need to write a `promise_type`, an `awaitable`, deal with `await_transform`, `final_suspend`, `initial_suspend`, return-object construction, and so on. Even minimal implementations of `generator<T>` involve dozens of lines of plumbing.[^minimal_gen]

[^minimal_gen]: Here is a [minimal implementation](https://gcc.godbolt.org/z/hz7e117fs) of a `generator<T>` -- adding iterator support would require even more code.

For a game I want a coroutine that is **part of an object's data**. When the object dies, the coroutine dies with it. When I serialize the object to a save buffer, the coroutine's state goes with it. When the optimizer is off, there is *no* extra cost compared to the equivalent state machine. C++20 coroutines do not provide any of these guarantees out of the box.

Let's build something that does.



### a tiny cutscene

Imagine a simple scripted exchange between two characters:

1. *"Hero: I finally got you."*
2. Wait one second.
3. *"Villain: You're too late!"*
4. Wait half a second.
5. *"Hero: We'll see about that!"*
6. Wait one second.
7. Hide the dialogue UI.

Here's how this looks as an `sfex::Coroutine`:

```cpp
struct DialogueScene : sfex::Coroutine
{
    Yield operator()(World& world)
    {
        SFEX_CO_BEGIN;

        world.showText("Hero", "I finally got you.");
        SFEX_CO_YIELD(Wait{1.0f});

        world.showText("Villain", "You're too late!");
        SFEX_CO_YIELD(Wait{0.5f});

        world.showText("Hero", "We'll see about that!");
        SFEX_CO_YIELD(Wait{1.0f});

        world.hideText();
        SFEX_CO_RETURN(Done{});
        SFEX_CO_END;
    }
};
```

It nicely reads top-to-bottom. Each `SFEX_CO_YIELD(Wait{...})` means *"pause this coroutine for this many seconds, then come back here"*. You can save the game between any two yields and reload it: the coroutine resumes at the right line, with all its members intact. There is no compiler magic -- the runtime cost is one `int` state field plus a `switch`.

Driving it from the main loop is equally simple:

```cpp
DialogueScene scene;
float waitTimer = 0.f;

while (true)
{
    const float dt = clock.restart().asSeconds();

    if (waitTimer > 0.f)
    {
        waitTimer -= dt;
    }
    else
    {
        scene(world).match(
            [&](NextFrame) { /* ...run again next frame...  */ },
            [&](Wait w)    { waitTimer = w.seconds;            },
            [&](Done)      { /* ...proceed to next scene... */ });
    }

    // ... draw, display ...
}
```

Quick note on the types: `Yield` is a `std::variant<NextFrame, Wait, Done>`, and `.match(...)` is a thin wrapper around `std::visit` using overloaded lambdas. The point is that the three yield *kinds* are explicit data -- the driver pattern-matches on them and decides what to do: ignore, sleep, or stop.



### the same thing as a state machine

It would be easy to make the coroutine look good by comparing it against a (deliberately) bloated FSM. To be fair, here's the *minimal* state-machine equivalent: one step counter and one wait timer.

```cpp
struct DialogueSceneFSM
{
    int   step      = 0;
    float waitTimer = 0.f;

    bool tick(World& world, float dt)
    {
        if (waitTimer > 0.f)
        {
            waitTimer -= dt;
            return true;
        }

        switch (step++)
        {
            case 0: world.showText("Hero",    "I finally got you.");    waitTimer = 1.0f; return true;
            case 1: world.showText("Villain", "You're too late!");      waitTimer = 0.5f; return true;
            case 2: world.showText("Hero",    "We'll see about that!"); waitTimer = 1.0f; return true;
            case 3: world.hideText();                                                     return false;
        }

        return false;
    }
};
```

For *this* particular shape -- a flat *"do X, wait, do Y, wait"* sequence with no loops or branches -- the step-counter FSM is short, readable, and saves to the same number of bytes as the coroutine.

IMHO, the coroutine clearly wins the moment your script starts orchestrating *visual* state on top of the dialogue.

Let's actually draw the two characters on screen and have something happen between the lines of dialogue: **the villain takes three menacing paces toward the hero, with a brief pause between each step.** The dialogue resumes once the approach is finished.

Here's the part of the coroutine that handles the menacing approach:

```cpp
struct CutsceneScene : sfex::Coroutine
{
    int       paceIdx = 0;
    float     t       = 0.f;
    sf::Vec2f paceStartPos;

    Yield operator()(World& world)
    {
        SFEX_CO_BEGIN;

        // ...

        for (paceIdx = 0; paceIdx < 3; ++paceIdx) // take three steps forward
        {
            paceStartPos = world.villainPos;
            t = 0.f;

            while (t < 1.f) // smoothly move by 100px per step
            {
                t += world.dt;
                world.villainPos.x = paceStartPos.x - 100.f * t;

                SFEX_CO_YIELD(NextFrame{});
            }

            SFEX_CO_YIELD(Wait{0.30f});  // menacing pause between paces
        }

        world.showText("Hero", "Don't come any closer!");
        SFEX_CO_YIELD(Wait{1.5f});

        SFEX_CO_RETURN(Done{});
        SFEX_CO_END;
    }
};
```

Three nested control-flow constructs: an outer `for` over the three paces, an inner `while` that does the smooth interpolation, a `Wait` after each pace.

Three pieces of persistent state: `paceIdx`, `t`, `paceStartPos`.

Again, you can read it top to bottom and visually see the temporal flow of events.

Very importantly, those three values *have to be struct members*, not function-locals inside `operator()`. There's a good reason -- the [rules and footguns section](#rules-and-footguns) below covers it -- but the rule of thumb is short: **anything that needs to survive a yield goes on the struct**.

Let's now try to compare the above solution with an FSM-based one. A clever FSM author might create a `Tween` helper to clean up the per-frame interpolation:

```cpp
struct Tween
{
    sf::Vec2f from, to;
    float duration = 0.f, t = 0.f;

    void start(sf::Vec2f f, sf::Vec2f tt, float d)
    {
        from = f;
        to = tt;
        duration = d;
        t = 0.f;
    }

    bool active() const
    {
        return t < 1.f;
    }

    sf::Vec2f tick(float dt)
    {
        t += dt;
        return from + (to - from) * t;
    }
};
```

That handles a *single* smooth movement nicely. But the outer *"do this three times with a menacing pause between each"* doesn't fit in `Tween`; it needs its own state. The painful region of the FSM looks roughly like this:

```cpp
case N:                                                // outer loop entry
    paceIdx = 0;
    step  = N + 1;
    continue;

case N + 1:                                            // start next pace
    if (paceIdx >= 3) { step = N + 3; continue; }      // exit loop
    villainTween.start(world.villainPos, world.villainPos + sf::Vec2f{-100.f, 0.f}, 0.35f);
    step = N + 2;
    return true;

case N + 2:                                            // tick the tween + post-pause
    if (villainTween.active())
    {
        world.villainPos = villainTween.tick(dt);
        return true;
    }
    waitTimer = 0.30f;
    ++paceIdx;
    step = N + 1;                                      // back to top of loop
    return true;
```

Three coordinated case branches plus a `paceIdx` member, just to express `for (paceIdx = 0; paceIdx < 3; ++paceIdx)`. The `Tween` helper covered *one* level of "thing happening over time"; the outer loop is the *second* level, and the FSM has to grow another mini-state-machine on top.

In contrast, the coroutine just has one `int paceIdx;` and lets ordinary C++ control flow do its job.[^tween_aside]

[^tween_aside]: The `Tween` helper is kind of a tiny one-shot coroutine -- start, tick-per-frame, done. The FSM author isn't *avoiding* the coroutine pattern; they're reinventing a more limited version of it for one specific case (linear motion). For arbitrary script-side control flow, they'd end up reinventing the rest too.

The full cutscene discussed above is in [`CoroutineDialogue.cpp`](https://github.com/vittorioromeo/VRSFML/blob/master/examples/coroutine_dialogue/CoroutineDialogue.cpp), and playable in the browser at the top of the article.

My claim is: **coroutines aren't dramatically better for trivial scripts, but they become so the moment we have more than one layer of _"thing happening over time"_.**



### how does it work?

Every SFEX coroutine expands to a switch-based state machine you'd be able to write by hand -- the macros just hide the bookkeeping.

Here is the body of `DialogueScene::operator()` after preprocessing (lightly cleaned up to drop some noise we'll talk about in a moment):

```cpp
Yield DialogueScene::operator()(World& world)
{
    switch (state) // SFEX_CO_BEGIN
    {              //
        case 0:    //

            world.showText("Hero", "I finally got you.");

            state = 1;         // SFEX_CO_YIELD(Wait{1.0f})
            return Wait{1.0f}; //
                               //
        case 1:                //

            world.showText("Villain", "You're too late!");

            state = 2;         // SFEX_CO_YIELD(Wait{0.5f})
            return Wait{0.5f}; //
                               //
        case 2:                //

            world.showText("Hero", "We'll see about that!");

            state = 3;         // SFEX_CO_YIELD(Wait{1.0f})
            return Wait{1.0f}; //
                               //
        case 3:                //

            world.hideText();

            state = 0;         // SFEX_CO_END
            return Done{};     //
    }                          //
                               //
    std::unreachable();        //
}
```

What to notice:

1. **`case` labels can be nested anywhere inside the switch**: This is legal C and C++. A `case` label can sit arbitrarily deep inside the switch's body -- between two normal statements, inside a block, inside a loop -- as long as no *other* `switch` opens between the dispatch and the label. We use this to drop a fresh case label after every yield so the next `switch (state)` jumps right back to where we left off.

2. **Each yield is two halves**: The "going-to-sleep" half (`state = N; return Wait{...};`) runs when the coroutine is currently executing. The "waking-up" half (`case N:`) runs when the driver re-enters after the wait expires. The two halves are placed adjacent to each other in source order, so the body reads top-to-bottom even though execution leaves and re-enters multiple times.

3. **The "where am I" state is just an `int`**: That's literally all that's needed for the coroutine to know where to resume. The rest of the "coroutine frame" -- locals, persistent counters, anything that has to survive a yield -- is just regular member data on your struct. The compiler can't change this representation; it's an integer plus whatever you wrote.

4. **`std::unreachable()` tells the compiler "control flow never reaches here"**: Without it, GCC and Clang would warn about a missing return.

In the *real* macro expansion, every yield is wrapped in a `do { ... } while(0)`:

```cpp
// `SFEX_CO_YIELD` expansion:

do
{
    state = 1;
    return Wait{1.0f};
    case 1:;
}
while (0);
```

This is the standard macro-hygiene trick: it makes the macro expand to a single *statement*, so writing `if (cond) SFEX_CO_YIELD(...);` followed by an `else` does the right thing. The case label sitting inside the `do` block is still part of the enclosing `switch` (rule 1 above), so jumping to it from outside the `do` works fine.

These are the actual macro definitions:

```cpp
// 1. Open the `switch` statement
// 2. Record the initial value of `__COUNTER__`
#define SFEX_CO_BEGIN                              \
    static constexpr int _sfex_base = __COUNTER__; \
    switch (state) { case 0:;

// 1. Update the `state` to the next resumption point
// 2. Return (yield) the specified `value`
// 3. Open the `case` for the next resumption point
#define SFEX_CO_YIELD(value)                    \
    do {                                        \
        state = (__COUNTER__ + 1) - _sfex_base; \
        return value;                           \
        case (__COUNTER__ - _sfex_base):;       \
    } while (0)

// 1. Reset the coroutine state
// 2. Return the specified `value`
#define SFEX_CO_RETURN(value) \
    do                        \
    {                         \
        state = 0;            \
        return value;         \
    } while (0)

// 1. Syntactically close the `switch` statement
// 2. Add the "unreachable" sentinel
#define SFEX_CO_END \
    }               \
                    \
    std::unreachable();
```



### ...counter?

You might be wondering: **why `__COUNTER__` and not `__LINE__`?**

The [Protothreads library](https://github.com/gburd/pt) by [Adam Dunkels](https://dunkels.com/adam/) (providing *"extremely lightweight stackless threads designed for severely memory constrained systems"*) uses `__LINE__` to generate unique state values, making the state values more readable -- *"we're paused at line 42"* is easy to audit. I specifically chose not to use `__LINE__`, and the reason is *serialization*.

Recall that the main pitch of this design is that the entire coroutine state is an `int` field. You save it to disk, you load it back, you resume right where you left off. That works only if the integer means *the same thing* across save and load.

With `__LINE__`, the state value is the source-code line number of the yield.

**Any cosmetic edit to the source file shifts those numbers.**[^relativize_line]

Add a blank line above the function... Reformat with `clang-format`... Insert a `// ...` comment...

Now the integer '47' in the save file -- which used to mean *"paused at the third yield"* -- means something completely different. Save files written before the edit resume mid-function at a random instruction. Not good.

[^relativize_line]: You can mitigate this with relativization (`state = __LINE__ - first_line`), the way Protothreads does internally. That gets you stability against edits *outside* the function -- adding includes, reordering functions in the file, etc. -- but it does *not* help with edits *between yields inside the function*. A `printf` inserted between two yields still shifts every state value below it. `__COUNTER__` survives that case unchanged.

`__COUNTER__` does not have this problem. It increments per *use*, not per source line, so cosmetic edits don't shift the values. As long as you don't add or remove yield points, the numbering for every existing yield stays stable. Even when you do add a yield, only the yields *after* the new one shift -- everything before keeps its identity.[^line_collision]

[^line_collision]: There's a secondary failure mode for `__LINE__`: two yields on the same physical source line (e.g. `SFEX_CO_YIELD(...); SFEX_CO_YIELD(...);`) collide and produce a duplicate `case` label, which is a compile error.

We relativize the counter to the start of each function thanks to `_sfex_base` so the case labels start at `1, 2, 3, ...` per coroutine and don't depend on what comes before in the translation unit.

The only price we pay versus `__LINE__` is that state values are no longer human-readable. For my use case, save-file stability matters far more than peeking at an integer in the debugger.

However, while `__COUNTER__` survives *cosmetic* edits to the function body, it does *not* survive *meaningful* edits. The moment you ship a patch that adds or removes a yield, every state value below the change shifts -- saves written under the previous build will resume into the wrong yield site. There's no easy way to avoid this in the design (the same hazard applies to FSMs, too).[^manually_specify]

[^manually_specify]: One possible solution is have users manually specify the `state` value for resumption points. This is clearly ugly/cumbersome, and requires having the foresight of "leaving some space" between those values in case a new `SFEX_CO_YIELD` is going to be added in the middle.

Two practical workarounds:

- **Tag every save with a coroutine-revision number** (a constant you bump whenever you touch a coroutine), and either refuse to load saves with a mismatched revision or keep an older version of the coroutine in the source code for backwards compatibility.

- **Save the high-level coroutine identity as data**, e.g. *"we're at the start of phase 3 of fight 2"* -- alongside the integer state, and rebuild the integer from the data on load when revisions don't match.



### composing coroutines: enemy AI

A scripted dialogue is a nice introductory example, but the real value shows up when you start *composing* coroutines. Here are two firing patterns for an enemy in a shoot-em-up (a la Touhou):

```cpp
struct RingFire : sfex::Coroutine
{
    int rings = 4;
    int i     = 0;

    Yield operator()(World& world, Enemy& self)
    {
        SFEX_CO_BEGIN;

        for (i = 0; i < rings; ++i)
        {
            world.spawnBulletRing(self.pos, /* count */ 8, /* speed */ 120.f);
            SFEX_CO_YIELD(Wait{0.4f});
        }

        SFEX_CO_RETURN(Done{});
        SFEX_CO_END;
    }
};

struct AimedBurst : sfex::Coroutine
{
    int shots = 5;
    int i     = 0;

    Yield operator()(World& world, Enemy& self)
    {
        SFEX_CO_BEGIN;

        for (i = 0; i < shots; ++i)
        {
            world.spawnBulletAimed(self.pos, world.player.pos, /* speed */ 220.f);
            SFEX_CO_YIELD(Wait{0.2f});
        }

        SFEX_CO_RETURN(Done{});
        SFEX_CO_END;
    }
};
```

If I want my enemy to *cycle forever* between the two patterns, with a one-second pause between each, it's as easy as using a `while` loop:

```cpp
struct EnemyAI : sfex::Coroutine
{
    RingFire   ring;
    AimedBurst aimed;

    Yield operator()(World& world, Enemy& self)
    {
        SFEX_CO_BEGIN;

        while (true)
        {
            ring = {};
            SFEX_CO_AWAIT(ring(world, self));
            SFEX_CO_YIELD(Wait{1.0f});

            aimed = {};
            SFEX_CO_AWAIT(aimed(world, self));
            SFEX_CO_YIELD(Wait{1.0f});
        }

        SFEX_CO_END;
    }
};
```

`SFEX_CO_AWAIT(child(...))` is the composition primitive: it runs the child to completion, propagating whatever the child yields up to the driver.

From the parent's point of view, awaiting a sub-coroutine looks identical to executing the sub-coroutine's *sequence* of yields inline. Yields propagate through arbitrarily many levels.

Each sub-coroutine is *owned* by its parent as a member -- `EnemyAI` contains `ring` and `aimed`. The `ring = {}` line resets the sub-coroutine's state before each cycle, which is just a normal copy-assign. There's no allocation, nor any opaque handle. When `EnemyAI` is destroyed, `ring` and `aimed` go with it.

Internally, `SFEX_CO_AWAIT` expands to the same `state = N; case N:;` pattern from `YIELD`, with a check in the case body:

```cpp
do
{
    state = 1;  // set resumption point to `1`
    case 1:     // begin resumption point `1`
    {
        auto _sfex_res = ring(world, self);

        if (!isFinished(_sfex_res))
            return _sfex_res;  // child still running -- propagate its yield
    }
}
while (0);
```

Each call: tick the child once, grab its yield.

- If the child isn't done, return the yield up to the parent's caller (so a `Wait{0.4f}` from inside `RingFire` propagates all the way out to the driver, which sleeps the parent's parent for 0.4 seconds).

- If the child *is* done, fall through past the `if`, past the `do { } while (0)`, and on to the next statement -- which, in the loop above, is `Wait{1.0f}` between cycles.

That's the whole composition mechanism: nested `case` labels and yields that bubble up by being returned.

Now let's compare to the FSM equivalent. To match the cycle behaviour, every `for` loop and every yield in the sub-coroutines becomes its own state. Even just the `RingFire` part:

```cpp
struct EnemyAIFSM
{
    enum class Phase
    {
        // Embedded `RingFire`:
        Ring_Init, Ring_Fire, Ring_Wait, Ring_Done,

        // Pause:
        WaitAfterRing,

        // Embedded `AimedBurst`:
        Aimed_Init, Aimed_Fire, Aimed_Wait, Aimed_Done,

        // Pause:
        WaitAfterAimed,
    };

    Phase phase     = Phase::Ring_Init;
    int   i         = 0;
    float waitTimer = 0.f;

    void tick(World& world, Enemy& self, float dt)
    {
        while (true)
        {
            switch (phase)
            {
                case Phase::Ring_Init:
                    i = 0;

                    phase = Phase::Ring_Fire;
                    continue;

                case Phase::Ring_Fire:
                    if (i >= 4)
                    {
                        phase = Phase::Ring_Done;
                        continue;
                    }

                    world.spawnRing(self.pos, 8, 120.f);
                    waitTimer = 0.4f;

                    phase = Phase::Ring_Wait;
                    return;

                case Phase::Ring_Wait:
                    waitTimer -= dt;
                    if (waitTimer > 0.f) return;

                    ++i;

                    phase = Phase::Ring_Fire;
                    continue;

                case Phase::Ring_Done:
                    waitTimer = 1.0f;

                    phase = Phase::WaitAfterRing;
                    return;

                case Phase::WaitAfterRing:
                    waitTimer -= dt;
                    if (waitTimer > 0.f) return;

                    phase = Phase::Aimed_Init;
                    continue;

                // ... the `Aimed_*` cases follow the same pattern ...
            }
        }
    }
};
```

Composition *disappears* in a non-hierarchical FSM: it has to flatten `RingFire` and `AimedBurst` into states alongside its own. Each child becomes inlined as a sub-region of the parent's enum, and the parent has to know which child it's currently running. Adding a third pattern -- say, a sweeping laser between rings and aimed shots -- means another set of states and another set of transitions to wire up, while the coroutine version is two new lines.

Hierarchical state machines and behaviour trees offer richer alternatives -- structured composition on top of FSMs -- but their composition primitives are explicit, while coroutines reuse C++'s existing control flow. The coroutine version has the smallest surface area and is, in my opinion, the simplest option (but, perhaps, not the most powerful).



### rules and footguns

This macro trickery is small but has *many* sharp edges. In rough order of how often they bite:

1. **Locals that must outlive a yield have to be struct members.** When the `switch` jumps to a `case` label deep inside the function, it skips over any local-variable declarations between the function entry and the label, so a local declared after the function entry but before a yield is silently re-initialized every time the coroutine is called. If the local sits inside a nested scope (e.g. inside a `for` loop), the compiler will reject the code with a *"jump into protected scope"*[^jumpinto] error instead.

    The convention is short and absolute: anything that must persist across yields becomes a struct member. The compiler does *not* catch the silent-reset case; you have to remember.

    For locals that *don't* need to survive the yield -- per-iteration scratch values that the rest of the loop body wants to be `const` -- there's a less heavy-handed fix: wrap them in their own inner `{ ... }` so the scope ends *before* the yield. The case label inside `SFEX_CO_YIELD` then lands at a point where the locals aren't in scope, and the *"jump into protected scope"* rule simply doesn't apply.

[^jumpinto]: Any jump, including a jump to a `switch` case, is not allowed to skip over a declaration that initializes a variable, except for a few specific cases. Violating this rule will cause a compilation error.

2. **RAII guards do not span yields.** A `std::lock_guard`, file handle, or `unique_ptr` declared as a local *before* a yield is destroyed *at* the yield, because the coroutine returns from the function. "Suspending" really means "returning, with `state` set so we know where to come back". The stack unwinds; RAII fires.

    If you need a resource held across a yield, put it on the coroutine struct -- its destructor runs when the coroutine struct itself is destroyed, not at every yield. Same workaround as locals.

    While unintuitive, I don't think this is a big problem in practice, as you'd probably use C++20 coroutines for any sort of resource-management task that involves locks, files, or networking.

3. **`__COUNTER__` is shared with the rest of the translation unit.** The relativization trick (`__COUNTER__ - _sfex_base`) handles `__COUNTER__` uses *outside* the function: the base captures the counter at function entry, so external uses cancel out. But it does *not* protect against `__COUNTER__` uses *inside* the same function body. Every coroutine yield uses two `__COUNTER__` increments; if a macro *between* two yields also bumps the counter, the case labels for everything below shift.

    Few macros use `__COUNTER__` in practice, but if you mix SFEX coroutines with another `__COUNTER__`-driven construct in the same function the failure mode is a runtime jump to the wrong yield site, not a compile error.

4. **Adding or removing a yield invalidates older save files.** The state integer is an offset into the coroutine's switch dispatch -- adding or removing a yield earlier in the function shifts every state value below it. The same hazard applies to any FSM whose enum you change. Discussed earlier, but here for completeness.

5. **Debugging is awkward.** After a yield, the program counter "zips" back into the switch dispatch on the next call rather than continuing in source order, so a stepping debugger doesn't follow the logical control flow. The call stack shows the current C++ invocation, not the logical nesting of awaited sub-coroutines either -- a deeply-nested `AWAIT` chain looks like one frame in the debugger.

    For non-trivial debugging, a logged `state` value plus a per-coroutine name (`"EnemyAI:state=3"`) is more informative than a stepping session. The state integer is small, stable, and hand-readable once you know which yield is which.

None of the above is fatal. The first rule is the most annoying and frequent one.



### what's next

We've seen:

- Why C++20 coroutines aren't a good fit for game logic.

- A ~200-line header that gives us lightweight coroutines with allocation-free state and trivial save/load.

- How the macros expand under the hood, and why `__COUNTER__` beats `__LINE__`.

- Composition via `SFEX_CO_AWAIT`, and how it crushes the FSM equivalent for branching scripts.

If there's interest in the topic, in a follow-up post I could cover:

- **Parallel composition**: `AWAIT_ALL` and `AWAIT_ANY` for "fire bullets *while* dashing" or "race a script against a timeout".

- **Generic time and yield types**: How the same primitives work for tick-counter games or non-real-time simulations.

If you want to read ahead, the full implementation of the [shoot-em-up example](https://github.com/vittorioromeo/VRSFML/blob/master/examples/coroutine/Coroutine.cpp) lives in VRSFML, and uses parallel composition primitives.



### shameless self-promotion

- I offer training, mentoring, and consulting services.

    If you are interested, check out [**romeo.training**](https://romeo.training/), alternatively you can reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).

- Check out my games on Steam, powered by VRSFML!

    - [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/): A clicker-idle game where you recruit cats to pop bubbles. That's it. Or is it...?

    - [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/): A fast-paced open-source arcade game with user-created content -- the de-facto community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

    - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)
