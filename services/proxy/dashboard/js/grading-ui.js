/* Grading UI: DOM builders for rubric, submissions, and results panels */
var Dashboard = Dashboard || {};

Dashboard.gradingUI = (function() {
    function esc(str) {
        var d = document.createElement('div');
        d.textContent = str;
        return d.innerHTML;
    }

    /* --- Rubric Editor (Panel 1) --- */
    function buildRubricEditor(containerId, state) {
        var el = document.getElementById(containerId);
        el.innerHTML =
            '<div class="grading-form">' +
            '<label>AI Model<select id="grading-model"></select></label>' +
            '<div class="criteria-header"><h3>Grading Criteria</h3>' +
            '<button type="button" class="btn btn-sm" id="btn-add-criterion">' +
            'Add Criterion</button></div>' +
            '<div id="criteria-list"></div>' +
            '<div class="grading-actions">' +
            '<button type="button" class="btn btn-primary" id="btn-rubric-next">' +
            'Next: Add Submissions</button></div></div>';

        Dashboard.models.populateSelect(
            document.getElementById('grading-model'), state.rubric.model);
        document.getElementById('grading-model').addEventListener('change', function() {
            state.rubric.model = this.value;
        });
        document.getElementById('btn-add-criterion').addEventListener('click', function() {
            Dashboard.grading.addCriterion();
        });
        document.getElementById('btn-rubric-next').addEventListener('click', function() {
            if (state.rubric.criteria.length === 0) {
                alert('Add at least one grading criterion.');
                return;
            }
            Dashboard.grading.setStep(2);
        });
        renderCriteria(state);
    }

    function renderCriteria(state) {
        var list = document.getElementById('criteria-list');
        if (!list) return;
        list.innerHTML = '';
        state.rubric.criteria.forEach(function(c, idx) {
            var row = document.createElement('div');
            row.className = 'criterion-row';
            row.innerHTML =
                '<div class="criterion-fields">' +
                '<input type="text" class="criterion-name" placeholder="Criterion name" ' +
                'value="' + esc(c.name) + '">' +
                '<textarea class="criterion-desc" rows="2" placeholder="Description...">' +
                esc(c.description) + '</textarea>' +
                '<div class="criterion-points-row"><label>Max Points ' +
                '<input type="number" class="criterion-points" value="' + c.maxPoints +
                '" min="1" max="100"></label>' +
                '<button type="button" class="btn btn-sm btn-danger btn-remove-criterion">' +
                'Remove</button></div></div>';
            row.querySelector('.criterion-name').addEventListener('input', function() {
                Dashboard.grading.updateCriterion(idx, 'name', this.value);
            });
            row.querySelector('.criterion-desc').addEventListener('input', function() {
                Dashboard.grading.updateCriterion(idx, 'description', this.value);
            });
            row.querySelector('.criterion-points').addEventListener('input', function() {
                Dashboard.grading.updateCriterion(idx, 'maxPoints', parseInt(this.value, 10) || 1);
            });
            row.querySelector('.btn-remove-criterion').addEventListener('click', function() {
                Dashboard.grading.removeCriterion(idx);
            });
            list.appendChild(row);
        });
    }

    /* --- Submission Panel (Panel 2) --- */
    function buildSubmissionPanel(containerId, state) {
        var el = document.getElementById(containerId);
        el.innerHTML =
            '<div class="grading-form">' +
            '<div class="submission-input-group">' +
            '<label>Submission Name<input type="text" id="sub-name" ' +
            'placeholder="Student name or ID"></label>' +
            '<label>Paste Content<textarea id="sub-content" rows="5" ' +
            'placeholder="Paste submission text here..."></textarea></label>' +
            '<button type="button" class="btn btn-sm" id="btn-add-sub">Add</button></div>' +
            '<div class="submission-upload"><label>Or upload files' +
            '<input type="file" id="sub-files" ' +
            'accept=".txt,.pdf,.md,.py,.java,.c,.js,.ts,.html,.css" multiple>' +
            '</label></div>' +
            '<h3>Queued Submissions (<span id="sub-count">0</span>)</h3>' +
            '<div id="submission-list"></div>' +
            '<div class="grading-actions">' +
            '<button type="button" class="btn" id="btn-sub-back">Back to Rubric</button>' +
            '<button type="button" class="btn btn-primary" id="btn-grade-all">' +
            'Grade All</button></div></div>';

        document.getElementById('btn-add-sub').addEventListener('click', function() {
            var name = document.getElementById('sub-name').value.trim();
            var content = document.getElementById('sub-content').value.trim();
            if (!name || !content) { alert('Enter both a name and content.'); return; }
            Dashboard.grading.addSubmission(name, content);
            document.getElementById('sub-name').value = '';
            document.getElementById('sub-content').value = '';
        });
        document.getElementById('sub-files').addEventListener('change', function(e) {
            var files = e.target.files;
            for (var i = 0; i < files.length; i++) {
                (function(f) {
                    var reader = new FileReader();
                    reader.onload = function(ev) {
                        Dashboard.grading.addSubmission(f.name, ev.target.result);
                    };
                    reader.readAsText(f);
                })(files[i]);
            }
            e.target.value = '';
        });
        document.getElementById('btn-sub-back').addEventListener('click', function() {
            Dashboard.grading.setStep(1);
        });
        document.getElementById('btn-grade-all').addEventListener('click', function() {
            if (state.submissions.length === 0) { alert('Add at least one submission.'); return; }
            Dashboard.grading.gradeAll();
        });
        renderSubmissionList(state);
    }

    function renderSubmissionList(state) {
        var list = document.getElementById('submission-list');
        var count = document.getElementById('sub-count');
        if (!list) return;
        list.innerHTML = '';
        if (count) count.textContent = state.submissions.length;
        state.submissions.forEach(function(sub) {
            var row = document.createElement('div');
            row.className = 'submission-row';
            var preview = sub.content.length > 80
                ? sub.content.substring(0, 80) + '...' : sub.content;
            row.innerHTML =
                '<span class="submission-name">' + esc(sub.name) + '</span>' +
                '<span class="submission-preview">' + esc(preview) + '</span>' +
                '<button type="button" class="btn btn-sm btn-danger btn-remove-sub">' +
                'Remove</button>';
            row.querySelector('.btn-remove-sub').addEventListener('click', function() {
                Dashboard.grading.removeSubmission(sub.id);
            });
            list.appendChild(row);
        });
    }

    /* --- Results Panel (Panel 3) --- */
    function buildResultsPanel(containerId, state) {
        var el = document.getElementById(containerId);
        el.innerHTML =
            '<div class="grading-form"><div class="results-toolbar">' +
            '<button type="button" class="btn btn-sm" id="btn-export-csv">Export CSV</button>' +
            '<button type="button" class="btn" id="btn-results-back">New Grading</button>' +
            '</div><div id="results-list"></div></div>';
        document.getElementById('btn-export-csv').addEventListener('click', function() {
            exportCSV(state);
        });
        document.getElementById('btn-results-back').addEventListener('click', function() {
            Dashboard.grading.setStep(1);
        });
        renderResults(state);
    }

    function renderResults(state) {
        var list = document.getElementById('results-list');
        if (!list) return;
        list.innerHTML = '';
        state.submissions.forEach(function(sub) {
            var card = document.createElement('div');
            card.className = 'result-card';
            card.setAttribute('data-sub-id', sub.id);
            if (sub.status === 'grading') {
                card.innerHTML = '<div class="result-header">' +
                    '<span class="result-name">' + esc(sub.name) + '</span>' +
                    '<span class="spinner"></span></div>';
            } else if (sub.status === 'error') {
                card.innerHTML = '<div class="result-header result-error">' +
                    '<span class="result-name">' + esc(sub.name) + '</span>' +
                    '<span class="result-status">Error</span></div>' +
                    '<p class="result-error-msg">' + esc(sub.error || 'Unknown error') + '</p>' +
                    '<button type="button" class="btn btn-sm btn-retry">Retry</button>';
                card.querySelector('.btn-retry').addEventListener('click', function() {
                    Dashboard.grading.retrySubmission(sub.id);
                });
            } else if (sub.status === 'done' && sub.result) {
                var r = sub.result;
                var pct = r.percentage;
                var cls = pct >= 80 ? 'score-good' : pct >= 50 ? 'score-ok' : 'score-bad';
                var ch = '';
                for (var i = 0; i < r.criteria.length; i++) {
                    var c = r.criteria[i];
                    ch += '<div class="criteria-detail-row">' +
                        '<span class="cd-name">' + esc(c.name) + '</span>' +
                        '<span class="cd-score">' + c.score + '/' + c.maxScore + '</span>' +
                        '<p class="cd-feedback">' + esc(c.feedback || '') + '</p></div>';
                }
                card.innerHTML = '<div class="result-header">' +
                    '<span class="result-name">' + esc(sub.name) + '</span>' +
                    '<span class="result-score ' + cls + '">' + r.totalScore + '/' +
                    r.totalMax + ' (' + pct + '%)</span></div>' +
                    '<p class="result-feedback">' + esc(r.overallFeedback || '') + '</p>' +
                    '<details class="criteria-detail">' +
                    '<summary>Per-criterion breakdown</summary>' + ch + '</details>';
            } else {
                card.innerHTML = '<div class="result-header">' +
                    '<span class="result-name">' + esc(sub.name) + '</span>' +
                    '<span class="result-status">Pending</span></div>';
            }
            list.appendChild(card);
        });
    }

    function exportCSV(state) {
        var rows = [['Name', 'Score', 'Max', 'Percentage', 'Feedback']];
        state.submissions.forEach(function(sub) {
            if (sub.status === 'done' && sub.result) {
                var r = sub.result;
                rows.push([sub.name, r.totalScore, r.totalMax, r.percentage + '%',
                    (r.overallFeedback || '').replace(/"/g, '""')]);
            }
        });
        var csv = rows.map(function(row) {
            return row.map(function(cell) {
                return '"' + String(cell).replace(/"/g, '""') + '"';
            }).join(',');
        }).join('\n');
        var blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = 'grading-results.csv';
        a.click();
        URL.revokeObjectURL(url);
    }

    return {
        buildRubricEditor: buildRubricEditor,
        renderCriteria: renderCriteria,
        buildSubmissionPanel: buildSubmissionPanel,
        renderSubmissionList: renderSubmissionList,
        buildResultsPanel: buildResultsPanel,
        renderResults: renderResults
    };
})();
