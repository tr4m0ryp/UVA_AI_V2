/* Grading history: session list shown on grading view mount */
var Dashboard = Dashboard || {};

Dashboard.gradingHistory = (function() {
    var _bound = false;

    function _esc(str) {
        var d = document.createElement('div');
        d.textContent = String(str);
        return d.innerHTML;
    }

    function _fmtDate(iso) {
        if (!iso) return '';
        return iso.slice(0, 16).replace('T', ' ');
    }

    function _bindEvents() {
        if (_bound) return;
        _bound = true;

        document.getElementById('btn-new-grading').addEventListener('click', function() {
            Dashboard.grading.startNew();
        });

        document.getElementById('btn-back-grading').addEventListener('click', function() {
            showHistory();
        });
    }

    function mount() {
        _bindEvents();
        load();
    }

    function load() {
        Dashboard.api.get('/api/dashboard/grading')
            .then(render)
            .catch(function(err) { console.error('grading history load:', err); });
    }

    function render(sessions) {
        var grid  = document.getElementById('grading-history-grid');
        var empty = document.getElementById('grading-history-empty');
        grid.innerHTML = '';

        if (!sessions || sessions.length === 0) {
            empty.classList.remove('hidden');
            return;
        }
        empty.classList.add('hidden');

        sessions.forEach(function(s) {
            var card = document.createElement('div');
            card.className = 'grading-card';
            card.innerHTML =
                '<div class="grading-card-title">' + _esc(s.title) + '</div>' +
                '<div class="grading-card-meta">' +
                _fmtDate(s.created_at) + ' &middot; ' +
                _esc(s.submission_count) + ' submission' +
                (s.submission_count !== 1 ? 's' : '') +
                '</div>' +
                '<div class="grading-card-actions">' +
                '<button type="button" class="btn btn-sm btn-danger btn-del-session">Delete</button>' +
                '</div>';

            card.addEventListener('click', function(e) {
                if (e.target.classList.contains('btn-del-session')) return;
                _openSession(s.id);
            });

            card.querySelector('.btn-del-session').addEventListener('click', function(e) {
                e.stopPropagation();
                _deleteSession(s.id);
            });

            grid.appendChild(card);
        });
    }

    function _openSession(id) {
        Dashboard.api.get('/api/dashboard/grading/' + id)
            .then(function(data) {
                var stored;
                try { stored = JSON.parse(data.results_json); }
                catch (e) { stored = {}; }
                Dashboard.grading.showStoredResults(stored);
            })
            .catch(function(err) { console.error('open session:', err); });
    }

    function _deleteSession(id) {
        if (!confirm('Delete this grading session?')) return;
        Dashboard.api.del('/api/dashboard/grading/' + id)
            .then(load)
            .catch(function(err) { console.error('delete session:', err); });
    }

    function showHistory() {
        document.getElementById('grading-header-history').classList.remove('hidden');
        document.getElementById('grading-header-workflow').classList.add('hidden');
        document.getElementById('grading-history-grid').classList.remove('hidden');
        document.getElementById('grading-workflow').classList.add('hidden');
        /* empty-state visibility is managed by render() */
    }

    function showWorkflow() {
        document.getElementById('grading-header-history').classList.add('hidden');
        document.getElementById('grading-header-workflow').classList.remove('hidden');
        document.getElementById('grading-history-grid').classList.add('hidden');
        document.getElementById('grading-history-empty').classList.add('hidden');
        document.getElementById('grading-workflow').classList.remove('hidden');
    }

    return {
        mount: mount,
        load: load,
        showHistory: showHistory,
        showWorkflow: showWorkflow
    };
})();
