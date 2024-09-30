A few months ago, I finished implementing the replay and secure leaderboard system for my game [Open Hexagon](https://store.steampowered.com/app/1358090/Open_Hexagon/), and it has worked very well so far. I've used a few interesting techniques and tricks to (1) *achieve deterministic gameplay*, (2) *prevent cheating by slowing down time*, and (3) *prevent cheating by modifying game files* which I am going to talk about in this post.

If you want to know more information about the replay system and the server/client implementation, check out the two previous devlogs [here](https://vittorioromeo.com/index/blog/oh_leaderboard_devlog_0.html) and [here](https://vittorioromeo.com/index/blog/oh_leaderboard_devlog_1.html).

At the end of the article, I'll also share some major milestones I've reached this year.



### achieving deterministic gameplay

What is *deterministic* gameplay? Given a particular initial state $S_0$ and a sequence of $N$ inputs and times $((I_n, T_n),  \ldots)$ then applying those onto $S_0$ should always result in the same final state $S_1$ independently of the machine running the game or of the current FPS of the simulation:

$$S_0 + ((I_n, T_n),  \ldots) \rightarrow S_1$$

Deterministic gameplay is a prerequisite for a solid input-based *replay system*. Rather than saving a video of your high-score attempt, Open Hexagon will save the sequence of inputs you have entered alongside some metadata (e.g. random generator seed, level name, player name, etc.). Not only this approach allows replay files to be very small in size, but it also allows the replays to be played back directly in the game engine and validated.

Therefore, rather than sending a player's final score to the leaderboard server (which can easily be spoofed), Open Hexagon will send the replay file. The server will then play back the received replay, and if everything seems valid it will accept the score and display it on the leaderboard. While it is technically possible to hand-craft a replay to cheat, it is much more difficult and time-consuming. There are also a few extra security measures to deter cheating that I will talk about later in this post.

Implementing determinism in Open Hexagon wasn't too hard, but it was a bit tricky because the [game's codebase](https://github.com/SuperV1234/SSVOpenHexagon/) is quite old and wasn't designed with neither determinism nor replays in mind. There were a few challenges here:

1. Have a deterministic random number generator that, given a certain seed, produces the same sequence of numbers on both Windows (client) and Linux (server).

2. Ensure that the gameplay loop and logic doesn't depend on the machine's performance.

Achieving number (1) was quite straightforward. Sadly, the [C++11 `<random>` library is *not* portable](https://stackoverflow.com/questions/14840901/is-the-random-library-in-c11-portable), but the excellent [PCG Random library](https://www.pcg-random.org/index.html) is, so I used the latter.

Number (2) seemed quite easy on paper -- just change the game logic from dynamically scaling with FPS to being fixed, and simply run the game logic more or less often depending on the current FPS. I even created [a video tutorial on the subject](https://www.youtube.com/watch?v=lW6ZtvQVzyg), check it out!

After making the changes, everything seemed fine. Except for one *major* issue: sometimes replay files were corrupted and de-synchronized from the original attempt that generated them. This issue was incredibly hard to pinpoint as it happened *rarely* and *unpredictably*. Sometimes after 100 attempts, one of the replays would immediately de-sync. Other times, a replay for an high-score of 300 seconds would de-sync only around the 250 seconds mark. Completely inexplicable.

Turns out, the culprit was the `-ffast-math` optimization flag, which had been enabled since the game's creation:

<center>

<blockquote class="twitter-tweet"><p lang="en" dir="ltr">Wanna see something interesting?<br><br>These are two overlapped Open Hexagon input replay recordings, each with 50% opacity. <br><br>One is on a build compiled with `-O3`, the other with `-Ofast`. There is a desync around 263s of gameplay.<br><br>The culprit is `-ffast-math`. Huh.<a href="https://twitter.com/hashtag/cpp?src=hash&amp;ref_src=twsrc%5Etfw">#cpp</a> <a href="https://t.co/QifsmzYsZ1">pic.twitter.com/QifsmzYsZ1</a></p>&mdash; Vittorio Romeo (@supahvee1234) <a href="https://twitter.com/supahvee1234/status/1382907921848221698?ref_src=twsrc%5Etfw">April 16, 2021</a></blockquote> <script async src="https://platform.twitter.com/widgets.js" charset="utf-8"></script>

</center>

I have not investigated this in depth, but it turns out [other projects had the same issue](https://t.co/LwxYc449lp?amp=1). In the end, I removed the optimization and enabled the `-frounding-math`, `-fsignaling-nans`, `-ffloat-store`, `-ffp-contract=off` flags to further increase determinism of floating points. Since then, I have not seen any de-synchronization or replay corruption.




### prevent cheating by slowing down time

It is trivial to slow down a game's simulation speed via tools like [Cheat Engine](https://www.cheatengine.org/). Of course, a lower simulation speed gives players an unfair advantage over others. Even worse, the resultant replay file of such cheated attempt would still be "valid"!

In order to prevent this form of cheating, we need to somehow measure the real-world duration of a high-score attempt. My initial idea was to use the system clock on the client: collect a timestamp $t_0$ when the attempt begins, a timestamp $t_1$ when the attempt ends, and compare the real-world $\Delta_t$ with the duration the replay should have had. This approach correctly rejects replays produced by changing the simulation speed, however it is quite easy to spoof the clock on the client machine. Therefore... why don't we do it on the server?

And that's pretty much what I did. When a high-score attempt begins, the (pre-authenticated) client will send a packet to the server saying *"hey, I am starting an attempt for this level"*. The server will then collect the $t_0$ timestamp. When the client finishes their attempt, they will send a packet to the server containing the replay. The server will then both collect the $t_1$ timestamp and validate the replay. The score will be accepted if and only if the replay is valid and the $\Delta_t$ elapsed between the two packets is reasonably close to the replay's duration (with some leeway for network and server processing latency).

Again, this system is not completely cheat-proof, but it makes a cheater's life much much harder. And if they decide to cheat, their cheated attempt will be recorded alongside their Steam ID and authenticated player account. So, if they're caught, they'll be permanently banned.

But there was still another cheating avenue to close...



### prevent cheating by modifying game files

Open Hexagon is completely customizable and moddable. One of the main selling points of the game is the ability to create your own levels via Lua scripting (check out [some cool community-made levels](https://www.youtube.com/watch?v=h4Jfj3lzWD4)). That implies that anyone could open up the game's Lua files and tweak the levels to give themselves an advantage.

Of course, changing a level script in a way that alters the gameplay (e.g. changing the obstacle patterns, or slowing down the wall speed) will result in the produced replay being rejected by the server, as the server will have to play back that replay against the official Lua script of the level.

However, level scripts also control the *aesthetics* of a level. Things like the background rotation speed, the background color, the color of the walls, and certain visual effects can have a huge impact on the difficulty of a level. Many levels are actually designed around gimmicks that rely on visual properties rather than gameplay ones. Changing a purely visual value in the Lua script does not affect the validity of the final replay, but it can give a serious unfair advantage to a cheater. How do we solve this issue?

I first thought about sending some sort of checksum/hash of the Lua level file to the server, but that can be spoofed, and it's also annoying that I cannot refactor my Lua script without breaking compatibility with a server. So, that was a dead end.

The second idea was embedding all the visual property information in the replay, per-frame. This would have solved the issue, but also would have made replay file sizes skyrocket. I could have built a checksum of the visual properties and periodically stored that in the replay, but I was too lazy to implement that, so I took the easy way out... I decided to imbue the value of those properties in the game's random generator state, once per frame:

```cpp
void HexagonGame::update(FT ft)
{
    // ...

    rng.advance(status.pulse);
    rng.advance(status.pulse3D);
    rng.advance(status.fastSpin);
    rng.advance(status.flashEffect);
    rng.advance(levelStatus.rotationSpeed);
    // ...and more visual effects...
}
```

By having the visual properties affect the random generator state, the level's gameplay is now dependent on the level's visual properties. Changing those properties in the Lua file with the intention of getting an unfair advantage will change the RNG's outcome, which will lead to the server rejecting the produced replay.

This works well -- it is a little bit less "clean" than the checksum solution I described above, but definitely more pragmatic. And that's it!



### milestones

I have recently reached two major milestones I'm really proud of! Firstly, Open Hexagon is finally out of Early Access on Steam:

<center>

<blockquote class="twitter-tweet"><p lang="en" dir="ltr">Excited to announce that Open Hexagon is officially out of Early Access! <br><br>Can&#39;t believe I released a complete game -- please RT! :)<a href="https://t.co/QYKAMkgOls">https://t.co/QYKAMkgOls</a><a href="https://twitter.com/hashtag/cpp?src=hash&amp;ref_src=twsrc%5Etfw">#cpp</a> <a href="https://twitter.com/hashtag/sfml?src=hash&amp;ref_src=twsrc%5Etfw">#sfml</a> <a href="https://twitter.com/sfmldev?ref_src=twsrc%5Etfw">@sfmldev</a> <a href="https://twitter.com/hashtag/devlog?src=hash&amp;ref_src=twsrc%5Etfw">#devlog</a> <a href="https://twitter.com/hashtag/gamedev?src=hash&amp;ref_src=twsrc%5Etfw">#gamedev</a> <a href="https://twitter.com/hashtag/indiegame?src=hash&amp;ref_src=twsrc%5Etfw">#indiegame</a> <a href="https://twitter.com/hashtag/indiedev?src=hash&amp;ref_src=twsrc%5Etfw">#indiedev</a> <a href="https://twitter.com/hashtag/gamedevelopment?src=hash&amp;ref_src=twsrc%5Etfw">#gamedevelopment</a> <a href="https://twitter.com/hashtag/games?src=hash&amp;ref_src=twsrc%5Etfw">#games</a> <a href="https://twitter.com/hashtag/game?src=hash&amp;ref_src=twsrc%5Etfw">#game</a> <a href="https://twitter.com/hashtag/apps?src=hash&amp;ref_src=twsrc%5Etfw">#apps</a> <a href="https://twitter.com/hashtag/dev?src=hash&amp;ref_src=twsrc%5Etfw">#dev</a> <a href="https://twitter.com/hashtag/indiegames?src=hash&amp;ref_src=twsrc%5Etfw">#indiegames</a> <a href="https://twitter.com/hashtag/gaming?src=hash&amp;ref_src=twsrc%5Etfw">#gaming</a></p>&mdash; Vittorio Romeo (@supahvee1234) <a href="https://twitter.com/supahvee1234/status/1455541497307312143?ref_src=twsrc%5Etfw">November 2, 2021</a></blockquote> <script async src="https://platform.twitter.com/widgets.js" charset="utf-8"></script>

</center>

And secondly, my book *"Embracing Modern C++ Safely"* is now available for pre-order:

<center>

<blockquote class="twitter-tweet"><p lang="en" dir="ltr">It&#39;s finally done -- &quot;Embracing Modern C++ Safely&quot; now has an official release date! 🎉<br><br>Get the digital version of the book on 23 December 2021. Available NOW for PRE-ORDER (see below).<br><br>Please re-tweet! :)<a href="https://twitter.com/hashtag/cpp?src=hash&amp;ref_src=twsrc%5Etfw">#cpp</a> <a href="https://twitter.com/hashtag/programming?src=hash&amp;ref_src=twsrc%5Etfw">#programming</a> <a href="https://twitter.com/hashtag/coding?src=hash&amp;ref_src=twsrc%5Etfw">#coding</a> <a href="https://twitter.com/hashtag/cplusplus?src=hash&amp;ref_src=twsrc%5Etfw">#cplusplus</a> <a href="https://twitter.com/pearson?ref_src=twsrc%5Etfw">@Pearson</a> <a href="https://t.co/UL9cwjswxi">pic.twitter.com/UL9cwjswxi</a></p>&mdash; Vittorio Romeo (@supahvee1234) <a href="https://twitter.com/supahvee1234/status/1453057591509229574?ref_src=twsrc%5Etfw">October 26, 2021</a></blockquote> <script async src="https://platform.twitter.com/widgets.js" charset="utf-8"></script>

</center>

I have worked really hard on both of these projects and I am really pleased with the outcome. It would mean the world to me if you could check them out and share the news with people you know who might be interested.

Thank you!
