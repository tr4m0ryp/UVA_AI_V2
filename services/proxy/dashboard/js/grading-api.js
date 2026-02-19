/* Grading API layer: prompt construction, /v1/chat/completions calls, JSON parsing */
var Dashboard = Dashboard || {};

Dashboard.gradingAPI = (function() {
    var controllers = [];

    function buildSystemPrompt(state) {
        var prompt =
            'You are a strict academic grader. Grade the following student submission according to ' +
            'the assignment and rubric below. You MUST respond with ONLY valid JSON, no markdown, ' +
            'no code fences.\n\n' +
            'ASSIGNMENT:\n' + (state.assignment.text || '(not provided)') + '\n\n' +
            'RUBRIC:\n' + state.rubric.text;

        if (state.exampleAnswer.text) {
            prompt += '\n\nEXAMPLE ANSWER:\n' + state.exampleAnswer.text;
        }

        prompt +=
            '\n\nIdentify the grading criteria from the rubric. For each criterion assign a score.\n\n' +
            'Required JSON format:\n' +
            '{\n' +
            '  "criteria": [\n' +
            '    {"name": "criterion name", "score": <number>, ' +
            '"maxScore": <number>, "feedback": "brief feedback"}\n' +
            '  ],\n' +
            '  "overallFeedback": "1-3 sentence summary"\n' +
            '}\n\n' +
            'Rules:\n' +
            '- Extract criteria and their point values directly from the rubric\n' +
            '- Score each criterion from 0 to its maxScore\n' +
            '- Be fair but rigorous\n' +
            '- Do NOT include any text outside the JSON object';

        return prompt;
    }

    function grade(state, submission, onResult, onError) {
        var controller = new AbortController();
        controllers.push(controller);

        var systemMsg = buildSystemPrompt(state);
        var userMsg = 'Student: ' + submission.name + ' (ID: ' + submission.studentId + ')\n\n' +
            '--- BEGIN SUBMISSION ---\n' + submission.content + '\n--- END SUBMISSION ---';

        var body = {
            model: 'gpt-5.1',
            messages: [
                { role: 'system', content: systemMsg },
                { role: 'user', content: userMsg }
            ],
            temperature: 0.2
        };

        var token = Dashboard.api.getToken();
        var headers = { 'Content-Type': 'application/json' };
        if (token) headers['Authorization'] = 'Bearer ' + token;

        fetch('/v1/chat/completions', {
            method: 'POST',
            headers: headers,
            body: JSON.stringify(body),
            signal: controller.signal
        })
        .then(function(res) {
            if (!res.ok) {
                return res.json().then(function(data) {
                    var msg = (data.error && data.error.message)
                        ? data.error.message : 'Grading request failed';
                    throw new Error(msg);
                });
            }
            return res.json();
        })
        .then(function(data) {
            removeController(controller);
            var content = data.choices && data.choices[0] &&
                data.choices[0].message && data.choices[0].message.content;
            if (!content) {
                onError(submission, new Error('Empty response from model'));
                return;
            }
            var parsed = parseGradingResponse(content);
            if (parsed.error) {
                onError(submission, new Error(parsed.error));
            } else {
                onResult(submission, parsed);
            }
        })
        .catch(function(err) {
            removeController(controller);
            if (err.name === 'AbortError') return;
            onError(submission, err);
        });
    }

    function parseGradingResponse(content) {
        var cleaned = content.replace(/^```(?:json)?\s*/i, '').replace(/\s*```\s*$/, '');
        cleaned = cleaned.trim();

        try {
            var obj = JSON.parse(cleaned);
        } catch (e) {
            return { error: 'Failed to parse grading response as JSON' };
        }

        if (!obj.criteria || !Array.isArray(obj.criteria)) {
            return { error: 'Response missing "criteria" array' };
        }

        var totalScore = 0;
        var totalMax = 0;
        for (var i = 0; i < obj.criteria.length; i++) {
            var c = obj.criteria[i];
            if (typeof c.score !== 'number' || typeof c.maxScore !== 'number') {
                return { error: 'Criterion missing score or maxScore' };
            }
            totalScore += c.score;
            totalMax += c.maxScore;
        }

        obj.totalScore = totalScore;
        obj.totalMax = totalMax;
        obj.percentage = totalMax > 0 ? Math.round((totalScore / totalMax) * 100) : 0;

        return obj;
    }

    function removeController(ctrl) {
        var idx = controllers.indexOf(ctrl);
        if (idx !== -1) controllers.splice(idx, 1);
    }

    function cancelAll() {
        for (var i = 0; i < controllers.length; i++) {
            controllers[i].abort();
        }
        controllers = [];
    }

    return {
        grade: grade,
        cancelAll: cancelAll,
        parseGradingResponse: parseGradingResponse
    };
})();
