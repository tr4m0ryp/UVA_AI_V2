/* Grading API layer: prompt construction, /v1/chat/completions calls, JSON parsing */
var Dashboard = Dashboard || {};

Dashboard.gradingAPI = (function() {
    var controllers = [];

    function buildSystemPrompt(state) {
        var prompt =
            'You are a strict academic grader. Grade the following student submission according to ' +
            'the assignment and rubric below.\n\n' +
            'ASSIGNMENT:\n' + (state.assignment.text || '(not provided)') + '\n\n' +
            'RUBRIC:\n' + state.rubric.text;

        if (state.exampleAnswer.text) {
            prompt += '\n\nEXAMPLE ANSWER:\n' + state.exampleAnswer.text;
        }

        prompt +=
            '\n\nIdentify the grading criteria from the rubric. For each criterion assign a score.\n\n' +
            'IMPORTANT: Your entire response must be a single JSON object and nothing else. ' +
            'No explanatory text, no markdown, no code fences. ' +
            'The JSON object MUST have a top-level key called "criteria" that is an array, ' +
            'and a top-level key called "overallFeedback" that is a string.\n\n' +
            'Exact schema:\n' +
            '{"criteria":[{"name":"<string>","score":<number>,"maxScore":<number>,' +
            '"feedback":"<string>"}],"overallFeedback":"<string>"}\n\n' +
            'Rules:\n' +
            '- The root object MUST contain "criteria" (array) and "overallFeedback" (string)\n' +
            '- Each element in "criteria" MUST have "name", "score", "maxScore", "feedback"\n' +
            '- Extract criteria and their point values directly from the rubric\n' +
            '- Score each criterion from 0 to its maxScore\n' +
            '- Be fair but rigorous\n' +
            '- Output ONLY the JSON object, no other text';

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
            temperature: 0.2,
            stream: false
        };

        var token = Dashboard.api.getToken();
        var headers = { 'Content-Type': 'application/json' };
        if (token) headers['Authorization'] = 'Bearer ' + token;

        fetch('/api/dashboard/chat', {
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

    function extractJson(raw) {
        /* Strip markdown code fences anywhere in the string */
        var stripped = raw.replace(/```(?:json)?\s*/gi, '').replace(/```/g, '');
        stripped = stripped.trim();

        /* Try direct parse first */
        try { return JSON.parse(stripped); } catch (e) { /* fall through */ }

        /* Find the outermost { ... } and try that */
        var first = raw.indexOf('{');
        var last = raw.lastIndexOf('}');
        if (first >= 0 && last > first) {
            try { return JSON.parse(raw.substring(first, last + 1)); }
            catch (e) { /* fall through */ }
        }

        return null;
    }

    function findCriteria(obj) {
        /* Direct top-level match */
        if (obj.criteria && Array.isArray(obj.criteria))
            return obj.criteria;

        /* Check common alternative key names */
        var alts = ['rubric_criteria', 'grading_criteria', 'grades',
                    'rubricCriteria', 'gradingCriteria', 'results'];
        for (var i = 0; i < alts.length; i++) {
            if (obj[alts[i]] && Array.isArray(obj[alts[i]]))
                return obj[alts[i]];
        }

        /* Search one level of nesting for an array of criterion objects */
        var keys = Object.keys(obj);
        for (var k = 0; k < keys.length; k++) {
            var val = obj[keys[k]];
            if (val && typeof val === 'object' && !Array.isArray(val)) {
                if (val.criteria && Array.isArray(val.criteria))
                    return val.criteria;
            }
            if (Array.isArray(val) && val.length > 0 && val[0] &&
                typeof val[0].name === 'string' &&
                typeof val[0].score === 'number') {
                return val;
            }
        }
        return null;
    }

    function parseGradingResponse(content) {
        var obj = extractJson(content);
        if (!obj || typeof obj !== 'object') {
            return { error: 'Failed to parse grading response as JSON' };
        }

        var criteria = findCriteria(obj);
        if (!criteria) {
            return { error: 'Response missing "criteria" array' };
        }
        obj.criteria = criteria;

        var totalScore = 0;
        var totalMax = 0;
        for (var i = 0; i < obj.criteria.length; i++) {
            var c = obj.criteria[i];
            /* Normalize: accept max_score / maxScore / max variants */
            if (typeof c.maxScore === 'undefined' && typeof c.max_score === 'number')
                c.maxScore = c.max_score;
            if (typeof c.maxScore === 'undefined' && typeof c.max === 'number')
                c.maxScore = c.max;
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
