<link rel="alternate" type="application/rss+xml" href="https://vittorioromeo.com/index.rss" />

{{#Entries}}
    {{Entry}}
{{/Entries}}

<nav class="pagination" aria-label="Pagination">
    <ul>
        {{#Subpages}}
            <li><a href="{{Subpage}}">{{SubpageLabel}}</a></li>
            <!-- You might need logic to mark the current page -->
            <!-- Example: <li {{#IsCurrent}}class="current"{{/IsCurrent}}><a href="{{Subpage}}">{{SubpageLabel}}</a></li> -->
        {{/Subpages}}
    </ul>
</nav>
