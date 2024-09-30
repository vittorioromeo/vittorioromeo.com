<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
<channel>
    <title>vittorio romeo's website</title>
    <description>Vittorio Romeo's personal blog/website</description>
    <link>http://vittorioromeo.com</link>
    <language>en-us</language>

    <atom:link href="{{FeedLink}}" rel="self" type="application/rss+xml" />

    {{#Items}}
        <item>
            <title>{{Title}}</title>
            <link>{{Link}}</link>
            <guid>{{Link}}</guid>
            <date>{{Date}}</date>
            <description>{{Description}}</description>
            <pubDate>{{PubDate}}</pubDate>
        </item>
    {{/Items}}

</channel>
</rss>
