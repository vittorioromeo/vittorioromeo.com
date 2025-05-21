<meta name="twitter:card" content="summary" />
<meta name="twitter:site" content="@supahvee1234" />
<meta name="twitter:title" content="{{Title}}" />
<meta name="twitter:description" content="Article/tutorial on http://vittorioromeo.com" />
<meta name="twitter:image" content="http://vittorioromeo.com/resources/img/logoMain.png" />


<article class="blog-entry">
    <header>
        {{PermalinkBegin}}
        <h2>{{Title}}</h2>
        {{PermalinkEnd}}

        <div class="entry-meta">
            <span class="date"><i class="fas fa-calendar-alt"></i> {{Date}}</span>
            <span class="tags">
                <i class="fas fa-tags"></i>
                {{#Tags}}
                    <a href="{{Link}}">{{Label}}</a>
                {{/Tags}}
            </span>
        </div>
    </header>

    <div class="entry-content">
        {{Text}}
    </div>

    {{#IncludeComments}}
    <hr style="margin: 2rem 0;">
    <section id="comments">
         <h3>Comments</h3>
         {{CommentsBox}}
    </section>
    {{/IncludeComments}}

</article>
