<!DOCTYPE html>
<html lang="en" data-theme="dark"> <!-- Set dark theme default -->

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="color-scheme" content="light dark" /> <!-- Support theme switching -->

    <!-- Use a specific title or a variable -->
    <title>{{PageTitle | default: "vittorio romeo's website"}}</title>
    <meta name="description" content="{{PageDescription | default: 'Vittorio Romeo personal blog/website'}}">

    <!-- Favicons (replace with your actual files) -->
    <link rel="icon" href="{{ResourcesPath}}/img/favicon.png" /> <!-- Adjust path -->
    <link rel="apple-touch-icon" href="{{ResourcesPath}}/img/favicon.png"> <!-- Adjust path -->

    <!-- Pico CSS -->
    <link rel="stylesheet" href="{{ResourcesPath}}/css/pico.min.css"> <!-- Adjust path -->

    <!-- Font Awesome (use CDN or download) -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css">

    <!-- Your Custom CSS (LOAD LAST) -->
    <link rel="stylesheet" href="{{ResourcesPath}}/css/custom.css"> <!-- Adjust path -->

    <!-- Original RSS Link -->
    <link rel="alternate" type="application/rss+xml" title="Vittorio Romeo's Website RSS Feed" href="https://vittorioromeo.com/index.rss" />


    <!-- MathJax (keep if needed) -->
    <script type="text/javascript" async
        src="https://cdnjs.cloudflare.com/ajax/libs/mathjax/2.7.1/MathJax.js?config=TeX-MML-AM_CHTML">
    </script>

    <!-- Highlight.js for code (optional, choose a theme) -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css"> <!-- Choose dark theme -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
    <!-- Add languages you need -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/cpp.min.js"></script>
    <script>hljs.highlightAll();</script>

</head>

<body>

    <!-- Header -->
    <header class="container page-header">
        <hgroup>
            <!-- Make title/subtitle configurable if needed -->
            <h1>Vittorio Romeo</h1>
            <p>thoughts on C++ and game development</p>
        </hgroup>
        <hr>

        <div class="footer-links" style="text-align: center">
             <!-- Add social links here -->
             <a href="mailto:vittorio.romeo@outlook.com" target="_blank" title="Email"><i class="fas fa-envelope"></i></a>
             <!-- <a href="http://www.facebook.com/vittorioromeovee" target="_blank" title="Facebook"><i class="fab fa-facebook"></i></a> -->
             <a href="https://github.com/SuperV1234" target="_blank" title="GitHub"><i class="fab fa-github"></i></a>
             <a href="http://www.youtube.com/user/SuperVictorius" target="_blank" title="YouTube"><i class="fab fa-youtube"></i></a>
             <a href="https://twitter.com/supahvee1234" target="_blank" title="Twitter"><i class="fab fa-twitter"></i></a>
             <a href="http://stackexchange.com/users/294676/vittorio-romeo" target="_blank" title="Stack Exchange"><i class="fab fa-stack-exchange"></i></a>
             <a href="https://vittorioromeo.com/index.rss" target="_blank" title="RSS Feed"><i class="fas fa-rss"></i></a>
             <a href="https://www.paypal.com/paypalme/vittorioromeocpp" target="_blank" title="Donate via PayPal"><i class="fa-brands fa-paypal"></i></a>


         </div>

    </header>
    <!-- ./ Header -->

    <!-- Main Navigation -->
    <div class="main-nav">
        <div class="container">
             {{MainMenu}} <!-- Render the main menu here -->
        </div>
    </div>
    <!-- ./ Main Navigation -->


    <!-- Main Content -->
    <main class="container">
        {{Main}} <!-- Render the main content block (posts, etc.) -->
    </main>
    <!-- ./ Main Content -->


    <!-- Footer -->
    <footer class="container page-footer">

        <!-- <script src="https://apis.google.com/js/platform.js" async defer></script> -->
        <!-- <div class="g-plusone" data-href="http://vittorioromeo.com"></div> -->

        <small>vittorio romeo © 2012 - 2025</small> <!-- Update year dynamically if possible -->
    </footer>
    <!-- ./ Footer -->

    <!-- Google Analytics (keep if needed) -->
    <script>
        (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){
        i[r].q=i[r].q||[]},i[r].l=1*new Date();a=s.createElement(o),
        m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)
        })(window,document,'script','//www.google-analytics.com/analytics.js','ga');

        ga('create', 'UA-43376035-1', 'vittorioromeo.com');
        ga('send', 'pageview');
    </script>

    <!-- Pico JS (optional, for theme switcher, dropdowns, modals) -->
    <!-- <script src="{{ResourcesPath}}/js/pico.min.js"></script> -->

</body>
</html>
