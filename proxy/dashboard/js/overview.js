/* Overview page with card counts and navigation */
var Dashboard = Dashboard || {};

Dashboard.overview = (function() {
    var built = false;

    function mount() {
        if (!built) {
            buildCards();
            built = true;
        }
        refreshCounts();
    }

    function buildCards() {
        var grid = document.getElementById('overview-grid');
        grid.innerHTML =
            '<div class="overview-card disabled">' +
                '<div class="overview-card-icon">&#9993;</div>' +
                '<div class="overview-card-title">Chat Interface</div>' +
                '<div class="overview-card-count">--</div>' +
                '<div class="overview-card-desc">Coming soon</div>' +
            '</div>' +
            '<div class="overview-card" data-nav="keys">' +
                '<div class="overview-card-icon">&#9919;</div>' +
                '<div class="overview-card-title">API Keys</div>' +
                '<div class="overview-card-count" id="overview-keys-count">--</div>' +
                '<div class="overview-card-desc">Manage your proxy API keys</div>' +
            '</div>' +
            '<div class="overview-card" data-nav="grading">' +
                '<div class="overview-card-icon">&#9998;</div>' +
                '<div class="overview-card-title">Automated Grading</div>' +
                '<div class="overview-card-count" id="overview-grading-count">--</div>' +
                '<div class="overview-card-desc">Grade submissions with AI</div>' +
            '</div>' +
            '<div class="overview-card" data-nav="coding">' +
                '<div class="overview-card-icon">&#10094;/&#10095;</div>' +
                '<div class="overview-card-title">Cloud Coding</div>' +
                '<div class="overview-card-count">--</div>' +
                '<div class="overview-card-desc">Code with AI in the cloud</div>' +
            '</div>';

        var cards = grid.querySelectorAll('.overview-card[data-nav]');
        for (var i = 0; i < cards.length; i++) {
            cards[i].addEventListener('click', function() {
                var target = this.getAttribute('data-nav');
                if (target) Dashboard.router.navigate(target);
            });
        }
    }

    function refreshCounts() {
        Dashboard.api.get('/api/dashboard/keys').then(function(keys) {
            var el = document.getElementById('overview-keys-count');
            if (el) el.textContent = (keys && keys.length) ? keys.length : '0';
        }).catch(function() {
            var el = document.getElementById('overview-keys-count');
            if (el) el.textContent = '0';
        });

        /* Grading count: number of submissions graded this session */
        var gradingEl = document.getElementById('overview-grading-count');
        if (gradingEl) {
            var count = 0;
            if (Dashboard.grading && Dashboard.grading.getState) {
                var st = Dashboard.grading.getState();
                count = st.submissions.filter(function(s) {
                    return s.status === 'done';
                }).length;
            }
            gradingEl.textContent = count;
        }
    }

    return { mount: mount };
})();
