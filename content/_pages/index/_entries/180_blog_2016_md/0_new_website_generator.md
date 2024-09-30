I've spent the last week rewriting the static website generator for [vittorioromeo.com](http://vittorioromeo.com) from scratch.

> Why?

The previous one had several limitations that couldn't be easily fixed due to poor design:

* No single page generation for posts.

    * No permalinking, no comments.

* No automatic pagination.

* Many things *(like page output files)* had to be specified manually.

    * No hierarchical website structure.


The new generator fixes all the aforementioned issues and also adds some cool features, such as **post tagging** and **long post truncation**. In terms of implementation, the dependencies are *(currently)* unchanged:

* My own [JSON library](https://github.com/SuperV1234/SSVUtils/tree/master/include/SSVUtils/Json) is being used to parse JSON metadata.

* My own [templating system](https://github.com/SuperV1234/SSVUtils/tree/master/include/SSVUtils/TemplateSystem) is being used to expand HTML templates from JSON metadata and Markdown sources.

* My own [filesystem library](https://github.com/SuperV1234/SSVUtils/tree/master/include/SSVUtils/Core/FileSystem) is being used to "understand" the website's hierarchical structure and to load content data and metadata.

The dependencies listed above are part of my [SSVUtils](https://github.com/SuperV1234/SSVUtils) general purpose C++14 library, which is quite old and not currently being worked on *(except for bugfixes and minor additions)*.

My future plans consist in replacing them with more modern and more widespread alternatives *(e.g. [boostache](https://github.com/cierelabs/boostache) and the [filesystem TS](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0218r0.html))*.

Content generation from JSON metadata has been improved significantly. The main idea is that JSON files contain an `"expand"` object with key-value pairs, where the keys are matched in the templating system and replaced with the values. The expansion is now **recursive**, allowing templates to generate complex nested hierarchies via nested JSON objects/arrays.

In addition, if one of the values inside an `"expand"` object ends with the `.md` extension, the value will be handled as a path: the file in that location will be read and sent to [**Pandoc**](http://pandoc.org/), which will produce HTML with [Pygments](http://pygments.org/)-like **syntax highlighting**, [MathJax](https://www.mathjax.org/) **mathematical expressions**, and many [other more cool features](http://pandoc.org/MANUAL.html).

Here are some examples:

```cpp
#include <iostream>

// random c++ code
int main()
{
    int x;
    constexpr std::size_t y;
}

void f(int, float) noexcept;
```

$100 + \frac{24}{25} - \theta$

$f(x)=\sum_{n=0}^\infty\frac{f^{(n)}(a)}{n!}(x-a)^n$


|  A  |  B  |  C  |
|:---:|:---:|:---:|
| 25% | 50% | 75% |


Table: Example table demonstration.

<br>

The generator is still experimental and not yet finished. The source code is [available here](https://github.com/SuperV1234/vittorioromeo.com) but currently is an **horribly hacked-together mess**. I will work on making it nicer in the future.

I am planning to start writing and sharing small articles/tutorials on here, now that I can permalink stuff!
