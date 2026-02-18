/* Grading orchestrator: state machine for the 3-step grading workflow */
var Dashboard = Dashboard || {};

Dashboard.grading = (function() {
    var state = {
        step: 1,
        rubric: {
            model: 'claude-sonnet-4.5',
            criteria: [],
            totalPoints: 0
        },
        submissions: [],
        nextSubmissionId: 1
    };

    var built = false;

    function mount() {
        if (!built) {
            buildAll();
            built = true;
        }
        showStep(state.step);
    }

    function unmount() {
        Dashboard.gradingAPI.cancelAll();
    }

    function buildAll() {
        Dashboard.gradingUI.buildRubricEditor('grading-panel-1', state);
        Dashboard.gradingUI.buildSubmissionPanel('grading-panel-2', state);
        Dashboard.gradingUI.buildResultsPanel('grading-panel-3', state);
    }

    function setStep(n) {
        if (n === 2) {
            /* Recalculate total points before leaving rubric */
            recalcTotalPoints();
        }
        state.step = n;
        showStep(n);

        /* Rebuild panel content when entering it */
        if (n === 1) {
            Dashboard.gradingUI.buildRubricEditor('grading-panel-1', state);
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

    function recalcTotalPoints() {
        var total = 0;
        for (var i = 0; i < state.rubric.criteria.length; i++) {
            total += state.rubric.criteria[i].maxPoints;
        }
        state.rubric.totalPoints = total;
    }

    /* Criterion operations */
    function addCriterion() {
        state.rubric.criteria.push({
            name: '',
            description: '',
            maxPoints: 10
        });
        Dashboard.gradingUI.renderCriteria(state);
    }

    function removeCriterion(idx) {
        state.rubric.criteria.splice(idx, 1);
        Dashboard.gradingUI.renderCriteria(state);
    }

    function updateCriterion(idx, field, value) {
        if (state.rubric.criteria[idx]) {
            state.rubric.criteria[idx][field] = value;
        }
    }

    /* Submission operations */
    function addSubmission(name, content) {
        state.submissions.push({
            id: state.nextSubmissionId++,
            name: name,
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

        /* Reset statuses for pending/error submissions */
        for (var i = 0; i < state.submissions.length; i++) {
            if (state.submissions[i].status !== 'done') {
                state.submissions[i].status = 'grading';
                state.submissions[i].result = null;
                state.submissions[i].error = null;
            }
        }

        Dashboard.gradingUI.renderResults(state);

        /* Fire off grading for each non-done submission */
        recalcTotalPoints();
        for (var j = 0; j < state.submissions.length; j++) {
            if (state.submissions[j].status === 'grading') {
                Dashboard.gradingAPI.grade(
                    state.rubric,
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
    }

    function onGradeError(submission, err) {
        var sub = findSubmission(submission.id);
        if (!sub) return;
        sub.status = 'error';
        sub.error = err.message || 'Unknown error';
        Dashboard.gradingUI.renderResults(state);
    }

    function retrySubmission(id) {
        var sub = findSubmission(id);
        if (!sub) return;
        sub.status = 'grading';
        sub.result = null;
        sub.error = null;
        Dashboard.gradingUI.renderResults(state);
        recalcTotalPoints();
        Dashboard.gradingAPI.grade(state.rubric, sub, onGradeResult, onGradeError);
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
        setStep: setStep,
        addCriterion: addCriterion,
        removeCriterion: removeCriterion,
        updateCriterion: updateCriterion,
        addSubmission: addSubmission,
        removeSubmission: removeSubmission,
        gradeAll: gradeAll,
        retrySubmission: retrySubmission,
        getState: getState
    };
})();
