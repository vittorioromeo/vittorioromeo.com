<link rel="alternate" type="application/rss+xml" href="https://vittorioromeo.com/index.rss" />

<article>

    {{#Entries}}
        {{Entry}}
    {{/Entries}}

    <div class="pagination">
        <ul>
            {{#Subpages}}
                <li class="pageButton"><a href="{{Subpage}}">{{SubpageLabel}}</a></li>
            {{/Subpages}}
        </ul>
    </div>
</article>

<div class="asideWrapper">

	{{#Asides}}
	    {{Aside}}
	{{/Asides}}
