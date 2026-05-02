

A very rare occurrence on my blog, but this post is not *(directly)* about C++. Having spoken at [several conferences](https://www.youtube.com/playlist?list=PLTEcWGdSiQelEQw64Z7EL2vk6LsKW_Ouj) and [meetups](https://skillsmatter.com/legacy_profile/vittorio-romeo#skillscasts), creating slides is a task that I've always found unsatisfactory for a plethora of reasons. I want my slides to:

1. **Be quick and simple to create.** I don't to spend time manually aligning text and shapes, I don't want to manually go through 10 slides if I decide to update a code snippet.

2. **Seamlessly support code snippets.** I want nice and readable *inline code keywords* and *code blocks* with syntax highlighting.

3. **Use a non-proprietary VCS-friendly format.** I like being able to put my slides' source code on `git` and keep track of changes over time.

4. **Natively support animations, step-by-step highlighting, and diagrams.** I believe that, if used properly, animations can help understand algorithms and data structures. Step-by-step highlighting in code snippets would prevent people watching talks on YouTube from squinting in order to see a tiny laser dot.

My main operating system is Arch Linux, but I'm also willing to use Windows 10 to present. I've tried several slide creation applications in the past:

* **Microsoft PowerPoint.** The only requirement it satisfies is (4). It allows me to almost effortlessly create/present simple animations and diagrams. I find the process of placing elements in slides and adding syntax-highlighted code snippets very painful and resistant to change. Another nice thing about PowerPoint is the ability to use a digital pen to add permanent "ink annotations" to the slides which can be used to nicely highlight elements.

* **Libreoffice Impress.** In my experience, this is exactly like Powerpoint but two times as painful. The interface is more cumbersome and there are no advanced features such as ink annotations. The big advantage over PowerPoint is the fact that it's free and open-source... which unfortunately doesn't help me when creating slides.

* **`reveal.js` and similar "web-based" solutions.** While these satisfy (2) and (3), I've always found them annoying to work with. There's always some boilerplate involed in the creation of a slide deck, and the free tooling available is not as powerful as I would like.

I was pretty sad about the state of slide creation software until I discovered [**Marp**](https://github.com/yhatt/marp). It's an Electron *(`node.js`)* desktop application with a split screen: you write Markdown on the left, and a live preview of your slides appears on the right. *Perfect.*

Markdown is, in my opinion, an excellent language for slide creation: it has minimal boilerplate, supports *inline code* and *code blocks*, and inline HTML for more complicated things. **Marp** has served me well in talks like [*“Implementing `variant` visitation using lambdas"*](https://www.youtube.com/watch?v=3KyW5Ve3LtI) and [*“You must type it three times”*](https://www.youtube.com/watch?v=I3T4lePH-yA) - creating the slides was mostly painless. Unfortunately, I encountered a few issues with it:

1. **Adding new themes requires changing the source code.** This is a minor issue, as all I need to do is create a new `.css` file for the theme and add it to two files in Marp's source code.

2. **No built-in presentation mode.** This was a very very annoying issue, as it required me to export my slides to `.pdf` before presenting. The result of the conversion can sometimes differ from what you see in the preview, especially with inline HTML and CSS *(e.g. using `position: fixed;`)*. Thankfully, [this fork by **long-long-float**](https://github.com/vittorioromeo/marp/commits?author=long-long-float) added a presentation mode. I merged it into [my own fork of Marp](https://github.com/vittorioromeo/marp), fixing some issues with margins and adding a keyboard shortcut for it.

3. **No way of creating macros/replacements.** Writing slides in Markdown is the antithesis of [DRY *(don't repeat yourself)*](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself). Need to write a few `<div>`s for your section headers? You need to copy-paste. Made a change to the title of your presentation? You will have to find&replace afterwards. Want to create a few slides that incrementally show bullet points? Copy-paste.

   You get it - there's a lot of manual work involved that I want to avoid. Especially when I'm doing things like adding a `wandbox.org` or `godbolt.org` link to every code snippet. I don't want to copy paste a few `div`s, I just want some sort of *macro* that expands to them. This desire is probably shared by other users *(e.g. [this issue](https://github.com/yhatt/marp/issues/179))*.



### implementing macros in marp

Marp is open-source, written in CoffeeScript. While I really dislike dynamically-typed languages, I can live with that. I also dislike the fact that Electron is a resource hog, but in retrospect it made hacking Marp a lot easier *(e.g. all dependencies are easily obtainable with `npm`)*. I wanted to create a macro system for Marp, so I started [experimenting on my own fork](https://github.com/vittorioromeo/marp).

The first idea was creating some sort of syntax that allows simple token replacements *(a la C++ preprocessor)*. The original design was something along the lines of:

```html
<!-- macro foo: bar -->
```

Before rendering the Markdown, all occurrences of the token `"foo"` would be replaced with `"bar"`. After implementing this I realized that I would have to basically reimplement the C++ preprocessor or something similar in order to achieve an useful level of flexibility. I then started thinking about integrating a template engine... when it suddenly hit me:

> **What if I just `eval()` the entire Markdown source?**

Marp is written in CoffeeScript, which is basically syntactic sugar for JavaScript. JavaScript has an abomination called [`eval(...)`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval), which, given a string, interprets the string as if it was JavaScript code. You can imagine how many vulnerabilities have been created in the past due to the abuse of `eval` *(e.g. calling it on a string provided through a `POST` request)*.

As I only want to write slides, I don't really care about those vulnerabilities, and I don't want to reimplement a preprocessor/scripting language. This was the perfect solution: **cheap to implement** and **extremely powerful**.

My idea was as follows:

* Every line in the Markdown source would be actually transformed into a JavaScript line of code that appends to a final `out` string variable.

* Every line in the Markdown source starting with `@@` would be interpreted as JavaScript code.

* Every occurrence of `@󠀠{󠀠{...󠀠}󠀠}` in the Markdown source would "inject" the result of some JavaScript code back into the slide.

Below are some examples.

#### example: hello world

```html
<!-- before processing -->

Hello world!
```

```javascript
// after processing

(function()
{
    out = '';
    out += 'Hello world!';
    return out;
})();
```
[![Result](resources/img/blog/marpjs/0.png)](resources/img/blog/marpjs/0.png)



#### example: loop and injection

```html
<!-- before processing -->

@@ const name = "Bob";
@@ for(let i = 0; i < 3; ++i)
@@ {

Hello @󠀠{󠀠{name󠀠}󠀠}!

@@ }
```

```javascript
// after processing

(function()
{
    out = '';
    const name = "Bob";
    for(let i = 0; i < 3; ++i)
    {
        out += `Hello ${name}!`;
    }
    return out;
})();
```

[![Result](resources/img/blog/marpjs/1.png)](resources/img/blog/marpjs/1.png)



#### example: wrapping text

```html
<!-- before processing -->

@@ const red = x => `<div style="color: red;">\n\n${x}\n\n</div>`;

@󠀠{󠀠{red("hello!")󠀠}󠀠}
```

[![Result](resources/img/blog/marpjs/2.png)](resources/img/blog/marpjs/2.png)



### conclusion

My fork of Marp can [be found here on GitHub](https://github.com/vittorioromeo/marp). Note that the code was quickly hacked together by someone with minimal previous CoffeeScript experience - the quality is pretty bad. PRs are always welcome.

Calling `eval` on the entire Markdown source with my custom `@@` and `@@{...}` syntax allows me to:

* Define textual replacements, by simply using JavaScript variables.

* Write parametrized functions that generate complex headers, links, footers. These would be simple JavaScript functions that return a string, which would then be injected into the Markdown source.

* Keep writing simple slides when required. The introduction of the JavaScript processing step doesn't require me to change the way I use Markdown to write the slides.

* Generate complicated slide sequences. As an example, I can generate a few slides that show a bullet list step-by-step.

* Include other JavaScript files to have "slide boilerplate" code reuse.

* Read files on the filesystem to include code blocks in my slides. I could also write complicated functions that look for markers/functions in a bigger code file and extract them for me before placing them in a Markdown code block.

Needlessly to say, I am very satisfied with this **quick hack**. The next major improvement is allowing the injection syntax to support multi-line arguments.
