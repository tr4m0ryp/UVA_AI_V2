/* Grading orchestrator: state machine for the 3-step grading workflow */
var Dashboard = Dashboard || {};

Dashboard.grading = (function() {
    var state = {
        step: 1,
        assignment:    { text: '', fileName: '' },
        rubric:        { text: '', fileName: '' },
        exampleAnswer: { text: '', fileName: '' },
        submissions: [],
        nextSubmissionId: 1,
        saved: false
    };

    var built = false;

    function mount() {
        Dashboard.gradingHistory.mount();
        Dashboard.gradingHistory.showHistory();
    }

    function startNew() {
        state.step = 1;
        state.assignment    = { text: '', fileName: '' };
        state.rubric        = { text: '', fileName: '' };
        state.exampleAnswer = { text: '', fileName: '' };
        state.submissions   = [];
        state.nextSubmissionId = 1;
        state.saved = false;

        if (!built) {
            buildAll();
            built = true;
        } else {
            buildAll();
        }
        showStep(1);
        Dashboard.gradingHistory.showWorkflow();
    }

    function showStoredResults(stored) {
        state.step = 3;
        state.assignment    = { text: '', fileName: stored.assignment || '' };
        state.rubric        = { text: '', fileName: stored.rubric || '' };
        state.exampleAnswer = { text: '', fileName: '' };
        state.saved = true;

        state.submissions = (stored.submissions || []).map(function(s, i) {
            return {
                id: i + 1,
                name: s.name || '',
                studentId: s.studentId || '',
                fileName: s.fileName || '',
                content: '',
                status: s.status || 'done',
                result: s.result || null,
                error: s.error || null
            };
        });
        state.nextSubmissionId = state.submissions.length + 1;

        if (!built) {
            buildAll();
            built = true;
        } else {
            Dashboard.gradingUI.buildResultsPanel('grading-panel-3', state);
        }
        setStep(3);
        Dashboard.gradingHistory.showWorkflow();
    }

    function unmount() {
        Dashboard.gradingAPI.cancelAll();
    }

    function buildAll() {
        Dashboard.gradingUI.buildAssignmentPanel('grading-panel-1', state);
        Dashboard.gradingUI.buildSubmissionPanel('grading-panel-2', state);
        Dashboard.gradingUI.buildResultsPanel('grading-panel-3', state);
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
        for (var i = 1; i <= 3; i++) {
            var panel = document.getElementById('grading-panel-' + i);
            if (panel) {
                panel.classList.toggle('hidden', i !== n);
            }
        }
        updateStepIndicators(n);
    }

    function updateStepIndicators(currentStep) {
        var steps = document.querySelectorAll('#grading-steps .step');
        for (var i = 0; i < steps.length; i++) {
            var stepNum = parseInt(steps[i].getAttribute('data-step'), 10);
            steps[i].classList.remove('active', 'completed');
            if (stepNum === currentStep) {
                steps[i].classList.add('active');
            } else if (stepNum < currentStep) {
                steps[i].classList.add('completed');
            }
        }
    }

    /* Assignment/rubric/example text setters */
    function setAssignmentText(text) {
        state.assignment.text = text;
    }

    function setAssignmentFile(fileName, text) {
        state.assignment.fileName = fileName;
        state.assignment.text = text;
    }

    function clearAssignmentFile() {
        state.assignment.fileName = '';
        state.assignment.text = '';
    }

    function setRubricText(text) {
        state.rubric.text = text;
    }

    function setRubricFile(fileName, text) {
        state.rubric.fileName = fileName;
        state.rubric.text = text;
    }

    function clearRubricFile() {
        state.rubric.fileName = '';
        state.rubric.text = '';
    }

    function setExampleText(text) {
        state.exampleAnswer.text = text;
    }

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
    }

    function onGradeResult(submission, result) {
        var sub = findSubmission(submission.id);
        if (!sub) return;
        sub.status = 'done';
        sub.result = result;
        Dashboard.gradingUI.renderResults(state);
        _maybeSaveSession();
    }

    function onGradeError(submission, err) {
        var sub = findSubmission(submission.id);
        if (!sub) return;
        sub.status = 'error';
        sub.error = err.message || 'Unknown error';
        Dashboard.gradingUI.renderResults(state);
        _maybeSaveSession();
    }

    function _maybeSaveSession() {
        if (state.saved) return;
        var allDone = state.submissions.every(function(s) {
            return s.status === 'done' || s.status === 'error';
        });
        if (allDone && state.submissions.length > 0) {
            _saveSession();
        }
    }

    function _saveSession() {
        state.saved = true;
        var title = (state.assignment.fileName || 'Assignment') +
                    ' - ' + new Date().toISOString().slice(0, 10);
        var results = {
            assignment: state.assignment.fileName,
            rubric: state.rubric.fileName,
            submissions: state.submissions.map(function(s) {
                return {
                    name: s.name,
                    studentId: s.studentId,
                    fileName: s.fileName,
                    status: s.status,
                    result: s.result,
                    error: s.error
                };
            })
        };
        Dashboard.api.post('/api/dashboard/grading', {
            title: title,
            submission_count: state.submissions.length,
            results_json: JSON.stringify(results)
        }).then(function() {
            Dashboard.gradingHistory.load();
        }).catch(function(err) {
            console.error('save session:', err);
        });
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

    function getState() {
        return state;
    }

    return {
        mount: mount,
        unmount: unmount,
        startNew: startNew,
        showStoredResults: showStoredResults,
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
        getState: getState
    };
})();
