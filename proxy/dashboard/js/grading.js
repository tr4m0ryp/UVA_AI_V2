/* Grading orchestrator: state machine for the 3-step grading workflow */
var Dashboard = Dashboard || {};

Dashboard.grading = (function() {
    var state = {
        step: 0,
        sessionId: null,
        assignment:    { text: '', fileName: '' },
        rubric:        { text: '', fileName: '' },
        exampleAnswer: { text: '', fileName: '' },
        submissions: [],
        nextSubmissionId: 1
    };

    var built = false;

    function mount() {
        if (!built) {
            buildAll();
            built = true;
        }
        showStep(0);
        Dashboard.gradingHistory.buildHistoryPanel('grading-history');
    }

    function unmount() {
        Dashboard.gradingAPI.cancelAll();
    }

    function buildAll() {
        Dashboard.gradingUI.buildAssignmentPanel('grading-panel-1', state);
        Dashboard.gradingUI.buildSubmissionPanel('grading-panel-2', state);
        Dashboard.gradingUI.buildResultsPanel('grading-panel-3', state);
    }

    function startNew() {
        resetState();
        setStep(1);
    }

    function resetState() {
        state.sessionId = null;
        state.assignment = { text: '', fileName: '' };
        state.rubric = { text: '', fileName: '' };
        state.exampleAnswer = { text: '', fileName: '' };
        state.submissions = [];
        state.nextSubmissionId = 1;
    }

    function setStep(n) {
        state.step = n;
        showStep(n);

        if (n === 1) {
            Dashboard.gradingUI.buildAssignmentPanel('grading-panel-1', state);
        } else if (n === 2) {
            Dashboard.gradingUI.buildSubmissionPanel('grading-panel-2', state);
        } else if (n === 3) {
            Dashboard.gradingUI.buildResultsPanel('grading-panel-3', state);
        }
    }

    function showStep(n) {
        /* History panel (step 0) */
        var historyPanel = document.getElementById('grading-history');
        var detailPanel = document.getElementById('grading-session-detail');
        var stepsEl = document.getElementById('grading-steps');

        if (historyPanel) historyPanel.classList.toggle('hidden', n !== 0);
        if (detailPanel) detailPanel.classList.toggle('hidden', n !== -1);
        if (stepsEl) stepsEl.classList.toggle('hidden', n <= 0);

        for (var i = 1; i <= 3; i++) {
            var panel = document.getElementById('grading-panel-' + i);
            if (panel) {
                panel.classList.toggle('hidden', i !== n);
            }
        }
        if (n >= 1) updateStepIndicators(n);
    }

    function updateStepIndicators(currentStep) {
        var steps = document.querySelectorAll('#grading-steps .step');
        for (var i = 0; i < steps.length; i++) {
            var stepNum = parseInt(steps[i].getAttribute('data-step'), 10);
            var numEl = steps[i].querySelector('.step-number');
            steps[i].classList.remove('active', 'completed');
            if (stepNum === currentStep) {
                steps[i].classList.add('active');
                numEl.textContent = stepNum;
            } else if (stepNum < currentStep) {
                steps[i].classList.add('completed');
                numEl.innerHTML = '<span class="material-symbols-outlined" style="font-size:18px">check</span>';
            } else {
                numEl.textContent = stepNum;
            }
        }
        var lines = document.querySelectorAll('#grading-steps .step-line');
        for (var j = 0; j < lines.length; j++) {
            lines[j].classList.toggle('done', j + 1 < currentStep);
        }
    }

    /* Assignment/rubric/example text setters */
    function setAssignmentText(text) { state.assignment.text = text; }
    function setAssignmentFile(fileName, text) {
        state.assignment.fileName = fileName;
        state.assignment.text = text;
    }
    function clearAssignmentFile() {
        state.assignment.fileName = '';
        state.assignment.text = '';
    }

    function setRubricText(text) { state.rubric.text = text; }
    function setRubricFile(fileName, text) {
        state.rubric.fileName = fileName;
        state.rubric.text = text;
    }
    function clearRubricFile() {
        state.rubric.fileName = '';
        state.rubric.text = '';
    }

    function setExampleText(text) { state.exampleAnswer.text = text; }
    function setExampleFile(fileName, text) {
        state.exampleAnswer.fileName = fileName;
        state.exampleAnswer.text = text;
    }
    function clearExampleFile() {
        state.exampleAnswer.fileName = '';
        state.exampleAnswer.text = '';
    }

    /* Submission operations */
    function addSubmission(name, studentId, fileName, content) {
        state.submissions.push({
            id: state.nextSubmissionId++,
            name: name,
            studentId: studentId,
            fileName: fileName,
            content: content,
            status: 'pending',
            result: null,
            error: null
        });
        Dashboard.gradingUI.renderSubmissionList(state);
    }

    function removeSubmission(id) {
        state.submissions = state.submissions.filter(function(s) {
            return s.id !== id;
        });
        Dashboard.gradingUI.renderSubmissionList(state);
    }

    /* Create session in backend, then start grading */
    function createSession(callback) {
        var sessionName = 'Grading ' + new Date().toLocaleDateString(undefined, {
            year: 'numeric', month: 'short', day: 'numeric',
            hour: '2-digit', minute: '2-digit'
        });

        Dashboard.api.post('/api/dashboard/grading/sessions', {
            name: sessionName,
            assignment_text: state.assignment.text,
            rubric_text: state.rubric.text,
            example_text: state.exampleAnswer.text
        }).then(function(resp) {
            state.sessionId = resp.id;
            if (callback) callback();
        }).catch(function(err) {
            console.error('Failed to create session:', err);
            if (callback) callback();
        });
    }

    /* Grading execution */
    function gradeAll() {
        setStep(3);

        for (var i = 0; i < state.submissions.length; i++) {
            if (state.submissions[i].status !== 'done') {
                state.submissions[i].status = 'grading';
                state.submissions[i].result = null;
                state.submissions[i].error = null;
            }
        }

        Dashboard.gradingUI.renderResults(state);

        /* Create session in backend, then fire off grading */
        createSession(function() {
            for (var j = 0; j < state.submissions.length; j++) {
                if (state.submissions[j].status === 'grading') {
                    Dashboard.gradingAPI.grade(
                        state,
                        state.submissions[j],
                        onGradeResult,
                        onGradeError
                    );
                }
            }
        });
    }

    function saveResultToBackend(submission) {
        if (!state.sessionId) return;

        var r = submission.result;
        var body = {
            student_name: submission.name,
            student_id_str: submission.studentId,
            file_name: submission.fileName || '',
            total_score: r ? r.totalScore : 0,
            total_max: r ? r.totalMax : 0,
            percentage: r ? r.percentage : 0,
            overall_feedback: r ? (r.overallFeedback || '') : '',
            criteria_json: r ? r.criteria : [],
            status: submission.status === 'done' ? 'done' : 'error',
            error_text: submission.error || ''
        };

        Dashboard.api.post(
            '/api/dashboard/grading/sessions/' + state.sessionId + '/results',
            body
        ).catch(function(err) {
            console.error('Failed to save result:', err);
        });
    }

    function checkAllDone() {
        var allDone = true;
        var doneCount = 0;
        var totalPct = 0;

        for (var i = 0; i < state.submissions.length; i++) {
            var s = state.submissions[i];
            if (s.status === 'grading' || s.status === 'pending') {
                allDone = false;
            }
            if (s.status === 'done' && s.result) {
                doneCount++;
                totalPct += s.result.percentage;
            }
        }

        if (allDone && state.sessionId) {
            var avgPct = doneCount > 0 ? totalPct / doneCount : 0;
            Dashboard.api.put(
                '/api/dashboard/grading/sessions/' + state.sessionId,
                {
                    submission_count: doneCount,
                    avg_percentage: Math.round(avgPct * 100) / 100,
                    status: 'completed'
                }
            ).catch(function(err) {
                console.error('Failed to finalize session:', err);
            });
        }
    }

    function onGradeResult(submission, result) {
        var sub = findSubmission(submission.id);
        if (!sub) return;
        sub.status = 'done';
        sub.result = result;
        Dashboard.gradingUI.renderResults(state);
        saveResultToBackend(sub);
        checkAllDone();
    }

    function onGradeError(submission, err) {
        var sub = findSubmission(submission.id);
        if (!sub) return;
        sub.status = 'error';
        sub.error = err.message || 'Unknown error';
        Dashboard.gradingUI.renderResults(state);
        saveResultToBackend(sub);
        checkAllDone();
    }

    function retrySubmission(id) {
        var sub = findSubmission(id);
        if (!sub) return;
        sub.status = 'grading';
        sub.result = null;
        sub.error = null;
        Dashboard.gradingUI.renderResults(state);
        Dashboard.gradingAPI.grade(state, sub, onGradeResult, onGradeError);
    }

    function findSubmission(id) {
        for (var i = 0; i < state.submissions.length; i++) {
            if (state.submissions[i].id === id) return state.submissions[i];
        }
        return null;
    }

    function viewSession(id) {
        Dashboard.api.get('/api/dashboard/grading/sessions/' + id)
            .then(function(session) {
                state.step = -1;
                showStep(-1);
                Dashboard.gradingHistory.buildSessionDetail(
                    'grading-session-detail', session);
            })
            .catch(function() {
                alert('Failed to load session.');
            });
    }

    function backToHistory() {
        showStep(0);
        Dashboard.gradingHistory.buildHistoryPanel('grading-history');
    }

    function getState() {
        return state;
    }

    return {
        mount: mount,
        unmount: unmount,
        startNew: startNew,
        setStep: setStep,
        setAssignmentText: setAssignmentText,
        setAssignmentFile: setAssignmentFile,
        clearAssignmentFile: clearAssignmentFile,
        setRubricText: setRubricText,
        setRubricFile: setRubricFile,
        clearRubricFile: clearRubricFile,
        setExampleText: setExampleText,
        setExampleFile: setExampleFile,
        clearExampleFile: clearExampleFile,
        addSubmission: addSubmission,
        removeSubmission: removeSubmission,
        gradeAll: gradeAll,
        retrySubmission: retrySubmission,
        viewSession: viewSession,
        backToHistory: backToHistory,
        getState: getState
    };
})();
