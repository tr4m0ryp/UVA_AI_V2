/* Grading API layer: prompt construction, /v1/chat/completions calls, JSON parsing */
var Dashboard = Dashboard || {};

Dashboard.gradingAPI = (function() {
    var controllers = [];

    function buildSystemPrompt(rubric) {
        var criteria = rubric.criteria.map(function(c) {
            return '- ' + c.name + ' (max ' + c.maxPoints + ' points): ' + c.description;
        }).join('\n');

        return 'You are a strict academic grader. Grade the following submission ' +
            'according to the rubric below. You MUST respond with ONLY valid JSON, ' +
            'no markdown, no explanation, no code fences.\n\n' +
            'Rubric (total: ' + rubric.totalPoints + ' points):\n' + criteria + '\n\n' +
            'Required JSON format:\n' +
            '{\n' +
            '  "criteria": [\n' +
            '    {"name": "criterion name", "score": <number>, ' +
            '"maxScore": <number>, "feedback": "brief feedback"}\n' +
            '  ],\n' +
            '  "overallFeedback": "1-3 sentence summary"\n' +
            '}\n\n' +
            'Rules:\n' +
            '- Score each criterion from 0 to its maxScore\n' +
            '- Be fair but rigorous\n' +
            '- Feedback should be specific and actionable\n' +
            '- Do NOT include any text outside the JSON object';
    }

    function grade(rubric, submission, onResult, onError) {
        var controller = new AbortController();
        controllers.push(controller);

        var systemMsg = buildSystemPrompt(rubric);
        var userMsg = 'Submission from: ' + submission.name + '\n\n' +
            '--- BEGIN SUBMISSION ---\n' + submission.content + '\n--- END SUBMISSION ---';

        var body = {
            model: rubric.model,
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
        /* Strip markdown code fences if present */
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

        /* Validate and compute total score */
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
