


I've recently released a script on my GitHub page that simplifies C++ compiler error messages: [**`camomilla`**](https://github.com/vittorioromeo/camomilla).

> What does it do?

`camomilla` uses simple text transformations to make `gcc` and `clang` errors smaller and easier to read. During the development of [`ecst`](https://github.com/vittorioromeo/ecst), a compile-time *Entity-Component-System* C++14 library developed for [my BCS thesis](https://github.com/vittorioromeo/bcs_thesis), I encountered a lot of huge undeciphrable errors that sometimes completely filled my terminal buffer. Here's an example:

* Before:

    ![*g++: before camomilla*](resources/img/blog/camomilla_release_gcc_before.png)

* After:

    ![*g++: after camomilla*](resources/img/blog/camomilla_release_gcc_after.png)

The reason for these enormous outputs is the fact that both `gcc` and `clang` completely expand all nested typename definitions in the context of the error.

### Transformations

The main text transformation used by `camomilla` to prevent full expansion of templates is *"template typename collapsing"*, which hides nested typenames up to a user-specified depth. Example:

```bash
echo "metavector<metatype<metawhatever<int>>>::method()" | camomilla -d0
# outputs
"metavector<?>::method()"

echo "metavector<metatype<metawhatever<int>>>::method()" | camomilla -d1
# outputs
"metavector<metatype<?>>::method()"

echo "metavector<metatype<metawhatever<int>>>::method()" | camomilla -d2
# outputs
"metavector<metatype<metawhatever<?>>>::method()"

echo "metavector<metatype<metawhatever<int>>>::method()" | camomilla -d3
# outputs
"metavector<metatype<metawhatever<int>>>::method()"
```

The two other current transformations offered by `camomilla` are simple regex replacements that can act on namespaces or generic symbols. They can be defined in `.json` configuration files *(which can recursively include each other!)* as follows:

```javascript
{
    // Add namespace replacements
    "namespaceReplacements": [
        "std": "",
        "boost::hana": "bh",
        "boost::fusion": "bf",
        "boost::spirit": "bs",
    ],

    // Add generic replacements
    "genericReplacements" : [
        "tuple": "tpl",
        "forward": "fwd"
    ]
}
```

The configuration file above can be loaded and used as follows:

```bash
g++ ./totally_real_file.cpp |& camomilla -c ./test_config.json
```

*(Note: `|&` pipes both `stdout` and `stderr` to `camomilla`.)*


### Reprocessing

Ever felt like you were *"debugging an error"* when dealing with huge template-heavy compiler outputs? `camomilla` helps with that by automatically caching the last processed original error, so that the user can play around with different transformations and options. Example:

```bash
# Let's compile this `Boost.Hana`-heavy project:
g++ ./bhtest.cpp |& camomilla

# Error output: huge list of nested `boost::hana::tuple`
# and `boost::hana::integral_constant` instances.

# No problem! Reprocess the last error with additional options.
# (Setting "template typename collapsing" depth to `0`)
camomilla -r -d0

# Too many typenames hidden away? Play with the depth:
camomilla -r -d1
camomilla -r -d2
camomilla -r -d3
# ...
```



### Solution or workaround?

*(From [`camomilla/README.md`](https://github.com/vittorioromeo/camomilla/blob/master/README.md).)*

`camomilla` is merely a workaround for the fact that compilers do not filter *(either automatically or through flags)* the depth of template typenames. Errors in projects making use of libraries such as `boost::hana` or `boost::fusion` therefore include a lot of *"typename boilerplate"* that can make the error harder to read.

Library developers are sometimes forced to make use of techniques to erase the long typenames in order to simplify the errors and *decrease compilation time*: [`boost::experimental::di`](https://github.com/boost-experimental/di) is an example.

I think this is something that should be addressed directly in the compilers - I've created a *feature request/bug report* both in the [GCC Bug Tracker](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=71167) and in the [Clang Bug Tracker](https://llvm.org/bugs/show_bug.cgi?id=27793).



### Results

I currently pipe everything I compile on my machine to `camomilla` - I find the reduced noise/boilerplate quite useful when constantly reading template-heavy errors *(...pretty much all of my code is template-heavy)*. This is a table that shows the size reduction of some errors generated from a real project, [ecst](http://github.com/vittorioromeo/ecst), by simply mispelling a member field name in a template-heavy context:

|               | Bytes (original) | Bytes (after camomilla) | Relative size change |
|---------------|------------------|-------------------------|----------------------|
| g++ 6.1.1     | 38487            | 3680                    | -90.43%              |
| clang++ 3.8.1 | 16856            | 2990                    | -82.26%              |

`clang` usually does a much better job than `gcc` in showing the source of the error and placing it at the top of the output, but the size reduction often means that mistakes are easier to pinpoint and track.




### Using/hacking `camomilla`

You can get and install `camomilla` by cloning the [official GitHub repository](https://github.com/vittorioromeo/camomilla) and using the `setup.py` script. Contributions are welcome - `camomilla` is written as a single Python 3 script file. *(I used Python instead of C++ because `camomilla` is the "evolution" of an older poorly designed script that attempted to simplify errors for `ecst`.)*
