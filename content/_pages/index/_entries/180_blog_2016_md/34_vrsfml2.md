Welcome back, SFML enthusiasts! This article is a follow-up to my [previous deep dive into my fork of SFML](https://vittorioromeo.com/index/blog/vrsfml.html).

If you want to know how I managed to draw 500k `sf::Sprite` objects at over ~60FPS compared to upstream's ~3FPS, keep on reading!

Here, we'll explore two new significant features in my fork:

1. **A batching system that works with every SFML drawable**

2. **Simplification of core SFML APIs to aggregates, including `sf::Transformable`**

These enhancements aim to improve performance and simplify usage for developers working with SFML.



### drawable batching

Let's talk about a long-standing problem with upstream SFML: it has always issued one draw call per drawable object (sometimes even two[^fn_two]).

[^fn_two]: Shape and text drawables issue two separate draw calls if they have a visible outline.

That means that drawing 1000 `sf::Sprite` objects will bombard the GPU with 1000 separate draw calls.

This approach doesn't scale well, forcing users to resort to lower-level primitives for performance-critical features like particle systems or games with a large number of active entities.

It's no surprise that batching in SFML has been on the community wish list for ages[^fn_batching_wish]. There have been a few solid attempts to implement it, but they never made it into the main codebase. Two notable pull requests ([PR #1802](https://github.com/SFML/SFML/pull/1802) and [PR #2166](https://github.com/SFML/SFML/pull/2166)) showcase these efforts. In my opinion, the first one was going in the right direction, while the second one is a bit too complicated.

[^fn_batching_wish]: <https://www.google.com/search?q=sfml+forum+batching>

#### the texture dilemma

The main stumbling block in previous attempts was dealing with drawables sporting different textures. Those proposed batching systems tried to solve the issue by sorting the drawables by texture, which would mess up the original drawing order and complicate their internals.

I decided to work around this issue rather than trying to solve it directly.

Users can now easily create a texture atlas on the fly by adding textures to the atlas one by one, and they will be internally packed[^fn_internally]. This solution significantly simplifies the design of a batching system. Enter the [`sf::TextureAtlas`](https://github.com/vittorioromeo/VRSFML/blob/master/include/SFML/Graphics/TextureAtlas.hpp) class:

[^fn_internally]: The [`stb_rect_pack` library](https://github.com/nothings/stb/blob/master/stb_rect_pack.h) is used under the hood.

```cpp
// Create a 1024x1024 empty texture atlas
sf::TextureAtlas atlas{sf::Texture::create({1024u, 1024u}).value()};

// Load an image of a cat from file
const auto catImage = sf::Image::loadFromFile("cat.png").value();

// Add the cat image to the texture atlas
// The returned rectangle is where the image lives in the atlas
const sf::FloatRect catRect = atlas.add(catImage).value();

// Fonts can also be associated with a texture atlas
// Glyphs are dynamically loaded on demand into the atlas
const auto fontTuffy = sf::Font::openFromFile("tuffy.ttf", &atlas).value();

// Example: drawing a sprite associated with a texture atlas
sf::Sprite catSprite{catRect};
someRenderTarget.draw(catSprite, atlas.getTexture());
```

#### batches are drawable

I realized that the most natural API for a batching system should draw an arbitrary number of drawables under the same `sf::RenderStates`, which notably stores a single `sf::Texture*`. While this would be a significant limitation if `sf::TextureAtlas` didn't exist, it's now not a big deal.

That observation also implies that drawable batches are "normal" drawables themselves!

In my fork, I have implemented a new drawable: [`sf::DrawableBatch`](https://github.com/vittorioromeo/VRSFML/blob/master/include/SFML/Graphics/DrawableBatch.hpp)[^fn_drawablebatch]. It's a fully-working batching system that supports any SFML drawable (and  also allows users to batch arbitrary vertices).

[^fn_drawablebatch]: At the moment, I actually have two different batch types: `sf::CPUDrawableBatch` which stores vertex data in a CPU buffer, and `sf::PersistentGPUDrawableBatch` which stores vertex in a persistently mapped GPU buffer. The latter is not supported on OpenGL ES, but it's faster.

```cpp
sf::DrawableBatch batch;

// ...
batch.clear();
// ...
batch.add(someSprite);
batch.add(someText);
batch.add(someCircleShape);
// ...

someRenderTarget.draw(batch,
                      sf::RenderStates{.texture = &atlas.getTexture()});
```

The usage is straightforward: add your drawables to the batch, then draw it all at once on any `sf::RenderTarget` with your chosen `sf::RenderStates`. This approach draws everything in a single draw call, resulting in massive performance gains. Implementing something like a particle system with `sf::Sprite` is now completely reasonable.

Conceptually, `sf::DrawableBatch` is very simple:

- Adding a drawable results in all of its vertices being added to an internal buffer, pre-transformed on the CPU based on the drawable's `sf::Transform`.

- Under the hood, `sf::RenderTarget` uses indexed drawing [via `glDrawElements`](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/src/SFML/Graphics/RenderTarget.cpp#L1024-L1033), making it straightforward to batch different types of primitives together (e.g., triangle strips, triangle fans, etc.).

- That's pretty much it -- it just works™.

Furthermore, `sf::DrawableBatch` is a `sf::Transformable`. This means that a bunch of drawables could be "cached" into a `sf::DrawableBatch` and then rendered with different transforms multiple times. This feature is particularly useful for drawables that require significant computation to initially generate vertices.



### streaming data to the GPU

Of course, the previous explanation glossed over some crucial details. One of the most pressing questions is: how do we optimally stream a massive amount of vertices to the GPU in OpenGL?

To be honest, I don't have a definitive answer.

However, after experimenting with various techniques, I've narrowed it down to two particularly effective methods.

#### opengl es and webgl

For OpenGL ES platforms (e.g. mobile devices, or WebGL via Emscripten), I found that [a straightforward "naive" call to `glBufferData` with `GL_STREAM_DATA`](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/src/SFML/Graphics/RenderTarget.cpp#L163-L166) yielded the best results. Surprisingly, more sophisticated approaches like double VBO buffering or buffer orphaning didn't provide any noticeable performance gains in my tests.

That said, I might have overlooked something, so feel free to take a crack at optimizing that code yourself -- you'll find it in `RenderTarget.cpp`, specifically in the `streamToGPU` function.

#### desktop platforms

On desktop platforms, we can leverage a more powerful technique: persistent buffer mapping. This approach allows us to create a buffer on the GPU and map it once, keeping around a pointer that we can write to directly. In essence, we get as close as possible to producing the pre-transformed vertices right in the GPU buffer.

However, the implementation is far from trivial:

1. I had to create a [`sf::GLPersistentBuffer`](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/src/SFML/Graphics/GLPersistentBuffer.hpp) class, essentially a GPU-side `std::vector` that dynamically resizes based on usage.

2. Explicit synchronization was necessary to prevent data races between the CPU and GPU. To handle this, I implemented a [`sf::GLSyncGuard`](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/src/SFML/Graphics/GLSyncGuard.hpp) RAII guard class.

My synchronization strategy is rudimentary but seems to work:

- Always utilize the entire buffer: clear it before adding drawables to the batch, fill it with vertices, then draw it all at once

- Lock the entire persistent buffer before the [`glDrawElements` call](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/src/SFML/Graphics/RenderTarget.cpp#L1024-L1033)

- Unlock it immediately after

I'll admit, there was a lot of trial and error involved here. My familiarity with the OpenGL API was limited, so there might be more efficient ways to achieve the same result. If you've got ideas for improvement, I'm all ears!

#### performance results

Despite the potential for optimization, both techniques deliver impressive results. On my high-end system[^fn_highend], I observed the following:

[^fn_highend]: i9 13900k, RTX 4090

| Scenario                       | Batching                       | FPS |
|--------------------------------|--------------------------------|-----|
| 500k `sf::Sprite` objects      | no batching                    | ~3  |
| 250k `sf::CircleShape` objects | no batching                    | ~2  |
| 500k `sf::Sprite` objects      | CPU buffer batching            | ~52 |
| 250k `sf::CircleShape` objects | CPU buffer batching            | ~35 |
| 500k `sf::Sprite` objects      | GPU persistent buffer batching | ~68 |
| 250k `sf::CircleShape` objects | GPU persistent buffer batching | ~51 |

*Note: These aren't rigorous benchmarks, but the performance improvements are evident.*

You can [**play around with the benchmark online**](https://vittorioromeo.github.io/VRSFML_HTML5_Examples/batching.html) -- I am very curious to hear if you get similar performance improvements, but keep in mind that the WebGL version of the benchmark is inherently slower than the desktop one.

#### future optimizations?

The current bottleneck lies in performing all the [vertex pre-transformation on the CPU](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/src/SFML/Graphics/DrawableBatchUtils.hpp#L65-L68). In theory, I could offload this to the GPU, but that introduces a whole new level of complexity that might not be worth the potential gains, given the already impressive results. Here's why I'm hesitant:

1. I'd need to pass a transform matrix per drawable to the shader, which can be done in several ways (e.g. SSBOs).

2. Ensuring compatibility with OpenGL ES and WebGL would further complicate the implementation.

3. There's no guarantee it would be significantly faster. The overhead from sending large amounts of extra data to the GPU could be substantial.

For now, I've decided to stick with the current implementation. But I might give GPU-side transformation a shot in the future.



### core API simplification

I've made significant changes to core SFML APIs, particularly `sf::Transformable`, aiming to simplify usage, improve performance, and leverage modern C++ features.

#### farewell to polymorphism

In upstream SFML, every drawable derives polymorphically from `sf::Drawable` and `sf::Transformable`. I've taken a different approach:

- Completely eliminated `sf::Drawable`

- Changed `sf::Transformable` to be non-polymorphic

Even in upstream SFML, `sf::Transformable` didn't expose any `virtual` member functions except for the destructor, so there was little point in polymorphism there.

#### transforming `sf::Transformable`

`sf::Transformable` is responsible for storing the *position*, *origin*, *rotation*, and *scale* of a drawable object. In upstream SFML, it also caches and stores the *transform* and *inverse transform* matrices of the drawable, plus some "dirty" boolean flags that keep track of whether those transform matrices must be recalculated.

In order to "cache" the transforms, the API exposed by `sf::Transformable` is OOP-like (based on getters and setters):

```cpp
// same as `sprite.setPosition(sprite.getPosition() + velocity))`
sprite.move(velocity);

// same as `sprite.setRotation(sprite.getRotation() + sf::radians(torque)))`
sprite.rotate(sf::radians(torque));

if ((sprite.getPosition().x > windowSize.x && velocity.x > 0.f) ||
    (sprite.getPosition().x < 0.f && velocity.x < 0.f))
    velocity.x = -velocity.x;
```

The upstream API, while functional, has several drawbacks:

1. Simple operations like moving or rotating a sprite require one or more function calls.

2. The syntax is noisy and hard to read compared to simple vector operations.

3. Function call overhead can accumulate in unoptimized (e.g., debug) builds.

In my fork, [`sf::Transformable` is now an aggregate](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/include/SFML/Graphics/Transformable.hpp#L34). The caching is gone, the getters and setters are gone, and the new API is straightforward and intuitive:

```cpp
sprite.position += velocity;
sprite.rotation += sf::radians(torque);

if ((sprite.position.x > windowSize.x && velocity.x > 0.f) ||
    (sprite.position.x < 0.f && velocity.x < 0.f))
    velocity.x = -velocity.x;
```

#### performance implications

You might think that removing the caching of transform matrices is a performance loss, as they now need to be computed on demand. However, it's actually a performance win for several reasons:

1. **Reduced Size**: The size of `sf::Transformable` decreased considerably (from 168B to 28B), improving CPU cache utilization.

2. **No Dirty Flags**: Operations mutating *position*, *origin*, *rotation*, and *scale* no longer need to set dirty flags.

3. **Fewer Function Calls**: Without wrapper functions, debug performance improves, and the compiler has more optimization opportunities in release mode.

4. **Optimized for Dynamic Scenes**: In games, most entities change every frame, meaning the transform would have to be recomputed anyway. Now there are fewer branches when doing so.

Additionally, profiling with Intel VTune had detected `std::sin` and `std::cos` as a bottleneck in the `sf::Transformable::getTransform` function. I've replaced those with a lookup table containing 65,536 entries, whose precision is acceptable and speedup significant.

#### size matters

The size reduction of `sf::Transformable` is significant. Let's break it down:

| Type                | Upstream SFML       | My fork            |
|---------------------|---------------------|--------------------|
| `sf::Transform`     | 64B, non-aggregate  | 24B, aggregate     |
| `sf::Transformable` | 128B, non-aggregate | 28B, aggregate     |
| `sf::Sprite`        | 280B, non-aggregate | 48B, non-aggregate |

*Note: Sizes may vary slightly depending on the system and compiler.*

This size reduction has a significant impact on memory usage. In upstream SFML, storing 100k `sf::Sprite` objects would take around ~26MB. With the optimizations in my fork, this memory footprint is drastically reduced.

The size improvements are inspired by [PR #3026](https://github.com/SFML/SFML/pull/3026) by [L0laapk3](https://github.com/L0laapk3), which hardcoded the bottom row of the transformation matrix to `{0, 0, 1}`. It was ultimately rejected due to unconvincing benchmarks and because it was deemed out of scope for SFML 3.x. In my fork, I've taken this concept much further by [storing only the six floats required for every 2D transformation rather than a `float[16]` matrix](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/include/SFML/Graphics/Transform.hpp#L253-L256).

#### the power of aggregates

The use of C++20 designated initializers makes it extremely convenient to initialize aggregate types. The following are all now aggregates:

- `sf::BlendMode`
- `sf::Color`
- `sf::RenderStates`
- `sf::Transform`
- `sf::Transformable`
- `sf::Vertex`
- `sf::View`
- `sf::Rect<T>`
- `sf::Vector2<T>`
- `sf::Vector3<T>`
- `sf::VideoMode`

For other types, I've created [small aggregate "settings" structs](https://github.com/vittorioromeo/VRSFML/blob/18877b20e8e2ec992c614fadc06cc4f78f51dd50/include/SFML/Graphics/Text.hpp#L54-L67) that are now taken during construction. Here's a comparison of how `sf::Text` is initialized:

```cpp
// My fork
sf::Text hudText(font,
                 {.position         = {5.0f, 5.0f},
                  .characterSize    = 14,
                  .fillColor        = sf::Color::White,
                  .outlineColor     = sf::Color::Black,
                  .outlineThickness = 2.0f});

// Upstream
sf::Text hudText(font);
hudText.setCharacterSize(14);
hudText.setFillColor(sf::Color::White);
hudText.setOutlineColor(sf::Color::Black);
hudText.setOutlineThickness(2.0f);
hudText.setPosition({5.0f, 5.0f});
```

The aggregate-based initialization in my fork not only simplifies the code but also allows the use of `const` where appropriate.

#### visual impact

To illustrate the real-world impact of these changes, I've prepared a visual comparison of the `Tennis.cpp` example between upstream SFML and my fork:

[![*tennis example visual comparison*](resources/img/blog/tenniscomparison.png)](resources/img/blog/tenniscomparison.png)

As you can see, my version is more concise and easier to read, while still maintaining all the functionality of the original.



### try it out

You can now experience many SFML examples[^examples] directly in your browser:

- **Visit [`vittorioromeo/VRSFML_HTML5_Examples`](https://vittorioromeo.github.io/VRSFML_HTML5_Examples/) to explore these demos.**

If you want to use my fork of SFML yourself, the source code is available [**here**](https://github.com/vittorioromeo/VRSFML).

- The fork is still a work-in-progress and experimental project, but it is mature enough to build something with it. The documentation is lackluster, but feel free to reach out and I will directly help you out get started!

By the way, the name of the fork is still undecided -- VRSFML is a placeholder which will change soon.



### shameless self-promotion

- I offer training, mentoring, and consulting services. If you are interested, check out [**romeo.training**](https://romeo.training/), alternatively you can reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

   - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)

- If you enjoy fast-paced open-source arcade games with user-created content, check out [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/), my VRSFML-powered game [available on Steam](https://store.steampowered.com/app/1358090/Open_Hexagon/) and [on itch.io](https://itch.io/t/1758441/open-hexagon-my-spiritual-successor-to-super-hexagon).

   - Open Hexagon is a community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.
