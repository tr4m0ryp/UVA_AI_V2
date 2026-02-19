/* Grading UI: DOM builders for assignment setup, submissions, and results panels */
var Dashboard = Dashboard || {};

Dashboard.gradingUI = (function() {
    var ACCEPTED_SETUP = '.txt,.md,.tex,.pdf,.py,.java,.c,.js,.ts,.html,.css';
    var ACCEPTED_SUB   = '.txt,.md,.pdf,.py,.java,.c,.js,.ts,.html,.css';

    function esc(str) {
        var d = document.createElement('div');
        d.textContent = str;
        return d.innerHTML;
    }

    /* Extract text from a PDF file using pdf.js. Returns a Promise<string>. */
    function extractPdfText(file) {
        return new Promise(function(resolve, reject) {
            var reader = new FileReader();
            reader.onload = function(ev) {
                var typedArray = new Uint8Array(ev.target.result);
                pdfjsLib.getDocument({ data: typedArray }).promise.then(function(pdf) {
                    var pages = [];
                    var pending = pdf.numPages;
                    if (pending === 0) { resolve(''); return; }
                    for (var i = 1; i <= pdf.numPages; i++) {
                        (function(pageNum) {
                            pdf.getPage(pageNum).then(function(page) {
                                page.getTextContent().then(function(tc) {
                                    var text = tc.items.map(function(item) {
                                        return item.str;
                                    }).join(' ');
                                    pages[pageNum - 1] = text;
                                    pending--;
                                    if (pending === 0) {
                                        resolve(pages.join('\n\n'));
                                    }
                                });
                            });
                        })(i);
                    }
                }).catch(function(err) {
                    reject(new Error('Failed to read PDF: ' + err.message));
                });
            };
            reader.readAsArrayBuffer(file);
        });
    }

    /* Read a file as text, auto-detecting PDFs for extraction. Returns Promise<string>. */
    function readFileAsText(file) {
        if (file.name.toLowerCase().endsWith('.pdf')) {
            return extractPdfText(file);
        }
        return new Promise(function(resolve) {
            var reader = new FileReader();
            reader.onload = function(ev) { resolve(ev.target.result); };
            reader.readAsText(file);
        });
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
            readFileAsText(file).then(function(text) {
                setFileFn(file.name, text);
                textarea.classList.add('hidden');
                badge.classList.remove('hidden');
                badge.querySelector('.file-badge-name').textContent = file.name;
            }).catch(function(err) {
                alert('Could not read file: ' + err.message);
            });
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
        var reqTag = optional
            ? ' <span class="section-optional">(optional)</span>'
            : ' <span class="section-required">*</span>';
        return '<div class="assignment-section">' +
            '<div class="section-header">' +
            '<span class="section-label">' + labelText + reqTag + '</span>' +
            '<label class="file-input-group">' +
            '<span class="btn btn-sm">' +
            '<span class="material-symbols-outlined" style="font-size:16px;vertical-align:middle;margin-right:4px">upload_file</span>' +
            'Upload</span>' +
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
            '<div class="grading-card">' +
            '<div class="grading-card-header">' +
            '<div class="grading-card-title">Assignment Setup</div>' +
            '<div class="grading-card-desc">' +
            'Provide the assignment description, grading rubric, and optionally an example answer. ' +
            'You can paste text directly or upload a file for each section.</div>' +
            '</div>' +
            buildSection('assignment', 'Assignment', false) +
            buildSection('rubric', 'Rubric', false) +
            buildSection('exampleAnswer', 'Example Answer', true) +
            '<div class="grading-actions">' +
            '<button type="button" class="btn btn-primary" id="btn-setup-next">' +
            'Next: Add Submissions' +
            '<span class="material-symbols-outlined" style="font-size:18px;vertical-align:middle;margin-left:6px">arrow_forward</span>' +
            '</button></div>' +
            '</div></div>';

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
            '<div class="grading-card">' +
            '<div class="grading-card-header">' +
            '<div class="grading-card-title">Student Submissions</div>' +
            '<div class="grading-card-desc">' +
            'Add each student\'s submission with their name, ID, and file. ' +
            'All queued submissions will be graded against your rubric.</div>' +
            '</div>' +
            '<div class="submission-input-group">' +
            '<div class="submission-field-row">' +
            '<label>Student Name <span class="section-required">*</span>' +
            '<input type="text" id="sub-name" placeholder="Full name"></label>' +
            '<label>Student ID <span class="section-required">*</span>' +
            '<input type="text" id="sub-studentid" placeholder="Student number"></label>' +
            '</div>' +
            '<div class="submission-file-row">' +
            '<label class="file-input-group submission-file-label">' +
            '<span class="btn btn-sm">' +
            '<span class="material-symbols-outlined" style="font-size:16px;vertical-align:middle;margin-right:4px">upload_file</span>' +
            'Choose file</span>' +
            '<span id="sub-file-name" class="sub-file-name">No file selected</span>' +
            '<input type="file" id="sub-file" accept="' + ACCEPTED_SUB + '" ' +
            'class="hidden-file-input">' +
            '</label>' +
            '<button type="button" class="btn btn-primary btn-sm" id="btn-add-sub">' +
            '<span class="material-symbols-outlined" style="font-size:16px;vertical-align:middle;margin-right:4px">add</span>' +
            'Add</button>' +
            '</div>' +
            '</div>' +
            '<div class="submission-queue-header">' +
            '<span class="submission-queue-title">Queued Submissions</span>' +
            '<span class="submission-queue-count" id="sub-count">0</span>' +
            '</div>' +
            '<div id="submission-list"></div>' +
            '<div class="grading-actions">' +
            '<button type="button" class="btn" id="btn-sub-back">' +
            '<span class="material-symbols-outlined" style="font-size:18px;vertical-align:middle;margin-right:4px">arrow_back</span>' +
            'Back to Setup</button>' +
            '<button type="button" class="btn btn-primary" id="btn-grade-all">' +
            '<span class="material-symbols-outlined" style="font-size:18px;vertical-align:middle;margin-right:4px">grading</span>' +
            'Grade All</button>' +
            '</div></div></div>';

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

            readFileAsText(capturedFile).then(function(text) {
                Dashboard.grading.addSubmission(capturedName, capturedSid,
                    capturedFile.name, text);
            }).catch(function(err) {
                alert('Could not read file: ' + err.message);
            });

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

        if (state.submissions.length === 0) {
            list.innerHTML = '<div class="submission-empty">' +
                'No submissions added yet. Use the form above to add student work.</div>';
            return;
        }

        state.submissions.forEach(function(sub) {
            var row = document.createElement('div');
            row.className = 'submission-row';
            row.innerHTML =
                '<span class="submission-name">' + esc(sub.name) + '</span>' +
                '<span class="submission-studentid">' + esc(sub.studentId) + '</span>' +
                '<span class="submission-filename">' + esc(sub.fileName) + '</span>' +
                '<button type="button" class="btn-icon danger btn-remove-sub" title="Remove submission">' +
                '<span class="material-symbols-outlined">close</span></button>';
            row.querySelector('.btn-remove-sub').addEventListener('click', function() {
                Dashboard.grading.removeSubmission(sub.id);
            });
            list.appendChild(row);
        });
    }

    /* --- Results Panel (Panel 3) --- */
    function buildResultsPanel(containerId, state) {
        var el = document.getElementById(containerId);

        /* Count completed results for summary */
        var doneCount = 0;
        var totalScore = 0;
        state.submissions.forEach(function(sub) {
            if (sub.status === 'done' && sub.result) {
                doneCount++;
                totalScore += sub.result.percentage;
            }
        });
        var avgText = doneCount > 0
            ? '<strong>' + Math.round(totalScore / doneCount) + '%</strong> average across <strong>' + doneCount + '</strong> submissions'
            : 'Grading in progress...';

        el.innerHTML =
            '<div class="grading-form">' +
            '<div class="results-toolbar">' +
            '<div class="results-summary">' + avgText + '</div>' +
            '<div class="results-toolbar-actions">' +
            '<button type="button" class="btn btn-sm" id="btn-export-csv">' +
            '<span class="material-symbols-outlined" style="font-size:16px;vertical-align:middle;margin-right:4px">download</span>' +
            'Export CSV</button>' +
            '<button type="button" class="btn" id="btn-results-back">' +
            '<span class="material-symbols-outlined" style="font-size:16px;vertical-align:middle;margin-right:4px">restart_alt</span>' +
            'New Grading</button>' +
            '<button type="button" class="btn" id="btn-back-sessions">' +
            '<span class="material-symbols-outlined" style="font-size:16px;vertical-align:middle;margin-right:4px">history</span>' +
            'Back to Sessions</button>' +
            '</div></div>' +
            '<div id="results-list"></div></div>';

        document.getElementById('btn-export-csv').addEventListener('click', function() {
            exportCSV(state);
        });
        document.getElementById('btn-results-back').addEventListener('click', function() {
            Dashboard.grading.startNew();
        });
        document.getElementById('btn-back-sessions').addEventListener('click', function() {
            Dashboard.grading.backToHistory();
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

            var headerName = esc(sub.name) +
                ' <span class="result-studentid">' + esc(sub.studentId) + '</span>';

            if (sub.status === 'grading') {
                card.innerHTML = '<div class="result-header">' +
                    '<span class="result-name">' + headerName + '</span>' +
                    '<span class="result-status">' +
                    '<div class="grading-spinner">' +
                    '<span class="grading-spinner-dot"></span>' +
                    '<span class="grading-spinner-dot"></span>' +
                    '<span class="grading-spinner-dot"></span>' +
                    '</div>Grading</span></div>';
            } else if (sub.status === 'error') {
                card.innerHTML = '<div class="result-header result-error">' +
                    '<span class="result-name">' + headerName + '</span>' +
                    '<span class="result-status">Failed</span></div>' +
                    '<p class="result-error-msg">' + esc(sub.error || 'Unknown error') + '</p>' +
                    '<button type="button" class="btn btn-sm btn-retry">' +
                    '<span class="material-symbols-outlined" style="font-size:16px;vertical-align:middle;margin-right:4px">refresh</span>' +
                    'Retry</button>';
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
                    '<span class="result-score ' + cls + '">' + r.totalScore + '/' +
                    r.totalMax + ' (' + pct + '%)</span></div>' +
                    '<div class="result-feedback">' + esc(r.overallFeedback || '') + '</div>' +
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
