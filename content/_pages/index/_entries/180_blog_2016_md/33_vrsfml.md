In November 2021, motivated by a desire to contribute to the growth and modernization of a library close to my heart[^close_to_my_heart], I joined the SFML team[^joined_the_sfml_team] after volunteering to lead modernization efforts[^volunteering].

Over the past few years, significant time and resources have been invested in the 3.x branch, targeting C++17 and modernizing both SFML's implementation and API.

I greatly appreciate the contributions of everyone involved in the project. However, I felt that a different approach would better align with my vision, which is why I decided to create my own fork: **VRSFML**.

I'm excited to introduce [**VRSFML**](https://github.com/vittorioromeo/VRSFML), a *work-in-progress* and *experimental* fork of SFML that brings several key improvements:

1. **Modern OpenGL and first-class support for Emscripten**[^emscripten_url]
2. **Enhanced API safety at compile-time**
3. **Flexible design approach over strict OOP principles**
4. **New audio API supporting multiple simultaneous devices**
5. **Built-in `SFML::ImGui` module**
6. **Remarkably fast compilation time & small run-time debug mode overhead**

This article will delve into these features, explore the underlying design philosophy, and draw comparisons between the upcoming SFML 3.x release and my fork.

[^close_to_my_heart]: SFML has been an integral part of my journey in learning C++, starting over 11 years ago!

[^joined_the_sfml_team]: <https://en.sfml-dev.org/forums/index.php?topic=28324.0>

[^volunteering]: <https://en.sfml-dev.org/forums/index.php?topic=28301.0>

[^emscripten_url]: <https://emscripten.org/>



### removal of legacy OpenGL and Emscripten support

A pivotal change in VRSFML, and the most challenging to research and implement, was the complete removal of legacy OpenGL calls (SFML 3.x still relies on calls such as `glMatrixMode`[^glmatrixmode]). VRSFML upgrades the rendering pipeline to modern OpenGL, targeting OpenGL ES 3.0+ for mobile and web devices, and OpenGL 4.x+ for desktop platforms.

[^glmatrixmode]: [SFML/Graphics/RenderTarget.cpp#L538-L543](https://github.com/SFML/SFML/blob/master/src/SFML/Graphics/RenderTarget.cpp#L538-L543)

The primary motivation behind this significant change was to provide Emscripten[^emscripten_url] support. I firmly believe that SFML's adoption has been hindered by the inability to deploy applications to the web. In today's landscape, users are hesitant to download executables for small 2D games or prototypes, and developers are reluctant to maintain extensive build matrices for multiple platforms unless absolutely necessary. Web deployment addresses these concerns, aligning VRSFML with other modern game development libraries.

You can now experience many SFML examples[^examples] directly in your browser.

- **Visit [`vittorioromeo/VRSFML_HTML5_Examples`](https://vittorioromeo.github.io/VRSFML_HTML5_Examples/) to explore these demos.**

[^examples]: You will notice that these original SFML examples are a bit lackluster -- I plan to create more engaging ones in the future.

Importantly, user code requires no explicit `#ifdef __EMSCRIPTEN__` directives. The only notable change is the use of an `SFML_GAME_LOOP` macro instead of a `while (true)` loop.

The implementation of Modern OpenGL and Emscripten support involves replacing the legacy SFML 3.x OpenGL pipeline with built-in shaders. The new shader interface is as follows:

<table>
<tr>
<th>Vertex Shader</th>
<th>Fragment Shader</th>
</tr>
<tr>
<td>

```glsl
uniform mat4 sf_u_textureMatrix;
uniform mat4 sf_u_modelViewProjectionMatrix;

in vec2 sf_a_position;
in vec4 sf_a_color;
in vec2 sf_a_texCoord;

out vec4 sf_v_color;
out vec2 sf_v_texCoord;
```

</td>
<td>

```glsl
in vec4 sf_v_color;
in vec2 sf_v_texCoord;

out vec4 sf_fragColor;





```

</td>
</tr>
</table>

Despite advocating for the removal of legacy OpenGL and the inclusion of Emscripten support upstream[^advocating], these proposals were ultimately deemed out-of-scope for SFML 3.x by the rest of the team. This decision was made even though some developers had already implemented makeshift solutions[^makeshift] for these features in SFML 2.x.

The team instead decided that it would be SFML 4.x's focus to develop a completely new, modern rendering pipeline, potentially based on Vulkan. While this is an ambitious goal, I think that it does harm SFML's userbase because (1) SFML 3.x will not bring significant improvements to the rendering pipeline or web support, potentially disappointing users who were expecting these updates, and (2) experienced SFML users might switch to alternative libraries rather than wait an indeterminate period for 4.x to be released.

[^advocating]: These discussions were had in SFML's Discord (`#contributing` channel).

[^makeshift]: E.g. [ZombiesChannel's fork](https://github.com/SFML/SFML/compare/master...Zombieschannel:SFML:SFML-2.6.x-GLES2-FixedResolutions#diff-de81ce9cdf20093c1dd8c3ebae57d412a269b8b6057faf6242c232e20ae079bfR778)



### prevent bugs: better APIs

A philosophy I really value in modern C++ libraries is *"make invalid states unrepresentable"*, preventing users from running into both logic and memory bugs. Combine that with the *"code is read more than it is written"* adage, and the principle of maximizing "local reasoning"[^local_reasoning], and you'll understand why I love APIs that leverage the C++ type system to improve safety and force users to think about error handling.

[^local_reasoning]: "Local reasoning is the idea that the reader can make sense of the code directly in front of them, without going on a journey discovering how the code works" ([source](https://medium.com/@nathangitter/local-reasoning-in-swift-6782e459d))

In June 2022, a GitHub issue[^error_issue] was created to discuss the topic of "error handling" in SFML 3.x. After a lot of discussion and support from both the SFML team and users that tracked the issue[^users_that], it was decided that SFML 3.x would embrace algebraic data types as a way to make error handling obvious and explicit through C++'s type system. As SFML's BDFL[^bdfl] stated at the end of the issue in May 2024 *(emphasis mine)*:

[^error_issue]: <https://github.com/SFML/SFML/issues/2139>

[^bdfl]: "Benevolent Dictator For Life"

[^users_that]: Positive GitHub reactions from non--SFML-team-members were given in issue comments in favour of ADT-based error handling, alongside positive feedback on Discord.

> Closing this, as *the direction has been decided and largely implemented*. Feel free to reopen if there are still points you think need discussing.

Thus, a massive amount of work and effort[^work_and_effort_empty] was put into changing SFML 2.x's error-prone API:

<center>

<table>
<tr>
<th>Legacy API</th>
</tr>
<tr>
<td>

```cpp
sf::Font font;
font.loadFromFile("arial.ttf"); // Can silently fail!

sf::Text text(font);
```

</td>
</tr>
<tr>
<th>New API</th>
</tr>
<tr>

<td>

```cpp
const auto font = sf::Font::loadFromFile("arial.ttf").value();
sf::Text text(font);
```

</td>
</tr>
</table>

</center>

[^work_and_effort_empty]: For a non-exhaustive list of PRs, see [#3088](https://github.com/SFML/SFML/pull/3088)'s description.

The SFML team's initial commitment to modernizing error handling and API safety in version 3.x was a significant step towards creating a more robust and user-friendly library. This approach, which leveraged factory functions returning `std::optional<T>`, brought several key benefits:

1. **Elimination of default empty states**: SFML types became non-default-constructible, ensuring that objects were always in a valid state.

2. **Compile-time prevention of bugs and undefined behavior**: The new design made it impossible to use uninitialized objects, a common source of errors in SFML 2.x.

3. **Forced consideration of failure cases**: By returning `std::optional<T>`, APIs required users to explicitly handle potential failures.

4. **Explicit, visible error handling**: The use of algebraic data types made error handling more apparent in the code, encouraging better practices.

5. **Simpler and faster implementation**: Member function implementations of SFML types do not have to check/assert for the empty state.

This design also led to elegant and safe resource management. As an example, consider the task of loading a potentially-missing texture from user-generated content, reverting to a default texture in case of failure:

```cpp
const auto texture = sf::Texture::loadFromFile(userTxPath).value_or(defaultTx);
```

However, despite my veto[^veto], the SFML team's decision in July 2024 to revert these changes and switch to exception-based error handling[^revertprurl] (reintroducing default constructors and empty states) represents, in my opinion, a significant regression in terms of API safety and modern C++ practices. This reversal:

[^revertprurl]: <https://github.com/SFML/SFML/pull/3152>
[^veto]: [SFML/pull/3152#issuecomment-2228238814](https://github.com/SFML/SFML/pull/3152#issuecomment-2228238814)

1. Reintroduces the possibility of invalid states, violating the "make invalid states unrepresentable" principle.

2. Removes the forced consideration of failure cases in the type system, potentially leading to less robust code.

3. Makes error handling implicit, reducing code readability and correctness.

4. Misses an opportunity to educate users about modern C++ features like `std::optional`.

5. Prevents SFML from being used in projects/platforms where exceptions are disabled/unavailable.

The justifications provided for this decision, such as users not being ready for `std::optional` or concerns about migration difficulty, seem to underestimate the capabilities of SFML users and prioritize ease of migration over substantial improvements to the library.

This reversal not only impacts the technical aspects of SFML but also its pedagogical value. As a library often used by beginners, SFML had an opportunity to introduce users to modern C++ practices and type-safe programming. By reverting to a more traditional but less safe API, this educational aspect has been diminished.

Ultimately, this decision appears to prioritize short-term convenience over long-term robustness and modernity. One of the SFML maintainers who approved the reversal described it[^advocating] quite concisely *(emphasis mine)*:

> The default constructor *(of SFML types)* is equivalent to the default constructor of `std::optional`. It *leaves the object in an empty state that is basically unusable*. This is how SFML has always been. Resources like `sf::Font` have an internal empty state so they're *secretly optionals in disguise*.

These statements raise concerns about the potential implications for API safety and robustness. For comparison, here's the SFML 3.x equivalent of the elegant VRSFML one-liner (shown above):

```cpp
sf::Texture texture; // Empty state... need to be careful.
                     // Also, can't use `const`!

if (!texture.loadFromFile(userTxPath))
    texture = defaultTx;
```

VRSFML fully adopts the factory-based API design as a commitment to safer, more explicit programming practices:

```cpp
// ERROR: Does not compile - VRSFML resources lack a default "empty state"
/* sf::SoundBuffer sb0; */

// OK: *Explicit* exception throwing on failure
const auto sb1 = sf::SoundBuffer::loadFromFile("ball.wav").value();

// OK: *Explicit* handling of failure case
const auto sb2 = sf::SoundBuffer::loadFromFile("ball.wav");
if (!optSoundBuffer.hasValue()) { return EXIT_FAILURE; }

// OK: *Explicit* fallback provision
const auto sb3 = sf::SoundBuffer::loadFromFile("ball.wav").valueOr(defSound);
```

VRSFML also applies this philosophy to APIs that SFML 3.x didn't originally modify, such as `sf::Window`, `sf::Image`, and `sf::Sprite`.



### solving the white square problem: lifetime tracking

The case of `sf::Sprite` is particularly noteworthy. In SFML 2.x and 3.x, `sf::Sprite` stores a raw pointer to `sf::Texture`, which can lead to lifetime issues:

```cpp
auto makeSprite()
{
    sf::Texture texture(/* ... */);
    return sf::Sprite(texture);
}

someRenderTarget.draw(makeSprite()); // UNDEFINED BEHAVIOR!
```

This "raw pointer relationship" is not unsual in SFML, as one of the principles that the library follows is that users should decide how to manage their own resources. However, especially for beginners, many[^many_issues] lifetime issues were caused by the texture associated to a sprite being destroyed prior to the sprite itself, resulting in undefined behavior and the dreaded "white square problem"[^white_square_problem].

[^many_issues]: It is sufficient to search online for "white square SFML" to find several reports of this issue.

[^white_square_problem]: <https://www.sfml-dev.org/tutorials/2.6/graphics-sprite.php#the-white-square-problem>

Upon investigating the issue, I devised a method to prevent a longstanding bug in SFML at compile-time. The `sf::Sprite` class only requires texture information at the time of rendering. Storing a `sf::Texture*` within the sprite is unnecessary, as it can be passed during the draw call. This approach ensures that a valid texture object must be provided by the caller at the point of the draw call, effectively eliminating the issue.

I anticipated that this solution would be well-received by the SFML team due to its technical merits. However, it was rejected despite its advantages, with arguments such as *"this does not follow OOP principles"* and *"this is not future-proof, as when we switch to Vulkan, the sprite might need to know the texture at construction time"*. The technical discussions and arguments can be found in [#3072](https://github.com/SFML/SFML/pull/3072) and [#3080](https://github.com/SFML/SFML/pull/3080), which document my two attempts at enhancing the safety of a fundamental SFML type for version 3.x.

My disappointment with the reception of this idea and the constant rejections is evident in these discussions. In retrospect, I acknowledge that my tone may have been influenced by emotion and frustration more than was appropriate for a technical discourse.

VRSFML implements my proposed solution, extending it to SFML shapes as well:

```cpp
// ERROR: Does not compile - sprites no longer store a texture pointer
/* sf::Sprite sprite(texture); */

// OK: Prepare the sprite to eventually display the entire texture
sf::Sprite sprite(texture.getRect());

// ERROR: Does not compile - texture must be provided during the draw call
/* window.draw(sprite); */

// OK: texture available at the point of the draw call - no lifetime issues
window.draw(sprite, texture);
```

VRSFML takes this approach further by introducing a toggleable lightweight lifetime tracking system for all SFML resources. This system detects common lifetime mistakes between dependee types (e.g., `sf::Font`) and dependent types (e.g., `sf::Text`) at runtime, providing users with readable error messages.

(This comprehensive solution was also proposed for inclusion in SFML 3.x[^lifetime_proposed] but was not accepted. Instead, [#3122](https://github.com/SFML/SFML/pull/3122) was chosen, which addresses only textures and relies on undefined behavior.)

[^lifetime_proposed]: <https://github.com/SFML/SFML/pull/3097>

When lifetime tracking is enabled and a dependee-dependant relationship is broken, the following error message is displayed (alongside a stack trace, if `SFML_ENABLE_STACK_TRACES` is provided):

> FATAL ERROR: a texture object was destroyed while existing sprite objects depended on it.
>
> Please ensure that every texture object outlives all of the sprite objects associated with it,
> otherwise those sprites will try to access the memory of the destroyed texture,
> causing undefined behavior (e.g., crashes, segfaults, or unexpected run-time behavior).
>
> One of the ways this issue can occur is when a texture object is created as a local variable
> in a function and passed to a sprite object. When the function has finished executing, the
> local texture object will be destroyed, and the sprite object associated with it will now be
> referring to invalid memory. Example:
>
> ```cpp
> sf::Sprite createSprite()
> {
>     sf::Texture texture(/* ... */);
>     sf::Sprite sprite(texture, /* ... */);
>
>     return sprite;
>     //     ^^^^^^
>
>     // ERROR: `texture` will be destroyed right after
>     //        `sprite` is returned from the function!
> }
> ```
>
> Another possible cause of this error is storing both a texture and a sprite together in a
> data structure (e.g., `class`, `struct`, container, pair, etc...), and then moving that
> data structure (i.e., returning it from a function, or using `std::move`) -- the internal
> references between the texture and sprite will not be updated, resulting in the same
> lifetime issue.
>
> In general, make sure that all your texture objects are destroyed *after* all the
> sprite objects depending on them to avoid these sort of issues.

The message is dynamic and changes depending on the types of the SFML objects. The impact on the implementation of the SFML types themselves is minimal, e.g.:

```cpp
// in `Font.hpp`, add:
SFML_DEFINE_LIFETIME_DEPENDEE(Font, Text);

// in `Text.hpp`, add:
SFML_DEFINE_LIFETIME_DEPENDANT(Font);
```



### type-safe API: shader uniforms

In SFML 2.x and 3.x, shader uniform manipulation is achieved through various overloads of the following form:

```cpp
void sf::Shader::setUniform(const std::string& name, bool x);
void sf::Shader::setUniform(const std::string& name, int x);
void sf::Shader::setUniform(const std::string& name, float x);
// ...
```

While functional, this API presents two significant issues:

1. It doesn't account for cases where `name` doesn't refer to an actual shared uniform variable.

2. Each `setUniform` call requires translating the `name` parameter to the uniform's location (an integer).

VRSFML addresses these issues with an improved API. First, users must locate a uniform using this new member function:

```cpp
std::optional<UniformLocation> getUniformLocation(std::string_view name);
```

The `UniformLocation` type is a strong typedef[^strong_typedef] over an integer. Like most VRSFML types, it's non-default-constructible and can only be created by `sf::Shader`. This ensures that possessing a `UniformLocation` object guarantees the existence of a real uniform location.

The previous overloads have been modified to accept a `UniformLocation`:

```cpp
void sf::Shader::setUniform(UniformLocation location, bool x);
void sf::Shader::setUniform(UniformLocation location, int x);
void sf::Shader::setUniform(UniformLocation location, float x);
// ...
```

This design ensures that `setUniform` can only be called with a valid `UniformLocation` object in scope, implying that `getUniformLocation` must have been called and the returned `std::optional` successfully unwrapped.

[^strong_typedef]: <https://arne-mertz.de/2016/11/stronger-types/>



### fewer OOP hierarchies

SFML has historically embraced the Object-Oriented Programming (OOP) paradigm, even when it presented clear drawbacks. For instance, all SFML types that could be rendered derived from a polymorphic `sf::Drawable` base class and were required to implement a `draw` pure `virtual` member function:

```cpp
virtual void draw(sf::RenderTarget&, const RenderStates&) const = 0;
```

While this may appear to be a logical decision, it exemplifies how attempting to map real-world concepts into a programming API can lead to several significant drawbacks:

1. **Encourages poor run-time performance**: SFML users, particularly those new to C++, were implicitly encouraged to store multiple drawable objects homogeneously in containers like `std::vector` of `std::shared_ptr<sf::Drawable>`[^people_actually_do_this]. This approach is detrimental to performance and cache utilization. The absence of a common polymorphic base class would guide users towards a more appropriate data-oriented design.

[^people_actually_do_this]: [GitHub search for `std::shared_ptr<sf::Drawable>`](https://github.com/search?q=%22std%3A%3Ashared_ptr%3Csf%3A%3ADrawable%3E%22&type=code)

2. **Promotes inheritance over composition**: Users tend to compose multiple drawable objects by creating their own wrapper that also derives from `sf::Drawable`, simply to fit into the existing hierarchy. In many cases, a simple function or `struct` would suffice to group multiple drawables together more efficiently.

3. **Incompatible with efficient rendering solutions**: The concept of rendering one object at a time is far removed from how GPUs actually function. Vertex batching is necessary for efficient use of modern hardware. Erasing the type of a drawable object would make the implementation of any batching API on top of existing SFML types quite difficult, whereas a batching API could be easily implemented for, e.g., a `std::vector<sf::Sprite>`.

4. **Results in suboptimal and potentially dangerous APIs**: Consider the `sf::Sprite` and `sf::Shape` "white square problem" mentioned earlier. This issue only existed because these types had to conform to the `sf::Drawable` interface, which did not accept a `sf::Texture` during the draw call, necessitating its storage beforehand.

VRSFML eliminates the `sf::Drawable` class entirely and converts `sf::Shape` and `sf::Transformable` into non-polymorphic base classes, encouraging the adoption of a more efficient data-oriented design.

Notably, neither the original SFML examples nor the source code of [Open Hexagon](https://store.steampowered.com/app/1358090/Open_Hexagon/)[^open_hexagon] required adaptation to accommodate these changes, suggesting that the original class hierarchies may not have been necessary in the first place.

[^open_hexagon]: Open Hexagon is a commercial open-source game I created using VRSFML, available on Steam

Note that, if needed, type erasure is nowadays trivial to achieve: a `std::function<void(sf::RenderTarget&)>` would easily provide the ability to draw anything polymorphically.

Furthermore, `sf::VertexArray` has been replaced with `std::vector<sf::Vertex>`. The former was essentially a thin wrapper around the latter, existing primarily to conform to the `sf::Drawable` interface. Useful vector APIs such as `reserve` or `emplace_back` were not available on `sf::VertexArray`, making it a less advantageous choice compared to `std::vector<sf::Vertex>` without any discernible benefits. This modification was proposed for upstream SFML but was rejected strongly[^propose_vertexarray_removal].

[^propose_vertexarray_removal]: <https://github.com/SFML/SFML/pull/3118>



### all the audio devices, at the same time

VRSFML has served as a playground for simplifying SFML's implementation, with a focus on minimizing hidden global state. In SFML 2.x and 3.x, all graphical objects inherit from `sf::GlResource` (which internally owns a `std::shared_ptr<void>` to a shared OpenGL context), and all audio objects inherit from `sf::AudioResource` (which internally owns a `std::shared_ptr<void>` to a shared `miniaudio` device).

That design primarily supports globally defined SFML objects, which I don't want to encourage. VRSFML completely removes `sf::GlResource` and `sf::AudioResource`. Instead, users create `sf::GraphicsContext` and `sf::AudioContext` "manager objects" at the start of `main` (or at an appropriate point). These context manager objects hold the state previously shared between objects. This change led to numerous simplifications in SFML's internals, including the elimination of transient OpenGL contexts.

Operations requiring an active OpenGL context now need a `sf::GraphicsContext&` parameter. This includes loading textures, fonts, or creating windows. While seemingly less ergonomic, it's not problematic in practice if the application is designed to pass the graphics context from `main` to its callees. This requirement ensures that any graphical operation will always have an OpenGL context by construction order, eliminating the need for transient contexts. For example:

```cpp
// Create graphics context
sf::GraphicsContext graphicsCtx;

// Create a render window
sf::RenderWindow window(graphicsCtx,
                        {.size  = {1024u, 768u},
                         .title = "SFML Example",
                         .style = sf::Style::Titlebar | sf::Style::Close});

// Load a graphical resource
const auto logoTx = sf::Texture::loadFromFile(graphicsCtx, "logo.png").value();
```

(Note the new API for window settings, utilizing C++20's designated initializers feature.)

The principle for `sf::AudioContext` is similar, but this simplification led to extending the API to support multiple simultaneous devices. While SFML 3.x allows setting the "current active audio device", users can't play different sounds through different devices simultaneously. VRSFML provides this functionality with an elegant, type-safe API:

```cpp
// Load audio resources
auto music0 = sf::Music::openFromFile("doodle_pop.ogg").value();
auto music1 = sf::Music::openFromFile("ding.flac").value();

// Create audio context
auto audioContext = sf::AudioContext::create().value();

// Query hardware audio device handles
const auto deviceHandles =
    sf::AudioContextUtils::getAvailablePlaybackDeviceHandles(audioContext);

// (Assuming there are at least two available playback devices)
const sf::PlaybackDeviceHandle& pdh0 = deviceHandles.at(0);
const sf::PlaybackDeviceHandle& pdh1 = deviceHandles.at(1);

// Create actual playback devices for the queried handles
sf::PlaybackDevice pd0(audioContext, pdh0);
sf::PlaybackDevice pd1(audioContext, pdh1);

// Simultaneously play the two musics on two different devices
music0.play(pd0);
music1.play(pd1);
```

I presented the idea of supporting multiple simultaneous devices when SFML 3.x audio-related changes were being discussed. The response[^audiodevice_response] was:

> This is a very very advanced topic, one so advanced that not many AAA games even dare to support simultaneously outputting different audio streams to different audio playback devices. While technically possible, this would require an entire redesign of the sfml-audio public API, and that is definitely out of scope for this PR and SFML 3.

[^audiodevice_response]: [SFML/pull/3029#issuecomment-2131745897](https://github.com/SFML/SFML/pull/3029#issuecomment-2131745897)



### dear imgui, out of the box

Omar Cornut's widely popular Dear ImGui[^dearimguiurl] library has long been compatible with SFML through Elias Daler's `imgui-sfml`[^imguisfmlurl] compatibility layer. During the development of SFML 3.x and VRSFML, updating `imgui-sfml`'s internals to match SFML's evolving API proved challenging. Managing this additional dependency in real SFML projects like Open Hexagon was also cumbersome.

[^dearimguiurl]: <https://www.dearimgui.com/>
[^imguisfmlurl]: <https://github.com/SFML/imgui-sfml>

To address these issues, `imgui-sfml` has been incorporated as an "official" VRSFML module: `SFML::ImGui`. Like other SFML modules, users can choose whether to build it (alongside the Dear ImGui dependency). The internals of `imgui-sfml` have also been updated to use a modern OpenGL backend, supporting Emscripten.

Hidden global state has been minimized and encapsulated in a `sf::ImGui::ImGuiContext` object, which can be created in `main` similarly to `sf::GraphicsContext` and `sf::AudioContext`. The following example demonstrates its usage:

```cpp
sf::GraphicsContext     graphicsCtx;
sf::ImGui::ImGuiContext imGuiCtx(graphicsCtx);

sf::RenderWindow window(graphicsCtx,
    {.size{1024u, 768u}, .title = "ImGui + SFML = <3"});

if (!imGuiCtx.init(window))
    return -1;

sf::Clock deltaClock;

SFML_GAME_LOOP
{
    while (const sf::base::Optional event = window.pollEvent())
        imGuiCtx.processEvent(window, *event);

    imGuiCtx.update(window, deltaClock.restart());

    ImGui::ShowDemoWindow();

    window.clear();
    imGuiCtx.render(window);
    window.display();
};
```



### moving away from the Standard Library

This change is likely to be controversial. I highly value rapid iteration times when developing software, particularly games. To achieve a satisfying and productive code-compile-test loop, a library must compile quickly and have minimal debug run-time overhead. It's reasonable to assume that a significant portion of the game development community shares these priorities. Improving compilation times has been a subject of great interest to me, which I've thoroughly researched, as demonstrated by my well-received talk on the topic[^improv_times].

Motivated by the game development community's frustration with modern C++, I've also conducted in-depth research on the impact of C++ abstractions on debug performance. This is evidenced by my September 2022 article[^debugperf], which was also well-received and prompted changes in all three major compilers to address some of the issues raised.

[^improv_times]: <https://www.youtube.com/watch?v=PfHD3BsVsAM>
[^debugperf]: [`debug_performance_cpp` article](https://vittorioromeo.info/index/blog/debug_performance_cpp.html)

Unfortunately, primarily due to historical and backward-compatibility constraints, all major C++ standard library implementations fall short of the compilation speed and debug performance standards I consider acceptable. Explaining why standard library headers are so heavyweight and why standard library abstractions are buried under numerous layers of templates would require an extensive article of its own.

It's important to emphasize that this situation is not due to incompetence or carelessness on the part of the dedicated volunteers who work on these fundamental components of software engineering. Rather, it's a result of a long and complex history of the C++ language and its standard library.

Given these constraints, I've made the decision to reduce dependencies on the C++ standard library in VRSFML by introducing `SFML::Base`. This new module provides essential utilities including: type traits, smart pointers, math functions, assertions, PImpl wrappers, basic algorithms, and much more.

These utilities are meticulously designed to:


1. Minimize header dependencies

2. Reduce the number of template instantiations

3. Decrease the depth of the call stack

Annotations like `[[gnu::always_inline]]` are employed to minimize run-time overhead of basic abstractions in debug mode. Additionally, numerous (opinionated) quality-of-life improvements have been implemented in the API and functionality of `SFML::Base` types compared to their standard counterparts.

VRSFML utilizes its own vocabulary types in the public API, such as:

- `base::Optional<T>` instead of `std::optional<T>`

- `base::UniquePtr<T>` instead of `std::unique_ptr<T>`

While this was a challenging decision, benchmarking and testing have demonstrated that the benefits in compilation time and debug performance justify this approach. I do intend to revisit this decision in the future if necessary, and potentially revert to the standard library or enable users of the library to choose their preferred types at configuration-time.



### other various improvements

- Optimized `sf::Shader` source loading by reading into a thread-local vector, enhancing performance.

- Improved `sf::Text` rendering with outlines, reducing it to a single draw call compared to the upstream SFML's two draw calls.

- Made `sf::priv::err` thread-safe and more ergonomic, eliminating the need for `std::endl`.

- Verified that all factory functions support RVO or NRVO, as confirmed by GCC's `-Wnrvo` flag.

- Added the useful `sf::Vector2<T>::movedTowards(T, Angle)` function.

- `sf::Color`, `sf::Vertex`, `sf::Vector2`, `sf::Vector3`, and `sf::Rect` are now aggregate types.

- Updated `sf::Socket` constructor to include a mandatory `isBlocking` parameter, ensuring users are aware of blocking behavior.

- Removed catch-all headers such as `SFML/Audio.hpp` to promote good header hygiene in user projects.



### Q/A

> How do I get started with VRSFML?

- There is no official documentation yet, and many APIs are still undocumented. Still, I believe it's quite easy to get started by looking at the [existing examples](https://github.com/vittorioromeo/VRSFML/tree/master/examples) and experimenting with them. The SFML documentation is also an incredibly valuable resource, but do keep in mind that many APIs have changed in VRSFML.

> Is VRSFML a direct competitor to SFML?

- I don't consider VRSFML a competitor to SFML. Many of the changes in VRSFML are quite opinionated and specifically tailored for more experienced C++ and SFML developers. Broadly speaking, SFML 3.x seems to target beginner C++ developers and is well-suited for smaller games, applications, or prototypes. In contrast, VRSFML is geared towards more experienced C++ developers and is better suited for larger games or applications that require scalability and robustness.

  Additionally, I view VRSFML as a valuable testing ground for features and changes that might eventually be incorporated into upstream SFML. The freedom to experiment and break things without the need for a formal PR review process has allowed me to push boundaries and explore new ideas. Perhaps the SFML team will find some of these ideas worth integrating into upstream SFML.

> Are you still part of the SFML team?

- Yes, I am. However, I've reduced the time I spend contributing to SFML to focus on my fork and other projects. While I remain interested in contributing to SFML and influencing its future direction, I intend to limit my contributions to smaller, less controversial changes to avoid lengthy technical debates or disagreements.

> Is VRSFML production-ready?

- No, VRSFML is still a work in progress with some issues that need to be resolved. Additionally, it has only been tested only with GCC and Clang, and only on Windows x64, Windows ARM, Linux x64, and Emscripten. Mobile devices have not been tested, and some mobile-specific and MSVC-specific code adjustments are necessary. Contributions are welcome!

> What are the future plans for VRSFML?

- I don't know yet. I'm interested in keeping the fork up-to-date with upstream, and in writing a nice small example game that runs on the browser to showcase Emscripten support. Apart from that, my future effort on the project will depend on the amount of interest it generates in the open-source community and on my personal future game development efforts using the library.



### shameless self-promotion

- I now offer 1-on-1 mentoring sessions and consulting services. If you're interested, please contact me at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

   - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)

- If you enjoy fast-paced open-source arcade games with user-created content, check out [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/), my VRSFML-powered game [available on Steam](https://store.steampowered.com/app/1358090/Open_Hexagon/) and [on itch.io](https://itch.io/t/1758441/open-hexagon-my-spiritual-successor-to-super-hexagon).

   - Open Hexagon is a community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.
