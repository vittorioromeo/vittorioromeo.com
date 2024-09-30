<meta name="twitter:card" content="summary" />
<meta name="twitter:site" content="@supahvee1234" />
<meta name="twitter:title" content="{{Title}}" />
<meta name="twitter:description" content="Article/tutorial on http://vittorioromeo.com" />
<meta name="twitter:image" content="http://vittorioromeo.com/resources/img/logoMain.png" />

<section>
    <div class="entryTitle">
        {{PermalinkBegin}}
        <h2 style="margin-bottom: -15px">{{Title}}</h2>
        {{PermalinkEnd}}

        <div class="blogDate" style="float: left; margin-top: 20px; height: 20px;">{{Date}}</div>
        <div class="blogDate" style="float: right; margin-top: 20px; height: 20px;">
            {{#Tags}}
                <span class="blogTag"><a href="{{Link}}">{{Label}}</a></span>
            {{/Tags}}
        </div>
    </div>
    <div class="entryText" style="margin-top: 70px">
        <div class="blogEntry">
            {{Text}}
        </div>
    </div>

    <hr>

</section>

{{CommentsBox}}
