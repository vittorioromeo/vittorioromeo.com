Keeping my promise and motivation up... here's the second installment of the Open Hexagon leaderboards devlog. I have made quite a lot of progress in the past few days, and have implemented the following features:

- Headless mode with replay validation

- Bought a Arch Linux VPS on Linode for the server

- Server and client communication

- `std::variant`-based packet system, with encryption and decryption

- Authentication via Steamworks

- `libsodium` key exchange

Still far from a functioning leaderboard system, but let me guide you through what's new so far!



### headless mode

In the last devlog, I expected this step to be very hard and time consuming:

> Need to completely separate game logic and rendering logic.
>
> - Definitely a big deal. I started working on this game almost a decade ago, and the codebase is not in a great state, despite my continuous efforts to improve it and clean it up.

Thankfully, it wasn't that hard. It took some refactoring to ensure proper separation of `update` and `draw` logic, and then preventing any window from being created was as simple as wrapping the game window into a `std::optional`:

```cpp
// from `main.cpp`

std::optional<ssvs::GameWindow> window;

if(!headless)
{
    window.emplace();
    // ...
}
```

The same approach was used for other components such as the Discord integration manager. The asset loader also needed a new `headless` flag in the constructor that gets propagated down to avoid loading graphics, sounds, and music in memory.

The component that controls updating and drawing is called `HexagonGame`, and its constructor had to be tweaked to accept raw pointers to some components (so that I can check if they're available or not):

```cpp
auto hg = std::make_unique<hg::HexagonGame>(steamManager,
    (discordManager.has_value() ? &*discordManager : nullptr), *assets,
    (window.has_value() ? &*window : nullptr), &*hc);
```

Really wish `std::optional` had a `.as_raw_pointer()` member function, it would be useful in cases like this one where the `std::optional` is being used to control whether an object is constructed or not, and then referring to that potential object from some other place. WG21 paper time...?

Adapting the implementation of `HexagonGame` was also not too difficult, but the path I chose was not the most elegant one. Rather than splitting this old and bulky monolith into smaller components that separately deal with updating or drawing, I decided to take the easy way out:

```cpp
void HexagonGame::draw()
{
    if(window == nullptr)
    {
        return;
    }

    // ...
}
```

A few more well placed `window == nullptr` checks here and there to suppress playing audio and updating insignificant game objects such as particles and camera effects, and headless mode was implemented!

```bash
./SSVOpenHexagon.exe -headless ./Replays/testReplay.ohreplay
```

> ```text
> Player died.
> Final time: 6.249s
> ```

You can see all the code changes in the [v2.0.5 pull request](https://github.com/vittorioromeo/SSVOpenHexagon/pull/330).



### arch linux vps

Not much to say here, but I ended up purchasing a Arch Linux VPS on Linode as I have been happy with their services in the past. It was pretty easy to set it up and get Open Hexagon up and running in headless mode on there.

Some tweaks were required to link against `libXcursor` and fix some build issues, but it wasn't too bad.

I used `ufw` to open up port `50505` for Open Hexagon, and that worked flawlessly.



### server

I ended up implementing two new components: `HexagonServer` and `HexagonClient`.

`HexagonServer` is created only in headless mode, when the `-server` argument is passed. It holds a reference to `HexagonGame` so that the server can start new sessions and execute replays when relevant packets are received.

Clients connected to the server are managed through this nested class and internal data structure:

```cpp
struct ConnectedClient
{
    sf::TcpSocket _socket;
    TimePoint _lastActivity;
    int _consecutiveFailures;
    bool _mustDisconnect;
    std::optional<SodiumPublicKeyArray> _clientPublicKey;
    std::optional<SodiumRTKeys> _rtKeys;
    bool _ready;

    explicit ConnectedClient(const TimePoint lastActivity);
    ~ConnectedClient();
};

std::list<ConnectedClient> _connectedClients;
using ConnectedClientIterator = std::list<ConnectedClient>::iterator;
```

As you can see, there is one TCP socket per connected client, alongside some information to track communication failures, last activity times, and encryption data. `std::list` was intentionally used over `std::vector` as `sf::TcpSocket` is not movable (SFML is still unfortunately a C++03 library in 2021) and as the pointer stability is useful here.

Rather than looping over all clients and checking whether they have sent any data, or using multiple threads, I used these handy utilities from SFML:

```cpp
sf::TcpListener _listener;
sf::SocketSelector _socketSelector;
```

The `_socketSelector` internally uses `select` to pick any socket that's ready from any connected client without busy waiting or looping, which is efficient and elegant. It can also check if the `_listener` is ready, so the server loop becomes quite simple:

```cpp
while(_running)
{
    // Wait for data to arrive or for a client to connect.
    // Timeout after 30 seconds so that we can purge clients anyway.
    if(_socketSelector.wait(sf::seconds(30)))
    {
        tryAcceptingNewClient();
        loopOverSockets();
    }

    // Deal with clients that timed out or that need to be disconnected.
    purgeClients();
}
```

The functions called in the loop are also quite simple. The first one deals with accepting new clients:

```cpp
bool HexagonServer::tryAcceptingNewClient()
{
    if(!_socketSelector.isReady(_listener))
    {
        return false;
    }

    // Allocate memory for a potential new client.
    ConnectedClient& potentialClient =
        _connectedClients.emplace_back(Clock::now());

    // Set its socket to blocking mode.
    sf::TcpSocket& potentialSocket = potentialClient._socket;
    potentialSocket.setBlocking(true);

    // Try accepting the new client.
    if(_listener.accept(potentialSocket) != sf::Socket::Done)
    {
        // Error, we won't get a new connection, delete the socket.
        _connectedClients.pop_back();
        return false;
    }

    // All good, add the new socket to the selector.
    _socketSelector.add(potentialSocket);
    return true;
}
```

The second one deals with processing existing clients:

```cpp
void HexagonServer::loopOverSockets()
{
    for(auto it = _connectedClients.begin(); it != _connectedClients.end();
        ++it)
    {
        ConnectedClient& connectedClient = *it;
        sf::TcpSocket& clientSocket = connectedClient._socket;

        if(!_socketSelector.isReady(clientSocket))
        {
            continue; // Nothing to do for this client.
        }

        // The client has sent some data, let's read it in its buffer.
        _packetBuffer.clear();
        if(clientSocket.receive(_packetBuffer) == sf::Socket::Done)
        {
            // `processPacket` deals with packet-specific logic.
            if(processPacket(connectedClient, _packetBuffer))
            {
                connectedClient._lastActivity = Clock::now();
                connectedClient._consecutiveFailures = 0;

                continue; // All good, onto next client.
            }
            else
            {
                ++connectedClient._consecutiveFailures;
            }
        }

        // Failed to receive data, check if we need to kick the client.
        ++connectedClient._consecutiveFailures;

        constexpr int maxConsecutiveFailures = 5;
        if(connectedClient._consecutiveFailures == maxConsecutiveFailures)
        {
            _socketSelector.remove(connectedClient._socket);
            it = _connectedClients.erase(it);
        }
    }
}
```

Finally, the last function deals with the removal of clients that are timed out or want to be disconnected gracefully:

```cpp
void HexagonServer::purgeClients()
{
    constexpr std::chrono::duration maxInactivity = std::chrono::seconds(60);
    const TimePoint now = Clock::now();

    for(auto it = _connectedClients.begin(); it != _connectedClients.end();
        ++it)
    {
        ConnectedClient& connectedClient = *it;

        if(connectedClient._mustDisconnect ||
           now - connectedClient._lastActivity > maxInactivity)
        {
            _socketSelector.remove(connectedClient._socket);
            it = _connectedClients.erase(it);

            continue;
        }
    }
}
```

Seems to work quite well so far.



### client

The client is way simpler than the server. It holds the following significant data members:

```cpp
// Connection stuff.
sf::TcpSocket _socket;
bool _socketConnected;

// Buffer to send/receive packets.
sf::Packet _packetBuffer;

// To send heartbeats periodically.
TimePoint _lastHeartbeatTime;

// Encryption stuff.
const SodiumPSKeys _clientPSKeys;
std::optional<SodiumPublicKeyArray> _serverPublicKey;
std::optional<SodiumRTKeys> _clientRTKeys;
```

The client exposes a `update` member function invoked every frame:

```cpp
void HexagonClient::update()
{
    if(!_socketConnected)
    {
        return;
    }

    sendHeartbeatIfNecessary();
    receiveDataFromServer(_packetBuffer);
}
```

Pretty self-explanatory. The client's socket is sent to non-blocking mode so that we do not need any additional thread --- everything is done on the main thread. `receiveDataFromServer` uses a recursive approach to ensure partially received data is read until completion, up to a certain amount of tries.



### packets

Packet types are simple structs stored in a `std::variant`:

```cpp
struct CTSPHeartbeat  { };
struct CTSPDisconnect { };
struct CTSPPublicKey  { SodiumPublicKeyArray key; };
struct CTSPReady      { };
struct CTSPPrint      { std::string msg; };

using PVClientToServer = std::variant<PInvalid, PEncryptedMsg, CTSPHeartbeat,
    CTSPDisconnect, CTSPPublicKey, CTSPReady, CTSPPrint>;
```

Some helper functions are provided to create and decode packets:

```cpp
template <typename T>
void makeClientToServerPacket(sf::Packet& p, const T& data);

// `keyTransmit` is used to encode encrypted packets. Not all packets are
// encrypted.
template <typename T>
[[nodiscard]] bool makeClientToServerEncryptedPacket(
    const SodiumTransmitKeyArray& keyTransmit, sf::Packet& p, const T& data);

// `keyReceive` is used to decode encrypted packets.
[[nodiscard]] PVClientToServer decodeClientToServerPacket(
    const SodiumReceiveKeyArray* keyReceive, std::ostringstream& errorOss,
    sf::Packet& p);
```

Adding a new packet type requires only a few steps:

1. Create a new `struct` for the packet type.

2. Add the new `struct` to the `std::variant` list of alternatives.

3. Handle the new packet type in the client/server packet handling code.

We will look at (3) soon. (1) and (2) seem pretty self-explanatory, but how to we encode/decode packet types?

I used some metaprogramming to retrieve the index of a packet type alternative from a `std::variant` type so that I do not manually have to assign a unique ID per packet type:

```cpp
template <typename...> struct TypeList { };

template <typename T, typename... Ts>
[[nodiscard]] constexpr bool contains(TypeList<Ts...>)
{
    return (std::is_same_v<T, Ts> || ...); // C++17 fold expression.
}

template <typename T, typename... Ts>
[[nodiscard]] constexpr std::size_t indexOfType(TypeList<Ts...>)
{
    static_assert(contains<T>(TypeList<Ts...>{}));

    constexpr std::array test{std::is_same_v<T, Ts>...};
    for(std::size_t i = 0; i < test.size(); ++i)
    {
        if(test[i])
        {
            return i; // Found it!
        }
    }

    throw; // If we reach this, compilation fails.
}
```

Calling `indexOfType` with a `TypeList` built from the `std::variant` containing all possible packet types is sufficient to get a unique ID at compile-time. I then encode that ID as a `std::uint8_t` in the preamble of my packet, alongside the game version and some other stuff.

The client and server then use a lambda-overload--based approach to ensure that all packet types are handled (otherwise compilation would fail):

```cpp
[[nodiscard]] bool HexagonServer::processPacket(
    ConnectedClient& c, sf::Packet& p)
{
    const PVClientToServer pv = decodeClientToServerPacket(p);

    return Utils::match(pv,
        [&](const CTSPHeartbeat& ctsp) { /* ... */ },
        [&](const CTSPDisconnect& ctsp) { /* ... */ },
        [&](const CTSPPublicKey& ctsp) { /* ... */ },
        [&](const CTSPReady& ctsp) { /* ... */ },
        [&](const CTSPPrint& ctsp) { /* ... */ }
    );
}
```

The `Utils::match` helper function performs variant visitation as I illustrated in the *"Implementing variant Visitation Using Lambdas - Vittorio Romeo [ACCU 2017]"* talk:

<center>

<div class="video-container">

<iframe width="560" height="315" src="https://www.youtube.com/embed/mqei4JJRQ7s?controls=0" frameborder="0" allow="accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>

</div>

</center>

The nice thing about this approach is that everything is kept in sync automatically thanks to the compiler, and that I cannot forget to handle a particular packet type otherwise I will get a compile-time failure.



### steamworks authentication

I used the [Encrypted Application Tickets](https://partner.steamgames.com/doc/features/auth) feature provided by the Steamworks SDK as an extra layer of security to have more confidence in the identity of a player being valid and not spoofed.

I'm not too fond of the Steamworks API, it feels like C and doesn't take advantage of C++'s type system at all. It was a bit painful to work with it, but in the end I managed.

I will still add some sort of registration step in-game to give users a chance to choose their user name and other options, but users will be linked to their SteamID as their primary lookup key.



### `libsodium` key exchange

I ended up using [`libsodium`'s Key Exchange API](https://libsodium.gitbook.io/doc/key_exchange) to share public keys between the server and each client, and generate a set of session keys (receive key and transmit key) that can be used to encrypt and decrypt messages.

A man-in-the-middle attack is still possible, but this approach should deter most cheaters.

I wrote some wrappers around `libsodium` to make it easier to work with in C++. I started with some `std::array` wrappers for keys, so that they can be easily copied:

```cpp
inline constexpr std::size_t sodiumPublicKeyBytes = crypto_kx_PUBLICKEYBYTES;
inline constexpr std::size_t sodiumSecretKeyBytes = crypto_kx_SECRETKEYBYTES;
inline constexpr std::size_t sodiumReceiveKeyBytes = crypto_kx_SESSIONKEYBYTES;
inline constexpr std::size_t sodiumTransmitKeyBytes = crypto_kx_SESSIONKEYBYTES;
inline constexpr std::size_t sodiumNonceBytes = crypto_secretbox_NONCEBYTES;

using SodiumPublicKeyArray = std::array<std::uint8_t, sodiumPublicKeyBytes>;
using SodiumSecretKeyArray = std::array<std::uint8_t, sodiumSecretKeyBytes>;
using SodiumReceiveKeyArray = std::array<std::uint8_t, sodiumReceiveKeyBytes>;
using SodiumTransmitKeyArray = std::array<std::uint8_t, sodiumTransmitKeyBytes>;
using SodiumNonceArray = std::array<std::uint8_t, sodiumNonceBytes>;
```

I then created some helper functions to wrap the ugly C APIs:

```cpp
[[nodiscard]] constexpr std::size_t getCiphertextLength(
    const std::size_t messageLength);

[[nodiscard]] SodiumNonceArray generateNonce();

struct SodiumPSKeys
{
    SodiumPublicKeyArray keyPublic;
    SodiumSecretKeyArray keySecret;
};

struct SodiumRTKeys
{
    SodiumReceiveKeyArray keyReceive;
    SodiumTransmitKeyArray keyTransmit;
};

[[nodiscard]] SodiumPSKeys generateSodiumPSKeys();

[[nodiscard]] std::optional<SodiumRTKeys> calculateServerSessionSodiumRTKeys(
    const SodiumPSKeys& serverPSKeys,
    const SodiumPublicKeyArray& clientPublicKey);

[[nodiscard]] std::optional<SodiumRTKeys> calculateClientSessionSodiumRTKeys(
    const SodiumPSKeys& clientPSKeys,
    const SodiumPublicKeyArray& serverPublicKey);
```

Encryption and decryption of packets works through a special packet class, `PEncryptedMsg`, which contains a `sf::Packet` containing encrypted packets. I then have a recursive decoding function that can decrypt that `sf::Packet` and decode its contents.



### next steps

That's it for now!

You can find all the code in the [v2.0.5 pull request](https://github.com/vittorioromeo/SSVOpenHexagon/pull/330).

The next steps will be:

- Fix broken CI on Linux :)

- Implement a registration and authentication protocol

- Add a SQL database and interface with it via C++

After that, the actual leaderboards and replay sending logic can be implemented, and then it's time for a lot of testing!
