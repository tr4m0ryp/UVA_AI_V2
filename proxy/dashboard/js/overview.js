/* Overview page navigation cards */
var Dashboard = Dashboard || {};

Dashboard.overview = (function() {
    var built = false;

    function mount() {
        if (!built) {
            buildCards();
            built = true;
        }
    }

    function buildCards() {
        var grid = document.getElementById('overview-grid');
        grid.innerHTML =
            '<div class="overview-card" data-nav="chat">' +
                '<div class="overview-card-icon"><span class="material-symbols-outlined">chat</span></div>' +
                '<div class="overview-card-title">Chat Interface</div>' +
                '<div class="overview-card-desc">Interact directly with UvA AI models through a built-in browser chat interface. No additional setup required.</div>' +
            '</div>' +
            '<div class="overview-card" data-nav="keys">' +
                '<div class="overview-card-icon"><span class="material-symbols-outlined">key</span></div>' +
                '<div class="overview-card-title">API Keys</div>' +
                '<div class="overview-card-desc">Generate and manage API keys to connect your own applications to UvA AI via the OpenAI-compatible proxy endpoint.</div>' +
            '</div>' +
            '<div class="overview-card" data-nav="grading">' +
                '<div class="overview-card-icon"><span class="material-symbols-outlined">grading</span></div>' +
                '<div class="overview-card-title">Automated Grading</div>' +
                '<div class="overview-card-desc">Batch-grade student submissions using a rubric and AI evaluation. Upload assignments, run grading, and export results.</div>' +
            '</div>' +
            '<div class="overview-card" data-nav="coding">' +
                '<div class="overview-card-icon"><span class="material-symbols-outlined">terminal</span></div>' +
                '<div class="overview-card-title">Cloud Coding</div>' +
                '<div class="overview-card-desc">Launch an AI-assisted coding environment in the cloud for interactive development sessions with UvA AI support.</div>' +
            '</div>' +
            '<div class="overview-card disabled">' +
                '<div class="overview-card-icon"><span class="material-symbols-outlined">auto_stories</span></div>' +
                '<div class="overview-card-title">Interactive Learning Space</div>' +
                '<div class="overview-card-badge">Coming soon</div>' +
                '<div class="overview-card-desc">Upload course materials, lecture slides, and papers to build a personal study space. Ask questions about your content, generate structured summaries, and deepen understanding through AI-guided exploration.</div>' +
            '</div>';

        var cards = grid.querySelectorAll('.overview-card[data-nav]');
        for (var i = 0; i < cards.length; i++) {
            cards[i].addEventListener('click', function() {
                var target = this.getAttribute('data-nav');
                if (target) Dashboard.router.navigate(target);
            });
        }
    }

    return { mount: mount };
})();
