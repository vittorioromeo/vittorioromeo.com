


An interesting **header-only** logging library, *spdlog*, was recently posted on Reddit. A [comment by Sean Middleditch][reddit_sean_comment] sparked an interesting discussion on the thread:

> "Header only" is an anti-feature. Fast compiles are important. PCHes only fix a fraction of the problems of header bloat. Avoiding 15 minutes of setup to get a library building/precompiled in exchange for months of lost productivity waiting for slow builds is a pretty bad trade off.

Here are some of the replies:

> "Honestly, I lost months of productivity trying to handle the compilation of twenty libraries on X different platforms with Y different link flags, with X and Y both > 10"
<br><br>
> "Respectfully, I disagree. Header-only libraries save a lot of setup stage time and they were a real breeze to work with when it comes down to cross-platform development. Some may argue that CMake solves that problem, but I would argue that CMake is a problem itself."

Another reply is particularly interesting, and inspired me to write this article:

> Header only is an anti-feature for large projects and production environments, but is really nice for quick setups, small and test projects (tutorial, workshops...), or having users evaluating the library easily. I like the http://fmtlib.net/latest/index.html approach to opt-in for header only, you can have both things.

Libraries can be designed and implemented in order to allow users to choose between **header-only usage**, **static linking** or **dynamic linking**. I've been doing this in all of my libraries since I've posted [this question on StackOverflow][stackoverflow_lib_design] and [this question on Reddit][reddit_library_architecture], both of which gave me a lot of great insight.

The main idea is to **conditionally include `.cpp` files** depending on a preprocessor macro, which can be defined during compilation. Functions also have to be **conditionally decorated with the [`inline` specifier](http://en.cppreference.com/w/cpp/language/inline)**.

### Example library

In this article/tutorial we'll see how this is implemented using an example library. The library will have the following physical structure:

```bash
└── example_lib
    │
    │   # API definition header.
    ├── api.hpp
    │
    │   # Library implementation folder.
    ├── library
    │   │
    │   │   # "Module 0" implementation folder.
    │   ├── module0
    │   │   ├── module0.cpp
    │   │   ├── module0.hpp
    │   │   └── module0.inl
    │   │
    │   │   # "Module 0" public header.
    │   ├── module0.hpp
    │   │
    │   │   # "Module 1" implementation folder.
    │   ├── module1
    │   │   ├── module1.cpp
    │   │   ├── module1.hpp
    │   │   └── module1.inl
    │   │
    │   │   # "Module 1" public header.
    │   └── module1.hpp
    │
    │   # "Main" public header - includes all modules.
    └── library.hpp
```

This is the physical structure model I use in my latest libraries.

* *Public* header files like `library.hpp`, `module0.hpp` or `module1.hpp` are meant to be included by users in their projects and to automatically include all their required dependencies.

    * My rule of thumb is to have a folder with the same name as the header file, containing all included implementation files.

* `.cpp` files are **conditionally included** by their respective header files, **only if** the library is being used in *header-only mode*.

* `.inl` files are **unconditionally included**, and usually contain `template` definitions or are required to avoid circular dependencies.

The library can be used in several modes:

* **Header-only mode**:

    * The user has to define `-DLIBRARY_HEADER_ONLY` during compilation.

    * The user must not compile `.cpp` files.

* **Static-linking mode**:

    * The user has to compile `.cpp` files into `.o` files.

    * The user must archive the compiled object files together *(e.g. by using `ar`)*.

* **Dynamic-linking mode**:

    * The user has to compile `.cpp` files into `.o` files, using `-fPIC`.

    * The user must create a `.so` file *(e.g. by using `g++ -shared`)*.

**[The code for the example library is available on GitHub.](https://github.com/SuperV1234/vittorioromeo.com/tree/master/extra/example_library)**

### API options

The `api.hpp` file will contain required preprocessor definitions:

```cpp
/*
    ./api.hpp
*/

#pragma once

#if defined(LIBRARY_HEADER_ONLY)
    #define LIBRARY_API inline
#elif defined(LIBRARY_EXPORT)
    #define LIBRARY_API __declspec(dllexport)
#elif defined(LIBRARY_IMPORT)
    #define LIBRARY_API __declspec(dllimport)
#else
    #define LIBRARY_API
#endif
```

The `LIBRARY_API` macro will depend on how the user desires to link the library. Note that `dllexport` and `dllimport` are [Microsoft-specific extensions](https://msdn.microsoft.com/en-us/library/3y1sfaz2.aspx) *(which I have not tested)*.

<!-- * -->

Every module of the library will include `api.hpp`.


### Library modules

*(Note: with the word **module** I am not referring to the current C++ modules proposal, but simply to "a self-contained unit" of a library.)*

#### Public header files

Public module header files will take care of including all the implementation files for a specific library module. They will also include source files if `LIBRARY_HEADER_ONLY` is defined.

```cpp
/*
    ./module0.hpp
*/

#pragma once

#include "../api.hpp"

// Include "non-public implementation" headers.
#include "./module0/module0.hpp"

// Include "unconditionally included" definitions.
#include "./module0/module0.inl"

// Include `.cpp` files only if `LIBRARY_HEADER_ONLY` is defined.
#if defined(LIBRARY_HEADER_ONLY)
    #include "./module0/module0.cpp"
#endif
```


#### Implementation header files

Implementation module header files will contain declarations. Functions that will be defined in `.cpp` source files will have to be decorated with the `LIBRARY_API` macro.

```cpp
/*
    ./module0/module0.hpp
*/

#pragma once

#include "../../api.hpp"

// Will be defined in `./module0/module0.inl`.
// (No need for `LIBRARY_API`.)
template <typename T>
void t_func0();

// Will be defined in `./module0/module0.cpp`.
// (Requires `LIBRARY_API`.)
LIBRARY_API void func0();
```


#### Implementation source files

Function implementations in `.cpp` source files and `.inl` definition files do not require any unusual decoration.


```cpp
/*
    ./module0/module0.inl
*/

#pragma once

#include "./module0.hpp"
#include <iostream>

template <typename T>
void t_func0()
{
    std::cout << "hello from t_func0()\n";
}
```

```cpp
/*
    ./module0/module0.cpp
*/

#include "./module0.hpp"
#include <iostream>

void func0()
{
    std::cout << "hello from func0()\n";
}
```



### Compilation/usage

In this section we'll cover some examples on how users can include the library in their projects. Imagine the following user code files:

```bash
└── src
    │
    │   # Includes `./library.hpp`.
    ├── main.cpp
    │
    │   # Includes `./library.hpp`.
    ├── src0.cpp
    │
    │   # Includes `./library.hpp`.
    └── src1.cpp
```

Every file will be compiled separately into its own object file and includes the entire library. This is where the `LIBRARY_API` macro becomes essential: if the library is being used in *header-only mode* it is **mandatory** to decorate `func0` and `func1` with `inline` to prevent *"multiple definition"* compilation errors.

The following examples were tested both with `g++ 6.1.1` and `clang++ 3.8.1`. You can find the test script [here on GitHub](https://github.com/SuperV1234/vittorioromeo.com/blob/master/extra/example_library/test.sh).

#### Header-only mode

```bash
# Build user application
g++ -DLIBRARY_HEADER_ONLY -c ./src/src0.cpp -o ./src0.o
g++ -DLIBRARY_HEADER_ONLY -c ./src/src1.cpp -o ./src1.o
g++ -DLIBRARY_HEADER_ONLY ./src0.o ./src1.o ./src/main.cpp
```


#### Static-linking mode

```bash
# Build library
g++ -c ./example_lib/library/module0/module0.cpp -o ./module0.o
g++ -c ./example_lib/library/module1/module1.cpp -o ./module1.o
ar rs ./example_lib.a ./module0.o ./module1.o

# Build user application
g++ -c ./src/src0.cpp -o ./src0.o
g++ -c ./src/src1.cpp -o ./src1.o
g++ ./src/main.cpp ./src0.o ./src1.o ./example_lib.a
```


#### Dynamic-linking mode

```bash
# Build library
g++ -c -fPIC ./example_lib/library/module0/module0.cpp -o ./module0.o
g++ -c -fPIC ./example_lib/library/module1/module1.cpp -o ./module1.o
g++ -shared -o ./example_lib.so ./module0.o ./module1.o

# Build user application
g++ -c ./src/src0.cpp -o ./src0.o
g++ -c ./src/src1.cpp -o ./src1.o
g++ ./src/main.cpp ./src0.o ./src1.o ./example_lib.so
```


#### CMake

The "opt-in header-only" approach can also be easily used with CMake. You may find [this StackOverflow question][stackoverflow_cmake_gen_makefile] I asked a while ago interesting.

You can see a real-life example in the `fmtlib/fmt` library:

* [`fmtlib/fmt/format.h`](https://github.com/fmtlib/fmt/blob/master/fmt/format.h).

    * This is where `FMT_API` is defined, which is analogous to the example `LIBRARY_API` macro.

* [`CMakeLists.txt`](https://github.com/fmtlib/fmt/blob/master/fmt/CMakeLists.txt).

    * This is where `FMT_HEADER_ONLY` is conditionally defined.

*EDIT:* [Steveire](https://twitter.com/steveire), a CMake maintainer, wrote a great response/follow-up to this article. [His blog post](https://steveire.wordpress.com/2016/08/09/opt-in-header-only-libraries-with-cmake/) shows an elegant way of setting up your `CMakeLists.txt` file which allows users to very easily switch between the *static*/*dynamic*/*header-only* modes.






[reddit_sean_comment]: https://www.reddit.com/r/cpp/comments/4vtyq2/first_official_version_of_spdlog_a_super_fast_c/d61mcdw 'Reddit: "First official version of spdlog, a super fast C++ logging library, released" - comment by /u/SeanMiddleditch'

[stackoverflow_lib_design]: http://stackoverflow.com/questions/25606736 'StackOverflow: "Library design: allow user to decide between “header-only” and dynamically linked?"'

[stackoverflow_cmake_gen_makefile]: http://stackoverflow.com/questions/27866669 'StackOverflow: "Preventing CMake-generated makefile for optional-header-only library from compiling source files in header-only mode"'

[reddit_library_architecture]: https://www.reddit.com/r/cpp/comments/3f1ryj/optionallyheaderonly_library/ 'Optionally-header-only library architecture/structure/design discussion'
