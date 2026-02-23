/* Grading History: session list and read-only session detail view */
var Dashboard = Dashboard || {};

Dashboard.gradingHistory = (function() {

    function esc(str) {
        var d = document.createElement('div');
        d.textContent = str;
        return d.innerHTML;
    }

    function formatDate(dateStr) {
        if (!dateStr) return '';
        var d = new Date(dateStr + 'Z');
        return d.toLocaleDateString(undefined, {
            year: 'numeric', month: 'short', day: 'numeric',
            hour: '2-digit', minute: '2-digit'
        });
    }

    function scoreBadgeClass(pct) {
        if (pct >= 80) return 'score-good';
        if (pct >= 50) return 'score-ok';
        return 'score-bad';
    }

    function buildHistoryPanel(containerId) {
        var el = document.getElementById(containerId);
        if (!el) return;
        el.innerHTML =
            '<div class="grading-form">' +
            '<div class="history-header">' +
            '<div class="history-header-text">' +
            '<span class="grading-card-title">Grading Sessions</span>' +
            '</div>' +
            '<button type="button" class="btn btn-primary" id="btn-new-session">' +
            '<span class="material-symbols-outlined" style="font-size:18px;vertical-align:middle;margin-right:4px">add</span>' +
            'New Session</button>' +
            '</div>' +
            '<div id="session-list" class="session-list"></div>' +
            '</div>';

        document.getElementById('btn-new-session').addEventListener('click', function() {
            Dashboard.grading.startNew();
        });

        loadSessions();
    }

    function loadSessions() {
        var list = document.getElementById('session-list');
        if (!list) return;
        list.innerHTML = '<div class="session-empty">Loading sessions...</div>';

        Dashboard.api.get('/api/dashboard/grading/sessions')
            .then(function(sessions) {
                renderSessionList(list, sessions);
            })
            .catch(function() {
                list.innerHTML = '<div class="session-empty">Failed to load sessions.</div>';
            });
    }

    function renderSessionList(container, sessions) {
        container.innerHTML = '';
        if (!sessions || sessions.length === 0) {
            container.innerHTML =
                '<div class="session-empty">' +
                '<span class="material-symbols-outlined" style="font-size:48px;color:var(--text-muted);margin-bottom:12px;display:block">grading</span>' +
                'No grading sessions yet.<br>' +
                'Click "New Session" to start grading student submissions.' +
                '</div>';
            return;
        }

        sessions.forEach(function(s) {
            var card = document.createElement('div');
            card.className = 'session-card';

            var avgPct = Math.round(s.avg_percentage || 0);
            var cls = scoreBadgeClass(avgPct);
            var statusLabel = s.status === 'completed' ? 'Completed' : 'In Progress';

            card.innerHTML =
                '<div class="session-card-main">' +
                '<div class="session-card-info">' +
                '<span class="session-card-name">' + esc(s.name) + '</span>' +
                '<span class="session-meta">' +
                formatDate(s.created_at) +
                ' -- ' + (s.submission_count || 0) + ' submission' +
                ((s.submission_count || 0) !== 1 ? 's' : '') +
                ' -- ' + statusLabel +
                '</span>' +
                '</div>' +
                '<div class="session-card-actions">' +
                (s.submission_count > 0
                    ? '<span class="session-avg ' + cls + '">' + avgPct + '%</span>'
                    : '') +
                '<button type="button" class="btn-icon danger btn-delete-session" title="Delete session">' +
                '<span class="material-symbols-outlined">delete</span></button>' +
                '</div></div>';

            /* Click card to view detail */
            card.addEventListener('click', function(e) {
                if (e.target.closest('.btn-delete-session')) return;
                Dashboard.grading.viewSession(s.id);
            });

            /* Delete button */
            card.querySelector('.btn-delete-session').addEventListener('click', function(e) {
                e.stopPropagation();
                if (!confirm('Delete this grading session? This cannot be undone.')) return;
                deleteSession(s.id);
            });

            container.appendChild(card);
        });
    }

    function deleteSession(id) {
        Dashboard.api.del('/api/dashboard/grading/sessions/' + id)
            .then(function() { loadSessions(); })
            .catch(function() { alert('Failed to delete session.'); });
    }

    function buildSessionDetail(containerId, session) {
        var el = document.getElementById(containerId);
        if (!el) return;

        var avgPct = Math.round(session.avg_percentage || 0);
        var cls = scoreBadgeClass(avgPct);
        var results = session.results || [];

        var avgText = results.length > 0
            ? '<strong>' + avgPct + '%</strong> average across <strong>' + results.length + '</strong> submissions'
            : 'No results';

        el.innerHTML =
            '<div class="grading-form">' +
            '<div class="session-detail-header">' +
            '<button type="button" class="btn btn-sm" id="btn-back-history">' +
            '<span class="material-symbols-outlined" style="font-size:18px;vertical-align:middle;margin-right:4px">arrow_back</span>' +
            'Back to Sessions</button>' +
            '<span class="grading-card-title">' + esc(session.name) + '</span>' +
            '<span class="session-meta">' + formatDate(session.created_at) + '</span>' +
            '</div>' +
            '<div class="results-toolbar">' +
            '<div class="results-summary">' + avgText + '</div>' +
            '</div>' +
            '<div id="session-detail-results"></div></div>';

        document.getElementById('btn-back-history').addEventListener('click', function() {
            Dashboard.grading.backToHistory();
        });

        renderDetailResults(results);
    }

    function renderDetailResults(results) {
        var list = document.getElementById('session-detail-results');
        if (!list) return;
        list.innerHTML = '';

        if (results.length === 0) {
            list.innerHTML = '<div class="session-empty">No results recorded for this session.</div>';
            return;
        }

        results.forEach(function(r) {
            var card = document.createElement('div');
            card.className = 'result-card';

            var headerName = esc(r.student_name) +
                ' <span class="result-studentid">' + esc(r.student_id_str) + '</span>';

            if (r.status === 'error') {
                card.innerHTML = '<div class="result-header result-error">' +
                    '<span class="result-name">' + headerName + '</span>' +
                    '<span class="result-status">Failed</span></div>' +
                    '<p class="result-error-msg">' + esc(r.error_text || 'Unknown error') + '</p>';
            } else {
                var pct = Math.round(r.percentage || 0);
                var cls = scoreBadgeClass(pct);
                var criteria = r.criteria || [];
                var ch = '';
                for (var i = 0; i < criteria.length; i++) {
                    var c = criteria[i];
                    var barPct = c.maxScore > 0 ? Math.round((c.score / c.maxScore) * 100) : 0;
                    ch += '<div class="criteria-detail-row">' +
                        '<div class="cd-header">' +
                        '<span class="cd-name">' + esc(c.name) + '</span>' +
                        '<span class="cd-score">' + c.score + '/' + c.maxScore + '</span>' +
                        '</div>' +
                        '<div class="cd-bar-track"><div class="cd-bar-fill" style="width:' + barPct + '%"></div></div>' +
                        (c.feedback ? '<p class="cd-feedback">' + esc(c.feedback) + '</p>' : '') +
                        '</div>';
                }
                card.innerHTML = '<div class="result-header">' +
                    '<span class="result-name">' + headerName + '</span>' +
                    '<span class="result-score ' + cls + '">' + r.total_score + '/' +
                    r.total_max + ' (' + pct + '%)</span></div>' +
                    '<div class="result-feedback">' + esc(r.overall_feedback || '') + '</div>' +
                    (ch ? '<details class="criteria-detail">' +
                    '<summary>Per-criterion breakdown</summary>' + ch + '</details>' : '');
            }
            list.appendChild(card);
        });
    }

    return {
        buildHistoryPanel: buildHistoryPanel,
        buildSessionDetail: buildSessionDetail,
        loadSessions: loadSessions
    };
})();
