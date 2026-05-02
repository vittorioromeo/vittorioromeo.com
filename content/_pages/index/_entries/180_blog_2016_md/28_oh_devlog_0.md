Hello everyone!

This is going to be a DevLog about the implementation of one particular feature for my game [Open Hexagon](https://store.steampowered.com/app/1358090/Open_Hexagon/): **online leaderboards**.



### open hexagon

First of all, what is Open Hexagon?

It's a fast-paced arcade and rhythm game heavily inspired by Super Hexagon, created with its author's permission (Terry Cavanagh). You control a small triangle spinning around the center of the screen, and your goal is to avoid various obstacles that hone in towards the center.

<center>

<iframe width="560" height="315" src="https://www.youtube.com/embed/TlSJZlQrVJ4?controls=0" frameborder="0" allow="accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>

</center>

Open Hexagon started as a clone of Super Hexagon, but it evolved into much more: it features new mechanics such as a *180° swap* and *curving walls*, and -- most importantly -- it was completely designed with *modding* and *customization* in mind.

Users can create *custom levels* using Lua scripts and publish them to the [Steam Workshop](https://steamcommunity.com/app/1358090/workshop/). Fans have created mind-blowing levels that push the engine to its boundaries.

Here's a short compilation of some very creative user-made levels:

<center>

<div class="video-container">

<iframe width="560" height="315" src="https://www.youtube.com/embed/h4Jfj3lzWD4?controls=0" frameborder="0" allow="accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>

</div>

</center>

I've also had players thank me for getting them into the world of programming, which feels amazing.

Despite being published on Steam, the game is also fully open-source [(on GitHub)](https://github.com/vittorioromeo/SSVOpenHexagon/), and anyone can contribute to it or build it themselves for free. My intention is to have the source always freely available so that people can learn from it and/or improve the game.



### goals

Now, onto the online leaderboards. The main goals for this feature are:

- After playing a level, the user's final score is automatically submitted to a server.

- While selecting levels, the user can see his ranking in a global leaderboard.

Stretch goals:

- Download replays of top scores directly in-game from the server, and view them in-game.



### issues

Sounds simple enough... but it isn't. Fortunately, I've worked hard to make the game logic fully deterministic and have implemented an input-based replay system already, but there absolutely are zero online capabilities in the game.

Also, I want to make cheating as hard as possible. It sucks when players work hard for high scores, just to have the leaderboard ruined by a hacker.

Therefore, I came up with the following list of things to consider:

- Need a database, somewhere, maybe my VPS or have to rent cloud servers.
    - Not a big deal, but costs and logistics are something to consider.

- Need to completely separate game logic and rendering logic.
    - Definitely a big deal. I started working on this game almost a decade ago, and the codebase is not in a great state, despite my continuous efforts to improve it and clean it up.

- Need headless version of Open Hexagon which only runs game logic without rendering.
    - Again, big deal. The code for the game logic is intertwined with some rendering code.

- Need to somehow encrypt/decrypt replay files to avoid tampering.
    - I have no prior experience in this, but [`libsodium`'s Key Exchange](https://libsodium.gitbook.io/doc/key_exchange) and [Steamworks Authentication](https://partner.steamgames.com/doc/features/auth) should help.

- Need Open Hexagon client to produce and send replay clients to the server.
    - What happens if the replay fails to be sent? When to retry?
    - What happens if a client spams the server with replays? How to prevent DDOS attacks?

- Need Open Hexagon server to listen for replay files, and run them to verify validity.
    - What happens on version mismatch between client and server?
    - What about malicious replays? Someone could create a basic custom level without obstacles, run for 10 hours, then send a 10 hour replay that will stop the server from processing legit replays.
    - Need to visually notify the client that (1) replay was received and (2) is being processed.

- Need to improve menu rendering to show leaderboards.
    - Must avoid frequent calls to the server caused by browsing through menu entries.

- What happens if a custom level author updates a level?
    - If it's just a "refactoring" change that doesn't affect the behavior of the level, the leaderboard should remain valid
    - Otherwise, existing replays will fail to work. How to detect it? Periodically re-run replays? Or do not detect it at all?

- How to attach player identity to a leaderboard?
    - Custom login system backed up by custom database? Would be a lot of work.
    - Use Steamworks API? Probably easier, but completely ties the game to the Steam platform.

Happy to hear your thoughts on any of the points above.



### first milestone

My first milestone will be to implement a flag in the game in order to run it in headless mode, accepting a replay file as input, running it, and printing out information about the replayed playthrough.

Ideally, I should be able to invoke Open Hexagon from the command line like this:

```
./SSVOpenHexagon.exe -headless <path_to_replay>
```

And get some sort of output like this:

```
Replay <path_to_replay> completed.

Level: <level>
Player: <name>
Final time: <time>
```

I will keep this DevLog updated as a way of motivating myself, documenting my progress, and getting feedback. Hope you enjoy!
