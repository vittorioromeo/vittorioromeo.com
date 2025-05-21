In [one of my previous articles](https://vittorioromeo.com/index/blog/vrsfml2.html), I discussed the design and implementation of the batching system in my fork of SFML.

That system works well, but it requires users to manually create a batch, add drawables to it, and then draw the batch onto a render target. While effective, this manual approach begs the question:

Wouldn't it be nice if drawables were automatically batched, whenever possible?

The answer is, of course, yes!

This short article will explore the design and implementation of the "autobatching" system aimed at achieving just that.




### batching recap

Let's quickly recap how the *manual* drawable batching works. Firstly, the user creates one of two possible batch types:

- `sf::CPUDrawableBatch`, which stores the vertex data in a standard CPU buffer;

- or `sf::PersistentGPUDrawableBatch`, which stores the vertex data in a persistent GPU buffer (mapping GPU memory to the CPU address space).

The latter generally offers better performance than the former, but it has prerequisites: it's not available on OpenGL ES and comes with a few other quirks.

Once the drawable batch is created, users can "add" drawable objects to it:

```cpp
batch.add(sf::CircleShape
          {
              .position = circlePosition,
              .origin   = {radius, radius},
              // ...other fields...
          });

batch.add(sf::Sprite
          {
              .position = spritePosition,
              .origin   = {0.f, 0.f},
              // ...other fields...
          });
```

When ready, the drawable batch can then be drawn onto any render target. Importantly, the batch itself is a transformable object, so it can be positioned, rotated, scaled, and so on:

```cpp
batch.position = {100.f, 100.f};
batch.scale = {2.f, 2.f};

window.draw(batch, sf::RenderStates{
                       .texture = textureAtlas.getTexture(),
                       .shader = &customShader
                   });
```

 key constraint is that all the batched drawable objects must use the same texture and shader. The entire batch is then drawn efficiently with a single draw call.

Internally, the drawable batch is essentially just a large array of vertices (`sf::Vertex`). It provides convenient overloads of the `add` function that take various drawables[^overloads], generate their pre-transformed vertices, and append them to this array. It's a straightforward design, but it performs extremely well -- on my hardware, I can draw 500k moving and rotating sprites at around ~60FPS using this method.

[^overloads]: There's nothing preventing an extra layer of abstraction where drawables specify how they want to be batched -- I just haven't yet found a compelling reason to add that complexity.



### autobatching

With "manual batching," the user explicitly creates a batch, calls `.add(/* ... */)` multiple times, and finally draws the batch onto a render target. Autobatching, in contrast, aims to achieve similar benefits *automatically*. It tries to greedily combine multiple consecutive `.draw(/* ... */)` invocations on a single render target, as long as the `sf::RenderStates` (texture, shader, blend mode, transform) remain consistent between those calls.

Again, it's a delightfully simple design, but it provides users with potentially massive speedups without requiring any extra effort or changes to their drawing code.

To make this work, we need to store a bit more state inside each `sf::RenderTarget`:

```cpp
class RenderTarget
{
    // ...

    CPUDrawableBatch cpuDrawableBatch; // Internal drawable batch
    RenderStates     lastRenderStates; // Last used render states
    bool             autoBatch = true; // Is autobatching enabled?

    // ...
};
```

As you might expect, every `sf::RenderTarget` now potentially holds its own internal, private drawable batch. When drawing anything on the render target, the logic checks if autobatching is enabled:

```cpp
void RenderTarget::draw(const Sprite& sprite, const RenderStates& states)
{
    if (autoBatch)
    {
        // If autobatching, try to add to the internal batch
        flushIfNeeded(states);
        cpuDrawableBatch.add(sprite);
    }
    else
    {
        // Otherwise, draw immediately (legacy behavior)
        // ...
    }
}
```

The `flushIfNeeded` private member function is the core of the autobatching logic. It decides whether the current internal batch needs to be "flushed" (i.e., actually drawn to the GPU) before adding the new drawable. Flushing is necessary if we exceed a predefined vertex capacity for the batch, or if the `sf::RenderStates` for the *new* drawable differ from those used for the current batch:

```cpp
void RenderTarget::flushIfNeeded(const RenderStates& states)
{
    // Flush if the batch is full OR if the render states are changing
    if (cpuDrawableBatch.getNumVertices() >= vertexThreshold ||
        lastRenderStates != states)
    {
        flush(); // Send the current batch to the GPU
    }

    // Update the last states to match the incoming drawable's states
    lastRenderStates = states;
}
```

Finally, the `flush` private member function simply draws the accumulated internal batch using the stored `lastRenderStates` and then clears the batch, ready for the next sequence of draw calls:

```cpp
void RenderTarget::flush()
{
    draw(cpuDrawableBatch, lastRenderStates);
    cpuDrawableBatch.clear();
}
```

The last piece of the puzzle is having the render target call `flush` automatically before the end of the frame. Since SFML's API requires `.display` to be invoked on any window or `sf::RenderTexture` to actually display the contents of the buffer on the screen, that felt like the natural place to flush the internal drawable batch.

And that's the essence of it! The `RenderTarget` accumulates drawables into its internal batch until a state change or the vertex limit forces it to draw the accumulated geometry.



### is it fast?

Autobatching delivers performance gains specifically when multiple drawables using the *exact same* `sf::RenderStates` are drawn consecutively. This means if your drawing loop frequently changes textures, blend modes, or shaders between individual sprites, you'll see little to no benefit.

However, if you often use the same shader (usually the default one) and leverage utilities like `sf::TextureAtlas` to combine multiple images into a single texture, a significant portion of your draw calls can likely be batched automatically.

Perhaps surprisingly, autobatching can sometimes even be *faster* than manual batching! This might seem counter-intuitive, but the performance difference likely stems from the `vertexThreshold` used inside `flushIfNeeded`. Sending several moderately sized batches to the GPU can sometimes be more efficient for the driver and hardware pipeline than submitting one single, enormous batch containing hundreds of thousands of vertices.

I wasn't initially expecting this outcome, but it's certainly encouraging that the simplest API (just calling `window.draw` repeatedly) can potentially surpass the performance of the more explicit, manual batching approach in certain scenarios.



### try it out

You can witness the impact of autobatching [**directly in your browser** with this interactive demo](https://vittorioromeo.github.io/VRSFML_HTML5_Examples/batching.html). Play around with the options -- you'll likely notice a dramatic performance drop when disabling autobatching.

If you're interested in using my fork of SFML yourself, the source code is available [**here on GitHub**](https://github.com/vittorioromeo/VRSFML/tree/bubble_idle).

- Keep in mind that the fork is still experimental and a work-in-progress, but it's definitely mature enough to build projects with. Documentation is currently sparse, but please feel free to reach out, and I'll gladly help you get started!

As a side note, the name of the fork (VRSFML) is still a placeholder and will change in the future.

My newly-released game on Steam, [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/), leverages this autobatching system internally. If you enjoy incremental or idle games, please consider checking it out -- it might be your cup of tea!



### shameless self-promotion

- I offer training, mentoring, and consulting services. If you are interested, check out [**romeo.training**](https://romeo.training/), alternatively you can reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).

- Check out my newly-released game on Steam: [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/) -- it's only $3.99 and one of those games that you can either play actively as a timewaster or more passively to keep you company in the background while you do something else.

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

   - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)

- If you enjoy fast-paced open-source arcade games with user-created content, check out [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/), my VRSFML-powered game [available on Steam](https://store.steampowered.com/app/1358090/Open_Hexagon/) and [on itch.io](https://itch.io/t/1758441/open-hexagon-my-spiritual-successor-to-super-hexagon).

   - Open Hexagon is a community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.
