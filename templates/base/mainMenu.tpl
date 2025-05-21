<nav>
    <ul>
    {{#MenuItems}}
        <li>
            <!-- Using <a> tags styled by custom.css to look like buttons -->
            <a href="{{Link}}" role="button">
               <!-- Optional: Add icons based on Title or a new Icon variable -->
               <!-- <i class="fas fa-home"></i> -->
               {{Title}}
            </a>
        </li>
    {{/MenuItems}}
    </ul>
</nav>
