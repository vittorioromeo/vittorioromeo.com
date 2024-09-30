

<style>
.inline-link
{
    font-size: small;
    margin-top: -2.6em;
    text-align: right;
    font-weight: bold;
}
</style>

It's been exactly 447 days since my last article, ["compile-time iteration with C++20 lambdas"](https://vittorioromeo.com/index/blog/cpp20_lambdas_compiletime_for.html). I should be ashamed.

In my defense, I've been very busy since then... and I'm going to tell you all about it in this blog post.

As my memory fades away more quickly due to getting older, I had to go through my various social media accounts to reconstruct a timeline of the most important events in these past months. While very convenient, I also have to admit that it is sort of scary to see how all my history is publicly available.

Anyway, let's begin!



### futures without type erasure

I gave a talk at three major conferences about [`orizzonte`](https://github.com/SuperV1234/orizzonte), an experimental library which provides an implementation of futures that doesn't use type erasure or dynamic allocation.

Basically, it encodes a graph of asynchronous computations in the type system and generates all the necessary buffers and synchronization primitives. Here's an example of its syntax:

```cpp
auto graph =
    leaf{[&] { /* computation A */ }}
        .then(all{leaf{[&] { /* computation B0 */ }},
                  leaf{[&] { /* computation B1 */ }}});
```

In the example above, `A` is executed first, then `B0` and `B1` run in parallel. I've given this talk at [ACCU 2018](https://www.youtube.com/watch?v=GehO6LPu4qA), [itCppCon18](https://www.youtube.com/watch?v=3RfR4C0Wy-w), and [C++Now 2018](https://www.youtube.com/watch?v=Avvhs3PLP7o). All of the conferences were well-organized and insightful, and I got a lot of valuable feedback.

![Screenshot from the talk](resources/img/blog/screen_futuretalk.png)

I didn't work on this experimental project any further (except for some small fixes), but I believe that the underlying ideas and the talks can be an excellent resource for developers trying to achieve something similar.



### iso c++ meeting: rapperswil

I traveled to the beautiful Rapperswil to represent my company, Bloomberg, at the June 2018 ISO C++ meeting. I spent most of my time in EWG, where I've seen the early steps of now mature proposals such as `consteval`. I also moved my paper forward: [*"function_ref: a non-owning reference to a Callable"*](https://wg21.link/p0792). I will talk about `function_ref` the following sections.

Besides the ISO C++ meeting, the highlight of this trip was petting and feeding a friendly giraffe:

<blockquote class="twitter-tweet"><p lang="en" dir="ltr">Already making new friends at <a href="https://twitter.com/hashtag/cpprap?src=hash&amp;ref_src=twsrc%5Etfw">#cpprap</a> <a href="https://t.co/YHSlyOiKbq">pic.twitter.com/YHSlyOiKbq</a></p>&mdash; Vittorio Romeo (@supahvee1234) <a href="https://twitter.com/supahvee1234/status/1003294207480614914?ref_src=twsrc%5Etfw">June 3, 2018</a></blockquote> <script async src="https://platform.twitter.com/widgets.js" charset="utf-8"></script>



### modern c++ training

When I joined Bloomberg, I was surprised to see that there was no training about C++11/14, despite the fact that a huge portion of the company was using those standards in production. I asked my management chain if I could fix the problem, and they happily agreed to allow me to work on a C++11/14 course during 20-30% of my work time.

After a few months and valuable help by members of the internal training team, I finished a 3-day course which covers all language features and is filled with interactive exercises and workshops.

The main idea of the course is to have attendees transform an existing barebones C++03 `std::vector` implementation into a C++14 one, incrementally improving both its interface and its internals as new features are learned during the lectures. The course was a **huge** success and I've only received positive feedback for all my 12~ sessions that I delivered internally in London, New York, and Tel Aviv.

Why am I telling you about this...?

Well - my course has been accepted as a [pre-conference class](https://cppcon.org/class-2019-cpp111403/) for the upcoming CppCon 2019!

If you are a C++03 developer that wants to get up to speed with the C++11/14 standards in an interactive way or know any colleague/friend in that situation, there's still some time [to register](https://cppcon.org/registration/).



### packt courses

Back in November 2017, I had published a video course through Packt: ["Mastering C++ Standard Library Features"](https://www.packtpub.com/application-development/mastering-c-standard-library-features-video). Starting from new additions to the Standard Library, the course goes quite in-depth into Modern C++ "philosophy" and "mechanics", also covering topics like *move semantics*. It was [very well received](https://www.udemy.com/mastering-c-standard-library-features/).

I published a new video course at the end of June 2018: ["Type-Safe Interfaces with Modern C++ "](https://www.packtpub.com/application-development/type-safe-interfaces-modern-c-video). This one is much shorter, but also much more interesting in my opinion: it shows the audience how to effectively use Modern C++ and additions such as `std::variant` and `std::optional` to create APIs that are very hard to misuse. It also covers a basic implementation of variant pattern matching, inspired by the [variant visitation talk](https://www.youtube.com/watch?v=3KyW5Ve3LtI) I gave back in 2017.

Unfortunately, it didn't sell as well as the previous one. I'm confident that the content is high-quality, but people might have been less interested due to the shorter length and more advanced topics. Please consider recommending these courses to friends and colleagues who might find them valuable!

For those interested, the profits from these courses have been very low and definitely not a proportional reward to the amount of work I put into the material. Regardless, I do not regret making these courses as I've had people personally thanking me for them, and because I personally love crafting high-quality C++ training content.

Stay tuned, as a new one might come out soon...



### `function_ref`

This is the name of a proposed C++20 library feature that is currently sitting in LWG, and that I hope will be merged into the working draft soon. You can read the paper [here](https://wg21.link/p0792). I've also given a talk about it and its most important use cases at four major conferences in 2019.

The talk covers the concept of an "higher-order function", shows various ways we can represent that concept in C++, and then introduces `function_ref`, which aims to be a vocabulary type to pass higher-order functions around efficiently *and* conveniently. The presentation also shows some real examples from my daily job where `function_ref` has successfully been used in production for a large-scale trading data backend.

You can find YouTube videos of the talk here: [C++ On Sea 2019](https://www.youtube.com/watch?v=TxhOTj_ynUM), [ACCU 2019](https://www.youtube.com/watch?v=WHRao43ab3I), and [C++Now 2019](https://www.youtube.com/watch?v=5V74RPUEu5s). The talk was also given at C++ Russia 2019, but there's no video available yet.

* [C++ On Sea](https://cpponsea.uk/) was a brand new conference organized by Phil Nash in Folkestone, which is not far from London, on the eastern shore of the UK. It was a lovely conference which ran surprisingly smoothly and that attracted many experts from all parts of the globe.

* [C++ Russia](https://cppconf.ru/en/) was also amazing, and it was the perfect occasion for me to finally visit Russia. Honestly, it was the most well-organized conference I've been to so far! The staff thought about everything and planned a nice day field trip with all the speakers to show how beautiful Moscow can be.

![Screenshot from the talk](resources/img/blog/screen_functionreftalk.png)



### other tidbits, and what's next

Let's see... I've also finally reached [60k reputation on StackOverflow](https://stackoverflow.com/users/598696/vittorio-romeo), almost only thanks to C++ questions and answers.

Also, I am co-authoring a book with John Lakos! More news on that soon.

My next C++-related stop will be the upcoming July 2019 Cologne ISO C++ Meeting - hopefully, `function_ref` will make some major progress there, and an agreement will be reached on "contracts". I hope to see you there or at CppCon 2019.

As I am still very busy with training, courses, and book-writing, I cannot make any promise on any new article or blog post. But I'll try my best if I come across an interesting idea or snippet that I just have to write about.

Ciao!
