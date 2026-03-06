It felt wrong to not participate in the Wordle craze, and what better way of doing so than by creating a purely compile-time version of the game in C++20? I proudly present to you... **Wordlexpr**!

<center>

<div class="video-container">

<iframe width="560" height="315" style="width:100%" src="https://www.youtube.com/embed/wp3LPrhu2Sk" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>

</div>

</center>

[*(You can play **Wordlexpr** on Compiler Explorer.)*](https://gcc.godbolt.org/z/4oo3PrvqY)

Carry on reading to understand the magic behind it!



### high-level overview

Wordlexpr is played entirely at compile-time as no executable is ever generated -- the game is experienced through compiler errors. Therefore, we need to solve a few problems to make everything happen:

1. Produce arbitrary human-readable output as a compiler diagnostic.

2. Random number generation at compile-time.

3. Retain state and keep track of the player's progress in-between compilations.



### error is the new `printf`

In order to abuse the compiler into outputting errors with an arbitrary string of our own liking, let's start by trying to figure out how to make it print out a simple string literal. The first attempt, `static_assert`, seems promising:

```cpp
static_assert(false, "Welcome to Wordlexpr!");
```

> ```
> error: static assertion failed: Welcome to Wordlexpr!
>     1 | static_assert(false, "Welcome to Wordlexpr!");
>       |               ^^^^^
> ```

However, our delight is short-lived, as `static_assert` only accepts a string literal -- a `constexpr` array of characters or `const char*` will not work as an argument:

```cpp
constexpr const char* msg = "Welcome to Wordlexpr!";
static_assert(false, msg);
```

> ```
> error: expected string-literal before 'msg'
>     2 | static_assert(false, msg);
>       |                      ^^^
> ```

So, how about storing the contents of our string as part of the type of a `struct`, then produce an error containing such type?

```cpp
template <char...> struct print;
print<'a', 'b', 'c', 'd'> _{};
```

> ```
> error: variable 'print<'a', 'b', 'c', 'd'> _'
>        has initializer but incomplete type
>     3 | print<'a', 'b', 'c', 'd'> _{};
>       |
> ```

Nice! We are able to see our characters in the compiler output, and we could theoretically mutate or generate the sequence of characters to our liking at compile-time. However, working with a `char...` template parameter pack is very cumbersome, and the final output is not very readable.

C++20's [P0732R2: **"Class Types in Non-Type Template Parameters"**](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0732r2.pdf) comes to the rescue here! In short, we can use any *literal type* as a non-type template parameter. We can therefore create our own little compile-time string literal type:

```cpp
struct ct_str
{
    char        _data[512]{};
    std::size_t _size{0};

    template <std::size_t N>
    constexpr ct_str(const char (&str)[N]) : _data{}, _size{N - 1}
    {
        for(std::size_t i = 0; i < _size; ++i)
            _data[i] = str[i];
    }
};
```

We can then accept `ct_str` as a template parameter for `print`, and use the same idea as before:

```cpp
template <ct_str> struct print;
print<"Welcome to Wordlexpr!"> _{};
```

> ```
> error: variable 'print<ct_str{"Welcome to Wordlexpr!", 21}> _' has
>        initializer but incomplete type
>    22 | print<"Welcome to Wordlexpr!"> _{};
>       |
> ```

Now we have a way of making the compiler emit whatever we'd like as an error. In fact, we can perform string manipulation at compile-time on `ct_str`:

```cpp
constexpr ct_str test()
{
    ct_str s{"Welcome to Wordlexpr!"};
    s._data[0] = 'w';
    s._data[11] = 'w';
    s._data[20] = '.';
    return s;
}

print<test()> _{};
```

> ```
> error: variable 'print<ct_str{"welcome to wordlexpr.", 20}> _' has
>        initializer but incomplete type
>    33 | print<test()> _{};
>       |               ^
> ```

By extending `ct_str` with functionalities such as `append`, `contains`, `replace`, etc... we will end up being able to create any sort of string at compile-time and print it out as an error.

First problem solved!



### compile-time random number generation

This is really not a big deal, if we allow our users to provide a seed on the command line via preprocessor defines. Pseudo-random number generation is always deterministic, and the final result only depends on the state of the RNG and the initially provided seed.

```cpp
g++ -std=c++20 ./wordlexpr.cpp -DSEED=123
```

It is fairly easy to port a common RNG engine such as Mersenne Twister to C++20 `constexpr`. For the purpose of Wordlexpr, the modulo operator (`%`) was enough:

```cpp
constexpr const ct_str& get_target_word()
{
    return wordlist[SEED % wordlist_size];
}
```

Second problem solved!



### retaining state and making progress

If we allow the user to give us a seed via preprocessor defines, why not also allow the user to make progress in the same game session by telling us where they left off last time they played? Think of it as any save file system in a modern game -- except that the "save file" is a short string which is going to be passed to the compiler:

```cpp
g++ -std=c++20 ./wordlexpr.cpp -DSEED=123 -DSTATE=DJYHULDOPALISHJRBFJNSWAEIM
```

The user doesn't have to come up with the state string themselves -- it will be generated by Wordlexpr on every step:

> ```
> error: variable 'print<ct_str{"You guessed `crane`. Outcome: `x-xx-`.
>        You guessed `white`. Outcome: `xxox-`.
>        You guessed `black`. Outcome: `xoxxx`.
>        You guessed `tower`. Outcome: `xxxoo`.
>        To continue the game, pass `-DSTATE=EJYHULDOPALISHJRAVDLYWAEIM`
>        alongside a new guess.", 242}> _' has initializer but incomplete
>        type
>  2612 |         print<make_full_str(SEED, guess, s)> _{};
>       |                                              ^
> ```

The state of the game is stored in this simple `struct`:

```cpp
struct state
{
    std::size_t _n_guesses{0};
    ct_str      _guesses[5];
};
```

All that's left to do is to define encoding and decoding functions for the state:

```cpp
constexpr ct_str encode_state(const state& s);
constexpr state decode_state(const ct_str& str);
```

In Wordlexpr, I used a simple Caesar cipher to encode the guesses into the string without making them human-readable. It is not really necessary, but generally speaking another type of compile-time game might want to hide the current state by performing some sort of encoding.

Third problem solved!



### conclusion

I hope you enjoyed this brief explanation of how Wordlexpr works. Remember that you can play it yourself and see the entire source code [on Compiler Explorer](https://gcc.godbolt.org/z/4oo3PrvqY). Feel free to reach out to ask any question!

Now, for some shameless self-promotion:

- My book [**"Embracing Modern C++ Safely"** is now available on all major resellers](http://emcpps.com/). Please consider purchasing it and share the news with your friends and colleagues -- that helps a lot!

    - For more information, read the following interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)

- If you are a fan of fast-paced open-source arcade games that allow user-created content, check out [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/), my first fully-released game [available on Steam](https://store.steampowered.com/app/1358090/Open_Hexagon/) and [on itch.io](https://itch.io/t/1758441/open-hexagon-my-spiritual-successor-to-super-hexagon).

    - Open Hexagon is a spiritual successor to the critically acclaimed Super Hexagon by Terry Cavanagh. [Terry fully supports my project!](https://twitter.com/terrycavanagh/status/1397372432877379587) Thanks!

- I offer 1-1 C++ mentoring and consulting sessions in my spare time. If it's something you are interested in, feel free to reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).
