/* Grading UI: DOM builders for assignment setup, submissions, and results panels */
var Dashboard = Dashboard || {};

Dashboard.gradingUI = (function() {
    var ACCEPTED_SETUP = '.txt,.md,.tex,.pdf,.py,.java,.c,.js,.ts,.html,.css';
    var ACCEPTED_SUB   = '.txt,.md,.py,.java,.c,.js,.ts,.html,.css';

    function esc(str) {
        var d = document.createElement('div');
        d.textContent = str;
        return d.innerHTML;
    }

    /* Attach file/text toggle logic to one assignment section.
       sectionKey: 'assignment' | 'rubric' | 'exampleAnswer'
       setTextFn, setFileFn, clearFileFn: grading module methods
       stateRef: state[sectionKey] object { text, fileName }
    */
    function attachSectionHandlers(sectionKey, stateRef, setTextFn, setFileFn, clearFileFn) {
        var fileInput  = document.getElementById('section-file-' + sectionKey);
        var textarea   = document.getElementById('section-text-' + sectionKey);
        var badge      = document.getElementById('section-badge-' + sectionKey);
        var clearBtn   = document.getElementById('section-clear-' + sectionKey);

        /* Restore state: if a file was already loaded show badge */
        if (stateRef.fileName) {
            textarea.classList.add('hidden');
            badge.classList.remove('hidden');
            badge.querySelector('.file-badge-name').textContent = stateRef.fileName;
        } else if (stateRef.text) {
            textarea.value = stateRef.text;
        }

        textarea.addEventListener('input', function() {
            setTextFn(this.value);
        });

        fileInput.addEventListener('change', function(e) {
            var file = e.target.files[0];
            if (!file) return;
            var reader = new FileReader();
            reader.onload = function(ev) {
                setFileFn(file.name, ev.target.result);
                textarea.classList.add('hidden');
                badge.classList.remove('hidden');
                badge.querySelector('.file-badge-name').textContent = file.name;
            };
            reader.readAsText(file);
            e.target.value = '';
        });

        clearBtn.addEventListener('click', function() {
            clearFileFn();
            badge.classList.add('hidden');
            textarea.classList.remove('hidden');
            textarea.value = '';
        });
    }

    function buildSection(sectionKey, labelText, optional) {
        var optTag = optional ? ' <span class="section-optional">(optional)</span>' : '';
        return '<div class="assignment-section">' +
            '<div class="section-header">' +
            '<span class="section-label">' + labelText + optTag + '</span>' +
            '<label class="file-input-group">' +
            '<span class="btn btn-sm">Upload file</span>' +
            '<input type="file" id="section-file-' + sectionKey + '" ' +
            'accept="' + ACCEPTED_SETUP + '" class="hidden-file-input">' +
            '</label></div>' +
            '<textarea id="section-text-' + sectionKey + '" rows="5" ' +
            'class="section-textarea" ' +
            'placeholder="Paste ' + labelText.toLowerCase() + ' text here..."></textarea>' +
            '<div id="section-badge-' + sectionKey + '" class="file-badge hidden">' +
            '<span class="material-symbols-outlined file-badge-icon">description</span>' +
            '<span class="file-badge-name"></span>' +
            '<button type="button" class="file-clear-btn" id="section-clear-' + sectionKey + '">' +
            '<span class="material-symbols-outlined">close</span></button>' +
            '</div></div>';
    }

    /* --- Assignment Setup Panel (Panel 1) --- */
    function buildAssignmentPanel(containerId, state) {
        var el = document.getElementById(containerId);
        el.innerHTML =
            '<div class="grading-form">' +
            buildSection('assignment', 'Assignment', false) +
            buildSection('rubric', 'Rubric', false) +
            buildSection('exampleAnswer', 'Example Answer', true) +
            '<div class="grading-actions">' +
            '<button type="button" class="btn btn-primary" id="btn-setup-next">' +
            'Next: Add Submissions</button></div></div>';

        attachSectionHandlers('assignment', state.assignment,
            Dashboard.grading.setAssignmentText,
            Dashboard.grading.setAssignmentFile,
            Dashboard.grading.clearAssignmentFile);

        attachSectionHandlers('rubric', state.rubric,
            Dashboard.grading.setRubricText,
            Dashboard.grading.setRubricFile,
            Dashboard.grading.clearRubricFile);

        attachSectionHandlers('exampleAnswer', state.exampleAnswer,
            Dashboard.grading.setExampleText,
            Dashboard.grading.setExampleFile,
            Dashboard.grading.clearExampleFile);

        document.getElementById('btn-setup-next').addEventListener('click', function() {
            if (!state.rubric.text.trim()) {
                alert('The Rubric field is required.');
                return;
            }
            Dashboard.grading.setStep(2);
        });
    }

    /* --- Submission Panel (Panel 2) --- */
    function buildSubmissionPanel(containerId, state) {
        var el = document.getElementById(containerId);
        el.innerHTML =
            '<div class="grading-form">' +
            '<div class="submission-input-group">' +
            '<label>Student Name<input type="text" id="sub-name" placeholder="Full name"></label>' +
            '<label>Student ID<input type="text" id="sub-studentid" placeholder="Student number"></label>' +
            '<label class="file-input-group submission-file-label">' +
            '<span class="btn btn-sm">Choose file</span>' +
            '<span id="sub-file-name" class="sub-file-name">No file selected</span>' +
            '<input type="file" id="sub-file" accept="' + ACCEPTED_SUB + '" ' +
            'class="hidden-file-input">' +
            '</label>' +
            '<button type="button" class="btn btn-sm" id="btn-add-sub">Add Submission</button>' +
            '</div>' +
            '<h3>Queued Submissions (<span id="sub-count">0</span>)</h3>' +
            '<div id="submission-list"></div>' +
            '<div class="grading-actions">' +
            '<button type="button" class="btn" id="btn-sub-back">Back to Setup</button>' +
            '<button type="button" class="btn btn-primary" id="btn-grade-all">' +
            'Grade All</button></div></div>';

        /* Track pending file for the add form */
        var pendingFile = null;

        document.getElementById('sub-file').addEventListener('change', function(e) {
            var file = e.target.files[0];
            if (!file) return;
            pendingFile = file;
            document.getElementById('sub-file-name').textContent = file.name;
        });

        document.getElementById('btn-add-sub').addEventListener('click', function() {
            var name = document.getElementById('sub-name').value.trim();
            var sid  = document.getElementById('sub-studentid').value.trim();
            if (!name) { alert('Student Name is required.'); return; }
            if (!sid)  { alert('Student ID is required.'); return; }
            if (!pendingFile) { alert('Please choose a submission file.'); return; }

            var capturedFile = pendingFile;
            var capturedName = name;
            var capturedSid  = sid;

            var reader = new FileReader();
            reader.onload = function(ev) {
                Dashboard.grading.addSubmission(capturedName, capturedSid,
                    capturedFile.name, ev.target.result);
            };
            reader.readAsText(capturedFile);

            /* Reset form */
            document.getElementById('sub-name').value = '';
            document.getElementById('sub-studentid').value = '';
            document.getElementById('sub-file').value = '';
            document.getElementById('sub-file-name').textContent = 'No file selected';
            pendingFile = null;
        });

        document.getElementById('btn-sub-back').addEventListener('click', function() {
            Dashboard.grading.setStep(1);
        });
        document.getElementById('btn-grade-all').addEventListener('click', function() {
            if (state.submissions.length === 0) {
                alert('Add at least one submission.');
                return;
            }
            Dashboard.grading.gradeAll();
        });
        renderSubmissionList(state);
    }

    function renderSubmissionList(state) {
        var list  = document.getElementById('submission-list');
        var count = document.getElementById('sub-count');
        if (!list) return;
        list.innerHTML = '';
        if (count) count.textContent = state.submissions.length;
        state.submissions.forEach(function(sub) {
            var row = document.createElement('div');
            row.className = 'submission-row';
            row.innerHTML =
                '<span class="submission-name">' + esc(sub.name) + '</span>' +
                '<span class="submission-studentid">' + esc(sub.studentId) + '</span>' +
                '<span class="submission-filename">' + esc(sub.fileName) + '</span>' +
                '<button type="button" class="btn btn-sm btn-danger btn-remove-sub">Remove</button>';
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

            var headerName = esc(sub.name) + ' <span class="result-studentid">(ID: ' +
                esc(sub.studentId) + ')</span>';

            if (sub.status === 'grading') {
                card.innerHTML = '<div class="result-header">' +
                    '<span class="result-name">' + headerName + '</span>' +
                    '<span class="spinner"></span></div>';
            } else if (sub.status === 'error') {
                card.innerHTML = '<div class="result-header result-error">' +
                    '<span class="result-name">' + headerName + '</span>' +
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
                    '<span class="result-name">' + headerName + '</span>' +
                    '<span class="result-score ' + cls + '">' + r.totalScore + '/' +
                    r.totalMax + ' (' + pct + '%)</span></div>' +
                    '<p class="result-feedback">' + esc(r.overallFeedback || '') + '</p>' +
                    '<details class="criteria-detail">' +
                    '<summary>Per-criterion breakdown</summary>' + ch + '</details>';
            } else {
                card.innerHTML = '<div class="result-header">' +
                    '<span class="result-name">' + headerName + '</span>' +
                    '<span class="result-status">Pending</span></div>';
            }
            list.appendChild(card);
        });
    }

    function exportCSV(state) {
        var rows = [['Name', 'StudentID', 'Score', 'Max', 'Percentage', 'Feedback']];
        state.submissions.forEach(function(sub) {
            if (sub.status === 'done' && sub.result) {
                var r = sub.result;
                rows.push([
                    sub.name,
                    sub.studentId,
                    r.totalScore,
                    r.totalMax,
                    r.percentage + '%',
                    (r.overallFeedback || '').replace(/"/g, '""')
                ]);
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
        buildAssignmentPanel: buildAssignmentPanel,
        buildSubmissionPanel: buildSubmissionPanel,
        renderSubmissionList: renderSubmissionList,
        buildResultsPanel: buildResultsPanel,
        renderResults: renderResults
    };
})();
